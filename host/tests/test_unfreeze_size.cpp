// The room makes way and keeps its size (v1.0.80, ADR 0056; the owner's pick A on renders/unfreeze_size,
// docs/research/unfreeze-size-2026-11-10.md): in HALL and CANYON a SIZE change after an unfreeze waits while the small room
// still rings, the room fades out in 0.3 s, and then SIZE lands, so a SIZE crossfade and the room never run together (the
// module's overload, the owner's "crackle when I turn SIZE during the second the pad fades out after I let go of FREEZE");
// and the room plays the SIZE the pad froze with, never the live knob.
//
// Asserted: after SIZE moved while frozen, SIZE waits from the unfreeze, the room stops within 0.4 s and SIZE lands then; a
// SIZE turned 0.1-1.0 s after the unfreeze waits from the turn the same way; a new freeze during the wait ends it; PLATE never
// waits. The wait only ever follows an unfreeze: SIZE turned through a HALL <-> CANYON reshape or a PLATE <-> HALL morph with
// no freeze, or through a reshape the switch started while a room rang after an unfreeze, never waits. SIZE turned while
// frozen and after the unfreeze never moves the room's lines. The output stays finite.

#include "DecayKnobFor.h"
#include "dsp/Engine.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

float uniform(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (1.0f / 2147483648.0f);
}

constexpr float kSr = 48000.0f;
constexpr int   kBlock = 48;

// A scripted hand: at t seconds, SIZE to v (kind 0), SPACE to switch position v (1), FREEZE tapped (2: pressed, released 0.1 s on).
struct Ev {
    double t;
    int    kind;
    float  v;
};

struct Run {
    double waitFrom = -1.0, waitTo = -1.0; // seconds from `from`
    double twinStop = -1.0;                // seconds from `from`
    bool   waitedEver = false, twinEver = false, finite = true;
    double flipAt = -1.0;                  // the first SPACE change's flip (the Engine's morph log), seconds from the start
    std::vector<float> out;
};

// Noise until `loudTo` s, then silence; `secs` in all. Times in the result count from `from` (s).
Run run(int space, float decay, const std::vector<Ev>& evs, double from, double loudTo = 3.0, double secs = 8.0)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->setParam(iv::ParamId::Decay, decayKnobFor(decay));
    e->setParam(iv::ParamId::Size, 0.25f);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(secs * kSr) / kBlock, tap = int(0.1f * kSr) / kBlock;
    std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    uint32_t s = 5;
    Run res;
    const auto at = [](double t) { return int(t * kSr) / kBlock; };
    for (int b = 0; b < blocks; ++b) {
        const bool loud = b * kBlock < int(loudTo * kSr);
        for (int i = 0; i < kBlock; ++i) {
            l[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
            r[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
        }
        for (const Ev& ev : evs) {
            if (ev.kind == 0 && b == at(ev.t)) e->setParam(iv::ParamId::Size, ev.v);
            if (ev.kind == 1 && b == at(ev.t)) e->setParam(iv::ParamId::Space, iv::switchToNormalised(int(ev.v)));
            if (ev.kind == 2 && b == at(ev.t)) e->button(true, 0);
            if (ev.kind == 2 && b == at(ev.t) + tap) e->button(false, 0);
        }
        e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) {
            if (!std::isfinite(ol[size_t(i)]) || !std::isfinite(orr[size_t(i)])) res.finite = false;
            res.out.push_back(ol[size_t(i)]);
            res.out.push_back(orr[size_t(i)]);
        }
        const double t = double(b + 1) * kBlock / kSr - from;
        if (e->unfreezeSizeWaiting()) res.waitedEver = true;
        if (e->twinActive()) res.twinEver = true;
        if (t > 0.0) {
            if (e->unfreezeSizeWaiting() && res.waitFrom < 0.0) res.waitFrom = t;
            if (!e->unfreezeSizeWaiting() && res.waitFrom >= 0.0 && res.waitTo < 0.0) res.waitTo = t;
            if (!e->twinActive() && res.twinStop < 0.0) res.twinStop = t;
        }
    }
    const auto& log = e->morphLogForTests();
    if (log.count > 0) res.flipAt = log.events[0].t;
    return res;
}

