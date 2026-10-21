#pragma once
// Normalised (0–1) → internal units. The only place mapping curves live, so a
// setting in the Plugin sounds identical on the module (SPEC §6). Curves land
// per milestone; numbers marked (start) in SPEC §3 are starting guesses.
#include "dsp/Drift.h"
#include "dsp/Era.h"
#include "params/EraVoicing.h"
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
    constexpr float kDark = 0.12f, kBright = 1.3f; // CW: the highs outlast the lows (a bright, thinning tail)
    const float noon = voicing::kDampingHighT60Ratio;
    return v < 0.5f ? kDark + (noon - kDark) * (v * 2.0f) : noon + (kBright - noon) * ((v - 0.5f) * 2.0f);
}

// SIZE (P5) in HALL: mean line delay, seconds (SPEC §3).
inline float hallMeanSeconds(float v)
{
    return 0.001f * expLerp(voicing::kHallMeanMinMs, voicing::kHallMeanMaxMs, v);
}

// SIZE (P5) in MASSIVE (ADR 0007, SPEC v1.0.6): from shorter, part-fanned
// lines with some diffusion (already a wash: the owner found SIZE 0's
// echoes "too rhythmic") to long, fanned-out lines with full diffusion (a
// boundless wash), the predelay growing a little with it.
inline float massiveMeanSeconds(float v) { return expLerp(0.35f, 2.0f, v); }
inline float massiveSpread(float v) { return 0.4f + 0.6f * v; }
inline float massiveDiffusion(float v) { return 0.45f + 0.55f * v; } // × the input diffusers' coefficients
inline float massivePredelaySeconds(float v) { return 0.1f * v * v; }

// MASSIVE's mass m(SIZE), 1 → 1.5 (ADR 0007): multiplies its attack.
inline float massiveMass(float size) { return 1.0f + 0.5f * size; }
// MASSIVE's built-in attack in NORMAL: A0(DECAY) 0.5 → 0.2 s (log; higher
// DECAY, faster swell, Sagittarius) × m(SIZE).
inline float massiveNormalAttackSeconds(float decay, float size) { return expLerp(0.5f, 0.2f, decay) * massiveMass(size); }

// TIME (P7), ADR 0015. NORMAL: predelay 0 → 250 ms, ∝ x². BLOOM: swell
// 0.1 → 4 s. REVERSE: segment length 0.2 → 2 s. Both exponential.
inline float normalPredelaySeconds(float v) { return 0.25f * v * v; }
inline float bloomSeconds(float v) { return expLerp(0.1f, 4.0f, v); }
inline float reverseSeconds(float v) { return expLerp(0.2f, 2.0f, v); }

// SIZE (P5) in PLATE: the tank's scale, 0.5 → 1.6 of Dattorro's (SPEC §3).
inline float plateScale(float v) { return expLerp(voicing::kPlateMinScale, voicing::kPlateMaxScale, v); }
// While frozen, SIZE and DECAY play the companion (ADR 0009, SPEC §4.2).
inline float companionScale(float v) { return expLerp(voicing::kCompanionMinScale, voicing::kCompanionMaxScale, v); }
inline float companionDecayRate(float v) { return 1.0f / expLerp(voicing::kCompanionT60Min, voicing::kCompanionT60Max, v); }

// DRIFT (P6), bipolar (ADR 0012) + ERA's cassette wow/flutter (SPEC §4.7).
inline dsp::DriftAmounts driftAmounts(float drift, float cassette)
{
    using namespace voicing;
    dsp::DriftAmounts a;
    if (drift < kDriftDeadLo) {
        const float t = (kDriftDeadLo - drift) / kDriftDeadLo;
        a.wowCents = kWowCents * t;
        a.wowHz = kWowHzMin + (kWowHzMax - kWowHzMin) * t;
        a.flutterCents = kFlutterCents * t;
        a.flutterHz = kFlutterHzMin + (kFlutterHzMax - kFlutterHzMin) * t;
    } else if (drift > kDriftDeadHi) {
        const float t = (drift - kDriftDeadHi) / (1.0f - kDriftDeadHi);
        a.driftCents = kDriftCents * t;
        a.driftHz = kDriftHzMin + (kDriftHzMax - kDriftHzMin) * t;
    }
    // The cassette's own wow and flutter add on top (rates blended by depth).
    if (cassette > 0.0f) {
        const float w = kCassetteWowCents * cassette, f = kCassetteFlutterCents * cassette;
        if (a.wowCents + w > 0.0f) a.wowHz = (a.wowCents * a.wowHz + w * kCassetteWowHz) / (a.wowCents + w);
        a.wowCents += w;
        if (a.flutterCents + f > 0.0f) a.flutterHz = (a.flutterCents * a.flutterHz + f * kCassetteFlutterHz) / (a.flutterCents + f);
        a.flutterCents += f;
    }
    return a;
}

// ERA (P4), stacking (ADR 0003): each layer fades in across its stretch
// (smoothstep) and stays.
inline float layer(float v, float from, float to)
{
    const float t = clamp01((v - from) / (to - from));
    return t * t * (3.0f - 2.0f * t);
}
inline dsp::EraMix eraMix(float v)
{
    using namespace voicing;
    dsp::EraMix m;
    m.neve = layer(v, kEraNeveFrom, kEraCassetteFrom);
    m.cassette = layer(v, kEraCassetteFrom, kEraMuFrom);
    m.mu = layer(v, kEraMuFrom, kEraMidiverbFrom);
    m.midiverb = layer(v, kEraMidiverbFrom, 1.0f);
    // The era's darkening, as a share of the high end's T60 (Era.h): Neve
    // a little, the cassette a lot (SPEC §4.7's 16 kHz / 9 kHz in spirit).
    m.highRatioScale = expLerp(expLerp(1.0f, kNeveHighRatio, m.neve), kCassetteHighRatio, m.cassette);
    m.even = kNeveEven * m.neve;
    m.drive = 1.0f + (kCassetteDrive - 1.0f) * m.cassette;
    m.muEngage = clamp01((v - kEraMuFrom) / kMuEngageSpan);
    m.muBits = kMuBitsStart + (kMuBitsEnd - kMuBitsStart) * clamp01((v - kEraMuFrom) / (kEraMidiverbFrom - kEraMuFrom));
    m.midiEngage = clamp01((v - kEraMidiverbFrom) / kMidiEngageSpan);
    m.midiBits = kMidiBitsStart + (kMidiBitsEnd - kMidiBitsStart) * clamp01((v - kEraMidiverbFrom) / (1.0f - kEraMidiverbFrom));
    const float makeupDb = kMakeupNeveDb * m.neve + kMakeupCassetteDb * m.cassette + kMakeupMuDb * m.mu + kMakeupMidiverbDb * m.midiverb;
    m.makeup = std::pow(10.0f, makeupDb / 20.0f);
    return m;
}

// The floor governor's weight across DECAY's crossover (ADR 0008): 0 while
// the tail is finite, 1 from infinite on.
inline float governorWeight(float decay)
{
    return clamp01((decay - kDecayFiniteTop) / (kDecayInfiniteAt - kDecayFiniteTop));
}

} // namespace iv::map
