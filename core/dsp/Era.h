#pragma once
// ERA (P4, SPEC §4.7): one wear amount (v1.0.42, the owner's pick C on renders/era_rethink; it replaces
// ADR 0021's five eras). The Neve's warmth stays in the loop as the base, always on; the wear itself is on
// the wet after the loop (dsp/EraTexture.h), with the tape's in-loop compression, hiss and crackle (the
// Engine). This file has the loop's part:
//
//   LoopEra   one per tank line, inside the loop after the line's loss: the
//             always-on soft saturation, with the Neve's even harmonics.
//
// The Neve's darkening is NOT a filter here: a filter in the loop takes its
// loss on every pass, so a short line (many passes a second) would darken and
// drain far faster than a long one, and an infinite tail would die. It scales
// the tank's high-end T60 instead (voicing::kNeveHighRatio, applied with TONE
// through the Jot loss), which is the same at every length. ERA adds no
// darkening of its own (TONE's job) and no pitch movement (DRIFT's).
//
// Every loop gain stays ≤ 1 (SPEC §2.3 rule 2): the soft clip only ever takes
// energy out.
#include "dsp/SizeOpt.h"
#include "params/EraVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

// The loop's ERA right now (the Engine fills it from voicing::kNeveEven, faded out by a freeze and
// across DECAY's infinite crossover).
struct EraMix {
    float even = 0.0f; // the Neve's x² share into the soft clip
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

class LoopEra {
public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; reset(); }
    IV_SIZE_OPT void reset() { evenMean_ = 0.0f; }

    // Once per control chunk.
    void set(const EraMix& m)
    {
        mix_ = m;
        evenCoeff_ = 1.0f - std::exp(-6.2831853f * 5.0f / sampleRate_);
    }

    float process(float x) { return saturate(x, mix_.even, evenCoeff_, evenMean_); }

    // process() over a block of one line (the line-major tank, Fdn::lineFast): io[s] = post *
    // process(pre * io[s]), with the mix and the even term's mean in locals (stores into io can't move
    // them).
    void run(float* io, int m, float pre, float post)
    {
        const float ev = mix_.even, ec = evenCoeff_;
        float mean = evenMean_;
        for (int s = 0; s < m; ++s) io[s] = post * saturate(pre * io[s], ev, ec, mean);
        evenMean_ = mean;
    }

private:
    // Soft saturation, always on, with the Neve's even harmonics. The even
    // term's own DC is taken off it (its slow mean), so the loop needs no DC
    // blocker, which would drain the lows of an infinite tail.
    static float saturate(float x, float ev, float ec, float& mean)
    {
        float even = 0.0f;
        if (ev > 0.0f) {
            const float sq = x * x;
            mean += ec * (sq - mean);
            even = ev * (sq - mean);
        }
        return softClip(x + even, voicing::kSatCeiling);
    }

    float  sampleRate_ = 48000.0f;
    EraMix mix_;
    float  evenCoeff_ = 0.001f, evenMean_ IV_ZI;
};

} // namespace iv::dsp
