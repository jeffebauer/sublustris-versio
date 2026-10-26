#pragma once
// Input diffusion (SPEC §4.1): a chain of Schroeder allpasses that smears a
// hit into a short cloud before it reaches the tank, so the tail starts
// dense instead of as a train of distinct echoes. Allpass: every frequency
// passes at the same level; only the timing is spread.
#include "dsp/Memory.h"

namespace iv::dsp {

struct Allpass {
    DelayLine line;
    uint32_t  length = 1;
    float     coeff = 0.5f;
    float     mod = 0.0f; // the PLATE's wandering first stage (v1.0.16, MI idea 2): extra delay in samples, ±

    IV_SIZE_OPT bool init(Arena& arena, uint32_t samples, float g)
    {
        length = samples < 1 ? 1 : samples;
        coeff = g;
        return line.init(arena, length);
    }
    float process(float x)
    {
        float d;
        if (mod != 0.0f) { // a wandering read, linear between samples (the buffer has room)
            const float dd = float(length) + mod;
            const auto  di = uint32_t(dd);
            const float t = dd - float(di);
            d = line.read(di) + t * (line.read(di + 1) - line.read(di));
        } else {
            d = line.read(length);
        }
        const float v = x + coeff * d;
        line.write(v);
        return d - coeff * v;
    }
};

template <int kStages>
struct Diffuser {
    Allpass stage[kStages];

    float process(float x)
    {
        for (auto& s : stage) x = s.process(x);
        return x;
    }
    void clear()
    {
        for (auto& s : stage) s.line.clear();
    }
};

} // namespace iv::dsp
