// v1.0.63 (review 2, A5 and A6): the gate's latch and the SPACE switch's settle (firmware/ControlLatch.h), and the
// Engine's SPACE flip that gives way when the switch comes back before its fade bottoms out (Engine::controlTick).
// The firmware reads the gate and the switch once a block; here a sample-accurate gate and switch are read the
// same way, block by block, into the Engine (SPEC §3 "Button + Gate", §7 M5 "pulses from a sequencer, bounces").

#include "DecayKnobFor.h"
#include "../../firmware/ControlLatch.h"
#include "dsp/Engine.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

int failures = 0;
constexpr float kSr    = 48000.0f;
constexpr int   kBlock = 48;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

uint32_t gSeed = 7;
float noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return float(int32_t(gSeed)) / 2147483648.0f;
}

// The firmware's callback, block by block. gate(sample) and space(sample) are the jacks / switch at sample
// resolution; latch false reads the gate's level only, as before v1.0.63. Settle false passes the switch raw.
struct Rig {
    iv::Engine            e;
    ivctl::GateLatch      latch;
    ivctl::SwitchSettle   settle;
    bool                  useLatch = true, useSettle = true, lastLevel = false;
    std::function<bool(long)> gate  = [](long) { return false; };
    std::function<int(long)>  space = [](long) { return 1; };
    long                  t         = 0;
    double                tailPower = 0.0; // the output's power over measure()'s window
    long                  measureFrom = -1, measureTo = -1;
    bool                  sawPaint = false;

    explicit Rig(int startSpace)
    {
        gSeed = 7; // every run hears the same noise
        e.prepare(kSr, kBlock);
        e.setParam(iv::ParamId::Space, iv::switchToNormalised(startSpace));
        e.setParam(iv::ParamId::Decay, decayKnobFor(0.6f));
        e.setParam(iv::ParamId::Size, 0.5f);
        e.setParam(iv::ParamId::Mix, 1.0f);
        e.reset();
    }

    void run(double seconds, bool feed)
    {
        const long n = long(seconds * kSr);
        float inL[kBlock], inR[kBlock], outL[kBlock], outR[kBlock];
        for (long done = 0; done < n; done += kBlock) {
            // The callback's start: the latch holds any rise since the last read (between the two reads), the
            // level is read now.
            bool rose = false;
            bool prev = gate(t - kBlock);
            for (long s = t - kBlock + 1; s <= t; ++s) {
                const bool g = gate(s);
                if (g && !prev) rose = true;
                prev = g;
            }
            const bool level = gate(t);
            if (useLatch) {
                latch.block(rose, level, [this](bool high) { e.gate(high, 0); });
            } else if (level != lastLevel) {
                e.gate(level, 0);
            }
            lastLevel = level;
            const int raw = space(t);
            e.setParam(iv::ParamId::Space, iv::switchToNormalised(useSettle ? settle.read(raw) : raw));
            for (int i = 0; i < kBlock; ++i) inL[i] = inR[i] = feed ? 0.3f * noise() : 0.0f;
            e.process(inL, inR, outL, outR, kBlock);
            if (t >= measureFrom && t < measureTo)
                for (int i = 0; i < kBlock; ++i) tailPower += double(outL[i]) * outL[i] + double(outR[i]) * outR[i];
            sawPaint = sawPaint || e.painting();
            t += kBlock;
        }
    }
};

double db(double p) { return 10.0 * std::log10(p + 1e-30); }

// A pulse [from, from + len) samples.
std::function<bool(long)> pulses(std::vector<std::pair<long, long>> p)
{
    return [p](long s) {
        for (const auto& q : p)
            if (s >= q.first && s < q.first + q.second) return true;
        return false;
    };
}

