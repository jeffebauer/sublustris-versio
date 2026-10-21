#pragma once
// The wet's last stages before MIX (SPEC §4.9, §4.10): DARKEN, the duck
// (ADR 0014), and the soft limiter. Both act on the wet only, so MIX fully
// CCW stays a clean passthrough.
#include "params/SpaceVoicing.h"

#include <cmath>

namespace iv::dsp {

// DARKEN: a two-pole low-pass on the wet whose cutoff closes with the duck
// amount (log steps from the top of the band to kDuckDarkHz), plus a dip.
class Darken {
public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; reset(); }
    void reset()
    {
        for (auto& v : z_) v = 0.0f;
        coeff_ = 1.0f;
        gain_ = 1.0f;
    }
    // amount 0..1, once per control chunk. At 0 the filter is exactly open
    // (coefficient 1, gain 1), so the wet passes bit for bit.
    void set(float amount)
    {
        amount_ = amount;
        const float hz = 20000.0f * std::pow(voicing::kDuckDarkHz / 20000.0f, amount);
        coeff_ = amount <= 0.0f || hz >= 0.45f * sampleRate_ ? 1.0f : 1.0f - std::exp(-6.2831853f * hz / sampleRate_);
        gain_ = std::pow(10.0f, voicing::kDuckDipDb * amount / 20.0f);
    }
    void process(float& l, float& r)
    {
        z_[0] += coeff_ * (l - z_[0]);
        z_[1] += coeff_ * (z_[0] - z_[1]);
        z_[2] += coeff_ * (r - z_[2]);
        z_[3] += coeff_ * (z_[2] - z_[3]);
        l = gain_ * z_[1];
        r = gain_ * z_[3];
    }

private:
    float sampleRate_ = 48000.0f, coeff_ = 1.0f, gain_ = 1.0f, amount_ = 0.0f;
    float z_[4] = {};
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
        const float need = a * gain_ > voicing::kLimitCeiling ? voicing::kLimitCeiling / a : 1.0f;
        if (need < gain_) gain_ = need;                // instant down
        else gain_ += release_ * (1.0f - gain_);       // slow back up
        l *= gain_;
        r *= gain_;
    }
    float gain() const { return gain_; }

private:
    float release_ = 0.0001f, gain_ = 1.0f;
};

} // namespace iv::dsp
