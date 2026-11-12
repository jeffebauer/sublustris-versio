// Regression tests for the bugs the overnight soak found (docs/research/soak-audit.md,
// tools/soak/soak.cpp). Each test is the smallest reproduction of one finding.

#include "DecayKnobFor.h"
#include "dsp/Engine.h"
#include "dsp/Filters.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using iv::ParamId;

struct Rig {
    iv::Engine e;
    int block;
    std::vector<float> inL, inR, outL, outR;
    uint32_t noise = 1;
    // ERA 0, the Neve alone (as ERA's default was until v1.0.42): the click watch's second difference reads
    // the wear's grit (its grain, clock and breath noise) as clicks everywhere; ERA's own clicks are test_era's.
    Rig(float fs, int b) : block(b), inL(size_t(b)), inR(size_t(b)), outL(size_t(b)), outR(size_t(b))
    {
        e.prepare(fs, b);
        e.setParam(ParamId::Era, 0.0f);
    }
    double phase = 0.0;
    // Click watch: the worst |second difference| of the output over its own recent level (10 ms).
    float y1[2] = {}, y2[2] = {}, dp[2] = {};
    float worstClick = 0.0f, maxD2 = 0.0f;
    // The quietest 1 ms of output since the last clearWatch().
    double minMs = 1e9, msAcc = 0.0;
    int msFill = 0;
    void clearWatch() { worstClick = 0.0f; maxD2 = 0.0f; minMs = 1e9; }

    // Runs `seconds` of input: noise at `level`, or a sine at `sineHz` (0 = noise, negative = a chord on
    // -sineHz); level 0 = silence.
    // Returns the output's mean power.
    double run(double seconds, float level, float sineHz = 0.0f)
    {
        const int blocks = int(seconds * e.sampleRate() / block);
        const int msLen = int(0.001f * e.sampleRate());
        const float a = 1.0f - std::exp(-1.0f / (0.01f * e.sampleRate()));
        double acc = 0.0;
        for (int k = 0; k < blocks; ++k) {
            for (int i = 0; i < block; ++i) {
                if (sineHz > 0.0f) {
                    phase += 2.0 * 3.14159265358979 * sineHz / e.sampleRate();
                    inL[size_t(i)] = inR[size_t(i)] = level * float(std::sin(phase));
                    continue;
                }
                if (sineHz < 0.0f) { // a chord on -sineHz: root, major third, fifth, octave
                    phase += 2.0 * 3.14159265358979 * -sineHz / e.sampleRate();
                    inL[size_t(i)] = inR[size_t(i)] = level * 0.25f
                        * float(std::sin(phase) + std::sin(phase * 1.26) + std::sin(phase * 1.5) + std::sin(phase * 2.0));
                    continue;
                }
                noise = noise * 1664525u + 1013904223u;
                inL[size_t(i)] = level * float(int32_t(noise)) * (1.0f / 2147483648.0f);
                noise = noise * 1664525u + 1013904223u;
                inR[size_t(i)] = level * float(int32_t(noise)) * (1.0f / 2147483648.0f);
            }
            e.process(inL.data(), inR.data(), outL.data(), outR.data(), block);
            for (int i = 0; i < block; ++i) {
                const float o[2] = {outL[size_t(i)], outR[size_t(i)]};
                double p = 0.0;
                for (int c = 0; c < 2; ++c) {
                    const float d2 = o[c] - 2.0f * y1[c] + y2[c];
                    const float ratio = std::fabs(d2) / (std::sqrt(dp[c]) + 1e-4f);
                    if (ratio > worstClick) worstClick = ratio;
                    if (std::fabs(d2) > maxD2) maxD2 = std::fabs(d2);
                    dp[c] += a * (d2 * d2 - dp[c]);
                    y2[c] = y1[c];
                    y1[c] = o[c];
                    p += 0.5 * double(o[c]) * o[c];
                }
                acc += p;
                msAcc += p;
                if (++msFill == msLen) {
                    minMs = std::min(minMs, msAcc / msLen);
                    msAcc = 0.0;
                    msFill = 0;
                }
            }
        }
        return acc / double(std::max(1, blocks * block));
    }
    void tap()
    {
        e.button(true, 0);
        run(0.05, 0.0f);
        e.button(false, 0);
    }
};

