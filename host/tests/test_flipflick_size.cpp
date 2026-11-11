// SIZE waits for the morph (v1.0.80, ADR 0057; the owner's pick B on renders/flipflick_size,
// docs/research/flipflick-size-2026-11-10.md): a PLATE <-> HALL / CANYON flip is a two-tank morph, and a SIZE flicked in its
// 3 s started the new tank's SIZE crossfade on top of both tanks (the module's overload, the owner's "switching between PLATE
// and CANYON ... and quickly moving the SIZE" crackle). The new tank keeps the SIZE it started with until the old tank has
// faded over the morph's own 3 s, then SIZE lands once.
//
// Asserted, for PLATE -> CANYON, CANYON -> PLATE, PLATE -> HALL and HALL -> PLATE, SIZE flicked end to end 0.1 s and 1.0 s after
// the flip: the new tank's SIZE crossfade never starts while the old tank still rings, the new tank's lengths don't move until
// the old tank has stopped (about 3 s after the flip), and then SIZE lands (the lengths reach the flicked SIZE). A freeze during
// the wait ends it: the pad keeps the new tank's lengths, and SIZE lands after the unfreeze. A HALL <-> CANYON reshape never
// waits: SIZE lands within the second. The output stays finite.

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

struct Run {
    double flipAt = -1.0, oldStop = -1.0;   // the morph log's new tank start (kind 1) and old tank stop (kind 4), s
    double xfDuringMorph = -1.0;            // the first block the new tank crossfaded SIZE while the old one rang, s
    double lengthsMoved = -1.0;             // the first block the new tank's lengths (or scale) left the flip's, s
    double waitFrom = -1.0, waitTo = -1.0;  // flipFlickWaitingForTests
    bool   finite = true, landed = false;
    float  startLen = 0.0f, endLen = 0.0f;  // the new tank's line 0 (FDN, s) or scale (PLATE) at the flip and at the end
    std::vector<float> out;
};

// Noise at SIZE `from` for 2.5 s then silence, SPACE switched at 1.0 s to `to`; SIZE to `flickTo` (over 0.2 s, in steps)
// starting `after` s after the flip; freezeAfter > 0: FREEZE tapped that many s after the flip, and again (unfreeze) 1.5 s later.
Run run(int from, int to, float sizeFrom, float flickTo, double after, double freezeAfter = -1.0, double secs = 9.0)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(from));
    e->setParam(iv::ParamId::Decay, decayKnobFor(0.7f));
    e->setParam(iv::ParamId::Size, sizeFrom);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(secs * kSr) / kBlock, tap = int(0.1f * kSr) / kBlock;
    std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    uint32_t s = 7;
    Run res;
    const bool newPlate = to == 0;
    const auto len = [&]() { return newPlate ? e->plate().scale() : e->fdn().lineSeconds(0); };
    int fb = -1; // the flip's block
    for (int b = 0; b < blocks; ++b) {
        const double t = double(b) * kBlock / kSr;
        const bool loud = t < 2.5;
        for (int i = 0; i < kBlock; ++i) {
            l[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
            r[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
        }
        if (b == int(1.0 * kSr) / kBlock) e->setParam(iv::ParamId::Space, iv::switchToNormalised(to));
        if (fb >= 0) {
            const double since = double(b - fb) * kBlock / kSr;
            if (since >= after && since <= after + 0.2) // the flick, end to end in 0.2 s
                e->setParam(iv::ParamId::Size, sizeFrom + (flickTo - sizeFrom) * float((since - after) / 0.2));
            if (since > after + 0.2) e->setParam(iv::ParamId::Size, flickTo);
            if (freezeAfter > 0.0) {
                const int fz = fb + int(freezeAfter * kSr) / kBlock, uf = fz + int(1.5 * kSr) / kBlock;
                if (b == fz || b == uf) e->button(true, 0);
                if (b == fz + tap || b == uf + tap) e->button(false, 0);
            }
        }
        e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) {
            if (!std::isfinite(ol[size_t(i)]) || !std::isfinite(orr[size_t(i)])) res.finite = false;
            res.out.push_back(ol[size_t(i)]);
            res.out.push_back(orr[size_t(i)]);
        }
        const auto& log = e->morphLogForTests();
        const double now = double(b + 1) * kBlock / kSr;
        int starts = 0, stops = 0;
        for (int k = 0; k < log.count; ++k) {
            const int kind = log.events[k].kind;
            if ((kind == 1 || kind == 3) && res.flipAt < 0.0) { // a new tank starts, or a reshape
                res.flipAt = log.events[k].t;
                fb = b;
                res.startLen = len();
            }
            if (kind == 4 && res.oldStop < 0.0) res.oldStop = log.events[k].t;
            starts += kind == 1;
            stops += kind == 4;
        }
        if (fb >= 0 && b > fb) {
            const bool morphing = starts > stops; // two tanks running
            const bool xf = e->plate().crossfading() || !e->fdn().xfIdle();
            if (morphing && xf && res.xfDuringMorph < 0.0) res.xfDuringMorph = now;
            if (std::fabs(len() - res.startLen) > 1e-6f * (1.0f + res.startLen) && res.lengthsMoved < 0.0) res.lengthsMoved = now;
        }
        if (e->flipFlickWaitingForTests() && res.waitFrom < 0.0) res.waitFrom = now;
        if (!e->flipFlickWaitingForTests() && res.waitFrom >= 0.0 && res.waitTo < 0.0) res.waitTo = now;
    }
    res.endLen = len();
    res.landed = std::fabs(res.endLen - res.startLen) > 1e-3f * res.startLen;
    return res;
}

} // namespace

