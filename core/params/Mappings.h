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
    constexpr float kDark = 0.08f, kBright = 1.5f; // M1 notes: wider ends (was 0.12, 1.3) // CW: the highs outlast the lows (a bright, thinning tail)
    const float noon = voicing::kDampingHighT60Ratio;
    return v < 0.5f ? kDark + (noon - kDark) * (v * 2.0f) : noon + (kBright - noon) * ((v - 0.5f) * 2.0f);
}

// HALL's SIZE scales its finite T60s, short for a small room (M1 notes).
inline float hallT60Scale(float size) { return expLerp(voicing::kHallSmallT60Scale, 1.0f, size); }

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
// MASSIVE in NORMAL: 0 → 250 ms in straight steps (v1.0.21, knob_feel). Its wet already arrives
// 0.3–0.6 s after a hit (the swell, the SIZE predelay), so x² added under 10 ms over the first 20 %.
inline float massiveNormalPredelaySeconds(float v) { return 0.25f * v; }
// BLOOM: 0.12 → 4 s (v1.0.21, knob_feel: below 0.12 s the swell stage crossfades to the direct path,
// Swell.h, so a 0.1 s start made the first notch jump 3-6×).
inline float bloomSeconds(float v) { return expLerp(0.12f, 4.0f, v); }
inline float reverseSeconds(float v) { return expLerp(0.2f, 2.0f, v); }
// DECAY in REVERSE: the short reverb's T60, how sudden the stop is.
// SIZE in REVERSE: the short reverb's mean line, the swell's grain.
inline float reverseMeanSeconds(float v) { return expLerp(voicing::kReverseMeanMinSeconds, voicing::kReverseMeanMaxSeconds, v); }
inline float reverseTailSeconds(float v) { return expLerp(voicing::kReverseTailMinSeconds, voicing::kReverseTailMaxSeconds, v); }

// SIZE (P5) in PLATE: the tank's scale, 0.5 → 1.59 of Dattorro's (SPEC §3).
inline float plateScale(float v) { return expLerp(voicing::kPlateMinScale, voicing::kPlateMaxScale, v); }

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
        a.wowCents = kWanderCents * t; // Wander: the same random generator, quicker, lines fanned out
        a.wowHz = kWanderHzMin + (kWanderHzMax - kWanderHzMin) * t;
        a.rateSpread = kWanderRateSpread;
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
    m.neve = clamp01((v - kEraCleanTop) / (kEraCassetteFrom - kEraCleanTop)); // straight steps (v1.0.21)
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
    {
        const float t = clamp01((v - kEraMidiverbFrom) / (1.0f - kEraMidiverbFrom));
        m.midiEngage = kMidiEngagePower == 2.0f ? t * t : std::pow(t, kMidiEngagePower); // across its stretch (v1.0.21)
    }
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

// MASSIVE's DECAY: as decayRate(v, 2), but its shortest T60 is at least kMassiveMinPasses mean
// lines (v1.0.21, knob_feel): 2 s at SIZE 0 (as before), ~2.5 s at noon, ~6 s at full SIZE.
inline float massiveDecayRate(float v, float size)
{
    const float lo = std::fmax(kT60Massive.minS, voicing::kMassiveMinPasses * massiveMeanSeconds(size));
    if (v <= kDecayFiniteTop) return 1.0f / expLerp(lo, kT60Massive.maxS, v / kDecayFiniteTop);
    if (v >= kDecayInfiniteAt) return 0.0f;
    return (kDecayInfiniteAt - v) / (kDecayInfiniteAt - kDecayFiniteTop) / kT60Massive.maxS;
}

// MASSIVE: the level a finite tail gains by ringing longer than decayRate(v, 2) would make it
// (massiveDecayRate's longer shortest tails at big SIZEs), taken off again so each setting keeps
// its loudness (energy goes with T60, as hallSizeLevelTrim); 1 where the two curves agree.
inline float massiveDecayLevelTrim(float v, float size)
{
    const float now = massiveDecayRate(v, size), was = decayRate(v, 2);
    return now > 0.0f && was > 0.0f ? std::sqrt(now / was) : 1.0f;
}

// DECAY's level make-up on the wet (v1.0.28): what a tail shorter than the finite top loses by being
// shorter (SpaceVoicing.h, kDecayLevelPower). The plain decayRate curve: HALL's SIZE has
// hallSizeLevelTrim, MASSIVE's longer shortest tails massiveDecayLevelTrim. 1 at the finite top, and
// through the crossover and infinite (T60 only grows there), so a held tail is as before.
inline float decayLevelMakeup(float v, int space)
{
    const float r = decayRate(v, space);
    if (r <= 0.0f) return 1.0f;
    const float t = 1.0f / r, top = t60Range(space).maxS, ref = std::fmin(voicing::kDecayLevelRefSeconds, top);
    const float shortGain = std::pow(ref / std::fmin(t, ref), voicing::kDecayLevelPower[space]);
    const float longGain = top > ref ? std::pow(top / std::fmin(std::fmax(t, ref), top), voicing::kDecayLevelPowerLong[space]) : 1.0f;
    return shortGain * longGain;
}

// REVERSE's level make-up (v1.0.28), so a short stop doesn't drop below the forward wet, which DECAY
// no longer turns down: measured (HALL, skank / vibes / pad, LUFS) 14 dB quieter at DECAY 0 than 1, not
// a T60 power (TIME's swell carries the long end). dB at DECAY 0, 0.25, 0.5, 0.75; none from 0.75 up,
// where REVERSE was already at the forward wet's level or above.
inline float reverseLevelMakeup(float v)
{
    constexpr float kDb[4] = {11.5f, 5.4f, 0.8f, 0.0f};
    const float x = std::fmin(std::fmax(v, 0.0f), 0.75f) * 4.0f;
    const int i = int(x) < 2 ? int(x) : 2;
    return std::pow(10.0f, (kDb[i] + (x - float(i)) * (kDb[i + 1] - kDb[i])) / 20.0f);
}

// HALL: The level a finite tail loses by being shorter (its energy goes with its
// T60), made up on the output; none at infinite, where nothing is shorter.
inline float hallSizeLevelTrim(float size, float decay)
{
    const float full = 1.0f / std::sqrt(hallT60Scale(size));
    return full + (1.0f - full) * governorWeight(decay);
}

} // namespace iv::map