double db(double power) { return 10.0 * std::log10(power + 1e-30); }

// Soak finding 1: a tap that lands part-way into one of the Engine's 32-sample control
// chunks (any MIDI-note gate in the Plugin, the Renderer's sample-accurate presses) was
// read as a paint: the gesture reader's tick ran at the chunk's start, before the press,
// and `now - pressAt` wrapped round. The tap never unfroze.
void tapMidChunkUnfreezes()
{
    for (int offset : {0, 5, 31, 33, 100}) {
        Rig r(48000.0f, 128);
        r.run(1.0, 0.3f);
        r.e.button(true, 0); // freeze
        r.run(0.05, 0.0f);
        r.e.button(false, 0);
        r.run(0.5, 0.0f);
        const bool frozeOk = r.e.frozen();
        // A 50 ms tap, its press `offset` samples into a block.
        r.e.button(true, offset);
        r.run(128.0 / 48000.0, 0.0f);
        const bool paintedEarly = r.e.painting();
        r.run(0.05, 0.0f);
        r.e.button(false, 0);
        r.run(0.1, 0.0f);
        char what[160];
        std::snprintf(what, sizeof what, "a 50 ms tap pressed %d samples into a block unfreezes (no paint)", offset);
        check(frozeOk && !paintedEarly && !r.e.frozen(), what);
    }
    // The gate, as the Plugin's MIDI notes drive it.
    Rig r(44100.0f, 512);
    r.run(1.0, 0.3f);
    r.e.gate(true, 17);
    r.e.gate(false, 300);
    r.run(0.5, 0.0f);
    r.e.gate(true, 211);
    r.run(0.05, 0.0f);
    r.e.gate(false, 7);
    r.run(0.1, 0.0f);
    check(!r.e.frozen(), "a short gate (MIDI note) at odd offsets freezes, then a second one unfreezes");
}

// Soak finding 2: a SPACE flip soon after an unfreeze, while the twin (what was played over the
// pad) still sounded. The flip faded the big tank's wet but not the twin's, then cut the twin off
// at the flip's silent point (a click), and the new space's twin could replay stale sound. Now the
// twin fades with the flip and restarts clean (x7.6-9.9 on this tone; it measured x30-123).
void flipAfterUnfreezeFadesTheTwin()
{
    for (int from = 0; from < 3; ++from) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(from));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.7f));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(3.0, 0.3f, 220.0f);
        r.tap();                  // freeze
        r.run(2.0, 0.3f, 330.0f); // play into the twin
        r.run(0.5, 0.0f);
        r.tap();          // unfreeze; the flip waits for the 1 s glide back
        r.e.setParam(ParamId::Space, iv::switchToNormalised((from + 1) % 3));
        r.clearWatch();
        r.run(2.0, 0.0f);
        char what[160];
        std::snprintf(what, sizeof what, "a flip %d->%d after an unfreeze, the twin still sounding, doesn't click (worst x%.1f)", from,
                      (from + 1) % 3, r.worstClick);
        // 17, was 15, at v1.0.73: the wet's flat +3 dB (ADR 0049) reads x13.5 as x15.4 with no new click: the ratio's
        // 1e-4 floor is absolute, so the same click reads higher on a louder wet.
        check(r.worstClick < 17.0f, what);
    }
}

