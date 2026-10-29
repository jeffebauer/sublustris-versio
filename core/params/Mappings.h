#pragma once
// Normalised (0–1) → internal units. The only place mapping curves live, so a
// setting in the Plugin sounds identical on the module (SPEC §6). Curves land
// per milestone; numbers marked (start) in SPEC §3 are starting guesses.
#include "dsp/Drift.h"
#include "dsp/Era.h"
#include "dsp/EraTexture.h"
#include "dsp/Transport.h"
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
IV_CTL_OPT inline MixGains mixGains(float v) { return {std::cos(v * 0.5f * kPi), std::sin(v * 0.5f * kPi)}; }

// DECAY (P2), SPEC §3: 0–0.80 T60 exponential per SPACE; 0.80–0.85 a
// crossover to infinite (1/T60 falls linearly to 0); 0.85–1 infinite.
// PLATE's and HALL's tops 30 / 60 s since v1.0.53 (ADR 0034; were 12 / 25).
constexpr float kDecayFiniteTop   = 0.80f;
constexpr float kDecayInfiniteAt  = 0.85f;
struct T60Range {
    float minS, maxS;
};
constexpr T60Range kT60Plate{0.4f, 30.0f};
constexpr T60Range kT60Hall{0.8f, 60.0f};
constexpr T60Range kT60Massive{2.0f, 120.0f};

inline const T60Range& t60Range(int space) { return space == 0 ? kT60Plate : (space == 1 ? kT60Hall : kT60Massive); }

// 1 / T60 in 1/s: 0 means infinite. Kept as a rate so the crossover is a
// straight line and a loop gain follows as 10^(−3 · length · rate).
IV_CTL_OPT inline float decayRate(float v, int space)
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
IV_CTL_OPT inline float hallMeanSeconds(float v)
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
// 0.1 → 4 s, exponential. SHIMMER: how much (the send, linear; ADR 0024).
inline float normalPredelaySeconds(float v) { return 0.25f * v * v; }
// MASSIVE in NORMAL: 0 → 250 ms in straight steps (v1.0.21, knob_feel). Its wet already arrives
// 0.3–0.6 s after a hit (the swell, the SIZE predelay), so x² added under 10 ms over the first 20 %.
inline float massiveNormalPredelaySeconds(float v) { return 0.25f * v; }
// BLOOM: 0.12 → 4 s (v1.0.21, knob_feel: below 0.12 s the swell stage crossfades to the direct path,
// Swell.h, so a 0.1 s start made the first notch jump 3-6×).
inline float bloomSeconds(float v) { return expLerp(0.12f, 4.0f, v); }

// SIZE (P5) in PLATE: the tank's scale, 0.5 → 1.59 of Dattorro's (SPEC §3).
inline float plateScale(float v) { return expLerp(voicing::kPlateMinScale, voicing::kPlateMaxScale, v); }

// DRIFT (P6, MOTION on the panel), bipolar (ADR 0012). (ERA's cassette added its own wow and flutter until v1.0.42.)
// v1.0.46: on the left side the tape transport on the wet carries most of the wow (transportAmounts), so the
// tank's lines keep kTransportLineShare of theirs; on the right Wander grows over the side's last half.
inline dsp::DriftAmounts driftAmounts(float drift)
{
    using namespace voicing;
    dsp::DriftAmounts a;
    if (drift < kDriftDeadLo) {
        const float t = (kDriftDeadLo - drift) / kDriftDeadLo;
        a.wowCents = kWowCents * t;
        a.wowHz = kWowHzMin + (kWowHzMax - kWowHzMin) * t;
        a.flutterCents = kFlutterCents * t;
        a.flutterHz = kFlutterHzMin + (kFlutterHzMax - kFlutterHzMin) * t;
        a.wowCents *= kTransportLineShare;
        a.flutterCents *= kTransportLineShare;
    } else if (drift > kDriftDeadHi) {
        const float t = (drift - kDriftDeadHi) / (1.0f - kDriftDeadHi);
        a.wowCents = kWanderCents * t; // Wander: the same random generator, quicker, lines fanned out
        a.wowHz = kWanderHzMin + (kWanderHzMax - kWanderHzMin) * t;
        a.rateSpread = kWanderRateSpread;
        const float x = t > 0.5f ? 2.0f * t - 1.0f : 0.0f; // 0 over the side's first half, 1 at its end
        const float g = 1.0f + kWanderGrow * x * x, r = 1.0f + kWanderRateGrow * x * x;
        a.wowCents *= g;
        a.wowHz *= r;
    }
    return a;
}

// 0 below from, a smoothstep to 1 at to.
inline float layer(float v, float from, float to)
{
    const float t = clamp01((v - from) / (to - from));
    return t * t * (3.0f - 2.0f * t);
}

