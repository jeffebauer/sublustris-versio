#pragma once
// ERA (P4, SPEC §4.7; ADR 0003, 0004, 0005): clean → Neve → cassette → μ-law
// → Midiverb, stacking. Each layer fades in across its stretch of the knob
// and then stays. This file has the two processors:
//
//   LoopEra   one per tank line, inside the loop after the line's loss:
//             the always-on soft saturation (Neve's even harmonics and the
//             cassette's drive go into it) and the Midiverb box (48 → 24 →
//             48 kHz, bits truncated toward zero, so the tail grows grainier
//             as it fades and can never hold a buzz).
//
// The era's darkening (Neve 16 kHz, cassette 9 kHz) is NOT a filter here: a
// filter in the loop takes its loss on every pass, so a short line (many
// passes a second) would darken and drain far faster than a long one, and an
// infinite tail would die. It scales the tank's high-end T60 instead
// (EraMix::highRatioScale, applied with TONE through the Jot loss), which
// is the same at every length.
//   MuLawOut  the wet output at 24 kHz through 12 → 10-bit μ-law: gritty
//             peaks, a smooth quiet tail.
//
// Every loop gain stays ≤ 1 (SPEC §2.3 rule 2): the low-pass and the soft
// clip only ever take energy out.
#include "dsp/Oversampler.h"
#include "params/EraVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

// What the ERA knob means right now (Mappings.h eraMix fills it).
struct EraMix {
    float neve = 0, cassette = 0, mu = 0, midiverb = 0; // layer weights 0..1, stacking
    float highRatioScale = 1.0f;                         // × the tank's high-end T60 share (the era's darkening)
    float drive = 1.0f, even = 0.0f;                     // into the soft clip
    float muBits = 16.0f, muEngage = 0.0f;               // output μ-law
    float midiBits = 16.0f, midiEngage = 0.0f;           // in-loop box
    bool  midiRound = false;                             // at infinite: round (no systematic loss) instead of truncate
    float makeup = 1.0f;                                 // wet gain keeping ERA's loudness level
    // DSP audit hooks (Engine::AuditHooks), off by default.
    bool  satBypass = false;                             // measurement: no soft clip
    int   midiPart = 0;                                  // measurement: 1 no truncation, 2 no dark loop
    bool  midiKeep = false;                              // E2 prototype: round above 16 steps, truncate below
    int   muMode = 0;                                    // E3: 1 off, 3 band limit only (measurements); 2 gain-ranging
};

// The loop's soft clip: exactly linear below the knee (so a tail at a normal
// level loses nothing, pass after pass: a cubic curve shaved ~0.1 % per
// pass at −20 dBFS and drained an infinite tail in minutes), then a smooth
// rational curve with slope 1 at the knee, never above ±c.
inline float softClip(float x, float c)
{
    const float k = voicing::kSatKnee * c;
    const float a = x < 0.0f ? -x : x;
    if (a <= k) return x;
    const float t = (a - k) / (c - k);
    const float y = k + (c - k) * t / (1.0f + t);
    return x < 0.0f ? -y : y;
}

// Truncates toward zero to `bits` against full scale 1 (in-loop: magnitude
// truncation, so a decaying tail always reaches silence; no dither, which in
// a gain-1 loop would pile up as noise).
inline float truncateBits(float x, float step)
{
    const float q = x / step;
    return float(int32_t(q)) * step; // int conversion truncates toward zero
}
// Rounds to the nearest step: no loss on average, so an infinite tail (gain
// 1, ADR 0008) keeps its level; truncation there would eat it pass by pass.
inline float roundBits(float x, float step) { return std::round(x / step) * step; }

class LoopEra {
public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; reset(); }
    void reset()
    {
        evenMean_ = 0.0f;
        down_.reset();
        up_.reset();
        phase_ = 0;
        held_ = pairFirst_ = darkLp_ = 0.0f;
    }

    // Once per control chunk.
    void set(const EraMix& m)
    {
        mix_ = m;
        midiStep_ = 2.0f / std::exp2(m.midiBits);
        evenCoeff_ = 1.0f - std::exp(-6.2831853f * 5.0f / sampleRate_);
    }

    float process(float x)
    {
        // Soft saturation, always on; Neve adds even harmonics, the
        // cassette drives it harder (level-compensated). The even term's
        // own DC is taken off it (its slow mean), so the loop needs no DC
        // blocker, which would drain the lows of an infinite tail.
        const float d = mix_.drive;
        float even = 0.0f;
        if (mix_.even > 0.0f) {
            const float sq = d * x * d * x;
            evenMean_ += evenCoeff_ * (sq - evenMean_);
            even = mix_.even * (sq - evenMean_);
        }
        float y = mix_.satBypass ? (d * x + even) / d : softClip(d * x + even, voicing::kSatCeiling) / d;
        // Midiverb box.
        if (mix_.midiEngage > 0.0f) {
            const float boxed = box(y);
            y += mix_.midiEngage * (boxed - y);
        }
        return y;
    }

