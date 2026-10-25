#pragma once
// ERA (P4, SPEC §4.7; ADR 0021, which replaces 0003's stacking): five eras from sweet to gritty, each
// on its own over the Neve's warmth: Neve → Midiverb → bucket-brigade → cassette → AM radio. This file
// has the loop's part (the BBD and radio are on the wet, dsp/EraStages.h):
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
//
// Every loop gain stays ≤ 1 (SPEC §2.3 rule 2): the low-pass and the soft
// clip only ever take energy out.
#include "dsp/Oversampler.h"
#include "dsp/SizeOpt.h"
#include "params/EraVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

// What the ERA knob means right now (Mappings.h eraMix fills it).
struct EraMix {
    float neve = 0, midiverb = 0, bbd = 0, cassette = 0, radio = 0; // weights 0..1 (Neve: 1, the base)
    float highRatioScale = 1.0f;                         // × the tank's high-end T60 share (the era's darkening)
    float drive = 1.0f, even = 0.0f;                     // into the soft clip
    float midiBits = 16.0f, midiEngage = 0.0f;           // in-loop box
    bool  midiRound = false;                             // at infinite: round (no systematic loss) instead of truncate
    float midiDark = 1.0f;                               // share of the box's dark loop in use (v1.0.23: less at infinite)
    float makeup = 1.0f;                                 // wet gain keeping ERA's loudness level
    float bbdT = 0, radioT = 0;                          // how far into the BBD's and radio's stretches (0 → 1)
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
        float y = saturate(x, mix_.drive, mix_.even, evenCoeff_, evenMean_);
        // Midiverb box.
        if (mix_.midiEngage > 0.0f) {
            const float boxed = box(y);
            y += mix_.midiEngage * (boxed - y);
        }
        return y;
    }

    // process() over a block of one line (the line-major tank, Fdn::lineFast): io[s] = post *
    // process(pre * io[s]), the same operations in the same order, with the mix and the even term's
    // mean in locals (stores into io can't move them).
    void run(float* io, int m, float pre, float post)
    {
        if (mix_.midiEngage > 0.0f) {
            for (int s = 0; s < m; ++s) io[s] = post * process(pre * io[s]);
            return;
        }
        const float d = mix_.drive, ev = mix_.even, ec = evenCoeff_;
        float mean = evenMean_;
        for (int s = 0; s < m; ++s) io[s] = post * saturate(pre * io[s], d, ev, ec, mean);
        evenMean_ = mean;
    }

private:
    // Soft saturation, always on; Neve adds even harmonics, the cassette
    // drives it harder (level-compensated). The even term's own DC is taken
    // off it (its slow mean), so the loop needs no DC blocker, which would
    // drain the lows of an infinite tail. At the Neve the drive is 1, and
    // x / 1 is x exactly: the divide (~16 cycles on the M7) is skipped.
    static float saturate(float x, float d, float ev, float ec, float& mean)
    {
        float even = 0.0f;
        if (ev > 0.0f) {
            const float sq = d * x * d * x;
            mean += ec * (sq - mean);
            even = ev * (sq - mean);
        }
        float y = softClip(d * x + even, voicing::kSatCeiling);
        if (d != 1.0f) y /= d;
        return y;
    }

    // 48 → 24 → 48 kHz with truncation at 24 kHz. One sample in, one out
    // (one sample late: pairs are formed on the odd samples). One shared copy (flash): process() is
    // inlined both into the tank's fast path (LoopEra::run) and into its slow one.
    IV_NOINLINE float box(float x)
    {
        // Still lows' low tank (12 kHz, v1.0.18): below ~300 Hz the box's halfbands and dark loop are
        // flat, and at this rate they'd darken the lows every pass; only the 13 bits stay.
        if (sampleRate_ < 18000.0f) return mix_.midiRound ? roundBits(x, midiStep_) : truncateBits(x, midiStep_);
        // The HALL / MASSIVE tank at 24 kHz (v1.0.35, ADR 0026) is already at the box's rate: its loop
        // low-pass and its bits, without the halfbands.
        if (sampleRate_ < 30000.0f) {
            darkLp_ += 0.5f * (x - darkLp_);
            x += mix_.midiDark * (darkLp_ - x);
            return mix_.midiRound ? roundBits(x, midiStep_) : truncateBits(x, midiStep_);
        }
        if (phase_ == 0) {
            pairFirst_ = x;
            phase_ = 1;
            return held_;
        }
        phase_ = 0;
        float in = down_.down(pairFirst_, x);
        // The Midiverb's loop low-pass: y = ½x + ½y' at its rate (about -3 dB at 2.7 kHz).
        darkLp_ += 0.5f * (in - darkLp_);
        in += mix_.midiDark * (darkLp_ - in);
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

} // namespace iv::dsp
