#pragma once
// The Plate (SPEC §4.2, ADR 0006): a Dattorro-family figure-8 tank, written
// from his paper ("Effect Design Part 1", JAES 1997, Fig. 1 and Table 1);
// no Plateau Versio code (GPL-3.0). Two branches, each a modulated allpass,
// a delay, a loss, a second allpass, a second delay and a loss, each
// feeding the other, so a sound circulates through both halves of the
// figure 8. The stereo outputs are sums of taps across both branches.
//
// What makes it ours: SIZE scales every length (gliding, ≤ a semitone of
// bend), every loss is a Jot absorbent filter set from DECAY and TONE like
// the FDN's, the floor governor holds infinite tails (ADR 0008), and L and
// R enter different branches (true stereo in, ADR 0013). The same code at a
// small size is the freeze companion (CONTEXT "Companion").
#include "dsp/Governor.h"
#include "dsp/Memory.h"

#include <cstddef>
#include <cstdint>

namespace iv::dsp {

// Dattorro's Table 1 lengths (samples at 29.761 kHz). Branch 0 is his left
// half (672, 4453, 1800, 3720), branch 1 his right (908, 4217, 2656, 3163).
namespace plate {
constexpr float kPaperRate = 29761.0f;
constexpr float kAp1[2] = {672.0f, 908.0f};
constexpr float kD1[2]  = {4453.0f, 4217.0f};
constexpr float kAp2[2] = {1800.0f, 2656.0f};
constexpr float kD2[2]  = {3720.0f, 3163.0f};
constexpr float kExcursion = 16.0f; // his maximum modulation excursion
} // namespace plate

class Plate {
public:
    static constexpr int kControlBlock = 32;

    // Floats of delay memory prepare() takes for a largest scale of maxScale.
    static constexpr size_t memoryFloats(float sampleRate, float maxScale)
    {
        const float k = maxScale * sampleRate / plate::kPaperRate;
        size_t n = 0;
        for (int b = 0; b < 2; ++b)
            n += size_t(plate::kAp1[b] * k + 2.0f * plate::kExcursion * sampleRate / plate::kPaperRate)
               + size_t(plate::kD1[b] * k) + size_t(plate::kAp2[b] * k) + size_t(plate::kD2[b] * k) + 4 * 8;
        return n;
    }

    bool prepare(Arena& arena, float sampleRate, float maxScale, uint32_t seed);
    void reset();

    // SIZE: scale of every length (1 = Dattorro's). Glides unless snap.
    void setScale(float scale, bool snap = false);
    void setDecayRate(float rate) { decayRate_ = rate; }
    void setHighT60Ratio(float ratio) { highRatio_ = ratio; }
    void setGovernor(float weight) { governor_.weight = weight; }
    void setFloorDb(float db) { governor_.floorDb = db; }
    // A held tail's ceiling, on the slow level (0 dBFS = no hold).
    void setHoldDb(float db) { governor_.holdDb = db; }
    float slowLevelDb() const { return governor_.slowLevelDb; }
    void resyncHoldLevel() { governor_.resyncSlow(); }
    float levelDb() const { return governor_.levelDb; }

    // Adds the wet to outL/outR (does not clear them).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    float scale() const { return scaleNow_; }

private:
    void control(int n);

    // One branch of the figure 8.
    struct Branch {
        ExactLine ap1, d1, ap2, d2; // modulated allpass, delay, allpass, delay
        float     ap1Len = 0, d1Len = 0, ap2Len = 0, d2Len = 0; // samples, at the current scale
        float     y1 = 0, y2 = 0;                               // absorbent filter states
        float     i1 = 0, i2 = 0, i3 = 0, i4 = 0;               // allpass-interpolation states
        Absorb    loss1{1, 0}, loss2{1, 0};
        float     lfoPhase = 0, lfoRate = 0, floorA = 0, floorB = 0, floorTarget = 0, floorCountdown = 0;
        float     modNow = 0, modStep = 0; // ap1's extra delay, samples
        float     out = 0;                 // this branch's output (feeds the other)
    };

    float    sampleRate_ = 48000.0f, maxScale_ = 1.0f, scaleNow_ = 1.0f, scaleTarget_ = 1.0f;
    Branch   b_[2];
    uint32_t rng_ = 1;
    float    decayRate_ = 0.2f, highRatio_ = 0.45f, levelGain_ = 1.0f;
    FloorGovernor governor_;
    bool     ready_ = false;
};

} // namespace iv::dsp