// Noise for 3 s at SIZE 0.25, a tap (freeze) at 1 s, SIZE to 0.8 at 2 s (frozen), a tap (unfreeze, its release at 3.1 s);
// refreezeAt > 0: another tap that many seconds after the unfreeze.
Run movedWhileFrozen(int space, float decay, double refreezeAt = -1.0)
{
    std::vector<Ev> evs = {{1.0, 2, 0}, {2.0, 0, 0.8f}, {3.0, 2, 0}};
    if (refreezeAt > 0.0) evs.push_back({3.1 + refreezeAt, 2, 0});
    return run(space, decay, evs, 3.1);
}

} // namespace

int main()
{
    const char* names[] = {"PLATE", "HALL", "CANYON"};
    for (int space = 1; space <= 2; ++space)
        for (float decay : {0.3f, 0.8f}) {
            char tag[64];
            std::snprintf(tag, sizeof tag, "%s DECAY %.1f", names[space], double(decay));
            const Run a = movedWhileFrozen(space, decay);
            std::printf("%s: the room stops %.2f s after the unfreeze (SIZE waits %.2f-%.2f s)\n", tag, a.twinStop, a.waitFrom, a.waitTo);
            check(a.waitFrom >= 0.0 && a.waitFrom < 0.05, std::string(tag) + ": SIZE moved while frozen waits from the unfreeze");
            check(a.twinStop > 0.0 && a.twinStop < 0.4, std::string(tag) + ": the room makes way within 0.4 s");
            check(a.waitTo > 0.0 && a.waitTo <= a.twinStop + 0.01, std::string(tag) + ": SIZE lands once the room has stopped");
            check(a.finite, std::string(tag) + ": finite output");
            const Run re = movedWhileFrozen(space, decay, 0.15); // a new freeze 0.15 s into the fade
            check(re.waitTo > 0.0 && re.waitTo < 0.3 && re.finite, std::string(tag) + ": a new freeze during the wait ends it");
            // The owner's gesture: SIZE still while frozen, turned (0.3 s) 0.1, 0.5 or 1.0 s after the unfreeze, either way.
            for (double after : {0.1, 0.5, 1.0})
                for (int up = 0; up < 2; ++up) {
                    const float lo = 0.25f, hi = 0.8f;
                    std::vector<Ev> evs = {{0.0, 0, up ? lo : hi}, {1.0, 2, 0}, {3.0, 2, 0}};
                    for (int k = 1; k <= 6; ++k) { // the hand: six steps over 0.3 s
                        const float u = float(k) / 6.0f;
                        evs.push_back({3.1 + after + 0.05 * (k - 1), 0, up ? lo + u * (hi - lo) : hi - u * (hi - lo)});
                    }
                    const Run t = run(space, decay, evs, 3.1 + after);
                    char g[96];
                    std::snprintf(g, sizeof g, "%s: SIZE %s %.1f s after the unfreeze", tag, up ? "up" : "down", after);
                    if (!t.twinEver || (t.twinStop >= 0.0 && t.twinStop < 0.002)) continue; // (no room left by then: nothing to wait for)
                    check(t.waitFrom > 0.0 && t.waitFrom < 0.1, std::string(g) + " waits from the turn");
                    check(t.twinStop > 0.0 && t.twinStop < 0.45, std::string(g) + ": the room stops within 0.45 s");
                    check(t.waitTo > 0.0 && t.waitTo <= t.twinStop + 0.01 && t.finite, std::string(g) + ": then SIZE lands");
                }
        }
    const Run plate = movedWhileFrozen(0, 0.5f);
    check(!plate.waitedEver, "PLATE: SIZE never waits");

    // The edge case (ADR 0054 / 0055): a room runs unfrozen only after an unfreeze, so a plain SIZE turn through a change waits never.
    struct Change {
        int from, to;
        const char* what;
    };
    const Change changes[] = {{1, 2, "HALL -> CANYON reshape"}, {2, 1, "CANYON -> HALL reshape"}, {0, 1, "PLATE -> HALL morph"},
                              {2, 0, "CANYON -> PLATE morph"}};
    for (const Change& c : changes) {
        const std::vector<Ev> evs = {{1.5, 1, float(c.to)}, {1.7, 0, 0.5f}, {1.9, 0, 0.8f}, {2.4, 0, 0.3f}, {3.5, 0, 0.6f}};
        const Run on = run(c.from, 0.8f, evs, 0.0, 6.0, 6.0);
        const std::string w = std::string(c.what) + ", SIZE turned through it, no freeze";
        check(on.flipAt > 0.0, w + ": the change ran");
        check(!on.twinEver && !on.waitedEver && on.finite, w + ": no room, SIZE never waits");
    }
    for (const Change& c : {changes[0], changes[1]}) { // a room ringing after an unfreeze (SIZE still), then the change, SIZE turned in it
        const std::vector<Ev> evs = {{1.0, 2, 0}, {2.0, 2, 0}, {3.6, 1, float(c.to)}, {4.6, 0, 0.6f}, {4.9, 0, 0.8f}};
        const Run on = run(c.from, 0.95f, evs, 4.6, 2.0, 7.0);
        const std::string w = std::string(c.what) + " while the room rings after an unfreeze, SIZE turned after the flip";
        check(on.twinEver && on.flipAt > 0.0 && on.flipAt < 4.6, w + ": the room ran and the flip came before the turn");
        check(on.twinStop >= 0.0 && on.twinStop < 0.002 && on.waitFrom < 0.0 && on.finite,
              w + ": the flip stopped the room, SIZE never waits");
    }
    // The room keeps its size: SIZE turned while frozen and after the unfreeze (as the owner's crackle), the room's line lengths
    // never move from the ones it started with.
    for (int space = 1; space <= 2; ++space) {
            auto e = std::make_unique<iv::Engine>();
            e->prepare(kSr, kBlock);
            e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
            e->setParam(iv::ParamId::Decay, decayKnobFor(0.95f));
            e->setParam(iv::ParamId::Size, 0.25f);
            e->setParam(iv::ParamId::Mix, 1.0f);
            e->reset();
            std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
            uint32_t s = 9;
            const auto at = [](double t) { return int(t * kSr) / kBlock; };
            float first[8] = {};
            bool started = false, moved = false, finite = true;
            for (int b = 0; b < at(6.0); ++b) {
                for (int i = 0; i < kBlock; ++i) {
                    l[size_t(i)] = b < at(3.0) ? 0.2f * uniform(s) : 0.0f;
                    r[size_t(i)] = b < at(3.0) ? 0.2f * uniform(s) : 0.0f;
                }
                if (b == at(1.0) || b == at(3.0)) e->button(true, 0);
                if (b == at(1.1) || b == at(3.1)) e->button(false, 0);
                if (b > at(1.5) && b < at(2.5)) e->setParam(iv::ParamId::Size, 0.25f + 0.6f * float(b - at(1.5)) / float(at(1.0)));
                if (b > at(3.2) && b < at(3.6)) e->setParam(iv::ParamId::Size, 0.85f - 0.7f * float(b - at(3.2)) / float(at(0.4)));
                e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
                for (int i = 0; i < kBlock; ++i)
                    if (!std::isfinite(ol[size_t(i)]) || !std::isfinite(orr[size_t(i)])) finite = false;
                if (!e->twinActive()) continue;
                if (!started) {
                    for (int k = 0; k < 8; ++k) first[k] = e->roomLineLengthForTests(k);
                    started = true;
                } else {
                    for (int k = 0; k < 8; ++k) moved |= e->roomLineLengthForTests(k) != first[k];
                }
            }
            const std::string w = std::string(names[space]) + ": the room keeps its size";
            check(started && finite, w + ": the room ran, finite output");
            check(!moved, w + ": SIZE turned frozen and after the unfreeze, its lines never move");
    }
    std::printf("%s\n", failures ? "test_unfreeze_size FAILED" : "test_unfreeze_size passed");
    return failures ? 1 : 0;
}