// Soak finding 3: a freeze that landed inside a SPACE flip's fade-out froze nothing: the tank
// restarted in the new space under the freeze. Now the flip gives way (the old space fades back
// in as the pad) and waits for the unfreeze, like every flip.
void freezeInsideAFlipKeepsThePad()
{
    for (int ms : {10, 25, 40}) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(1));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.7f));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(3.0, 0.3f);
        const double before = r.run(0.5, 0.0f);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(2));
        r.run(ms / 1000.0, 0.0f);
        r.tap();
        r.run(3.0, 0.0f);
        const double pad = r.run(2.0, 0.0f);
        char what[160];
        std::snprintf(what, sizeof what, "a freeze %d ms into a flip's fade-out keeps the sound (pad %.1f dB, before %.1f dB, space %d)", ms,
                      db(pad), db(before), r.e.activeSpace());
        check(r.e.frozen() && r.e.activeSpace() == 1 && db(pad) > db(before) - 8.0, what);
        r.tap(); // the flip goes ahead after the unfreeze
        r.run(1.5, 0.0f);
        check(r.e.activeSpace() == 2, "  ... and the flip happens after the unfreeze");
    }
}

// Soak finding 4: in a small HALL, SIZE jumps (CV, a fast twist) clicked loudly: the in-loop
// allpasses' lengths jumped with the knob, and crossing noon switched them in or out of the loop at
// full length (a dropout of stale sound). Now they glide, and join or leave the loop at one sample.
void smallHallSizeJumpsDontClick()
{
    const float moves[][2] = {{0.0f, 0.3f}, {0.0f, 0.1f}, {0.3f, 0.7f}, {0.7f, 0.3f}, {0.0f, 1.0f}};
    for (const auto& m : moves) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(1));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.6f));
        r.e.setParam(ParamId::Size, m[0]);
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(3.0, 0.3f, 700.0f);
        r.run(1.0, 0.0f);
        r.clearWatch();
        r.e.setParam(ParamId::Size, m[1]);
        r.run(1.5, 0.0f);
        char what[160];
        std::snprintf(what, sizeof what, "a small HALL's SIZE jump %.1f->%.1f in a tail doesn't click (worst x%.1f)", m[0], m[1], r.worstClick);
        check(r.worstClick < 6.0f, what);
    }
    // Unfreezing after SIZE moved across noon while frozen: the big tank takes SIZE back at once.
    Rig r(48000.0f, 48);
    r.e.setParam(ParamId::Space, iv::switchToNormalised(1));
    r.e.setParam(ParamId::Decay, decayKnobFor(0.6f));
    r.e.setParam(ParamId::Size, 1.0f);
    r.e.setParam(ParamId::Mix, 1.0f);
    r.e.reset();
    r.run(3.0, 0.3f);
    r.tap();
    r.run(3.0, 0.0f);
    r.e.setParam(ParamId::Size, 0.0f);
    const double before = r.run(1.0, 0.0f);
    r.clearWatch();
    r.tap();
    r.run(0.3, 0.0f);
    char what[160];
    std::snprintf(what, sizeof what, "a HALL unfrozen after SIZE 1->0 doesn't drop out or click (quietest ms %.1f dB under, worst x%.1f)",
                  db(before) - db(r.minMs), r.worstClick);
    check(db(r.minMs) > db(before) - 10.0 && r.worstClick < 8.0f, what);
}

