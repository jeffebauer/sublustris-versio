// PROTOTYPE (renders/plate_size_xfade): PLATE's SIZE by crossfade (Plate::setSizeCrossfade, behind the renderer's
// IV_PLATE_XFADE). Asserted, with the crossfade on (0.3 s):
// - a SIZE move takes effect once SIZE rests, in one jump (no glide), under the module's knob noise too
//   (±2e-4 a block, the hysteresis as built);
// - a slow LFO on SIZE (0.2 Hz, 0.3..0.7) starts a crossfade only near its turning points, not over and over
//   (at most 2 per cycle, plus one);
// - an endless PLATE tail holds its level through a minute of random SIZE moves
//   against the same minute with SIZE held (within 1 dB; the glide's printed);
// - with the crossfade off, SIZE still glides (as built).
#include "dsp/Engine.h"
#include "params/Mappings.h"

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

std::unique_ptr<iv::Engine> plate(bool xf, float size, float decay)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(48000.0f, 48);
    e->setPlateCrossfadePrototype(xf, 0.3f);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(0));
    e->setParam(iv::ParamId::Size, size);
    e->setParam(iv::ParamId::Decay, decay);
    e->setParam(iv::ParamId::Mix, 1.0f);
    e->reset();
    return e;
}

} // namespace

int main()
{
    std::vector<float> in(48, 0.0f), l(48), r(48);
    // A move under knob noise: the scale jumps once, within a second.
    for (float noise : {0.0f, 2e-4f}) {
        auto e = plate(true, 0.62f, 0.5f);
        uint32_t seed = 9;
        for (int b = 0; b < 500; ++b) {
            e->setParam(iv::ParamId::Size, 0.62f + noise * uniform(seed));
            e->process(in.data(), in.data(), l.data(), r.data(), 48);
        }
        const float before = e->plate().scale();
        int jumps = 0, took = -1;
        float last = before;
        for (int b = 0; b < 3000; ++b) {
            e->setParam(iv::ParamId::Size, 0.30f + noise * uniform(seed));
            e->process(in.data(), in.data(), l.data(), r.data(), 48);
            const float s = e->plate().scale();
            if (s != last) {
                ++jumps;
                if (took < 0) took = b + 1;
            }
            last = s;
        }
        check(took > 0 && took <= 1000 && jumps == 1 && std::fabs(last - iv::map::plateScale(0.30f)) < 0.01f,
              "noise " + std::to_string(noise) + ": SIZE 0.62 -> 0.30 jumps once after " + std::to_string(took) + " ms (" +
                  std::to_string(jumps) + " jump(s)), scale " + std::to_string(last));
    }
    // A slow LFO on SIZE: crossfades only at its turning points.
    {
        auto e = plate(true, 0.3f, 0.7f);
        int starts = 0;
        bool was = false;
        const int blocks = 20000; // 20 s, four cycles
        for (int b = 0; b < blocks; ++b) {
            const double t = b * 0.001;
            e->setParam(iv::ParamId::Size, float(0.5 - 0.2 * std::cos(2.0 * 3.141592653589793 * 0.2 * t)));
            e->process(in.data(), in.data(), l.data(), r.data(), 48);
            const bool now = e->plate().crossfading();
            if (now && !was) ++starts;
            was = now;
        }
        check(starts >= 2 && starts <= 9, "0.2 Hz LFO on SIZE for 20 s: " + std::to_string(starts) + " crossfades (2..9)");
    }
    // An endless tail through a minute of SIZE moves.
    double still = 0.0;
    for (int mode : {2, 1, 0}) { // 2: SIZE held (the tail's own drift), 1: crossfade, 0: glide
        const bool xf = mode >= 1;
        auto e = plate(xf, 0.3f, 1.0f);
        uint32_t seed = 3;
        std::vector<float> noise(48);
        auto level = [&](int blocks, bool feed, float size) {
            double p = 0.0;
            for (int b = 0; b < blocks; ++b) {
                for (auto& v : noise) v = feed ? 0.1f * uniform(seed) : 0.0f;
                e->setParam(iv::ParamId::Size, size);
                e->process(noise.data(), noise.data(), l.data(), r.data(), 48);
                for (int i = 0; i < 48; ++i) p += double(l[i]) * l[i] + double(r[i]) * r[i];
            }
            return 10.0 * std::log10(p / blocks + 1e-30);
        };
        level(3000, true, 0.3f);
        const double start = level(2000, false, 0.3f);
        uint32_t ms = 11;
        for (int k = 0; k < 40; ++k) {
            const int blocks = 500 + int(1000.0f * (0.5f + 0.5f * uniform(ms)));
            const float size = 0.5f + 0.5f * uniform(ms);
            level(blocks, false, mode == 2 ? 0.3f : size);
        }
        level(2000, false, 0.3f);
        const double end = level(2000, false, 0.3f);
        if (mode == 2) {
            still = end - start;
            std::printf("       endless PLATE, the same minute with SIZE held: %+.2f dB\n", still);
        } else if (xf)
            check(std::fabs(end - start - still) < 1.0, "endless PLATE, a minute of SIZE moves (crossfade): " + std::to_string(end - start) +
                                                           " dB, held " + std::to_string(still) + " dB");
        else std::printf("       endless PLATE, a minute of SIZE moves (glide, as built): %+.2f dB\n", end - start);
    }
    // Off: the glide, as built.
    {
        auto e = plate(false, 0.3f, 0.5f);
        e->setParam(iv::ParamId::Size, 0.9f);
        for (int b = 0; b < 100; ++b) e->process(in.data(), in.data(), l.data(), r.data(), 48);
        const float s = e->plate().scale();
        check(s > iv::map::plateScale(0.3f) + 1e-3f && s < iv::map::plateScale(0.9f) - 1e-3f, "off: SIZE glides (scale " + std::to_string(s) + " after 0.1 s)");
    }
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
