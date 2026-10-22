// M5: the Plate, the freeze twin, freeze and paint (SPEC §4.2, §3 "Button +
// Gate", §7 M5; ADR 0006, 0009, 0010, 0011, 0013). The gesture reader's
// taps and holds (button and gate alike); the Plate's T60, stability and
// metallic check; all three SPACEs matched in loudness; a frozen pad holds
// its level for 5 minutes in every SPACE; painting adds a quiet layer to the
// pad, boundedly; unfreeze and freeze are click-free; the twin answers SIZE
// and DECAY while frozen and fades back out to the pad; REVERSE rests while
// frozen.

#include "Metrics.h"
#include "dsp/Engine.h"
#include "params/GestureReader.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;
constexpr float kSr = 48000.0f;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

uint32_t gSeed = 23;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}

struct Stereo {
    std::vector<float> l, r;
};

struct Press {
    double at;
    bool   down;
    bool   viaGate = false;
};

struct Setting {
    int   space = 1, attack = 0;
    float decay = 0.5f, size = 0.5f, mix = 1.0f, time = 0.0f;
    float mod = 1.0f;  // the tanks' own modulation (Engine::setModulationScale)
    float era = -1.0f; // < 0: ERA's default
};

// knobAt: optional {seconds, SIZE, DECAY} change while running.
struct KnobMove {
    double at;
    float  size, decay;
};

Stereo render(const std::vector<float>& in, const Setting& s, const std::vector<Press>& presses = {},
              const std::vector<KnobMove>& moves = {}, std::vector<int>* frozenAt = nullptr)
{
    iv::Engine e;
    e.prepare(kSr, 48);
    e.setParam(iv::ParamId::Space, iv::switchToNormalised(s.space));
    e.setParam(iv::ParamId::Attack, iv::switchToNormalised(s.attack));
    e.setParam(iv::ParamId::Decay, s.decay);
    e.setParam(iv::ParamId::Size, s.size);
    e.setParam(iv::ParamId::Mix, s.mix);
    e.setParam(iv::ParamId::Time, s.time);
    if (s.era >= 0.0f) e.setParam(iv::ParamId::Era, s.era);
    e.setModulationScale(s.mod);
    e.reset();
    Stereo out{std::vector<float>(in.size()), std::vector<float>(in.size())};
    size_t pi = 0, mi = 0;
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        while (pi < presses.size() && size_t(presses[pi].at * kSr) < pos + size_t(n)) {
            const int off = int(size_t(presses[pi].at * kSr) - pos);
            if (presses[pi].viaGate) e.gate(presses[pi].down, off);
            else e.button(presses[pi].down, off);
            ++pi;
        }
        while (mi < moves.size() && size_t(moves[mi].at * kSr) < pos + size_t(n)) {
            e.setParam(iv::ParamId::Size, moves[mi].size);
            e.setParam(iv::ParamId::Decay, moves[mi].decay);
            ++mi;
        }
        e.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        if (frozenAt) frozenAt->push_back(e.frozen() ? (e.painting() ? 2 : 1) : 0);
    }
    return out;
}

double rmsDb(const Stereo& s, double from, double to)
{
    const size_t a = size_t(from * kSr), b = size_t(to * kSr);
    double sum = 0.0;
    for (size_t i = a; i < b && i < s.l.size(); ++i) sum += double(s.l[i]) * s.l[i] + double(s.r[i]) * s.r[i];
    return 10.0 * std::log10(sum / double(2 * (b - a)) + 1e-30);
}

bool finite(const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}

// ---- The gesture reader itself (ADR 0009–0011) ---------------------------------

