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
// twin is a second Plate of the same size while frozen (CONTEXT "Twin").
#include "dsp/Drift.h"
#include "dsp/Era.h"
#include "dsp/Governor.h"
#include "dsp/Memory.h"

#include <cstddef>
#include <cstdint>

// PROTOTYPE (renders/plate_size_xfade; the owner: PLATE's SIZE should crossfade like HALL's and CANYON's, no pitch
// bend): the crossfade's code is in every desktop build (Plate::setSizeCrossfade, off by default: the glide, as
// built) and in a firmware built with IV_PLATE_XFADE (firmware/Makefile PLATE_XFADE=1), where it is always on.
#if !defined(IV_FIXED_VOICINGS) || defined(IV_PLATE_XFADE)
#define IV_PLATE_XF_CODE 1
#endif

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

#ifdef IV_PLATE_XF_CODE
// A Plate's SIZE crossfade state (Plate.cpp, "SIZE by crossfade"), outside the Plate so the Engine can keep it as its
// last members: the Plate grows by 12 bytes only, so the Engine's members after it barely move (flash: ~200 B).
struct PlateXf {
    // Per branch: the old reads (ap1, d1, ap2, d2), their allpass-interpolation states, the old loss filters, the new
    // d1 / d2 reads' one-trip decay correction.
    struct Branch {
        float  len[4], st[4];
        float  lnGain[2], hold[2], corr[2];
        Absorb loss[2];
    };
    struct Agree { // how well a pair of reads agrees, and its gain (Bristow-Johnson's crossfade, as Fdn::xfMix)
        float oo, nn, on, K, dK; // dK: K's step a sample, toward control()'s value
    };
    Branch  xf_[2];
    Agree   agree_[9]; // [4 b + e]: branch b's read e (ap1, d1, ap2, d2); [8]: the output taps
    float   xfSeconds_, xfRest_, xfLastTarget_, xfR_, xfKOld_;
    float   xfNew_, xfOld_, xfCos_, xfSin_, xfRotC_, xfRotS_;
    int32_t xfLen_, xfLeft_, xfEnd_; // samples
};
#endif

class Plate {
public:
    static constexpr int kControlBlock = 32;

    // Floats of delay memory prepare() takes for a largest scale of maxScale;
    // colour: ERA and DRIFT in the loop (the big Plate and the twin; false
    // leaves out DRIFT's swing room, for a small Plate in DTCM, SPEC §5.2).
    static constexpr size_t memoryFloats(float sampleRate, float maxScale, bool colour)
    {
        const float k = maxScale * sampleRate / plate::kPaperRate;
        const float room = colour ? voicing::kDriftMaxSeconds * sampleRate : 0.0f;
        size_t n = 0;
        for (int b = 0; b < 2; ++b)
            n += size_t(plate::kAp1[b] * k + 2.0f * plate::kExcursion * sampleRate / plate::kPaperRate + 2.0f * room)
               + size_t(plate::kD1[b] * k + 2.0f * voicing::kPlateLongModSeconds * sampleRate) + size_t(plate::kAp2[b] * k)
               + size_t(plate::kD2[b] * k + 2.0f * voicing::kPlateLongModSeconds * sampleRate) + 4 * 8;
        return n;
    }

    bool prepare(Arena& arena, float sampleRate, float maxScale, bool colour, uint32_t seed);
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
    void resyncHoldLevelToFast() { governor_.resyncSlowToFast(); }
    float fastLevelDb() const { return governor_.fastLevelDb(); }
    float levelDb() const { return governor_.levelDb; }
    // ERA's in-loop layers and DRIFT (ignored without colour).
    void setEra(const EraMix& m)
    {
        for (auto& b : b_) b.era.set(m);
    }
    void setDrift(const DriftAmounts& a) { drift_ = a; }
    void setExtraRate(float rate) { extraRate_ = rate; }
    // Scales Dattorro's chorus (the modulated allpasses' sine): 1 normally,
    // lower on a frozen pad, where a lossless loop piles a few cents a pass
    // into a seasick wobble (owner, M5 listening).
    void setModDepth(float scale) { modDepth_ = scale; }
    // The long delays d1 and d2 wander too (SPEC §2.3 rule 1; Beads moves both of its long
    // delays; owner's pick, v1.0.12): 1 normally, 0 on a frozen pad, which stays as still as
    // before (the frozen cards were a coin flip).
    void setLongDelayMod(float amount) { longMod_ = amount; }
    // Test hook: scales every wander in the loop (chorus, floor, long delays); 0 = still.
    void setModScale(float k) { modScale_ = k; }

