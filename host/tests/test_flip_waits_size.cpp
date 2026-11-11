// The flip waits for SIZE (v1.0.80, ADR 0058; the owner's pick A on renders/flip_waits_size,
// docs/research/flipflick-landing-2026-11-11.md 2a): SIZE turned, then SPACE flipped within about a second, ran the old tank's
// SIZE crossfade under the morph (or the reshape) beside the new tank's start-up: the module's worst crackle left after SIZE
// waits for the morph (114-125 %). The switch isn't read while the tank in use crossfades its SIZE or waits for the knob's
// rest to, up to Engine::kFlipWaitsSizeCapSeconds (1.3 s) from the switch; then the switch's latest position starts a normal
// change.
//
// Asserted, for PLATE -> CANYON, CANYON -> PLATE, PLATE -> HALL, HALL -> PLATE, HALL -> CANYON and CANYON -> HALL, SIZE end to
// end, then SPACE switched 0.2 s after: the old tank (or the reshaped one) never crossfades SIZE once the change has begun, the
// change waits past the flip's own 0.05 s fade-out, and it starts within the cap. With SIZE still, the change starts at once
// (its fade-out). A SIZE that never rests (a slow sine, CV) can't hold SPACE: the change starts by the cap. A SIZE moved inside
// the flip's 0.05 s fade-out waits for the handover (no crossfade under the morph). The output stays finite.

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
const char* kName[3] = {"PLATE", "HALL", "CANYON"};

struct Run {
    double changeAt = -1.0;   // the morph log's first change (kind 1, a new tank; kind 3, a reshape), s
    bool   overlap = false;   // a SIZE crossfade ran in the tank leaving (or the one reshaped) once the change had begun
    bool   finite = true;
    std::vector<float> out;
};

// Noise for 3 s then silence, at SPACE `from`. SIZE goes 0 -> 1 over 0.2 s from 1.0 s (wobble: a slow sine from 1.0 s on, never
// resting); SPACE switched to `to` at `switchAt`; sizeAfterSwitch > 0: SIZE instead jumps 0 -> 1 that long after the switch.
Run run(int from, int to, double switchAt, bool move = true, bool wobble = false, double sizeAfterSwitch = -1.0)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(from));
    e->setParam(iv::ParamId::Decay, decayKnobFor(0.7f));
    e->setParam(iv::ParamId::Size, 0.0f);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(7.0 * kSr) / kBlock;
    std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    uint32_t s = 11;
    Run res;
    bool begun = false, busyBefore = false;
    for (int b = 0; b < blocks; ++b) {
        const double t = double(b) * kBlock / kSr;
        for (int i = 0; i < kBlock; ++i) {
            l[size_t(i)] = t < 3.0 ? 0.2f * uniform(s) : 0.0f;
            r[size_t(i)] = t < 3.0 ? 0.2f * uniform(s) : 0.0f;
        }
        if (sizeAfterSwitch > 0.0) {
            if (t >= switchAt + sizeAfterSwitch) e->setParam(iv::ParamId::Size, 1.0f);
        } else if (wobble) {
            if (t >= 1.0) e->setParam(iv::ParamId::Size, 0.5f - 0.4f * float(std::cos(2.0 * 3.14159265 * 0.5 * (t - 1.0))));
        } else if (move && t >= 1.0) {
            e->setParam(iv::ParamId::Size, float(std::fmin(1.0, (t - 1.0) / 0.2)));
        }
        if (t >= switchAt) e->setParam(iv::ParamId::Space, iv::switchToNormalised(to));
        e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) {
            if (!std::isfinite(ol[size_t(i)]) || !std::isfinite(orr[size_t(i)])) res.finite = false;
            res.out.push_back(ol[size_t(i)]);
            res.out.push_back(orr[size_t(i)]);
        }
        const auto& log = e->morphLogForTests();
        int starts = 0, stops = 0;
        bool reshape = false;
        for (int k = 0; k < log.count; ++k) {
            const int kind = log.events[k].kind;
            if ((kind == 1 || kind == 3) && res.changeAt < 0.0) res.changeAt = log.events[k].t;
            starts += kind == 1;
            stops += kind == 4;
            if (kind == 3) reshape = true;
        }
        const bool oldPlate = from == 0;
        const bool busy = oldPlate ? e->plate().crossfading() : !e->fdn().xfIdle();
        if (res.changeAt >= 0.0 && !begun) {
            begun = true;
            if (busyBefore && !wobble) res.overlap = true; // the tank crossfaded SIZE as the change began
            continue; // the handover's own block
        }
        busyBefore = busy;
        if (!begun || wobble || reshape) continue; // (a reshape crossfades the one tank's lengths itself)
        // A morph: the tank leaving crossfading SIZE while both run.
        if (starts > stops && busy) res.overlap = true;
    }
    return res;
}

} // namespace

int main()
{
    const int pairs[6][2] = {{0, 2}, {2, 0}, {0, 1}, {1, 0}, {1, 2}, {2, 1}};
    for (const auto& p : pairs) {
        const std::string name = std::string(kName[p[0]]) + " -> " + kName[p[1]];
        const double sw = 1.2 + 0.2; // SIZE stops moving at 1.2 s; the switch 0.2 s after
        const Run on = run(p[0], p[1], sw);
        check(!on.overlap, name + ": no SIZE crossfade runs once the change has begun");
        check(on.changeAt - sw > 0.08, name + ": the change waits for SIZE (" + std::to_string(on.changeAt - sw) + " s after the switch)");
        check(on.changeAt > 0.0 && on.changeAt - sw <= iv::Engine::kFlipWaitsSizeCapSeconds + 0.06,
              name + ": the change starts within the cap");
        check(on.finite, name + ": finite");
        const Run still = run(p[0], p[1], sw, false);
        check(still.changeAt > 0.0 && still.changeAt - sw <= 0.06 && still.finite,
              name + ", SIZE still: the change starts at once (" + std::to_string(still.changeAt - sw) + " s after the switch)");
    }
    {
        const double sw = 2.0;
        const Run on = run(2, 0, sw, true, true);
        check(on.changeAt > 0.0 && on.changeAt - sw <= iv::Engine::kFlipWaitsSizeCapSeconds + 0.06 && on.changeAt - sw >= 1.2,
              "CANYON -> PLATE, SIZE never resting: the change goes at the cap (" + std::to_string(on.changeAt - sw) + " s)");
        check(on.finite, "SIZE never resting: finite");
    }
    for (const auto& p : pairs) {
        const std::string name = std::string(kName[p[0]]) + " -> " + kName[p[1]];
        const Run on = run(p[0], p[1], 1.5, true, false, 0.02);
        check(!on.overlap && on.finite, name + ", SIZE moved in the flip's fade-out: it waits for the handover (no crossfade under the change)");
    }
    std::printf("%s\n", failures == 0 ? "all passed" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
