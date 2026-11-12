// PLATE's SIZE by crossfade (v1.0.68, ADR 0043; the owner's picks on renders/plate_size_xfade and _xfade2). Asserted:
// - a SIZE move takes effect once SIZE rests, in one jump (no glide), under the module's knob noise too
//   (±2e-4 a block, the hysteresis as built);
// - a slow LFO on SIZE (0.2 Hz, 0.3..0.7) starts a crossfade only near its turning points, not over and over
//   (at most 2 per cycle, plus one);
// - an endless PLATE tail holds its level through a minute of random SIZE moves
//   against the same minute with SIZE held (within 1 dB);
// - a small move (SIZE 0.30 -> 0.31, under 4 % of the scale) glides at 0.0193 a second and never crossfades;
// - frozen, a SIZE move and the unfreeze that follows glide (no crossfade).
#include "DecayKnobFor.h"
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

std::unique_ptr<iv::Engine> plate(float size, float decay)
{
    auto e = std::make_unique<iv::Engine>();
    e->prepare(48000.0f, 48);
    e->setParam(iv::ParamId::Space, iv::switchToNormalised(0));
    e->setParam(iv::ParamId::Size, size);
    e->setParam(iv::ParamId::Decay, decayKnobFor(decay));
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
        auto e = plate(0.62f, 0.5f);
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
        auto e = plate(0.3f, 0.7f);
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
    for (int mode : {2, 1}) { // 2: SIZE held (the tail's own drift), 1: SIZE moved
        auto e = plate(0.3f, 1.0f);
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
        } else
            check(std::fabs(end - start - still) < 1.0, "endless PLATE, a minute of SIZE moves: " + std::to_string(end - start) +
                                                           " dB, held " + std::to_string(still) + " dB");
    }
    // Round 2: a small move (within 4 % of the scale) glides slowly, never crossfades; frozen, SIZE glides as built.
    {
        auto e = plate(0.3f, 0.7f);
        for (int b = 0; b < 500; ++b) e->process(in.data(), in.data(), l.data(), r.data(), 48);
        const float from = e->plate().scale();
        e->setParam(iv::ParamId::Size, 0.31f);
        bool xf = false;
        float maxStep = 0.0f, last = from;
        for (int b = 0; b < 3000; ++b) {
            e->process(in.data(), in.data(), l.data(), r.data(), 48);
            xf |= e->plate().crossfading();
            maxStep = std::fmax(maxStep, std::fabs(e->plate().scale() - last));
            last = e->plate().scale();
        }
        const float to = iv::map::plateScale(0.31f);
        check(!xf && std::fabs(last - to) < 1e-4f && maxStep < 0.0193f * 0.001f * 1.01f,
              "SIZE 0.30 -> 0.31 glides (no crossfade), scale " + std::to_string(from) + " -> " + std::to_string(last) +
                  ", at most " + std::to_string(maxStep * 1000.0f) + " a second");
    }
    // Frozen: SIZE moved, then the unfreeze: the PLATE glides throughout (the owner's pick A on the frozen card).
    {
        auto e = plate(0.3f, 0.7f);
        uint32_t seed = 5;
        std::vector<float> noise(48);
        bool xf = false;
        for (int b = 0; b < 6000; ++b) {
            if (b == 1000) e->button(true, 0);
            if (b == 1100) e->button(false, 0);
            if (b == 2000) e->setParam(iv::ParamId::Size, 0.85f);
            if (b == 3000) e->button(true, 0); // a tap while frozen unfreezes on its release
            if (b == 3050) e->button(false, 0);
            for (auto& v : noise) v = b < 1000 ? 0.1f * uniform(seed) : 0.0f;
            e->process(noise.data(), noise.data(), l.data(), r.data(), 48);
            if (b >= 1000) xf |= e->plate().crossfading();
        }
        check(!xf, "frozen, SIZE 0.30 -> 0.85, then the unfreeze: no crossfade (it glides), scale " + std::to_string(e->plate().scale()));
    }
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