// MOTION's left side (v1.0.46, dsp/Transport.h), t 0 at the dead zone, 1 fully left: the transport's wow and
// flutter, more variance over the side's last half (eased in), crinkle over its last 40 % (rising straight), and
// a counted share of the crinkle bursts with a dip.
IV_SIZE_OPT inline dsp::TransportAmounts transportAmounts(float drift)
{
    using namespace voicing;
    dsp::TransportAmounts a;
    const float t = drift < kDriftDeadLo ? (kDriftDeadLo - drift) / kDriftDeadLo : 0.0f;
    a.wowCents = kTransportWowCents * t;
    a.wowHz = kTransportWowHzMin + (kTransportWowHzMax - kTransportWowHzMin) * t;
    a.flutterCents = kTransportFlutterCents * t;
    a.flutterHz = kTransportFlutterHzMin + (kTransportFlutterHzMax - kTransportFlutterHzMin) * t;
    a.crinkleRate = kTransportCrinklePerSecond * clamp01((t - 0.6f) / 0.4f);
    a.variance = layer(t, 0.5f, 1.0f);
    a.wowCents *= 1.0f + 0.7f * a.variance;    // 12 -> ~20 c at full
    a.flutterCents *= 1.0f + 0.5f * a.variance; // 3 -> 4.5 c
    a.crinkleDip = kTransportDipShare;
    return a;
}

// ERA (P4; v1.0.42, the owner's pick C on renders/era_rethink, "one wear amount"; it replaces ADR 0021's
// five eras): the Neve stays in the loop as the base at every setting (its warmth and its ×0.7 highs,
// voicing::kNeveHighRatio, never darker with ERA; no wow or flutter, DRIFT's), and the wear on the wet
// (dsp/EraTexture.h) comes in over the first twentieth of the knob, then deepens with it: tape saturation,
// the BBD's compander breathing, the radio's band and wandering dip; the grit (the BBD's clock and the grain,
// the Midiverb's floating step) from 0.65 on (v1.0.49); and with them the tape's in-loop compression, its
// hiss and the crackle into the tank. ERA 0 is the Neve alone.
struct EraWear {
    dsp::TexMix tex;                           // the wet's chain (dsp/EraTexture.h)
    float compress = 0, hiss = 0, crackle = 0; // the tape's in-loop compression, hiss and crackle, 0..1
    float makeup = 1.0f;                       // wet gain keeping ERA's loudness level
};
IV_CTL_OPT inline EraWear eraMix(float v)
{
    using namespace voicing;
    EraWear e;
    dsp::TexMix& x = e.tex;
    const float t = clamp01(v / 0.05f);
    const float s = t * t * (3.0f - 2.0f * t); // a smoothstep over the first twentieth
    x.tapeW = x.breathW = s;
    x.tapeT = x.breathT = v;
    e.compress = e.hiss = e.crackle = s * v;
    // The grit (the BBD's clock and the grain) from 0.65 on the knob (v1.0.49, the owner's pick D on
    // renders/wear_grit: "it feels like the bit reduction is coming in too early"). Past 0.65 the rest of the
    // knob is u (0..1): the grit's blend comes in over its first fifth (a smoothstep) and its intensity goes
    // from its finest (the clock at 32 kHz, the grain at 9 bits) to its full at WEAR 1.
    const float u = clamp01((v - kWearGritStart) / (1.0f - kWearGritStart));
    const float a = clamp01(u / 0.2f);
    const float g = a * a * (3.0f - 2.0f * a);
    x.clockW = g * u;
    x.clockT = u;
    x.grainW = g;
    x.grainT = u;
    x.bandW = s * v * std::sqrt(v);
    x.bandT = v;
    // The chain's level: its own slow hold (EraTexture), and what it leaves on music (LUFS against the
    // Neve base, HALL and MASSIVE: the skank, keys, the full mix's send): the band ~0.7 dB.
    const float bandDb = 0.7f * x.bandW; // its own statement: no fused multiply-add (renders as picked)
    e.makeup = std::pow(10.0f, (kMakeupNeveDb + bandDb) / 20.0f);
    return e;
}

// The floor governor's weight across DECAY's crossover (ADR 0008): 0 while
// the tail is finite, 1 from infinite on.
IV_CTL_OPT inline float governorWeight(float decay)
{
    return clamp01((decay - kDecayFiniteTop) / (kDecayInfiniteAt - kDecayFiniteTop));
}

