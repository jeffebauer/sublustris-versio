#pragma once
// The wet's last stages before MIX (SPEC §4.9, §4.10): TONE's tilt and the
// soft limiter (the ducking is a gain in Engine, ADR 0014). Both act on the wet only, so MIX fully
// CCW stays a clean passthrough.
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"

#include <cmath>

namespace iv::dsp {

// TONE's tilt (M1 notes: dark and bright "don't feel like extreme ends...
// lower lows, higher highs"). TONE's loop damping (the highs' T60) barely
// shows on a short tail or the first sound, so the wet is also tilted round
// kToneTiltHz: CCW more lows and fewer highs, CW the opposite, noon flat.
// One one-pole split per side: y = low·lp + high·(x − lp).
class Tilt {
public:
    void prepare(float sampleRate)
    {
        coeff_ = 1.0f - std::exp(-6.2831853f * voicing::kToneTiltHz / sampleRate);
        reset();
    }
    void reset() { lp_[0] = lp_[1] = 0.0f; }
    // TONE 0..1, once per control chunk.
    void set(float tone)
    {
        const float d = tone < 0.5f ? (0.5f - tone) * 2.0f : 0.0f, b = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
        lowTarget_ = std::pow(10.0f, (d * voicing::kToneDarkLowDb + b * voicing::kToneBrightLowDb) / 20.0f);
        highTarget_ = std::pow(10.0f, (d * voicing::kToneDarkHighDb + b * voicing::kToneBrightHighDb) / 20.0f);
    }
    void snap() { low_ = lowTarget_; high_ = highTarget_; }
    void process(float& l, float& r)
    {
        low_ += kGlide * (lowTarget_ - low_); // per sample, so a fast TONE move can't step
        high_ += kGlide * (highTarget_ - high_);
        lp_[0] += coeff_ * (l - lp_[0]);
        lp_[1] += coeff_ * (r - lp_[1]);
        l = low_ * lp_[0] + high_ * (l - lp_[0]);
        r = low_ * lp_[1] + high_ * (r - lp_[1]);
    }

private:
    static constexpr float kGlide = 0.002f; // ~10 ms at 48 kHz
    float coeff_ = 0.1f, low_ = 1.0f, high_ = 1.0f, lowTarget_ = 1.0f, highTarget_ = 1.0f, lp_[2] IV_ZI;
};

// The soft limiter: instant attack on the peak, kLimitReleaseSeconds release,
// so the wet never passes kLimitCeiling; gain() is what it applies (LEDs).
class Limiter {
public:
    void prepare(float sampleRate)
    {
        release_ = 1.0f - std::exp(-1.0f / (voicing::kLimitReleaseSeconds * sampleRate));
        reset();
    }
    void reset() { gain_ = 1.0f; }
    void process(float& l, float& r)
    {
        const float a = std::fmax(std::fabs(l), std::fabs(r));
        const float need = a > voicing::kLimitCeiling ? voicing::kLimitCeiling / a : 1.0f; // the most this sample allows
        if (need < gain_) gain_ = need;                // instant down
        else gain_ = std::fmin(need, gain_ + release_ * (1.0f - gain_)); // slow back up, never past what this sample allows
        l *= gain_;
        r *= gain_;
    }
    float gain() const { return gain_; }

private:
    float release_ = 0.0001f, gain_ = 1.0f;
};

} // namespace iv::dsp
