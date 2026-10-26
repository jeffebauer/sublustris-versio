// The freeze twin's level against the frozen pad (v1.0.41, the owner's pick on renders/twin_louder: "Let's
// shoot for 8db under the pad"). The twin's tail stops within kTwinMaxT60 (8 s) while HALL's and MASSIVE's run
// to 14 s or infinite, so at kTwinLevel alone it sat 12-15 dB (A-weighted, on music) under a pad; its level now
// comes back by the square root of how much shorter it is, up to kTwinFdnGain (+5.4 dB). A twin that isn't cut
// short (a short HALL) and PLATE's are as before.
//
// Two runs per setting: the pad (noise 0-3 s, a tap at 2.5 s, then silence) and the twin (silence frozen at
// 2.5 s, the same noise played over it from 3 s). Their output levels over 5-9 s, twin minus pad, broadband
// white noise (so not the A-weighted music figures: on noise the twin reads a few dB lower).
//
// Asserted: each setting within 1.5 dB of the level it measured when v1.0.41 was set (printed), the long HALL /
// MASSIVE tails 3-7 dB louder than they were before v1.0.41 (the lift), and the short HALL within 0.2 dB of its
// pre-v1.0.41 value (no lift).

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

// Output power (dB) over 5-9 s: the pad (padRun) or the twin.
double level(int space, float size, float decay, bool padRun)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(kSr, kBlock);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(space));
    e->setParam(iv::ParamId::Size, size);
    e->setParam(iv::ParamId::Decay, decay);
    e->setParam(iv::ParamId::Era, 0.1f);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    const int blocks = int(9.0f * kSr) / kBlock, press = int(2.5f * kSr) / kBlock, tap = int(0.1f * kSr) / kBlock;
    std::vector<float> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    uint32_t s = 7;
    double sum = 0.0;
    long count = 0;
    for (int b = 0; b < blocks; ++b) {
        const float t = float(b * kBlock) / kSr;
        const bool on = padRun ? t < 3.0f : t >= 3.0f;
        for (int i = 0; i < kBlock; ++i) {
            const float a = uniform(s), c = uniform(s); // the same noise either way
            l[size_t(i)] = on ? 0.2f * a : 0.0f;
            r[size_t(i)] = on ? 0.2f * c : 0.0f;
        }
        if (b == press) e->button(true, 0);
        if (b == press + tap) e->button(false, 0);
        e->process(l.data(), r.data(), ol.data(), orr.data(), kBlock);
        if (t >= 5.0f)
            for (int i = 0; i < kBlock; ++i, ++count) sum += 0.5 * (double(ol[size_t(i)]) * ol[size_t(i)] + double(orr[size_t(i)]) * orr[size_t(i)]);
    }
    return 10.0 * std::log10(sum / double(count) + 1e-30);
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        int space;
        float size, decay;
        double expect, before; // twin - pad, dB: at v1.0.41; and before it (v1.0.40, this test built against it)
    };
    const Case cases[] = {
        {"MASSIVE S0.9 D0.9", 2, 0.9f, 0.9f, -8.28, -13.67},
        {"MASSIVE S0.3 D1.0", 2, 0.3f, 1.0f, -9.81, -15.21},
        {"MASSIVE S0.9 D0.6", 2, 0.9f, 0.6f, -7.06, -12.44},
        {"HALL S0.9 D0.9", 1, 0.9f, 0.9f, -12.52, -17.92},
        {"HALL S0.3 D0.6 (not cut short)", 1, 0.3f, 0.6f, -6.89, -6.89},
        {"PLATE S0.9 D0.9", 0, 0.9f, 0.9f, -7.08, -7.08},
    };
    for (const auto& c : cases) {
        const double d = level(c.space, c.size, c.decay, false) - level(c.space, c.size, c.decay, true);
        std::printf("%-32s twin - pad %+6.2f dB (v1.0.41 %+.1f, before %+.1f)\n", c.name, d, c.expect, c.before);
        check(std::fabs(d - c.expect) <= 1.5, std::string(c.name) + ": within 1.5 dB of v1.0.41's level");
        if (c.expect != c.before) check(d - c.before >= 3.0 && d - c.before <= 7.0, std::string(c.name) + ": 3-7 dB louder than before v1.0.41");
        else check(std::fabs(d - c.before) <= 0.2, std::string(c.name) + ": as before v1.0.41 (no lift)");
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
