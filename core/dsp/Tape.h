#pragma once
// Tape out (ERA's cassette stage on the wet, outside the loop; ADR 0019). v1.0.27, the owner's pick on
// renders/tape_out (E on all 24 cards, "a little stronger" with light hysteresis; docs/research/tape-out.md):
//   record pre-emphasis: a first-order high shelf at kTapeEmphHz, up to +kTapeEmphDb, so the highs reach
//                        the tape first (Strymon's tape paper);
//   drive:               up to +kTapeDriveDb across the cassette layer, taken off again after the curve
//                        (unity for small signals: only peaks and the highs change);
//   light hysteresis:    a play operator (rate-independent backlash, the textbook hysteresis) whose width
//                        follows the level (our own, unsourced as audio);
//   the curve:           the asymmetric rational tanh (a third of v1.0.22's bias, v1.0.26), at 2x
//                        (v1.0.38, owner on renders/tape_os: "no audible difference"; was 4x);
//   de-emphasis:         the exact inverse shelf, then a 5 Hz DC blocker.
#include "dsp/Oversampler.h"
#include "dsp/SizeOpt.h"
#include "params/EraVoicing.h"

#include <cmath>

namespace iv::dsp {

class TapeOut {
public:
    void reset()
    {
        preLp_ = deLp_ = dc_ = play_ = rms_ = 0.0f;
        os1_.reset();
#ifndef IV_FIXED_VOICINGS
        os2_.reset();
#endif
    }

#ifndef IV_FIXED_VOICINGS
    // The curve at 2x (only the outer Oversampler runs) since v1.0.38 (overnight review, 26 Oct 2026;
    // the owner's pick on renders/tape_os). setFactor(4): both, as before v1.0.38 (desktop only).
    void setFactor(int f) { factor_ = f == 4 ? 4 : 2; }
#endif

    // c: the cassette layer 0..1. Recomputes the coefficients only when c (or the rate) changes.
    void set(float c, float sampleRate)
    {
        if (c == c_ && sampleRate == sr_) return;
        c_ = c;
        sr_ = sampleRate;
        gEmph_ = std::pow(10.0f, voicing::kTapeEmphDb * c / 20.0f);
        drive_ = std::pow(10.0f, voicing::kTapeDriveDb * c / 20.0f);
        wPre_ = 1.0f - std::exp(-6.2831853f * voicing::kTapeEmphHz / sampleRate);
        wDe_ = 1.0f - std::exp(-6.2831853f * voicing::kTapeEmphHz / gEmph_ / sampleRate);
        wRms_ = 1.0f - std::exp(-1.0f / (0.05f * sampleRate));
        wDc_ = 6.2831853f * 5.0f / sampleRate;
        b_ = voicing::kTapeBias * c;
        tb_ = th(b_);
    }

#if defined(IV_FIXED_VOICINGS)
    IV_NOINLINE // one shared copy (inlined, GCC pasted it 4x into Engine::process: 1,766 vs 960 B)
#endif
    float process(float x)
    {
        preLp_ += wPre_ * (x - preLp_);
        const float e = x + (gEmph_ - 1.0f) * (x - preLp_);
        rms_ += wRms_ * (e * e - rms_); // the play width follows the level
        const float width = voicing::kTapePlayWidth * std::sqrt(rms_) * drive_;
        auto sat = [&](float u) {
            u *= drive_;
            if (play_ < u - width) play_ = u - width;
            if (play_ > u + width) play_ = u + width;
            return (th(play_ + b_) - tb_) / (1.0f - tb_ * tb_) / drive_;
        };
#if !defined(IV_FIXED_VOICINGS)
        Oversampler& inner = os2_;
        float y = factor_ == 4 ? os1_.process(e, [&](float u) { return inner.process(u, sat); }) : os1_.process(e, sat);
#else
        float y = os1_.process(e, sat);
#endif
        deLp_ += wDe_ * (y - deLp_);
        y = y + (1.0f / gEmph_ - 1.0f) * (y - deLp_);
        dc_ += wDc_ * (y - dc_);
        return y - dc_;
    }

private:
    // A rational tanh, exact enough for a saturator (no std::tanh in flash).
    static float th(float x)
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float c_ = -1.0f, sr_ IV_ZI;
    float gEmph_ = 1.0f, drive_ = 1.0f, wPre_ IV_ZI, wDe_ IV_ZI, wRms_ IV_ZI, wDc_ IV_ZI, b_ IV_ZI, tb_ IV_ZI;
    float preLp_ IV_ZI, deLp_ IV_ZI, dc_ IV_ZI, play_ IV_ZI, rms_ IV_ZI;
    Oversampler os1_;
#ifndef IV_FIXED_VOICINGS
    Oversampler os2_; // setFactor(4) only
    int factor_ = 2;  // setFactor
#endif
};

} // namespace iv::dsp