// Soak finding 5 (a sound change, for the owner): an infinite tail fed loud enough to sit above
// the floor plunged when the input stopped (PLATE 27 dB, HALL 10 dB; in BLOOM also MASSIVE 5 dB)
// and the hold's makeup took a minute or two to bring it back. The floor judged it on its 1.5 s
// level, which lagged the falling tail; and a long BLOOM, still feeding the tank after the input
// stopped, had its hold level taken while the tail was rising. Now the floor judges a falling tail
// on a 50 ms level too (first only once the input was quiet, which missed an input fading out over
// seconds: a decaying hit still plunged 64 dB), and the hold waits for the tank's own input to go
// quiet. ERA's tape compression (the cassette's until v1.0.42), judged on the same lagging level, did the same (fixed alike).
// A tail now settles at the floor within a couple of dB and holds there.
void infiniteTailDoesntPlungeWhenTheInputStops()
{
    struct Case {
        int space;
        float attack;
        const char* name;
        bool fade;
    };
    const Case cases[] = {{0, 0.0f, "PLATE", false}, {1, 0.0f, "HALL", false}, {0, 0.5f, "PLATE BLOOM", false},
                          {2, 0.5f, "MASSIVE BLOOM", false}, {0, 0.0f, "PLATE, input fading", true}, {1, 0.0f, "HALL, input fading", true}};
    for (const Case& c : cases) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(c.space));
        r.e.setParam(ParamId::Size, 0.18f);
        r.e.setParam(ParamId::Decay, decayKnobFor(1.0f));
        r.e.setParam(ParamId::Time, 1.0f);
        r.e.setParam(ParamId::Attack, c.attack);
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(5.0, 0.7f);
        if (c.fade) // the input fades out over a few seconds instead of stopping (a decaying hit)
            for (float lvl = 0.7f; lvl > 0.0005f; lvl *= 0.9f) r.run(0.05, lvl);
        std::vector<double> levels;
        for (int s = 0; s < 45; ++s) levels.push_back(db(r.run(1.0, 0.0f)));
        const double lowest = *std::min_element(levels.begin(), levels.begin() + 30), end = levels.back();
        char what[160];
        std::snprintf(what, sizeof what, "%s infinite: no plunge when a loud input stops (lowest %.1f dB, %.1f under where it holds)", c.name,
                      lowest, end - lowest);
        check(end - lowest < 4.0, what);
    }
}

// A held endless tail keeps its spectrum, not just its level (v1.0.58, ADR 0008 amended; review 2's recommendation:
// the soak and hold tests measured the total alone, which stayed put while as built drained the top end for ever,
// 40+ dB at 4 kHz by 5 minutes on the review's pad, and its make-up lifted the lows alone). Six seconds of noise, then
// four minutes of silence at endless DECAY, TONE fully clockwise, PLATE and HALL; 10 s windows of 4th-order bands from
// 10 s after the input. Above 2 kHz the tail mellows by the light settle (kEndlessTopDb; PLATE 6.3 dB, HALL 1.7) and
// has settled by two minutes. Below: on bright material, whose total is its top end's, the hold's flat make-up lifts
// the rest as the top mellows (PLATE: under 500 Hz +8.3 dB, under 20 Hz +8.9, ~35 dB under the total; v1.0.57 +1.5 /
// +0.8 on this noise; on the review's pad +0.2 / +3.7, v1.0.57 -0.8 / +1.5), bounded here at 10 dB. (As built's drain
// depends on the material: it doesn't show on this noise, so this guards v1.0.58's settle, not its fix.) And the settle
// follows the playing's brightness (voicing::kEndlessBright*): a dark chord (220-440 Hz) in PLATE gets none, so its
// top end keeps where it settled (within 1 dB; the noise falls 6 dB).
void endlessHoldKeepsItsSpectrum()
{
    for (int c3 = 0; c3 < 3; ++c3) {
        const int space = c3 == 1 ? 1 : 0;
        const bool chord = c3 == 2;
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(space));
        r.e.setParam(ParamId::Decay, decayKnobFor(1.0f));
        r.e.setParam(ParamId::Tone, 1.0f);
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(6.0, chord ? 0.5f : 0.3f, chord ? -220.0f : 0.0f);
        iv::dsp::Biquad hi[2][2], lo[2][2], mid[2][2];
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) {
                hi[c][k].setHighpass(2000.0f, 0.7071f, 48000.0f);
                lo[c][k].setLowpass(20.0f, 0.7071f, 48000.0f);
                mid[c][k].setLowpass(500.0f, 0.7071f, 48000.0f);
            }
        std::vector<double> top, sub, under;
        r.run(10.0, 0.0f);
        for (int w = 0; w < 24; ++w) {
            double pt = 0.0, ps = 0.0, pu = 0.0;
            for (int b = 0; b < 10 * 1000; ++b) {
                r.run(0.001, 0.0f);
                for (int i = 0; i < r.block; ++i) {
                    const float o[2] = {r.outL[size_t(i)], r.outR[size_t(i)]};
                    for (int c = 0; c < 2; ++c) {
                        const float h = hi[c][1].process(hi[c][0].process(o[c])), l = lo[c][1].process(lo[c][0].process(o[c]));
                        const float u = mid[c][1].process(mid[c][0].process(o[c]));
                        pt += double(h) * h;
                        ps += double(l) * l;
                        pu += double(u) * u;
                    }
                }
            }
            top.push_back(db(pt));
            sub.push_back(db(ps));
            under.push_back(db(pu));
        }
        const double topFall = top[0] - top.back(), topLate = std::fabs(top.back() - top[11]);
        const double subRise = *std::max_element(sub.begin(), sub.end()) - sub[0];
        const double underRise = *std::max_element(under.begin(), under.end()) - under[0];
        char what[240];
        std::snprintf(what, sizeof what, "%s endless hold of %s, 4 min: >2 kHz falls %.1f dB (< %s), moves %.1f after 2 min (< 2); "
                      "<20 Hz rises %.1f (< 10); <500 Hz %.1f (< 10)", space ? "HALL" : "PLATE", chord ? "a dark chord" : "noise", topFall,
                      chord ? "1, no settle" : "12", topLate, subRise, underRise);
        check((chord ? std::fabs(topFall) < 1.0 : topFall < 12.0) && topLate < 2.0 && subRise < 10.0 && underRise < 10.0, what);
    }
}