// The floor governor's own weight (v1.0.56, ADR 0008 amended): PLATE and MASSIVE ease it in from
// voicing::kGovernorEaseFrom as t^3, so turning towards endless glides instead of dropping at 0.80;
// HALL as governorWeight. 0 at and below 0.70 and 1 from 0.85 in every SPACE. The duck, the hold and
// everything else DECAY's crossover fades keep governorWeight.
IV_CTL_OPT inline float floorWeight(float decay, int space)
{
    if (space == 1) return governorWeight(decay);
    const float t = clamp01((decay - voicing::kGovernorEaseFrom) / (kDecayInfiniteAt - voicing::kGovernorEaseFrom));
    return t * t * t;
}

// MASSIVE's high-end T60 share (v1.0.31, ADR 0022): TONE and ERA's share scaled to MASSIVE's own
// (kMassiveHighT60Ratio), gliding back to the shared one across DECAY's crossover, so an infinite
// MASSIVE still darkens towards its floor (ADR 0008).
IV_CTL_OPT inline float massiveHighRatio(float toneRatio, float decay)
{
    const float own = toneRatio * (voicing::kMassiveHighT60Ratio / voicing::kDampingHighT60Ratio);
    return own + governorWeight(decay) * (toneRatio - own);
}

// MASSIVE's DECAY: as decayRate(v, 2), but its shortest T60 is at least kMassiveMinPasses mean
// lines (v1.0.21, knob_feel; 6 since v1.0.32, ADR 0023): ~2.1 s at SIZE 0, ~5 s at noon, ~7 s at
// SIZE 0.7, 12 s at full SIZE.
IV_CTL_OPT inline float massiveDecayRate(float v, float size)
{
    const float lo = std::fmax(kT60Massive.minS, voicing::kMassiveMinPasses * massiveMeanSeconds(size));
    if (v <= kDecayFiniteTop) return 1.0f / expLerp(lo, kT60Massive.maxS, v / kDecayFiniteTop);
    if (v >= kDecayInfiniteAt) return 0.0f;
    return (kDecayInfiniteAt - v) / (kDecayInfiniteAt - kDecayFiniteTop) / kT60Massive.maxS;
}

// MASSIVE: the level a finite tail gains by ringing longer than decayRate(v, 2) would make it
// (massiveDecayRate's longer shortest tails at big SIZEs), taken off again so each setting keeps
// its loudness (energy goes with T60, as hallSizeLevelTrim); 1 where the two curves agree.
IV_CTL_OPT inline float massiveDecayLevelTrim(float v, float size)
{
    const float now = massiveDecayRate(v, size), was = decayRate(v, 2);
    return now > 0.0f && was > 0.0f ? std::sqrt(now / was) : 1.0f;
}

// DECAY's level make-up on the wet (v1.0.28): what a tail shorter than the v1.0.28 finite top
// (kDecayLevelTopSeconds) loses by being shorter (SpaceVoicing.h, kDecayLevelPower). The plain decayRate
// curve: HALL's SIZE has hallSizeLevelTrim, MASSIVE's longer shortest tails massiveDecayLevelTrim. 1 at
// that top and past it, and at infinite. PLATE and HALL (v1.0.53, ADR 0034): their longer tails are
// trimmed back to the level the same DECAY had before (kDecayLongTrimDb), 0 dB again at infinite; the trim
// lets go on the floor governor's weight (PLATE's eases in from 0.70 since v1.0.56).
IV_CTL_OPT inline float decayLevelMakeup(float v, int space)
{
    const float r = decayRate(v, space);
    if (r <= 0.0f) return 1.0f;
    const float t = 1.0f / r, top = voicing::kDecayLevelTopSeconds[space], ref = std::fmin(voicing::kDecayLevelRefSeconds, top);
    const float shortGain = std::pow(ref / std::fmin(t, ref), voicing::kDecayLevelPower[space]);
    const float longGain = top > ref ? std::pow(top / std::fmin(std::fmax(t, ref), top), voicing::kDecayLevelPowerLong[space]) : 1.0f;
    float g = shortGain * longGain;
    if (space != 2) {
        const float w = 1.0f - floorWeight(v, space);
        const float db = voicing::kDecayLongTrimDb[space] * layer(v, voicing::kDecayLongTrimFrom[space], voicing::kDecayLongTrimTo[space]) * (w * w * w);
        g *= std::pow(10.0f, -db / 20.0f);
    }
    return g;
}

// HALL: The level a finite tail loses by being shorter (its energy goes with its
// T60), made up on the output; none at infinite, where nothing is shorter.
IV_CTL_OPT inline float hallSizeLevelTrim(float size, float decay)
{
    const float full = 1.0f / std::sqrt(hallT60Scale(size));
    return full + (1.0f - full) * governorWeight(decay);
}

} // namespace iv::map
