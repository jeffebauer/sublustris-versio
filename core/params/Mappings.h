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
inline float bloomSeconds(float v) { return expLerp(0.1f, 4.0f, v); }
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
inline float midiEngageFeel(float v);
inline float eraKnobFeel(float v);
// feelEra: the knob-feel audit's ERA prototype (feel::kEra, below); false = today.
inline dsp::EraMix eraMix(float v, bool feelEra = false)
{
    using namespace voicing;
    if (feelEra) v = eraKnobFeel(v);
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
    m.midiEngage = feelEra ? midiEngageFeel(v) : clamp01((v - kEraMidiverbFrom) / kMidiEngageSpan);
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

// ---- PROTOTYPES: the M8 knob-feel audit (docs/research/knob-feel-audit.md) ------------------
// Smoother curves for the owner's pick page (renders/knob_feel), each off unless its bit is set
// (Engine::setKnobFeel; IV_KNOB_FEEL in the Renderer). With no bit set nothing here is called.
namespace feel {
constexpr unsigned kDecay = 1u;  // MASSIVE's shortest DECAY lasts a few passes of its lines
constexpr unsigned kEra   = 2u;  // ERA: Clean only at the very end; the Midiverb fades in across its stretch
constexpr unsigned kTime  = 4u;  // TIME in MASSIVE NORMAL: predelay in even steps
constexpr unsigned kBloom = 8u;  // TIME in BLOOM: starts where the swell is all taps
constexpr unsigned kHits  = 16u; // MASSIVE's early taps build evenly into the first pass
// Tuning, set by the Renderer for the audit's trials (defaults are the page's B).
struct Tuning {
    float massivePasses = 3.0f;  // kDecay: the shortest T60 is at least this many mean lines
    float eraCleanTop   = 0.03f; // kEra: Clean below this, Neve from here (today 0.10)
    float midiPower     = 2.0f;  // kEra: the Midiverb box's engage = (share of its stretch)^this
    float bloomMin      = 0.12f; // kBloom: BLOOM's shortest swell, s (today 0.1)
    float earlyTapScale = 0.71f; // kHits: MASSIVE's early taps × this (twice the taps, the same energy)
    int   earlyTapCount = 6;     // kHits: early taps per line (today 3)
    float earlyTapPower = 1.0f;  // kHits: their gains rise with their place to this power (today 1)
    int   earlyTapEven  = 1;     // kHits: 1 = the lines' taps interleaved, shorter lines first (Fdn::setEarlyLayout); 0 = seeded jitter (today)
};
inline Tuning& tuning()
{
    static Tuning t;
    return t;
}
} // namespace feel

// kDecay: MASSIVE's DECAY from max(2 s, passes × the mean line) to 120 s. With a 0.84 s mean
// line (SIZE noon) and a 2 s T60, one pass loses 25 dB, so the tail is the build and one
// echo, and the first notch (3.3 s) jumped to a real tail; a few passes is the shortest a
// big space can ring (as the PLATE's comment says of a big plate). Crossover unchanged.
inline float massiveDecayRateFeel(float v, float size)
{
    const float lo = std::fmax(kT60Massive.minS, feel::tuning().massivePasses * massiveMeanSeconds(size));
    if (v <= kDecayFiniteTop) return 1.0f / expLerp(lo, kT60Massive.maxS, v / kDecayFiniteTop);
    if (v >= kDecayInfiniteAt) return 0.0f;
    return (kDecayInfiniteAt - v) / (kDecayInfiniteAt - kDecayFiniteTop) / kT60Massive.maxS;
}

// kEra: the knob read so Clean ends at eraCleanTop instead of 0.10 and Neve's share grows in
// straight steps (the layers above keep their places), and the Midiverb box engages across
// its whole stretch (0.85 → 1, as the share squared, which spreads the tail's shortening
// most evenly) instead of within 2 % of it. A share of the box in the loop: |a·H + (1 − a)| ≤ 1 when |H| ≤ 1.
inline float eraKnobFeel(float v)
{
    const float c = feel::tuning().eraCleanTop, n = voicing::kEraNeveFrom;
    if (v >= voicing::kEraCassetteFrom || c <= 0.0f) return v;
    if (v <= c) return v * (n / c); // still Clean (below kEraNeveFrom)
    // Read through the inverse of layer()'s smoothstep, so Neve's share grows in straight steps
    // from the first notch instead of starting flat.
    const float t = (v - c) / (voicing::kEraCassetteFrom - c);
    const float straight = 0.5f - std::sin(std::asin(1.0f - 2.0f * t) / 3.0f);
    return n + straight * (voicing::kEraCassetteFrom - n);
}
inline float midiEngageFeel(float v)
{
    const float t = clamp01((v - voicing::kEraMidiverbFrom) / (1.0f - voicing::kEraMidiverbFrom));
    return std::pow(t, feel::tuning().midiPower);
}

// kTime: MASSIVE's NORMAL predelay 0 → 250 ms in straight steps. Its wet already arrives
// 0.3–0.6 s after a hit (the swell, the SIZE predelay), so x² added under 10 ms over the
// first 20 %; heard as a share of that arrival, straight steps are even.
inline float massiveNormalPredelayFeel(float v) { return 0.25f * v; }

// kBloom: BLOOM's swell bloomMin → 4 s. Below 0.12 s the swell stage crossfades to the
// direct path (Swell.h), so BLOOM's first notch (0.1 → 0.145 s) crossed the last of that
// crossfade and the wet's arrival jumped 3-4×.
inline float bloomSecondsFeel(float v) { return expLerp(feel::tuning().bloomMin, 4.0f, v); }

// HALL: The level a finite tail loses by being shorter (its energy goes with its
// T60), made up on the output; none at infinite, where nothing is shorter.
inline float hallSizeLevelTrim(float size, float decay)
{
    const float full = 1.0f / std::sqrt(hallT60Scale(size));
    return full + (1.0f - full) * governorWeight(decay);
}

} // namespace iv::map
