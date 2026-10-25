// SIZE under a noisy knob (overnight review, 26 Oct 2026, arch F1). On the module the pot's reading
// moves by a hair every block; each move shifted the FDN lines' targets, which restarted the tank's
// rest timer (kSizeRestSeconds), so the SIZE crossfade (ADR 0025) never started: SIZE never took
// effect in MASSIVE, and in HALL from about ±2e-4 of noise. Engine::setSizeHysteresis (a prototype
// behind a hook, IV_SIZE_HYST in the Renderer; off as built) sends a new mean only when it has moved
// by more than a share of the last one sent.
//
// Asserted: with the hysteresis at 1e-3 and uniform ±2e-4 noise on SIZE every 48-sample block (as the
// firmware's AudioCallback feeds it), a move 0.62 -> 0.30 takes effect within 1 s in MASSIVE and HALL,
// and without noise it takes effect as fast as as-built. Printed, not asserted: how long as-built takes
// under the same noise (the bug), for the review to quote.

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

float uniform(uint32_t& s) // -1..1
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (1.0f / 2147483648.0f);
}

// Seconds after the move until line 7's length has changed by 10 ms (the crossfade has started), or
// -1 if it never did within `seconds`; `last`: line 7's length at the end (s).
double tookEffect(int space, float noise, float hyst, double seconds = 8.0, float* last = nullptr)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(48000.0f, 48);
#ifndef IV_FIXED_VOICINGS
    e->setSizeHysteresis(hyst);
#endif
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->setParam(iv::ParamId::Size, 0.62f);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    std::vector<float> in(48, 0.0f), l(48), r(48);
    uint32_t seed = 5;
    const int moveAt = 1000, blocks = moveAt + int(seconds * 1000.0);
    const float before = e->fdn().lineSeconds(7);
    double took = -1.0;
    for (int b = 0; b < blocks; ++b) {
        const float target = b < moveAt ? 0.62f : 0.30f;
        e->setParam(iv::ParamId::Size, target + noise * uniform(seed));
        e->process(in.data(), in.data(), l.data(), r.data(), 48);
        if (b >= moveAt && took < 0.0 && std::fabs(e->fdn().lineSeconds(7) - before) > 0.01f) {
            took = (b - moveAt + 1) * 0.001;
            if (!last) break;
        }
    }
    if (last) *last = e->fdn().lineSeconds(7);
    return took;
}

std::string said(double t) { return t < 0.0 ? std::string("never (8 s)") : std::to_string(t) + " s"; }

} // namespace

int main()
{
    const char* names[] = {"PLATE", "HALL", "MASSIVE"};
    for (int space = 1; space <= 2; ++space) {
        const std::string sp = names[space];
        const double quiet = tookEffect(space, 0.0f, 0.0f);
        const double quietHyst = tookEffect(space, 0.0f, 1e-3f);
        std::printf("       %s, no noise: as built %s, hysteresis 1e-3 %s\n", sp.c_str(), said(quiet).c_str(), said(quietHyst).c_str());
        for (float noise : {2e-5f, 1e-4f, 2e-4f}) {
            const double off = tookEffect(space, noise, 0.0f);
            std::printf("       %s, ±%.0e knob noise, as built (no hysteresis): SIZE took effect after %s\n", sp.c_str(), noise,
                        said(off).c_str());
        }
        for (float noise : {5e-4f, 1e-3f}) {
            const double big = tookEffect(space, noise, 1e-3f);
            std::printf("       %s, ±%.0e knob noise, hysteresis 1e-3: SIZE took effect after %s\n", sp.c_str(), noise, said(big).c_str());
        }
        float lineOn = 0.0f, lineQuiet = 0.0f;
        tookEffect(space, 0.0f, 0.0f, 3.0, &lineQuiet);
        const double on = tookEffect(space, 2e-4f, 1e-3f, 3.0, &lineOn);
        check(std::fabs(lineOn / lineQuiet - 1.0f) < 2e-3f, sp + ": held, the line lands within 0.2 % of where a still knob puts it ("
                                                                + std::to_string(lineOn) + " s vs " + std::to_string(lineQuiet) + " s)");
        check(on >= 0.0 && on <= 1.0, sp + ": with ±2e-4 knob noise and the hysteresis at 1e-3, SIZE 0.62 -> 0.30 takes effect within 1 s ("
                                          + said(on) + ")");
        check(quietHyst >= 0.0 && quietHyst <= quiet + 0.002, sp + ": without noise the hysteresis takes effect as fast as as-built ("
                                                                  + said(quietHyst) + " vs " + said(quiet) + ")");
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