void gestures()
{
    using E = iv::GestureReader::Event;
    iv::GestureReader g;
    g.prepare(kSr);
    const uint64_t ms = 48;
    bool ok = g.edge(true, 0) == E::Freeze && g.frozen();       // a press freezes at once
    ok &= g.tick(500 * ms) == E::None;                           // holding the freezing press: nothing
    ok &= g.edge(false, 600 * ms) == E::None && g.frozen();      // its release: nothing
    ok &= g.edge(true, 1000 * ms) == E::None;                    // frozen: a press opens the window
    ok &= g.tick(1100 * ms) == E::None;
    ok &= g.edge(false, 1200 * ms) == E::Unfreeze && !g.frozen(); // released inside 0.3 s: unfreeze
    check(ok, "gestures: a press freezes at once; a tap while frozen unfreezes on its release");

    ok = g.edge(true, 2000 * ms) == E::Freeze;
    ok &= g.edge(false, 2050 * ms) == E::None;
    ok &= g.edge(true, 3000 * ms) == E::None;
    ok &= g.tick(3299 * ms) == E::None;                           // still inside the window
    ok &= g.tick(3301 * ms) == E::PaintStart && g.painting();    // held past 0.3 s: paint
    ok &= g.edge(false, 6000 * ms) == E::PaintStop && g.frozen() && !g.painting(); // release: stays frozen
    check(ok, "gestures: held past 0.3 s while frozen paints; its release stays frozen");

    // Through the Engine: the gate does the same as the button (ADR 0011).
    std::vector<float> in(size_t(3.0f * kSr), 0.0f);
    std::vector<int> viaButton, viaGate;
    const std::vector<Press> b = {{0.5, true}, {0.55, false}, {1.0, true}, {1.6, false}, {2.0, true}, {2.1, false}};
    std::vector<Press> gp = b;
    for (auto& p : gp) p.viaGate = true;
    render(in, {}, b, {}, &viaButton);
    render(in, {}, gp, {}, &viaGate);
    const auto at = [&](const std::vector<int>& v, double t) { return v[size_t(t * kSr / 48.0)]; };
    ok = viaButton == viaGate && at(viaButton, 0.4) == 0 && at(viaButton, 0.7) == 1 && at(viaButton, 1.5) == 2
      && at(viaButton, 1.8) == 1 && at(viaButton, 2.5) == 0;
    check(ok, "Engine: button and gate give the same gestures (freeze, paint, stay frozen, unfreeze)");
}

// ---- The Plate (SPEC §4.2) --------------------------------------------------------

double lowBandT60(const Stereo& out)
{
    std::vector<float> lp(out.l.size());
    const float c = 1.0f - std::exp(-2.0f * 3.14159265f * 500.0f / kSr);
    float a = 0.0f, b = 0.0f;
    for (size_t i = 0; i < lp.size(); ++i) {
        a += c * (0.5f * (out.l[i] + out.r[i]) - a);
        b += c * (a - b);
        lp[i] = b;
    }
    return iv::metrics::compute({lp}, kSr).t60S;
}

