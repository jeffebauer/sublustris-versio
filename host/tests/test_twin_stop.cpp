// How long the freeze twin runs after an unfreeze (overnight review, 26 Oct 2026, lit-plate finding 6).
// The twin is a second tank; it stops once its input is shut and its level is under -100 dB. As built
// that level is the 1.5 s power smoother, which falls only 2.9 dB a second once the tail is gone, so
// the twin ran 26-30 s after every unfreeze whatever DECAY was (its tail is gone within kTwinMaxT60).
// Engine::setTwinStopFast (a prototype behind a hook, IV_TWIN_STOP_FAST in the Renderer) judges the
// 50 ms level instead. Not bit-identical: the twin's output under -100 dB stops being added.
//
// Asserted, with the hook on: the twin stops within 12 s of the unfreeze in every SPACE at DECAY 0.2,
// 0.5 and 0.8, and the output differs from as built by less than -80 dBFS anywhere. Printed: how long
// it runs as built and with the hook, for the review to quote.

#include "DecayKnobFor.h"
#include "dsp/Engine.h"

#include <algorithm>
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

// Noise into SPACE at DECAY for 4 s, a tap (freeze) at 1 s, a tap (unfreeze) at 3 s, then silence to
// 45 s. Returns the seconds from the unfreeze until the twin stopped (-1: still running), the output in `out`.
double twinRuns(int space, float decay, bool fast, std::vector<float>& out)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
#ifndef IV_FIXED_VOICINGS
    e->setTwinStopFast(fast);
#endif
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->setParam(iv::ParamId::Decay, decayKnobFor(decay));
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(45.0f * kSr) / kBlock;
    const int pressA = int(1.0f * kSr) / kBlock, pressB = int(3.0f * kSr) / kBlock, tap = int(0.1f * kSr) / kBlock;
    std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    out.assign(size_t(blocks) * kBlock, 0.0f);
    uint32_t s = 11;
    double stopped = -1.0;
    bool seen = false;
    for (int b = 0; b < blocks; ++b) {
        const bool loud = b * kBlock < int(4.0f * kSr);
        for (int i = 0; i < kBlock; ++i) {
            l[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
            r[size_t(i)] = loud ? 0.2f * uniform(s) : 0.0f;
        }
        if (b == pressA || b == pressB) e->button(true, 0);
        if (b == pressA + tap || b == pressB + tap) e->button(false, 0);
        e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) out[size_t(b) * kBlock + size_t(i)] = 0.5f * (ol[size_t(i)] + orr[size_t(i)]);
        if (e->twinActive()) seen = true;
        if (b > pressB && seen && stopped < 0.0 && !e->twinActive()) stopped = double(b - pressB) * kBlock / kSr;
    }
    return seen ? stopped : -2.0;
}

} // namespace

int main()
{
    const char* names[] = {"PLATE", "HALL", "MASSIVE"};
    for (int space = 0; space < 3; ++space)
        for (float decay : {0.2f, 0.5f, 0.8f}) {
            std::vector<float> a, b;
            const double built = twinRuns(space, decay, false, a);
            const double fast = twinRuns(space, decay, true, b);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) worst = std::max(worst, std::fabs(a[i] - b[i]));
            const double worstDb = 20.0 * std::log10(double(worst) + 1e-30);
            std::printf("       %s DECAY %.1f: the twin runs %.2f s after the unfreeze as built, %.2f s judged on its 50 ms level;"
                        " largest output difference %.1f dBFS\n",
                        names[space], decay, built, fast, worstDb);
            const std::string what = std::string(names[space]) + " DECAY " + std::to_string(decay).substr(0, 3);
            check(fast >= 0.0 && fast <= 12.0, what + ": judged on its 50 ms level, the twin stops within 12 s of the unfreeze");
            check(worstDb < -80.0, what + ": and the output stays within -80 dBFS of as built");
        }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