int main()
{
    const char* names[] = {"PLATE", "HALL", "CANYON"};
    const int pairs[][2] = {{0, 2}, {2, 0}, {0, 1}, {1, 0}};
    for (const auto& p : pairs)
        for (double after : {0.1, 1.0})
            for (int dir = 0; dir < 2; ++dir) {
                const float a = dir == 0 ? 0.0f : 1.0f, z = 1.0f - a;
                char tag[96];
                std::snprintf(tag, sizeof tag, "%s -> %s, SIZE %.0f->%.0f %.1f s after the flip", names[p[0]], names[p[1]],
                              double(a), double(z), after);
                const Run v = run(p[0], p[1], a, z, after);
                const std::string m = tag;
                check(v.finite && v.flipAt > 0.0 && v.oldStop > 0.0, m + ": a two-tank morph, finite");
                check(v.xfDuringMorph < 0.0, m + ", no SIZE crossfade while the old tank rings");
                check(v.lengthsMoved < 0.0 || v.lengthsMoved >= v.oldStop, m + ", the new tank's lengths held until the old tank stopped");
                check(v.landed, m + ", then SIZE lands");
                check(v.waitFrom > 0.0 && v.waitTo > 0.0 && std::fabs(v.waitTo - v.oldStop) < 0.01,
                      m + ", waits from the flick to the old tank's stop");
                char d[96];
                std::snprintf(d, sizeof d, ", the old tank goes %.2f s after the flip (its 3 s)", v.oldStop - v.flipAt);
                check(std::fabs(v.oldStop - v.flipAt - 3.0) < 0.05, m + d);
            }

    // A freeze during the wait ends it; the pad keeps the new tank's lengths (PLATE -> CANYON, flicked 0.1 s after the flip, frozen
    // 0.25 s or 1.5 s after it, unfrozen 1.5 s later): no SIZE crossfade while the old tank rings, and SIZE lands after the
    // unfreeze. Early (0.25 s: the old tank still holds most of the sound, so it becomes the pad and the change waits for the
    // unfreeze, morphHoldOld), and late (1.5 s: the new tank is the pad).
    for (double fz : {0.25, 1.5}) {
        char tag[96];
        std::snprintf(tag, sizeof tag, "PLATE -> CANYON, a freeze %.2f s after the flip, during the wait", fz);
        const std::string m = tag;
        const Run v = run(0, 2, 0.0f, 1.0f, 0.1, fz);
        check(v.finite, m + ", finite");
        check(v.waitFrom > 0.0 && v.waitTo > 0.0 && v.waitTo < v.flipAt + fz + 0.05, m + ", the freeze ends the wait");
        check(v.xfDuringMorph < 0.0, m + ", no SIZE crossfade while two tanks run");
        if (fz > 1.0)
            check(v.lengthsMoved < 0.0 || v.lengthsMoved > v.flipAt + fz + 1.5, m + ", the pad keeps the new tank's lengths until the unfreeze");
        check(v.landed, m + ", SIZE lands after the unfreeze");
        check(v.oldStop > 0.0 && v.oldStop - (v.flipAt + fz) < 0.4, m + ", the freeze sends a tank out in 0.3 s");
    }

    // A HALL <-> CANYON reshape (one tank, in place) never waits: SIZE lands within the second.
    for (const auto& p : {std::pair<int, int>{1, 2}, std::pair<int, int>{2, 1}}) {
        const Run v = run(p.first, p.second, 0.0f, 1.0f, 0.1);
        char tag[96];
        std::snprintf(tag, sizeof tag, "%s -> %s reshape: never waits, SIZE lands", names[p.first], names[p.second]);
        check(v.finite && v.waitFrom < 0.0 && v.landed && v.lengthsMoved > 0.0 && v.lengthsMoved < v.flipAt + 1.1, tag);
    }

    std::printf("%s\n", failures == 0 ? "ALL PASS" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