// Soak finding 6 (found with REVERSE, kept for SHIMMER, ADR 0024: ATTACK's third position keeps
// running through flips): a SPACE flip while playing in REVERSE cleared REVERSE's recording line, which
// then held a hard edge where the input was cut off: the swell clicked when its taps reached it, up
// to a TIME after the flip. REVERSE doesn't depend on SPACE, so it now keeps running through flips
// (faded with the wet). After the flip's own fade, the sharpest step stays near the steady sound's.
void thirdAttackFlipWhilePlayingDoesntClickLater()
{
    for (int from = 0; from < 3; ++from) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(from));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.7f));
        r.e.setParam(ParamId::Time, 0.7f);
        r.e.setParam(ParamId::Attack, iv::switchToNormalised(2));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(3.0, 0.3f, -330.0f);
        r.clearWatch();
        r.run(1.0, 0.3f, -330.0f);
        const float steady = r.maxD2;
        r.e.setParam(ParamId::Space, iv::switchToNormalised((from + 1) % 3));
        r.run(0.15, 0.3f, -330.0f); // the flip's own fade
        r.clearWatch();
        r.run(3.0, 0.3f, -330.0f);
        char what[160];
        std::snprintf(what, sizeof what, "SHIMMER: a flip %d->%d while playing doesn't click later (sharpest step x%.1f the steady sound's)", from,
                      (from + 1) % 3, r.maxD2 / steady);
        check(r.maxD2 < 2.6f * steady, what); // it measured x3.0-6.2; a pure sine still shows x7-9 (see the audit)
    }
}

