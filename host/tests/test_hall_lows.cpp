// HALL's low-note resonances (v1.0.76, ADR 0052, the owner's pick D on renders/hall_lows; the owner on the rack, on a phrase
// of low notes: "a resonant build up in the left channel, but not the right"). HALL's still low tank has no modulation, so
// its narrow resonances each lean left or right by chance; 90.8 Hz leaned left by 9 dB and 80 Hz right by 13 dB on v1.0.75.
//
// Asserted, on six hits of a sine at each pitch, a semitone apart from 40 to 240 Hz plus the pitches v1.0.75 and the page's D
// leaned on most, at SIZE noon, 0.35 and 0.8, DECAY 0.7, WEAR 0, 100 % wet: no pitch more than kMaxLeanDb between the sides.
// Measured here: furthest 5.8 / 6.7 / 5.5 dB (the low tank at full width and without its diffusion: 16.5 / 14.3 / 12.7). A bare
// sine leans a little further than the page's patch, which stays under 6 dB (tools/pages/hall_lows_gates.py's lean).
// CANYON and PLATE don't change: the golden fingerprints hold them to v1.0.75's.

#include "dsp/Engine.h"
#include "params/SpaceVoicing.h"

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

constexpr float  kSr = 48000.0f;
constexpr int    kBlock = 48;
constexpr double kMaxLeanDb = 7.0;
constexpr double kPi = 3.14159265358979323846;

// Left over right, dB, over six hits and their tail.
double leanDb(iv::Engine& e, float size, float hz)
{
    e.setParam(iv::ParamId::Size, size);
    e.reset();
    const int blocks = int(9.0f * kSr) / kBlock;
    float in[kBlock], l[kBlock], r[kBlock];
    double el = 0.0, er = 0.0, ph = 0.0;
    const double inc = 2.0 * kPi * double(hz) / double(kSr);
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < kBlock; ++i) {
            const double t = (double(b) * kBlock + i) / double(kSr) - 0.1;
            const bool on = t >= 0.0 && t < 6.0 && std::fmod(t, 1.0) < 0.6;
            in[i] = on ? float(0.178 * std::sin(ph)) : 0.0f; // -15 dBFS
            ph += inc;
        }
        e.process(in, in, l, r, kBlock);
        for (int i = 0; i < kBlock; ++i) {
            el += double(l[i]) * l[i];
            er += double(r[i]) * r[i];
        }
    }
    return 10.0 * std::log10((el + 1e-30) / (er + 1e-30));
}

} // namespace

int main()
{
    check(iv::voicing::kHallLowsWidth == 0.30f, "HALL's low tank keeps 0.30 of its left-right difference");
    check(iv::voicing::kHallLowsInLoopDiffusion == 0.60f, "HALL's low tank has in-loop diffusion 0.60 at every SIZE");
    check(std::fabs(20.0f * std::log10(iv::voicing::kHallLowsOutputTrim) - 3.3f) < 0.01f, "HALL's low tank sits +3.3 dB up");

    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(1)); // HALL
    e->setParam(iv::ParamId::Decay, 0.7f);
    e->setParam(iv::ParamId::Era, 0.0f);
    e->setParam(iv::ParamId::Mix, 1.0f);

    std::vector<float> pitches = {41.2f, 42.4f, 72.5f, 78.2f, 80.0f, 90.8f, 94.0f, 99.1f, 115.8f, 119.9f, 141.7f, 151.0f, 163.7f, 169.5f};
    for (float hz = 40.0f; hz < 240.0f; hz *= 1.059463f) pitches.push_back(hz);

    for (const float size : {0.5f, 0.35f, 0.8f}) {
        double worst = 0.0;
        float at = 0.0f;
        for (const float hz : pitches) {
            const double lean = leanDb(*e, size, hz);
            if (std::fabs(lean) > std::fabs(worst)) {
                worst = lean;
                at = hz;
            }
        }
        char msg[160];
        std::snprintf(msg, sizeof msg, "HALL SIZE %.2f: no low note leans over %.0f dB (furthest %+.1f dB at %.1f Hz, of %zu pitches)",
                      double(size), kMaxLeanDb, worst, double(at), pitches.size());
        check(std::fabs(worst) <= kMaxLeanDb, msg);
    }
    return failures == 0 ? 0 : 1;
}