private:
    // 48 → 24 → 48 kHz with truncation at 24 kHz. One sample in, one out
    // (one sample late: pairs are formed on the odd samples).
    float box(float x)
    {
        // Still lows' low tank (12 kHz, v1.0.18): below ~300 Hz the box's halfbands and dark loop are
        // flat, and at this rate they'd darken the lows every pass; only the 13 bits stay.
        if (sampleRate_ < 30000.0f) return mix_.midiPart == 1 ? x : quantise(x);
        if (phase_ == 0) {
            pairFirst_ = x;
            phase_ = 1;
            return held_;
        }
        phase_ = 0;
        float in = down_.down(pairFirst_, x);
        // The Midiverb's loop low-pass: y = ½x + ½y' at its rate (about -3 dB at 2.7 kHz).
        darkLp_ += 0.5f * (in - darkLp_);
        if (mix_.midiPart != 2) in = darkLp_;
        const float low = mix_.midiPart == 1 ? in : quantise(in);
        float a, b;
        up_.up(low, a, b);
        held_ = b;
        return a;
    }

    // At infinite, round (no systematic loss); else truncate toward zero. E2 prototype (midiKeep): round
    // while the line is above kKeepSteps steps, so only the tail's last ~24 dB pays the truncation's loss
    // (the classic cure for limit cycles is truncation near zero: the small-signal end is where they live).
    float quantise(float x) const
    {
        constexpr float kKeepSteps = 16.0f;
        if (mix_.midiRound || (mix_.midiKeep && std::fabs(x) > kKeepSteps * midiStep_)) return roundBits(x, midiStep_);
        return truncateBits(x, midiStep_);
    }

    float    sampleRate_ = 48000.0f;
    EraMix   mix_;
    float    evenCoeff_ = 0.001f, evenMean_ = 0.0f;
    float    midiStep_ = 1.0f / 32768.0f;
    Halfband down_, up_;
    int      phase_ = 0;
    float    held_ = 0.0f, pairFirst_ = 0.0f, darkLp_ = 0.0f;
};

// The μ-law output layer (stereo, the wet only).
class MuLawOut {
public:
    void reset()
    {
        for (auto& h : down_) h.reset();
        for (auto& h : up_) h.reset();
        phase_ = 0;
        held_[0] = held_[1] = first_[0] = first_[1] = 0.0f;
    }
    void set(const EraMix& m)
    {
        engage_ = m.muMode == 1 ? 0.0f : m.muEngage;
        levels_ = std::exp2(m.muBits - 1.0f); // codes per polarity
        ranging_ = m.muMode == 2;
        unquantised_ = m.muMode == 3;
    }
    void process(float& l, float& r)
    {
        if (engage_ <= 0.0f) return;
        float in[2] = {l, r}, out[2];
        if (phase_ == 0) {
            first_[0] = in[0];
            first_[1] = in[1];
            out[0] = held_[0];
            out[1] = held_[1];
            phase_ = 1;
        } else {
            phase_ = 0;
            for (int c = 0; c < 2; ++c) {
                const float low = companded(down_[c].down(first_[c], in[c]));
                float a, b;
                up_[c].up(low, a, b);
                held_[c] = b;
                out[c] = a;
            }
        }
        l += engage_ * (out[0] - l);
        r += engage_ * (out[1] - r);
    }

private:
    // μ-law encode, quantise to the code count, decode.
    float companded(float x) const
    {
        if (unquantised_) return x; // measurement: the layer's band limit only
        if (ranging_) return gainRanged(x);
        const float fs = voicing::kMuFullScale, mu = voicing::kMuLaw;
        float a = std::fabs(x) / fs;
        if (a > 1.0f) a = 1.0f;
        const float enc = std::log1p(mu * a) / std::log1p(mu);
        const float q = std::round(enc * levels_) / levels_;
        const float dec = (std::pow(1.0f + mu, q) - 1.0f) / mu;
        return (x < 0.0f ? -dec : dec) * fs;
    }

    // E3 PROTOTYPE (DSP audit, 24 Oct 2026): a gain-ranging converter as the Lexicon 224X's (a 12-bit
    // mantissa and a 2-bit exponent: four 6 dB ranges), at the same bits as the mu-law layer. Each
    // sample takes the smallest range it fits, so the noise steps with the level in 6 dB bands.
    float gainRanged(float x) const
    {
        const float a = std::fabs(x);
        float range = voicing::kMuFullScale;
        for (int e = 0; e < 3 && a < 0.5f * range; ++e) range *= 0.5f;
        const float step = range / levels_;
        float q = std::round(x / step) * step;
        if (q > voicing::kMuFullScale) q = voicing::kMuFullScale;
        if (q < -voicing::kMuFullScale) q = -voicing::kMuFullScale;
        return q;
    }

    Halfband down_[2], up_[2];
    int      phase_ = 0;
    float    held_[2] = {}, first_[2] = {};
    float    engage_ = 0.0f, levels_ = 2048.0f;
    bool     ranging_ = false, unquantised_ = false;
};

} // namespace iv::dsp
