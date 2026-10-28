#pragma once
// ERA's small building blocks (dsp/EraTexture.h): a state-variable filter, a bit-exact float compare for
// set()'s early-outs, and a noise generator. (ADR 0021's bucket-brigade and AM radio stages lived here
// until v1.0.42, when ERA became one wear amount.)
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace iv::dsp {
// Simper's trapezoidal state-variable filter (low, band, high from one state).
struct EraSvf {
    float ic1 IV_ZI, ic2 IV_ZI, a1 = 1, a2 IV_ZI, a3 IV_ZI, k = 1.4f;
    IV_SIZE_OPT void set(float fc, float q, float sr) { setGain(gain(fc, sr), q); }
    // set() in two halves, so filters at one frequency share the sin and cos (the same bits).
    IV_SIZE_OPT static float gain(float fc, float sr)
    {
        const float w = 3.14159265f * std::fmin(fc, 0.45f * sr) / sr;
        return std::sin(w) / std::cos(w); // not std::tan: it pulled 2.7 KB of maths into the firmware
    }
    IV_SIZE_OPT void setGain(float g, float q)
    {
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // Another filter's coefficients (its state stays).
    void copyCoefficients(const EraSvf& o)
    {
        a1 = o.a1;
        a2 = o.a2;
        a3 = o.a3;
        k = o.k;
    }
    void reset() { ic1 = ic2 = 0.0f; }
    // Returns the low-pass; band and high through the references.
    float tick(float x, float& band, float& high)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        band = v1;
        high = x - k * v1 - v2;
        return v2;
    }
    float lp(float x) { float b, h; return tick(x, b, h); }
    float hp(float x) { float b, h; tick(x, b, h); return h; }
};

// The same bits (not ==, which takes -0 for +0), for set()'s early-outs (EraTexture).
inline bool sameBits(float a, float b)
{
    uint32_t x, y;
    std::memcpy(&x, &a, sizeof x);
    std::memcpy(&y, &b, sizeof y);
    return x == y;
}

struct EraRng {
    uint32_t s = 0x2545F491u;
    float next() { s = s * 1664525u + 1013904223u; return float(int32_t(s)) * (1.0f / 2147483648.0f); } // -1..1
};

} // namespace iv::dsp
