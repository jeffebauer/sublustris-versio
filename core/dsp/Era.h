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
    bool  midiDark = false;                              // PROTOTYPE (renders/midiverb, audit E1): the real unit's ½ one-pole every pass
    float makeup = 1.0f;                                 // wet gain keeping ERA's loudness level
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
        float y = softClip(d * x + even, voicing::kSatCeiling) / d;
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
        if (phase_ == 0) {
            pairFirst_ = x;
            phase_ = 1;
            return held_;
        }
        phase_ = 0;
        float in = down_.down(pairFirst_, x);
        if (mix_.midiDark) { // the Midiverb's loop low-pass: y = ½x + ½y' at its rate (about -3 dB at 2.7 kHz)
            darkLp_ += 0.5f * (in - darkLp_);
            in = darkLp_;
        }
        const float low = mix_.midiRound ? roundBits(in, midiStep_) : truncateBits(in, midiStep_);
        float a, b;
        up_.up(low, a, b);
        held_ = b;
        return a;
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
        engage_ = m.muEngage;
        levels_ = std::exp2(m.muBits - 1.0f); // codes per polarity
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
        const float fs = voicing::kMuFullScale, mu = voicing::kMuLaw;
        float a = std::fabs(x) / fs;
        if (a > 1.0f) a = 1.0f;
        const float enc = std::log1p(mu * a) / std::log1p(mu);
        const float q = std::round(enc * levels_) / levels_;
        const float dec = (std::pow(1.0f + mu, q) - 1.0f) / mu;
        return (x < 0.0f ? -dec : dec) * fs;
    }

    Halfband down_[2], up_[2];
    int      phase_ = 0;
    float    held_[2] = {}, first_[2] = {};
    float    engage_ = 0.0f, levels_ = 2048.0f;
};

} // namespace iv::dsp