// Soak finding 7 (found with REVERSE, kept for SHIMMER): REVERSE coming in while playing (an ATTACK switch, or the unfreeze that ends
// REVERSE's rest) fed its freshly restarted short reverb the whole swell at once, which clicked
// when it first came round the reverb's lines, 25-80 ms later (x18-46). The taps now fade in.
void thirdAttackComingInDoesntClick()
{
    for (int sp = 0; sp < 3; ++sp)
        for (int how = 0; how < 2; ++how) {
            Rig r(48000.0f, 48);
            r.e.setParam(ParamId::Space, iv::switchToNormalised(sp));
            r.e.setParam(ParamId::Decay, decayKnobFor(0.6f));
            r.e.setParam(ParamId::Time, 0.6f);
            r.e.setParam(ParamId::Mix, 1.0f);
            r.e.setParam(ParamId::Attack, iv::switchToNormalised(how == 0 ? 2 : 0));
            r.e.reset();
            r.run(3.0, 0.3f, -330.0f);
            if (how == 0) { // freeze (SHIMMER rests), then unfreeze
                r.e.button(true, 0);
                r.run(0.05, 0.3f, -330.0f);
                r.e.button(false, 0);
                r.run(2.0, 0.3f, -330.0f);
            }
            r.clearWatch();
            if (how == 0) {
                r.e.button(true, 0);
                r.run(0.05, 0.3f, -330.0f);
                r.e.button(false, 0);
            } else {
                r.e.setParam(ParamId::Attack, iv::switchToNormalised(2));
            }
            r.run(0.5, 0.3f, -330.0f);
            char what[160];
            std::snprintf(what, sizeof what, "space %d: SHIMMER coming in (%s) while playing doesn't click (worst x%.1f)", sp,
                          how == 0 ? "unfreeze" : "ATTACK switch", r.worstClick);
            check(r.worstClick < 6.0f, what);
        }
}

// Soak finding 8: letting go after painting lifted the pad's ceiling, and the paint's "room left"
// jumped to full while the paint was still fading: a burst of the twin into the pad. The room now
// only moves while painting.
void paintReleaseDoesntBurst()
{
    for (int sp = 0; sp < 3; ++sp) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(sp));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.6f));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(3.0, 0.3f, -220.0f);
        r.tap();                    // freeze
        r.run(30.0, 0.3f, -330.0f); // the pad settles, the twin plays
        r.e.button(true, 0);
        r.run(4.0, 0.3f, -330.0f);  // paint
        r.clearWatch();
        r.e.button(false, 0);       // let go
        r.run(0.3, 0.3f, -330.0f);
        char what[160];
        std::snprintf(what, sizeof what, "space %d: letting go after painting doesn't burst (worst x%.1f)", sp, r.worstClick);
        check(r.worstClick < 4.0f, what);
    }
}

// Soak finding 9: freezing a tail that was dying fast (a short DECAY, the input just stopped) gave
// a pad that swelled for minutes, 25-55 dB: its ceiling was taken from the 8 s level, which still
// remembered the tail from before the freeze, and the makeup lifted the pad up towards it. The
// ceiling is now taken from the pad's own level when the 8 s level is still well above it.
void padFrozenFromADyingTailHolds()
{
    struct Case {
        int space;
        double wait;
    };
    const Case cases[] = {{0, 0.5}, {0, 1.0}, {1, 1.0}, {0, 0.3}, {1, 0.3}};
    for (const Case& c : cases) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(c.space));
        r.e.setParam(ParamId::Decay, decayKnobFor(c.wait < 0.4 ? 0.15f : 0.0f)); // the 0.3 s cases: a slightly longer tail, frozen at -50..-70 dB
        r.e.setParam(ParamId::Size, 0.0f);
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(4.0, 0.3f);
        r.run(c.wait, 0.0f);
        r.tap();
        r.run(28.0, 0.0f);
        const double early = db(r.run(4.0, 0.0f));
        r.run(120.0, 0.0f);
        const double late = db(r.run(4.0, 0.0f));
        char what[160];
        std::snprintf(what, sizeof what, "space %d: a pad frozen %.1f s after a short tail holds (%.1f dB at 30 s, %.1f dB at 2.5 min)", c.space, c.wait,
                      early, late);
        check(std::fabs(late - early) < 2.0, what);
    }
}