void plate()
{
    // Where the tail spans at least ~3 passes round the figure 8 (one pass is
    // ~0.65 s at SIZE noon): shorter T60s are set by the structure itself, not
    // by DECAY (measured +11 % at DECAY 0.2, 1.4 passes; SpaceVoicing.h).
    for (float d : {0.4f, 0.6f}) {
        const double want = 1.0 / iv::map::decayRate(d, 0);
        std::vector<float> in(size_t((2.0 * want + 3.0) * kSr), 0.0f);
        in[100] = 0.5f;
        Setting s;
        s.space = 0;
        s.decay = d;
        const double got = lowBandT60(render(in, s));
        char msg[160];
        std::snprintf(msg, sizeof msg, "PLATE T60 at DECAY %.1f: %.2f s, mapping %.2f s (within 10%%)", d, got, want);
        check(std::isfinite(got) && std::fabs(got / want - 1.0) < 0.10, msg);
    }

    std::vector<float> hits(size_t(10.0f * kSr), 0.0f);
    for (int h = 0; h < 3; ++h)
        for (size_t i = 0; i < 1200; ++i) hits[size_t(h) * 24000 + i] = 0.6f * noise() * std::exp(-float(i) / 200.0f);
    for (float z : {0.0f, 1.0f})
        for (float d : {0.0f, 0.6f}) {
            Setting s;
            s.space = 0;
            s.size = z;
            s.decay = d;
            const Stereo out = render(hits, s);
            const auto m = iv::metrics::compute({out.l, out.r}, kSr);
            char msg[160];
            std::snprintf(msg, sizeof msg, "PLATE not metallic at SIZE %.0f DECAY %.1f: ringing %.1f dB", z, d,
                          std::isfinite(m.ringingDb) ? m.ringingDb : 0.0);
            check(finite(out) && !m.ringing, msg);
        }

    // Every output tap inside its line: a tap past the end read off the buffer at full SIZE and
    // clicked once a lap in the left channel only (owner, M5 listening). Each channel on its own.
    {
        std::vector<float> tone(size_t(8.0f * kSr), 0.0f);
        for (size_t i = 0; i < size_t(4.0f * kSr); ++i)
            tone[i] = 0.25f * std::sin(2.0f * 3.14159265f * 330.0f * float(i) / kSr) * std::fmin(1.0f, float(i) / 2400.0f);
        Setting s;
        s.space = 0;
        s.size = 1.0f;
        s.decay = 0.7f;
        const Stereo out = render(tone, s);
        const long cl = iv::metrics::compute({out.l}, kSr).clickCount, cr = iv::metrics::compute({out.r}, kSr).clickCount;
        check(cl == 0 && cr == 0, "PLATE at full SIZE: no clicks in either channel (L " + std::to_string(cl) + ", R " +
                                      std::to_string(cr) + ")");
    }

    std::vector<float> burst(size_t(8.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(0.5f * kSr); ++i) burst[i] = 0.3f * noise();
    int bad = 0;
    for (float d : {0.0f, 0.8f, 1.0f})
        for (float z : {0.0f, 1.0f})
            for (int a : {0, 1, 2}) {
                Setting s;
                s.space = 0;
                s.decay = d;
                s.size = z;
                s.attack = a;
                s.time = 0.5f;
                const Stereo out = render(burst, s);
                float pk = 0.0f;
                for (float v : out.l) pk = std::fmax(pk, std::fabs(v));
                bad += !finite(out) || pk > 2.0f;
            }
    check(bad == 0, "PLATE stable at 18 DECAY x SIZE x ATTACK corners");
}

// SPEC §7 M5: PLATE, HALL and MASSIVE loudness within ±1.5 dB.
void loudness()
{
    std::vector<float> in(size_t(12.0f * kSr), 0.0f);
    for (int k = 0; k < 4; ++k)
        for (size_t i = 0; i < 12000; ++i) in[size_t(k) * 96000 + i] = 0.3f * noise();
    double lo = 1e9, hi = -1e9;
    std::string detail;
    for (int space : {0, 1, 2})
        for (float z : {0.0f, 1.0f}) {
            Setting s;
            s.space = space;
            s.size = z;
            const double db = rmsDb(render(in, s), 0.0, 12.0);
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
            char buf[40];
            std::snprintf(buf, sizeof buf, " %c/%.0f %.1f", "PHM"[space], z, db);
            detail += buf;
        }
    check(hi - lo <= 3.0, "PLATE / HALL / MASSIVE loudness within ±1.5 dB:" + detail);
}

// ---- Freeze (ADR 0009) ---------------------------------------------------------------

void padHolds()
{
    std::vector<float> in(size_t(305.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(3.0f * kSr); ++i) in[i] = 0.25f * noise();
    for (int space : {0, 1, 2}) {
        Setting s;
        s.space = space;
        const Stereo out = render(in, s, {{2.5, true}, {2.6, false}});
        double lo = 1e9, hi = -1e9;
        for (double t = 25.0; t < 300.0; t += 25.0) {
            const double db = rmsDb(out, t, t + 2.0);
            lo = std::fmin(lo, db);
            hi = std::fmax(hi, db);
        }
        char msg[160];
        std::snprintf(msg, sizeof msg, "%s pad holds 5 min: %.2f .. %.2f dBFS (within ±0.5 dB, from 25 s)",
                      space == 0 ? "PLATE" : (space == 1 ? "HALL" : "MASSIVE"), lo, hi);
        check(finite(out) && hi - lo <= 1.0, msg);
    }
}

// Playing over the pad is not dry: the twin answers, and SIZE and DECAY
// play it (longer DECAY, longer twin tail), but it always fades back out
// (within kTwinMaxT60), leaving the pad as it was.
void twin()
{
    std::vector<float> in(size_t(14.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(1.0f * kSr); ++i) in[i] = 0.25f * noise();          // the pad's source
    for (size_t i = size_t(6.0f * kSr); i < size_t(6.05f * kSr); ++i) in[i] = 0.5f * noise(); // a hit over the pad
    const std::vector<Press> freeze = {{1.5, true}, {1.6, false}};
    Setting s;
    const Stereo withHit = render(in, s, freeze, {{3.0, 0.5f, 0.1f}});
    std::vector<float> noHit = in;
    for (size_t i = size_t(6.0f * kSr); i < size_t(6.05f * kSr); ++i) noHit[i] = 0.0f;
    const Stereo padOnly = render(noHit, s, freeze, {{3.0, 0.5f, 0.1f}});
    const Stereo longer = render(in, s, freeze, {{3.0, 0.5f, 0.9f}});
    // The hit's own reverb = with the hit minus the pad alone (deterministic).
    auto diffDb = [&](const Stereo& a, double t0, double t1) {
        double sum = 0.0;
        for (size_t i = size_t(t0 * kSr); i < size_t(t1 * kSr); ++i) {
            const double l = double(a.l[i]) - padOnly.l[i], r = double(a.r[i]) - padOnly.r[i];
            sum += l * l + r * r;
        }
        return 10.0 * std::log10(sum / (2.0 * (t1 - t0) * kSr) + 1e-30);
    };
    const double shortTail = diffDb(withHit, 7.0, 8.0), longTail = diffDb(longer, 7.0, 8.0);
    const double early = diffDb(withHit, 6.05, 6.3);
    const double gone = diffDb(longer, 13.5, 14.0); // 7.5 s after the hit, DECAY at infinite
    char msg[240];
    std::snprintf(msg, sizeof msg, "twin: a hit over the pad gets a reverb (%.1f dBFS), DECAY plays it "
                                   "(1-2 s later: %.1f short vs %.1f long), and even at infinite it fades back out (%.1f, 7.5 s later)",
                  early, shortTail, longTail, gone);
    check(early > -60.0 && longTail > shortTail + 6.0 && gone < early - 40.0, msg);
}

void paint()
{
    std::vector<float> in(size_t(60.0f * kSr));
    for (auto& x : in) x = 0.25f * noise();
    Setting s;
    const Stereo out = render(in, s, {{1.0, true}, {1.1, false}, {30.0, true}, {45.0, false}});
    const double before = rmsDb(out, 26.0, 29.0), during = rmsDb(out, 40.0, 44.0), after = rmsDb(out, 55.0, 59.0);
    char msg[200];
    std::snprintf(msg, sizeof msg, "paint: a quiet layer grows into the pad while held (%.1f -> %.1f dBFS, more than +0.3 dB, "
                                   "at most +1.5 dB + the twin: never dominant) and stays after (%.1f)", before, during, after);
    check(finite(out) && during > before + 0.3 && during < before + 3.0 && after > before, msg);
}

void clickFree()
{
    const size_t n = size_t(12.0f * kSr);
    std::vector<float> in(n);
    for (size_t i = 0; i < n; ++i) in[i] = 0.2f * std::sin(2.0f * 3.14159265f * 220.0f * float(i) / kSr) + 0.02f * noise();
    for (int space : {0, 1, 2}) {
        Setting s;
        s.space = space;
        s.mix = 0.5f;
        const Stereo out = render(in, s, {{2.0, true}, {2.1, false}, {5.0, true}, {6.0, false}, {8.0, true}, {8.1, false}},
                                  {{3.5, 0.9f, 0.9f}});
        const auto m = iv::metrics::compute({out.l, out.r}, kSr);
        check(finite(out) && m.clickCount == 0, std::string("freeze, paint, unfreeze (knobs moved while frozen) in ") +
                                                    "PHM"[space] + ": no clicks (" + std::to_string(m.clickCount) + ")");
    }
}

// ADR 0015: REVERSE rests while frozen (a still pad reversed is the same pad).
void reverseRestsWhileFrozen()
{
    std::vector<float> in(size_t(10.0f * kSr), 0.0f);
    for (size_t i = 0; i < size_t(2.0f * kSr); ++i) in[i] = 0.25f * noise();
    Setting s;
    s.attack = 2;
    s.time = 0.4f;
    const Stereo out = render(in, s, {{2.0, true}, {2.1, false}});
    // A pad played forwards is steady; one still reversing would cut out at
    // every segment edge. Steady = small spread of 50 ms levels.
    double lo = 1e9, hi = -1e9;
    for (double t = 5.0; t < 9.0; t += 0.05) {
        const double db = rmsDb(out, t, t + 0.05);
        lo = std::fmin(lo, db);
        hi = std::fmax(hi, db);
    }
    check(hi - lo < 6.0, "REVERSE rests while frozen: the pad is steady (50 ms levels within " + std::to_string(hi - lo) + " dB)");
}

} // namespace

// Costello (Valhalla): a reverb should sound good with its modulation off; modulation is
// seasoning, and it can hide a ringing structure. With every tank's own wander off (ERA clean,
// DRIFT centred), noise bursts must not ring. At a scale of exactly 1.6 the PLATE lined up one
// 9.8 kHz mode (15 dB, only at the knob's last notch; 8 dB at 99 %), so full SIZE is 1.59 (v1.0.11).
void notMetallicUnmodulated()
{
    std::vector<float> hits(size_t(10.0f * kSr), 0.0f);
    for (int h = 0; h < 3; ++h)
        for (size_t i = 0; i < 1200; ++i) hits[size_t(h) * 24000 + i] = 0.6f * noise() * std::exp(-float(i) / 200.0f);
    struct Case { int space; float size; };
    for (const Case c : {Case{1, 0.0f}, Case{1, 1.0f}, Case{2, 0.0f}, Case{2, 1.0f}, Case{0, 0.0f}, Case{0, 0.5f},
                         Case{0, 1.0f}}) {
        Setting s;
        s.space = c.space;
        s.size = c.size;
        s.decay = 0.6f;
        s.mod = 0.0f;
        s.era = 0.0f;
        const Stereo out = render(hits, s);
        const auto m = iv::metrics::compute({out.l, out.r}, kSr);
        char msg[180];
        std::snprintf(msg, sizeof msg, "%s at SIZE %g not metallic with its modulation off: ringing %.1f dB",
                      c.space == 0 ? "PLATE" : (c.space == 1 ? "HALL" : "MASSIVE"), c.size,
                      std::isfinite(m.ringingDb) ? m.ringingDb : 0.0);
        check(finite(out) && !m.ringing, msg);
    }
}

int main()
{
    gestures();
    plate();
    loudness();
    padHolds();
    twin();
    paint();
    clickFree();
    reverseRestsWhileFrozen();
    notMetallicUnmodulated();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
