#pragma once
// DRIFT (P6, bipolar, ADR 0012), one generator per tank line: how far that
// line's read wanders, in samples, updated at control rate. Left of noon:
// tape wow (smooth random) and flutter; right of noon: Wander, the same
// smooth random movement but quicker, each line's rate fanned out ±50 %
// (v1.0.20). ERA's
// cassette stage adds its own wow and flutter on top (SPEC §4.7). Noon is
// still (the tank's modulation floor still runs).
//
// Pitch deviation p from a delay swinging by A samples at f Hz is
// 2π f A / fs, so for a depth in cents A = (2^(c/1200) − 1) fs / (2π f).
#include "dsp/SizeOpt.h"
#include "params/EraVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

struct DriftAmounts {
    float wowCents = 0, wowHz = 0.5f;
    float flutterCents = 0, flutterHz = 7.0f;
    float rateSpread = 0.3f; // how far the lines' rates fan out: 0.85-1.15 (tape), 0.5-1.5 (Wander)
#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (renders/drift_rethink): a regular sine on top, its phase spread across the lines
    // (sinePhaseSpan of a cycle from the first line to the last) and its rate fanned by sineRateSpread.
    float sineCents = 0, sineHz = 3.0f, sineRateSpread = 0.12f, sinePhaseSpan = 0.75f;
#endif
};

class Drift {
public:
    void prepare(float sampleRate, uint32_t seed, float spread)
    {
        sampleRate_ = sampleRate;
        rng_ = seed ? seed : 1u;
        spread_ = spread; // 0..1: this line's rate offset
        reset();
    }
    void reset()
    {
        wowA_ = wowB_ = wowTarget_ = 0.0f;
        wowCountdown_ = 0.0f;
        flutterPhase_ = 0.37f * spread_;
        value_ = 0.0f;
#ifndef IV_FIXED_VOICINGS
        sinePhase_ = 0.0f;
#endif
    }

    // Advances dt seconds; returns the read offset in samples. maxSamples
    // caps the swing (a share of the line's length, the room in memory).
    IV_SIZE_OPT float control(float dt, const DriftAmounts& a, float maxSamples) // once per block: built for size (flash)
    {
        const float rateSkew = 1.0f - 0.5f * a.rateSpread + a.rateSpread * spread_;
        // Wow: a smooth random line (new target every ~1/rate, smoothed twice).
        wowCountdown_ -= dt;
        if (wowCountdown_ <= 0.0f) {
            wowTarget_ = uniform();
            wowCountdown_ = (0.7f + 0.6f * (0.5f + 0.5f * uniform())) / (a.wowHz * rateSkew);
        }
        const float arg = -6.2831853f * a.wowHz * rateSkew * dt;
        if (arg != wcArg_) { // the same every block until DRIFT or ERA moves (std::exp ~110 cycles on the M7)
            wcArg_ = arg;
            wc_ = 1.0f - std::exp(arg);
        }
        const float wc = wc_;
        wowA_ += wc * (wowTarget_ - wowA_);
        wowB_ += wc * (wowA_ - wowB_);
        flutterPhase_ += a.flutterHz * rateSkew * dt;
        flutterPhase_ -= float(int(flutterPhase_));

        if (a.wowCents != depthIn_[0] || a.wowHz != depthIn_[1] || a.flutterCents != depthIn_[2] || a.flutterHz != depthIn_[3]) {
            depthIn_[0] = a.wowCents; // the depths' std::pow, only when DRIFT or ERA moves
            depthIn_[1] = a.wowHz;
            depthIn_[2] = a.flutterCents;
            depthIn_[3] = a.flutterHz;
            wowDepth_ = samplesFor(a.wowCents, a.wowHz);
            flutterDepth_ = samplesFor(a.flutterCents, a.flutterHz);
        }
        float v = wowDepth_ * wowB_ + (flutterDepth_ != 0.0f ? flutterDepth_ * std::sin(6.2831853f * flutterPhase_) : 0.0f);
#ifndef IV_FIXED_VOICINGS
        if (a.sineCents > 0.0f) { // PROTOTYPE (renders/drift_rethink)
            const float hz = a.sineHz * (1.0f + a.sineRateSpread * (spread_ - 0.5f));
            sinePhase_ += hz * dt;
            sinePhase_ -= float(int(sinePhase_));
            v += samplesFor(a.sineCents, hz) * std::sin(6.2831853f * (sinePhase_ + a.sinePhaseSpan * spread_));
        }
#endif
        if (v > maxSamples) v = maxSamples;
        if (v < -maxSamples) v = -maxSamples;
        value_ = v;
        return v;
    }
    float value() const { return value_; }

private:
    float samplesFor(float cents, float hz) const
    {
        if (cents <= 0.0f || hz <= 0.0f) return 0.0f;
        return (std::pow(2.0f, cents / 1200.0f) - 1.0f) * sampleRate_ / (6.2831853f * hz);
    }
    float uniform()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return float(int32_t(rng_)) * (1.0f / 2147483648.0f);
    }

    float    sampleRate_ = 48000.0f, spread_ = 0.0f, value_ = 0.0f;
    uint32_t rng_ = 1;
    float    wowA_ = 0, wowB_ = 0, wowTarget_ = 0, wowCountdown_ = 0, flutterPhase_ = 0;
    float    wcArg_ = 1.0f, wc_ = 0.0f;                                  // the wow's smoothing, for wcArg_
    float    depthIn_[4] = {-1.0f, -1.0f, -1.0f, -1.0f}, wowDepth_ = 0.0f, flutterDepth_ = 0.0f;
#ifndef IV_FIXED_VOICINGS
    float    sinePhase_ = 0.0f; // DriftAmounts::sineCents (PROTOTYPE)
#endif
};

} // namespace iv::dsp
