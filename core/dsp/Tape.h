#pragma once
// PROTOTYPE (owner, 24 Oct 2026: tape out "very subtle — really only audible on MASSIVE drums. Can we
// make the saturation a little stronger? Should we explore adding tape hysteresis as well?"). The
// cassette stage's tape out, stronger, three ways (docs/research/tape-out.md):
//   strength (see process()): today's exactly, a little stronger, or stronger:
//   stronger:     record pre-emphasis (a high shelf, so the highs saturate first; Strymon's tape
//                 paper), drive up to +24 dB across the cassette stretch, the curve, then the exact
//                 de-emphasis and the drive taken off again (unity for small signals: only peaks change);
//   light hysteresis: a play operator (rate-independent backlash, the textbook hysteresis)
//                 before the curve, its width following the level (our own, unsourced as audio);
//   full hysteresis: a Jiles-Atherton hysteresis in place of the curve, solved by RK2 at 4x
//                 (Chowdhury, DAFx-19, from the paper's equations; normalised parameters are ours).
#include "dsp/Oversampler.h"
#include "params/EraVoicing.h"

#include <cmath>

namespace iv::dsp {

class TapeChannel {
public:
    void reset()
    {
        preLp_ = deLp_ = dc_ = play_ = rms_ = 0.0f;
        h1_ = hd1_ = m_ = 0.0f;
        os1_.reset();
        os2_.reset();
    }

    // c: the cassette layer 0..1. mode = strength × 10 + hysteresis:
    //   strength 0 today's tape out exactly (drive 1 + 2c, no emphasis), 1 a little stronger
    //   (+12 dB drive, +6 dB emphasis at full), 2 stronger (+24 / +12 dB);
    //   hysteresis 0 none, 1 light (a play operator), 2 full (Jiles-Atherton).
    float process(float x, float c, int mode, float sampleRate)
    {
        const int strength = (mode / 10) % 10, hyst = mode % 10; // 100 = today through this path (a check)
        const float k = strength == 2 ? 1.0f : 0.5f;
        const float gEmph = strength == 0 ? 1.0f : std::pow(10.0f, 12.0f * k * c / 20.0f);
        const float drive = strength == 0 ? 1.0f + 2.0f * c : std::pow(10.0f, 24.0f * k * c / 20.0f);
        const float wPre = 1.0f - std::exp(-6.2831853f * kEmphHz / sampleRate);
        const float wDe = 1.0f - std::exp(-6.2831853f * kEmphHz / gEmph / sampleRate);
        preLp_ += wPre * (x - preLp_);
        const float e = x + (gEmph - 1.0f) * (x - preLp_);
        const float b = voicing::kTapeBias * c * (strength == 0 ? drive : 1.0f); // today's: the bias scales with its drive
        const float fs4 = 4.0f * sampleRate;
        rms_ += (1.0f - std::exp(-1.0f / (0.05f * sampleRate))) * (e * e - rms_); // the play width follows the level
        const float width = 0.15f * std::sqrt(rms_) * drive;
        auto curve = [&](float u) {
            if (hyst == 1) {
                if (play_ < u - width) play_ = u - width;
                if (play_ > u + width) play_ = u + width;
                u = play_;
            }
            if (hyst == 2) return ja(u, fs4);
            return asym(u, b);
        };
        auto sat = [&](float u) { return curve(u * drive) / drive; };
        Oversampler& inner = os2_;
        float y = os1_.process(e, [&](float u) { return inner.process(u, sat); });
        deLp_ += wDe * (y - deLp_);
        y = y + (1.0f / gEmph - 1.0f) * (y - deLp_);
        dc_ += 6.2831853f * 5.0f / sampleRate * (y - dc_);
        return y - dc_;
    }

private:
    static constexpr float kEmphHz = 2000.0f;

    static float th(float x)
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    // Today's asymmetric curve at unity drive: slope 1 at 0, even and odd harmonics.
    static float asym(float u, float b)
    {
        const float tb = th(b);
        return (th(u + b) - tb) / (1.0f - tb * tb);
    }

    // Jiles-Atherton, normalised (Ms = 1): Langevin anhysteretic, irreversible pinning k, reversible c,
    // mean-field alpha. H = u; output M × 3a (so the anhysteretic slope at 0 is 1).
    float ja(float h, float fs)
    {
        constexpr float a = 0.6f, alpha = 1.0e-3f, k = 0.45f, c = 0.2f, ms = 1.0f;
        if (h > 10.0f) h = 10.0f;
        if (h < -10.0f) h = -10.0f;
        const float T = 1.0f / fs;
        const float hd = 2.0f * (h - h1_) * fs - hd1_; // trapezoidal derivative
        auto langevin = [](float q, float& dl) {
            if (std::fabs(q) < 1.0e-4f) { dl = 1.0f / 3.0f; return q / 3.0f; }
            const float ct = 1.0f / std::tanh(q);
            dl = 1.0f - ct * ct + 1.0f / (q * q);
            return ct - 1.0f / q;
        };
        auto f = [&](float hh, float hdd, float m) {
            float dl;
            const float man = ms * langevin((hh + alpha * m) / a, dl);
            const float dman = ms / a * dl;
            const float delta = hdd >= 0.0f ? 1.0f : -1.0f;
            const float diff = man - m;
            const float deltaM = delta * diff > 0.0f ? 1.0f : 0.0f;
            float denom = (1.0f - c) * delta * k - alpha * diff;
            if (std::fabs(denom) < 1.0e-6f) denom = denom < 0.0f ? -1.0e-6f : 1.0e-6f;
            const float irr = (1.0f - c) * deltaM * diff / denom * hdd;
            const float rev = c * dman * hdd;
            return (irr + rev) / (1.0f - c * alpha * dman);
        };
        const float k1 = T * f(h1_, hd1_, m_);
        const float k2 = T * f(0.5f * (h + h1_), 0.5f * (hd + hd1_), m_ + 0.5f * k1);
        m_ += k2;
        if (!(std::fabs(m_) < 2.0f)) m_ = 0.0f; // NaN or runaway: start over (ChowTape's guard, the idea)
        h1_ = h;
        hd1_ = hd;
        return m_ * 3.0f * a;
    }

    float preLp_ = 0.0f, deLp_ = 0.0f, dc_ = 0.0f, play_ = 0.0f, rms_ = 0.0f;
    float h1_ = 0.0f, hd1_ = 0.0f, m_ = 0.0f;
    Oversampler os1_, os2_;
};

} // namespace iv::dsp
