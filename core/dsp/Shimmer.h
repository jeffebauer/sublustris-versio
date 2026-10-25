#pragma once
// SHIMMER (SPEC §4.11, ADR 0024): ATTACK's third position. What you play, shifted an octave up, is
// sent into the tank with the input; no feedback, so one octave, steady and in tune (the owner's
// pick over fed-back shimmers, reverses, echo-verbs and gates, 25 Oct 2026).
//
// The shifter is a delay line read by kShimmerHeads heads, each sweeping a window of
// kShimmerWindowSeconds at twice the speed of time and faded by sin² of its place in it; evenly
// spaced sin² windows sum to a constant. Each head starts every grain at a random extra delay (up to
// kShimmerScatter of a window): heads at fixed spacings cancel each other at fixed frequencies, a
// comb on the octave (4 heads of 100 ms: the 110 / 165 / 330 Hz notes' octaves nearly gone, heard
// as out-of-tune notes). Random onsets move the cancellation every grain (Valhalla's randomised
// shifter, Mutable Instruments' grains). Before the shift, an 8th-order low-pass at kShimmerFeedHz:
// an octave up doubles every frequency, and above 12 kHz that passes 24 kHz and folds back as
// inharmonic tones, what a sample-rate reducer does (round 1-2's "bit crushing").
#include "dsp/Memory.h"
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

class Shimmer {
public:
    static constexpr int kHeads = voicing::kShimmerHeads;

    static constexpr size_t memoryFloats(float sampleRate)
    {
        return 2 * nextPow2(size_t(maxDelaySeconds() * sampleRate) + 8);
    }

    IV_SIZE_OPT bool prepare(Arena& arena, float sampleRate) // set-up: built for size (flash)
    {
        window_ = voicing::kShimmerWindowSeconds * sampleRate;
        const size_t n = size_t(maxDelaySeconds() * sampleRate) + 8;
        bool ok = line_[0].init(arena, n) && line_[1].init(arena, n);
        // Butterworth, 8th order: four biquads (RBJ low-pass) at the Butterworth Qs.
        static constexpr float kQ[4] = {0.5098f, 0.6013f, 0.9000f, 2.5629f};
        const float w = 6.2831853f * voicing::kShimmerFeedHz / sampleRate, cw = std::cos(w), sw = std::sin(w);
        for (int k = 0; k < 4; ++k) {
            const float al = sw / (2.0f * kQ[k]), a0 = 1.0f + al;
            b0_[k] = (1.0f - cw) * 0.5f / a0;
            b1_[k] = (1.0f - cw) / a0;
            a1_[k] = -2.0f * cw / a0;
            a2_[k] = (1.0f - al) / a0;
        }
        afterA_ = 1.0f - std::exp(-6.2831853f * voicing::kShimmerAfterHz / sampleRate);
        lowCutA_ = 1.0f - std::exp(-6.2831853f * voicing::kShimmerLowCutHz / sampleRate);
        reset();
        return ok;
    }

    void reset()
    {
        line_[0].clear();
        line_[1].clear();
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) z_[c][k][0] = z_[c][k][1] = 0.0f;
            after_[c] = lowCut_[c] = 0.0f;
            for (int h = 0; h < kHeads; ++h) {
                phase_[c][h] = (float(h) + 0.5f * float(c)) / float(kHeads);
                offset_[c][h] = 0.0f;
            }
        }
        seed_ = 0x5A1A7u;
    }

    // Always records (cheap); with shift, also returns the octave in l and r.
    void process(float& l, float& r, bool shift)
    {
        float* io[2] = {&l, &r};
        for (int c = 0; c < 2; ++c) {
            float x = *io[c];
            for (int k = 0; k < 4; ++k) { // transposed direct form II
                const float y = b0_[k] * x + z_[c][k][0];
                z_[c][k][0] = b1_[k] * x - a1_[k] * y + z_[c][k][1];
                z_[c][k][1] = b0_[k] * x - a2_[k] * y;
                x = y;
            }
            line_[c].write(x);
            if (!shift) {
                *io[c] = 0.0f;
                continue;
            }
            float y = 0.0f;
            const float step = 1.0f / window_; // an octave up: the delay shrinks one sample per sample
            for (int h = 0; h < kHeads; ++h) {
                float& p = phase_[c][h];
                p -= step;
                if (p < 0.0f) {
                    p += 1.0f;
                    seed_ = seed_ * 1664525u + 1013904223u;
                    offset_[c][h] = voicing::kShimmerScatter * window_ * (float(seed_ >> 8) * (1.0f / 16777216.0f));
                }
                const float d = 2.0f + offset_[c][h] + window_ * p;
                const auto  di = uint32_t(d);
                const float t = d - float(di);
                const float a = line_[c].read(di), b = line_[c].read(di + 1);
                // sin(pi p), Bhaskara's approximation (within 0.2 %): no libm call per head and sample.
                const float q = p * (1.0f - p), s = 16.0f * q / (5.0f - 4.0f * q);
                y += s * s * (a + t * (b - a));
            }
            y *= 2.0f / float(kHeads);
            after_[c] += afterA_ * (y - after_[c]); // each octave a little softer than the note
            lowCut_[c] += lowCutA_ * (after_[c] - lowCut_[c]);
            *io[c] = after_[c] - lowCut_[c];
        }
    }

private:
    static constexpr float maxDelaySeconds()
    {
        return voicing::kShimmerWindowSeconds * (1.0f + voicing::kShimmerScatter) + 0.001f;
    }

    DelayLine line_[2];
    float     window_ = 19200.0f;
    float     b0_[4] = {}, b1_[4] = {}, a1_[4] = {}, a2_[4] = {}, z_[2][4][2] = {};
    float     afterA_ = 1.0f, lowCutA_ = 0.0f, after_[2] = {}, lowCut_[2] = {};
    float     phase_[2][kHeads] = {}, offset_[2][kHeads] = {};
    uint32_t  seed_ = 1;
};

} // namespace iv::dsp