void gatePulses()
{
    const long b = long(0.2 * kSr) / kBlock * kBlock; // a block boundary
    // A 0.5 ms trigger 10 samples into a block: between two reads.
    for (int latch = 0; latch <= 1; ++latch) {
        Rig r(1);
        r.useLatch = latch;
        r.gate     = pulses({{b + 10, 24}});
        r.run(0.3, true);
        if (latch)
            check(r.e.frozen(), "a 0.5 ms gate pulse between two block reads freezes");
        else
            check(!r.e.frozen(), "  (without the latch it was lost, as review 2 measured)");
    }
    // Pulses of 1 sample and 47 samples, and a second pulse that unfreezes.
    for (long len : {1L, 47L}) {
        Rig r(1);
        r.gate = pulses({{b + 1, len}, {b + long(0.5 * kSr) + 1, len}});
        r.run(0.4, true);
        const bool froze = r.e.frozen();
        r.run(0.4, false);
        check(froze && !r.e.frozen(), "a " + std::to_string(len) + "-sample pulse freezes, the next one unfreezes");
    }
    // A pulse that straddles a read is seen by the level, once (not twice).
    {
        Rig r(1);
        r.gate = pulses({{b - 10, 24}});
        r.run(0.4, true);
        check(r.e.frozen(), "a pulse across a block read freezes once (not freeze-then-unfreeze)");
    }
    // Frozen, a gate held past 0.33 s paints, and its release leaves the pad frozen.
    {
        Rig r(1);
        const long hold = b + long(0.5 * kSr);
        r.gate = pulses({{b + 5, 20}, {hold + 7, long(0.6 * kSr)}});
        r.run(1.5, true);
        check(r.sawPaint && r.e.frozen() && !r.e.painting(), "frozen, a gate held 0.6 s paints, and the pad stays frozen after");
    }
    // A short pulse while frozen is a tap: it unfreezes, never paints.
    {
        Rig r(1);
        r.gate = pulses({{b + 5, 20}, {b + long(0.5 * kSr) + 30, 10}});
        r.run(1.2, true);
        check(!r.sawPaint && !r.e.frozen(), "frozen, a 10-sample pulse unfreezes without painting");
    }
}

// The tail's power 0.4-0.9 s after a SPACE brush at 0.6 s, after 0.5 s of noise.
double tailAfterBrush(int away, double awaySeconds, bool settle, int* activeAfter)
{
    Rig r(1);
    r.useSettle   = settle;
    const long at = long(0.6 * kSr), back = at + long(awaySeconds * kSr);
    r.space       = [=](long s) { return (s >= at && s < back) ? away : 1; };
    r.measureFrom = long(1.0 * kSr);
    r.measureTo   = long(1.5 * kSr);
    r.run(0.5, true);
    r.run(1.0, false);
    if (activeAfter) *activeAfter = r.e.activeSpace();
    return r.tailPower;
}

void spaceBrushes()
{
    const double still = tailAfterBrush(1, 0.0, true, nullptr);
    for (int away : {0, 2}) {
        // Back inside the firmware's settle: the Engine never hears it; the output is the same to the bit.
        int sp = -1;
        const double p = tailAfterBrush(away, 0.010, true, &sp);
        check(p == still && sp == 1, "SPACE brushed to " + std::to_string(away) + " for 10 ms: nothing happens (the same output)");
        // Past the settle, back inside the flip's 50 ms fade-out: the wet dips and comes back, the tail carries on.
        // The Engine alone, as the Plugin would see a host automating SPACE there and back.
        const double q = tailAfterBrush(away, 0.030, false, &sp);
        char what[200];
        std::snprintf(what, sizeof what, "SPACE to %d and back in 30 ms (no settle): the tail carries on (%.2f dB from still), no restart",
                      away, db(q) - db(still));
        check(sp == 1 && std::fabs(db(q) - db(still)) < 1.0, what);
    }
    // A real flip still happens, past the settle.
    int sp = -1;
    tailAfterBrush(2, 10.0, true, &sp);
    check(sp == 2, "SPACE moved and left there flips after the settle");
    // And a switch that stays away past the fade's bottom commits to the change (ADR 0054): the tail is kept, not wiped, and when
    // the change has finished, the switch's latest position decides (it's back at HALL, so back it goes). A tank restarted at
    // the change (the tail gone), a change that never starts, or a switch left unread after it finishes each fail here.
    for (int away : {0, 2}) {
        Rig r(1);
        r.useSettle   = false;
        const long at = long(0.6 * kSr), back = at + long(0.2 * kSr);
        r.space       = [=](long s) { return (s >= at && s < back) ? away : 1; };
        r.measureFrom = long(1.0 * kSr);
        r.measureTo   = long(1.5 * kSr);
        r.run(0.5, true);
        r.run(1.0, false);
        const int during = r.e.activeSpace();
        r.run(6.5, false); // a morph's 3 s, the short wait, and the 3 s back
        char what[200];
        std::snprintf(what, sizeof what, "SPACE to %d for 200 ms: the change goes ahead and the tail carries on (%.1f dB from still)", away, db(r.tailPower) - db(still));
        check(during == away && db(r.tailPower) > db(still) - 6.0, what);
        check(r.e.activeSpace() == 1, "  and when it has finished, the switch (back at HALL) takes it back");
    }
}

} // namespace

int main()
{
    gatePulses();
    spaceBrushes();
    std::printf("%s\n", failures ? "SOME TESTS FAILED" : "all passed");
    return failures ? 1 : 0;
}
