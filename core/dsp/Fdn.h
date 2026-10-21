#pragma once
// The FDN: Hall and Massive's big tank (SPEC §4.3, ADR 0006). Eight
// modulated delay lines whose outputs are mixed back into their inputs by a
// Hadamard matrix (every line feeds every other, energy preserved), so one
// sound becomes a dense, smooth wash. Each line's loss is set from its own
// length (Jot), so DECAY never retunes the tail, and a one-pole absorbent
// filter per line makes the highs die sooner than the lows (TONE).
//
// What keeps it from sounding metallic (SPEC §2.3 rule 1): line lengths are
// distinct primes, every read wanders slowly (the modulation floor), and
// damping sits in the loop.
//
// Control work (SIZE slew, modulation, gains) runs every kControlBlock
// samples; the per-sample loop only reads, filters, mixes and writes.
#include "dsp/Memory.h"
#include "params/SpaceVoicing.h"

#include <cstddef>
#include <cstdint>

namespace iv::dsp {

class Fdn {
public:
    static constexpr int kN            = voicing::kFdnLines;
    static constexpr int kControlBlock = 32;

    // Floats of delay memory prepare() takes from its arena at this rate.
    static constexpr size_t memoryFloats(float sampleRate)
    {
        return size_t(kN) * nextPow2(size_t(voicing::kFdnMaxLineSeconds * sampleRate) + 4);
    }

    bool prepare(Arena& arena, float sampleRate, uint32_t seed);
    void reset();

    // Targets, read at the next control tick. meanSeconds: the lines' mean
    // length; decayRate: 1/T60 in 1/s (0 = infinite); highT60Ratio: the
    // high end's T60 over the low end's (TONE). snap = jump straight there
    // (at prepare/reset), else SIZE glides (kSizeSlewPerSecond).
    void setMeanSeconds(float meanSeconds, bool snap = false);
    void setDecayRate(float rate) { decayRate_ = rate; }
    void setHighT60Ratio(float ratio) { highRatio_ = ratio; }

    // Adds the wet to outL/outR (does not clear them).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    float lineSeconds(int i) const { return lenNow_[i] / sampleRate_; }

private:
    void control(int n);

    float     sampleRate_ = 48000.0f;
    DelayLine line_[kN];
    float     lenTarget_[kN] = {}; // samples
    float     lenNow_[kN]    = {}; // samples, gliding toward the target
    float     maxLen_        = 0.0f;
    // Modulation floor, per line: a random target, smoothed twice.
    float     floorTarget_[kN] = {}, floorA_[kN] = {}, floorB_[kN] = {};
    float     floorCountdown_[kN] = {};
    uint32_t  rng_ = 1;
    // Per-sample read position for the current control block.
    float     readNow_[kN] = {}, readStep_[kN] = {};
    // Absorbent filters: y = g (1 − b) x + b y.
    float     gain_[kN] = {}, pole_[kN] = {}, absorbY_[kN] = {};
    float     decayRate_ = 0.2f, highRatio_ = voicing::kDampingHighT60Ratio;
    bool      ready_ = false;
};

} // namespace iv::dsp