    // Adds the wet to outL/outR (does not clear them).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    float scale() const { return scaleNow_; }

#ifdef IV_PLATE_XF_CODE
    // SIZE by crossfade instead of the glide (ADR 0025's phase 2 for the PLATE): once SIZE has rested
    // kSizeRestSeconds every length jumps and the old reads fade out over `seconds` (Plate.cpp, "SIZE by crossfade").
    void setSizeCrossfade(bool on, float seconds)
    {
#ifndef IV_PLATE_XFADE
        xfOn_ = on;
#endif
        (void)on;
        if (xs_) xs_->xfSeconds_ = seconds;
    }
    // Where the crossfade keeps its state (before prepare(); none: SIZE glides).
    void setCrossfadeState(PlateXf* s); // reset() sets the rest before any use
    bool sizeCrossfade() const { return xfOn_ && xs_; }
    bool crossfadeStateSet() const { return xs_ != nullptr; }
    bool crossfading() const { return xfSince_ >= 0; }
    // While true SIZE glides as built even with the crossfade on: the Engine sets it while FREEZE is on or its
    // unfreeze still glides back (the owner's pick A on renders/plate_size_xfade's frozen card).
    void setSizeGlideNow(bool on) { xfGlideNow_ = on; }
#endif

    void setAir(float air) { air_ = air; } // 1/T60 in 1/s: see absorbent() (v1.0.25)
    // The least the highs lose, 1/T60 in 1/s: an endless tail's settling mellow (v1.0.58; Engine::applySpace). 0 = none.
    void setEndlessTop(float top) { endlessTop_ = top; }

private:
    void control(int n);

    // One branch of the figure 8.
    struct Branch {
        ExactLine ap1, d1, ap2, d2; // modulated allpass, delay, allpass, delay
        float     ap1Len = 0, d1Len = 0, ap2Len = 0, d2Len = 0; // samples, at the current scale
        float     y1 = 0, y2 = 0, x1 = 0, x2 = 0;               // absorbent filter states
        float     i1 = 0, i2 = 0, i3 = 0, i4 = 0;               // allpass-interpolation states
        Absorb    loss1{1, 0}, loss2{1, 0};
        AbsorbMemo memo1, memo2; // loss1 / loss2, worked out again only when their inputs change
        float     lfoPhase = 0, lfoRate = 0, floorA = 0, floorB = 0, floorTarget = 0, floorCountdown = 0;
        float     modNow = 0, modStep = 0; // ap1's extra delay, samples
        // d1 and d2's own slow wander (setLongDelayMod), samples.
        float     longPhase[2] = {0, 0}, longFloorA[2] = {}, longFloorB[2] = {}, longTarget[2] = {}, longCountdown[2] = {};
        float     longNow[2] = {0, 0}, longStep[2] = {0, 0};
        float     out = 0;                 // this branch's output (feeds the other)
        LoopEra   era;
        Drift     drift;
    };

    float    sampleRate_ = 48000.0f, maxScale_ = 1.0f, scaleNow_ = 1.0f, scaleTarget_ = 1.0f;
    Branch   b_[2];
    uint32_t rng_ = 1;
    float    decayRate_ = 0.2f, highRatio_ = 0.45f, levelGain_ = 1.0f, extraRate_ = 0.0f;
    bool     colour_ = true;
    float    modDepth_ = 1.0f;
    float    shelfK_ = 0.0f, modScale_ = 1.0f; // lossShelfK(sampleRate_)
    float    air_ = 0.0f;
    float    endlessTop_ IV_ZI; // setEndlessTop (v1.0.58)
    float    longMod_ = 1.0f; // d1 and d2 wander; 0 while frozen
    DriftAmounts drift_;
    FloorGovernor governor_;
    bool     ready_ = false;
#ifdef IV_PLATE_XF_CODE
    void startCrossfade(float rate, float k);
    void cancelCrossfade(float target);
    void processXf(const float* inL, const float* inR, float* outL, float* outR, int start, int m);
#ifdef IV_PLATE_XFADE
    static constexpr bool xfOn_ = true; // the firmware's prototype build: always (the glide compiled out)
#else
    bool     xfOn_ = false;
#endif
    int32_t  xfSince_ = -1; // samples since the jump; -1: no crossfade
    bool     xfGlideNow_ = false; // setSizeGlideNow
    PlateXf* xs_ = nullptr;  // setCrossfadeState: the crossfade's state, kept by the Engine (its last members)
#endif
};

} // namespace iv::dsp