// The same plunge from ERA's tape compression (the cassette's until v1.0.42), which also judged the tail on
// the 1.5 s level: an infinite PLATE at ERA 0.82, a loud tone, then silence (soak seed 225's settings).
// v1.0.74 (ADR 0050): 5.7 dB, was 3.7 (limit 4), and not a plunge: the tank's own slow beating on one lone sine, with less modulation
// to smear it. It's this tone at this rate: 2.8 at the module's 48 kHz (v1.0.73 3.2); chords 1.0-1.4, a pad 1.1. Limit 6.5.
void cassetteCompressionDoesntPlunge()
{
    Rig r(44100.0f, 64);
    r.e.setParam(ParamId::Space, iv::switchToNormalised(0));
    r.e.setParam(ParamId::Size, 0.8127f);
    r.e.setParam(ParamId::Decay, decayKnobFor(1.0f));
    r.e.setParam(ParamId::Era, 0.82f);
    r.e.setParam(ParamId::Tone, 0.37f);
    r.e.setParam(ParamId::Drift, 0.947f);
    r.e.setParam(ParamId::Time, 0.49f);
    r.e.setParam(ParamId::Mix, 1.0f);
    r.e.reset();
    r.run(4.0, 0.582f, 1000.0f);
    std::vector<double> levels;
    for (int s = 0; s < 60; ++s) levels.push_back(db(r.run(1.0, 0.0f)));
    const double lowest = *std::min_element(levels.begin(), levels.begin() + 30);
    const double end = 0.5 * (levels[58] + levels[59]);
    char what[160];
    std::snprintf(what, sizeof what, "PLATE infinite, ERA 0.82's tape compression: no plunge when a loud tone stops (lowest %.1f dB, %.1f under where it holds)",
                  lowest, end - lowest);
    check(end - lowest < 6.5, what);
}

// ...and with TIME and SIZE still gliding when the flip reaches its silent point (found with REVERSE,
// kept for SHIMMER): the flip snapped
// REVERSE's taps and short reverb to their targets, a jump now that REVERSE runs on (a click
// 20-35 ms into the fade back in; confirmation soak). Now they glide on, as without a flip.
void thirdAttackFlipWhileTimeGlidesDoesntClick()
{
    // SHIMMER plays the forward wet (REVERSE replaced it), so the flip's own restart is heard: a sine
    // through a freshly restarted PLATE starts with a small edge in NORMAL too (x3.6-4 its steady
    // sound's sharpest step). The test asks what it was for: the third position adds nothing to a flip.
    auto sharpestAt = [](int from, bool flip, int attack, double offset) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(from));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.7f));
        r.e.setParam(ParamId::Time, 0.4f);
        r.e.setParam(ParamId::Size, 0.3f);
        r.e.setParam(ParamId::Attack, iv::switchToNormalised(attack));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(4.0 + offset, 0.3f, -330.0f);
        if (flip) r.e.setParam(ParamId::Space, iv::switchToNormalised((from + 1) % 3));
        r.e.setParam(ParamId::Time, 0.5f); // both glide on past the flip's silent point (~50 ms)
        r.e.setParam(ParamId::Size, 0.5f);
        r.run(0.045, 0.3f, -330.0f);
        r.clearWatch();
        r.run(0.2, 0.3f, -330.0f);
        return r.worstClick; // each sample's sharpness against its own recent sound's (brightness cancels)
    };
    // The restart's own edge depends on where in the waveform the flip lands (x3-11 in NORMAL), so
    // each mode's worst over a few flip moments.
    auto sharpest = [&](int from, bool flip, int attack = 2) {
        float worst = 0.0f;
        for (double offset : {0.0, 0.0131, 0.0277, 0.0413}) worst = std::fmax(worst, sharpestAt(from, flip, attack, offset));
        return worst;
    };
    for (int from = 0; from < 3; ++from) {
        const float withFlip = sharpest(from, true), normal = sharpest(from, true, 0);
        char what[180];
        std::snprintf(what, sizeof what,
                      "SHIMMER: a flip %d->%d while TIME and SIZE glide adds no click (worst x%.1f, NORMAL's x%.1f)",
                      from, (from + 1) % 3, withFlip, normal);
        check(withFlip < 1.5f * normal, what);
    }
}

} // namespace

