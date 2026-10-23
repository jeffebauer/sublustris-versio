// Regression tests for the bugs the overnight soak found (docs/research/soak-audit.md,
// tools/soak/soak.cpp). Each test is the smallest reproduction of one finding.

#include "dsp/Engine.h"

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
    Rig(float fs, int b) : block(b), inL(size_t(b)), inR(size_t(b)), outL(size_t(b)), outR(size_t(b)) { e.prepare(fs, b); }
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
        r.e.setParam(ParamId::Decay, 0.7f);
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
        check(r.worstClick < 15.0f, what);
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
        r.e.setParam(ParamId::Decay, 0.7f);
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
        r.e.setParam(ParamId::Decay, 0.6f);
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
    r.e.setParam(ParamId::Decay, 0.6f);
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
// quiet. ERA's cassette compression, judged on the same lagging level, did the same (fixed alike).
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
        r.e.setParam(ParamId::Decay, 1.0f);
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

// Soak finding 6: a SPACE flip while playing in REVERSE cleared REVERSE's recording line, which
// then held a hard edge where the input was cut off: the swell clicked when its taps reached it, up
// to a TIME after the flip. REVERSE doesn't depend on SPACE, so it now keeps running through flips
// (faded with the wet). After the flip's own fade, the sharpest step stays near the steady sound's.
void reverseFlipWhilePlayingDoesntClickLater()
{
    for (int from = 0; from < 3; ++from) {
        Rig r(48000.0f, 48);
        r.e.setParam(ParamId::Space, iv::switchToNormalised(from));
        r.e.setParam(ParamId::Decay, 0.7f);
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
        std::snprintf(what, sizeof what, "REVERSE: a flip %d->%d while playing doesn't click later (sharpest step x%.1f the steady sound's)", from,
                      (from + 1) % 3, r.maxD2 / steady);
        check(r.maxD2 < 2.6f * steady, what); // it measured x3.0-6.2; a pure sine still shows x7-9 (see the audit)
    }
}

// Soak finding 7: REVERSE coming in while playing (an ATTACK switch, or the unfreeze that ends
// REVERSE's rest) fed its freshly restarted short reverb the whole swell at once, which clicked
// when it first came round the reverb's lines, 25-80 ms later (x18-46). The taps now fade in.
void reverseComingInDoesntClick()
{
    for (int sp = 0; sp < 3; ++sp)
        for (int how = 0; how < 2; ++how) {
            Rig r(48000.0f, 48);
            r.e.setParam(ParamId::Space, iv::switchToNormalised(sp));
            r.e.setParam(ParamId::Decay, 0.6f);
            r.e.setParam(ParamId::Time, 0.6f);
            r.e.setParam(ParamId::Mix, 1.0f);
            r.e.setParam(ParamId::Attack, iv::switchToNormalised(how == 0 ? 2 : 0));
            r.e.reset();
            r.run(3.0, 0.3f, -330.0f);
            if (how == 0) { // freeze (REVERSE rests), then unfreeze
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
            std::snprintf(what, sizeof what, "space %d: REVERSE coming in (%s) while playing doesn't click (worst x%.1f)", sp,
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
        r.e.setParam(ParamId::Decay, 0.6f);
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
        r.e.setParam(ParamId::Decay, c.wait < 0.4 ? 0.15f : 0.0f); // the 0.3 s cases: a slightly longer tail, frozen at -50..-70 dB
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

// The same plunge from ERA's cassette compression, which also judged the tail on the 1.5 s level:
// an infinite PLATE in the cassette range, a loud tone, then silence (soak seed 225's settings).
void cassetteCompressionDoesntPlunge()
{
    Rig r(44100.0f, 64);
    r.e.setParam(ParamId::Space, iv::switchToNormalised(0));
    r.e.setParam(ParamId::Size, 0.8127f);
    r.e.setParam(ParamId::Decay, 1.0f);
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
    std::snprintf(what, sizeof what, "PLATE infinite, ERA's cassette: no plunge when a loud tone stops (lowest %.1f dB, %.1f under where it holds)",
                  lowest, end - lowest);
    check(end - lowest < 4.0, what);
}

} // namespace

int main()
{
    tapMidChunkUnfreezes();
    flipAfterUnfreezeFadesTheTwin();
    freezeInsideAFlipKeepsThePad();
    smallHallSizeJumpsDontClick();
    infiniteTailDoesntPlungeWhenTheInputStops();
    reverseFlipWhilePlayingDoesntClickLater();
    reverseComingInDoesntClick();
    paintReleaseDoesntBurst();
    padFrozenFromADyingTailHolds();
    cassetteCompressionDoesntPlunge();
    std::printf("%s\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
    return failures ? 1 : 0;
}
