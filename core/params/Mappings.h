#pragma once
// Normalised (0–1) → internal units. The only place mapping curves live, so a
// setting in the Plugin sounds identical on the module (SPEC §6). Curves land
// per milestone; numbers marked (start) in SPEC §3 are starting guesses.
#include "params/SpaceVoicing.h"

#include <cmath>

namespace iv::map {

constexpr float kPi = 3.14159265358979323846f;

inline float expLerp(float lo, float hi, float v) { return lo * std::exp(v * std::log(hi / lo)); }
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// MIX (P1): equal-power, fully CCW dry only, fully CW wet only (ADR 0012).
struct MixGains {
    float dry, wet;
};
inline MixGains mixGains(float v) { return {std::cos(v * 0.5f * kPi), std::sin(v * 0.5f * kPi)}; }

// DECAY (P2), SPEC §3: 0–0.80 T60 exponential per SPACE; 0.80–0.85 a
// crossover to infinite (1/T60 falls linearly to 0); 0.85–1 infinite.
constexpr float kDecayFiniteTop   = 0.80f;
constexpr float kDecayInfiniteAt  = 0.85f;
struct T60Range {
    float minS, maxS;
};
constexpr T60Range kT60Plate{0.4f, 12.0f};
constexpr T60Range kT60Hall{0.8f, 25.0f};
constexpr T60Range kT60Massive{2.0f, 120.0f};

inline const T60Range& t60Range(int space) { return space == 0 ? kT60Plate : (space == 1 ? kT60Hall : kT60Massive); }

// 1 / T60 in 1/s: 0 means infinite. Kept as a rate so the crossover is a
// straight line and a loop gain follows as 10^(−3 · length · rate).
inline float decayRate(float v, int space)
{
    const T60Range& r = t60Range(space);
    if (v <= kDecayFiniteTop) return 1.0f / expLerp(r.minS, r.maxS, v / kDecayFiniteTop);
    if (v >= kDecayInfiniteAt) return 0.0f;
    const float share = (kDecayInfiniteAt - v) / (kDecayInfiniteAt - kDecayFiniteTop);
    return share / r.maxS;
}

// TONE (P3), SPEC §3 / ADR 0012: noon flat (the fixed damping only); CCW
// darkens each pass, CW brightens, never above a loop gain of 1. Expressed
// as the high end's T60 over the low end's (1 = no damping at all).
// M1 placeholder for the tilt (SPEC §4.6): one ratio per line through its
// absorbent filter; the pivoted shelf lands with the loop chain (M6).
inline float toneHighT60Ratio(float v)
{
    constexpr float kDark = 0.12f, kBright = 0.92f;
    const float noon = voicing::kDampingHighT60Ratio;
    return v < 0.5f ? kDark + (noon - kDark) * (v * 2.0f) : noon + (kBright - noon) * ((v - 0.5f) * 2.0f);
}

// SIZE (P5) in HALL: mean line delay, seconds (SPEC §3).
inline float hallMeanSeconds(float v)
{
    return 0.001f * expLerp(voicing::kHallMeanMinMs, voicing::kHallMeanMaxMs, v);
}

// TIME (P7) in NORMAL: predelay 0 → 250 ms, ∝ x² (ADR 0015).
inline float normalPredelaySeconds(float v) { return 0.25f * v * v; }

} // namespace iv::map