// The owner's "slight burst of noise" (v1.0.49, rack): switching ONSET (SHIMMER to BLOOM) with DECAY,
// SIZE and TIME past noon. BLOOM's attack is TIME, the others' MASSIVE's own or none, so the swell's
// attack glided across the change and its taps swept over seconds of their line in ~0.3 s, re-set once
// a chunk: scrambled, sped-up sound into the tank, hiss in the tail's highs for its whole length (into
// or out of BLOOM, x18-53 the steady sound's sharpest step here). Now the swell dips out, restarts in
// its new voicing and dips back in. And a SPACE flip (HALL to and from MASSIVE) faded the tank's input
// after the input diffusers and the swell, so the predelay's jump at the silent point (and the swell's
// cleared line under an unfaded input) rang into the new space as clicks (HALL to MASSIVE x2.2 here);
// now the input side fades too. Both while a chord plays, after the change's own dip.
void onsetAndSpaceChangesDontBurst()
{
    // The sharpest step over the 3 s after a change at 4 s (after its own 0.15 s dip or fade), against the
    // sharpest of the two settings' own over the same 3 s unchanged (SHIMMER's octave is sharper).
    auto worst = [](int space, int onset, int toSpace, int toOnset) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(space));
        r.e.setParam(ParamId::Decay, decayKnobFor(0.75f));
        r.e.setParam(ParamId::Size, 0.75f);
        r.e.setParam(ParamId::Time, 0.75f);
        r.e.setParam(ParamId::Attack, iv::switchToNormalised(onset));
        r.e.setParam(ParamId::Mix, 1.0f);
        r.e.reset();
        r.run(4.0, 0.3f, -220.0f);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(toSpace));
        r.e.setParam(ParamId::Attack, iv::switchToNormalised(toOnset));
        r.run(0.15, 0.3f, -220.0f);
        r.clearWatch();
        r.run(3.0, 0.3f, -220.0f);
        return r.maxD2;
    };
    char what[160];
    for (int sp = 1; sp < 3; ++sp)
        for (int from = 0; from < 3; ++from)
            for (int to = 0; to < 3; ++to) {
                if (to == from) continue;
                const float steady = std::max(worst(sp, from, sp, from), worst(sp, to, sp, to));
                const float x = worst(sp, from, sp, to) / steady;
                std::snprintf(what, sizeof what, "%s: ONSET %d->%d while playing doesn't burst (sharpest step x%.2f the steady sound's)",
                              sp == 1 ? "HALL" : "MASSIVE", from, to, x);
                check(x < 1.5f, what);
            }
    for (int from = 1; from < 3; ++from) {
        const float steady = std::max(worst(from, 0, from, 0), worst(3 - from, 0, 3 - from, 0));
        const float x = worst(from, 0, 3 - from, 0) / steady;
        std::snprintf(what, sizeof what, "a flip %s while playing doesn't burst (sharpest step x%.2f the steady sound's)",
                      from == 1 ? "HALL->MASSIVE" : "MASSIVE->HALL", x);
        check(x < 1.5f, what);
    }
}

int main()
{
    tapMidChunkUnfreezes();
    flipAfterUnfreezeFadesTheTwin();
    freezeInsideAFlipKeepsThePad();
    smallHallSizeJumpsDontClick();
    infiniteTailDoesntPlungeWhenTheInputStops();
    thirdAttackFlipWhilePlayingDoesntClickLater();
    thirdAttackComingInDoesntClick();
    paintReleaseDoesntBurst();
    padFrozenFromADyingTailHolds();
    cassetteCompressionDoesntPlunge();
    thirdAttackFlipWhileTimeGlidesDoesntClick();
    onsetAndSpaceChangesDontBurst();
    endlessHoldKeepsItsSpectrum();
    std::printf("%s\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
