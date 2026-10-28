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

    // One channel's ring, floats: exactly the longest read and a little room, not the next power of two
    // (CPU hunt, 28 Oct 2026): 150 KB a channel, so the firmware keeps both on chip (Engine::d2Floats,
    // axiFloats), where SDRAM's ~200-cycle line fills cost the 16 heads ~8 % of the module's time.
    static constexpr size_t ringFloats(float sampleRate) { return size_t(maxDelaySeconds() * sampleRate) + 12; }
    static constexpr size_t memoryFloats(float sampleRate) { return 2 * ringFloats(sampleRate); }

    IV_SIZE_OPT bool prepare(Arena& arena, float sampleRate) { return prepare(arena, arena, sampleRate); }
    // Each channel's ring from its own arena (the firmware's: D2 SRAM and AXI SRAM).
    IV_SIZE_OPT bool prepare(Arena& left, Arena& right, float sampleRate) // set-up: built for size (flash)
    {
        window_ = voicing::kShimmerWindowSeconds * sampleRate;
        step_ = 1.0f / window_; // an octave up: the delay shrinks one sample per sample
        const size_t n = ringFloats(sampleRate);
        bool ok = line_[0].init(left, n) && line_[1].init(right, n);
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

    IV_SIZE_OPT void reset()
    {
        line_[0].clear();
        line_[1].clear();
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) z_[c][k][0] = z_[c][k][1] = 0.0f;
            after_[c] = lowCut_[c] = 0.0f;
            for (int h = 0; h < kHeads; ++h) {
                phase_[c][h] = (float(h) + 0.5f * float(c)) / float(kHeads);
                base_[c][h] = 2.0f; // 2 + the grain's extra delay (none yet)
            }
        }
        seed_ = 0x5A1A7u;
    }

#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (overnight review, 26 Oct 2026; cost-input c1): recording starts again after a rest
    // (the Engine skipped process() outside SHIMMER). What was recorded before the rest is forgotten
    // (a lazy clear: no memory is touched) and the feed filter starts from rest, so the first grains
    // after the switch have only what was played since (up to a window and its scatter, ~0.8 s).
    void restart()
    {
        line_[0].clear();
        line_[1].clear();
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) z_[c][k][0] = z_[c][k][1] = 0.0f;
    }
#endif

    // Always records (cheap); with shift, also returns the octave in l and r. Both channels' feed filters
    // first, then each channel's heads (the random onsets drawn in the same order as ever: channel by
    // channel, head by head), the same float operations as before, so the same output to the bit.
    void process(float& l, float& r, bool shift)
    {
        float x[2] = {l, r};
        for (int k = 0; k < 4; ++k) // transposed direct form II, the two channels side by side
            for (int c = 0; c < 2; ++c) {
                const float y = b0_[k] * x[c] + z_[c][k][0];
                z_[c][k][0] = b1_[k] * x[c] - a1_[k] * y + z_[c][k][1];
                z_[c][k][1] = b0_[k] * x[c] - a2_[k] * y;
                x[c] = y;
            }
        line_[0].write(x[0]);
        line_[1].write(x[1]);
        if (!shift) {
            l = r = 0.0f;
            return;
        }
        const float step = step_, window = window_;
        float out[2];
        for (int c = 0; c < 2; ++c) {
            const Ring&    ln   = line_[c];
            const float*   buf  = ln.buf;
            const uint32_t n    = ln.n, pos = ln.pos, fill = ln.fill;
            float y = 0.0f;
            for (int h = 0; h < kHeads; ++h) {
                float& p = phase_[c][h];
                p -= step;
                if (p < 0.0f) {
                    p += 1.0f;
                    seed_ = seed_ * 1664525u + 1013904223u;
                    const float extra = voicing::kShimmerScatter * window * (float(seed_ >> 8) * (1.0f / 16777216.0f));
                    base_[c][h] = 2.0f + extra; // its own statement: never fused (2 + extra + window p, as before)
                }
                const float d = base_[c][h] + window * p;
                const auto  di = uint32_t(d);
                const float t = d - float(di);
                // Ring::read(di) and read(di + 1): silence from before a clear().
                const uint32_t i0 = pos >= di ? pos - di : pos + n - di, i1 = i0 ? i0 - 1 : n - 1;
                const float a = di <= fill ? buf[i0] : 0.0f, b = di < fill ? buf[i1] : 0.0f;
                // sin(pi p), Bhaskara's approximation (within 0.2 %): no libm call per head and sample.
                const float q = p * (1.0f - p), s = 16.0f * q / (5.0f - 4.0f * q);
                y += s * s * (a + t * (b - a));
            }
            y *= 2.0f / float(kHeads);
            after_[c] += afterA_ * (y - after_[c]); // each octave a little softer than the note
            lowCut_[c] += lowCutA_ * (after_[c] - lowCut_[c]);
            out[c] = after_[c] - lowCut_[c];
        }
        l = out[0];
        r = out[1];
    }

private:
    static constexpr float maxDelaySeconds()
    {
        return voicing::kShimmerWindowSeconds * (1.0f + voicing::kShimmerScatter) + 0.001f;
    }

    // A ring of exactly n floats (DelayLine's power of two would be 256 KB a channel): DelayLine's
    // write, and its read's lazy clear (fill: writes since the last clear(), up to n).
    struct Ring {
        float*   buf IV_ZI;
        uint32_t n IV_ZI, pos IV_ZI, fill IV_ZI;
        IV_SIZE_OPT bool init(Arena& arena, size_t len)
        {
            n = uint32_t(len);
            buf = arena.take(len);
            pos = 0;
            fill = n; // handed out zeroed by the arena
            return buf != nullptr;
        }
        void clear() { fill = 0; }
        void write(float x)
        {
            buf[pos] = x;
            pos = pos + 1 == n ? 0 : pos + 1;
            if (fill < n) ++fill;
        }
    };

    Ring      line_[2];
    float     window_ = 19200.0f, step_ IV_ZI;
    float     b0_[4] IV_ZI, b1_[4] IV_ZI, a1_[4] IV_ZI, a2_[4] IV_ZI, z_[2][4][2] IV_ZI;
    float     afterA_ = 1.0f, lowCutA_ IV_ZI, after_[2] IV_ZI, lowCut_[2] IV_ZI;
    float     phase_[2][kHeads] IV_ZI, base_[2][kHeads] IV_ZI; // base_: 2 + the grain's random extra delay
    uint32_t  seed_ = 1;
};

} // namespace iv::dsp
