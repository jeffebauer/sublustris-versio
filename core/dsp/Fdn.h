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
// samples; the per-sample work only reads, filters, mixes and writes, and
// runs line by line over the block (each line's whole block in one go, then
// the mix), the same sums in the same order as sample by sample.
#include "dsp/Drift.h"
#include "dsp/Era.h"
#include "dsp/Governor.h"
#include "dsp/Memory.h"
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace iv::dsp {

class Fdn {
public:
    static constexpr int kN            = voicing::kFdnLines;
    static constexpr int kControlBlock = 32;

    // Floats of delay memory prepare() takes from its arena at this rate.
    // maxLineSeconds: the longest a line may get (a smaller FDN can ask for less memory).
    static constexpr size_t memoryFloats(float sampleRate, float maxLineSeconds = voicing::kFdnMaxLineSeconds)
    {
        return size_t(kN) * (nextPow2(size_t(maxLineSeconds * sampleRate) + 4)
                             + 2 * nextPow2(size_t(voicing::kInLoopMaxSeconds * sampleRate) + 4));
    }
    // The part of memoryFloats() that is the in-loop allpass lines (kN x 2 slices of apLineFloats()),
    // which prepare() takes from apArena when one is given (the firmware's AXI SRAM: overnight review,
    // 26 Oct 2026; 32 of the tank's SDRAM streams). Only addresses change, never a sample.
    static constexpr size_t apLineFloats(float sampleRate)
    {
        return nextPow2(size_t(voicing::kInLoopMaxSeconds * sampleRate) + 4);
    }
    static constexpr size_t apFloats(float sampleRate) { return size_t(kN) * 2 * apLineFloats(sampleRate); }
    static constexpr int    kApSlices = kN * 2;

    // apArena: where the in-loop allpass lines go (nullptr: the same arena as the lines).
    bool prepare(Arena& arena, float sampleRate, uint32_t seed, float maxLineSeconds = voicing::kFdnMaxLineSeconds,
                 Arena* apArena = nullptr);
    void reset();

    // Targets, read at the next control tick. meanSeconds: the lines' mean
    // length; decayRate: 1/T60 in 1/s (0 = infinite); highT60Ratio: the
    // high end's T60 over the low end's (TONE). snap = jump straight there
    // (at prepare/reset), else SIZE crossfades once it rests (kSizeCrossfadeSeconds, ADR 0025).
    // spread: how far the lines' lengths fan out from the mean, 0..1
    // (Supermassive's Warp: near 0 they're almost equal and the tail is a
    // train of echoes; at 1 they're kFdnRatios apart, a wash).
    void setMeanSeconds(float meanSeconds, float spread = 1.0f, bool snap = false);
    void setDecayRate(float rate) { decayRate_ = rate; }
    void setHighT60Ratio(float ratio) { highRatio_ = ratio; }
    // Floor governor weight, 0..1 (ADR 0008): 1 at infinite DECAY. Above
    // the floor level the loop loses a little more per pass, so an infinite
    // tail fed for minutes settles at the floor instead of growing.
    void setGovernor(float weight) { governor_.weight = weight; }
    void setFloorDb(float db) { governor_.floorDb = db; }
    // A held tail's ceiling, on the slow level (0 dBFS = no hold).
    void setHoldDb(float db) { governor_.holdDb = db; }
    float slowLevelDb() const { return governor_.slowLevelDb; }
    void resyncHoldLevel() { governor_.resyncSlow(); }
    void resyncHoldLevelToFast() { governor_.resyncSlowToFast(); }
    float fastLevelDb() const { return governor_.fastLevelDb(); }
    // In-loop diffusion amount 0..1 (MASSIVE's SIZE); 0 bypasses it.
    void setInLoopDiffusion(float amount) { inLoopTarget_ = amount; }
    // The in-loop allpasses' lengths × k (1 = MASSIVE's; HALL scales them with its room, v1.0.17).
    void setInLoopScale(float k) { apScale_ = k; }
    // ERA's in-loop layers and DRIFT, set at control rate (SPEC §4.6, §4.7).
    void setEra(const EraMix& m)
    {
        for (auto& e : era_) e.set(m);
    }
    void setDrift(const DriftAmounts& a) { drift_ = a; }
    // More loss on top of DECAY (ERA's cassette compression), 1/T60 in 1/s.
    void setExtraRate(float rate) { extraRate_ = rate; }
    // Early taps 0..1 (MASSIVE): besides each line's end, the output also
    // reads kEarlyTaps points partway along every line, so the first sound
    // leaves a long tank within a fraction of a line length (the owner, M4:
    // "too much delay before the reverb starts swelling") and every pass is
    // denser. Output only: the loop itself is unchanged.
    void setEarlyTaps(float amount) { earlyTarget_ = amount; }
    // n of the early taps per line (v1.0.36, ADR 0026: HALL 2, MASSIVE 4), the same early energy;
    // re-lays them out, so call it where the tank restarts (a SPACE flip, reset).
    void setEarlyTapCount(int n);
    void setAir(float air) { air_ = air; } // 1/T60 in 1/s: see absorbent() (v1.0.25)
    // The modulation floor's depth × modDepth (Engine::setModulationScale, a test hook).
    void setModDepth(float k) { modDepth_ = k; }
    // Extra output level for a voicing (MASSIVE's loudness match).
    void setOutputTrim(float trim) { outputTrim_ = trim; }
    float levelDb() const { return governor_.levelDb; }
    // Still lows (v1.0.18): the low tank follows its leader's governor (its loss and darkening), and
    // the leader's governor hears the low tank's level too (mean power per sample, the last block's),
    // so the two together hold one floor, as one tank did.
    void setGovernorLeader(const Fdn* leader) { leader_ = leader; }
    void setExternalPower(float meanPower) { externalPower_ = meanPower; }

    // Adds the wet to outL/outR (does not clear them).
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    float lineSeconds(int i) const { return lenNow_[i] / sampleRate_; }

    // SIZE's crossfade in two groups (v1.0.39, the owner's pick B on renders/size_stagger; on the module a
    // SIZE crossfade in MASSIVE reached ~110 % of the CPU, every line read twice for 0.3 s): lines 0, 2, 4,
    // 6 crossfade first and 1, 3, 5, 7 straight after (two fades of kSizeCrossfadeSeconds back to back), so
    // only half the lines read twice at once. Where a small HALL's in-loop diffusion joins or leaves with
    // the move, all lines fade together. 1: all together, as before v1.0.39.
    // PROTOTYPE (renders/size_moves): 4 or 8 groups, lines g, g + 4 (g + 8), a quarter (an eighth) reading twice.
    void setStagger(int groups) { xfStagger_ = groups == 1 || groups == 4 || groups == 8 ? groups : 2; }

private:
    bool staggered() const { return xfStagger_ > 1; }
    void control(int n);
    // The tank line by line (Fdn.cpp, "Line by line"): m samples on one control block's values.
    bool blockFits(int m) const;
    void runBlock(const float* inL, const float* inR, float* outL, float* outR, int m);
    bool lineFast(int i, int m, float apG, float eraIn, float eraOut, float* y);
    void lineSlow(int i, int m, int q, const float* xn, const float* xo, float apG, float apGOld, float eraIn,
                  float eraOut, float* y);
    void tapsMoving(int i, int m, const float* tl, const DelayLine& was, float* e) const;
    void tapsOld(int i, int q, const float* tl, const DelayLine& was, float* ol, float* orr) const;
    float xfMix(int i, float nw, float od, float gOld);
    float xfAllpass(int i, float x, float g, bool old);
    float xfOldAllpass(int i, int k, float x, float d);
    void  layoutEarly();
    void controlInLoop(float dt, int n, float slew, bool pending);
    uint32_t apTargetLength(int i, int k) const;

    float     sampleRate_ = 48000.0f;
    DelayLine line_[kN];
    float     lenTarget_[kN] IV_ZI; // samples
    float     lenNow_[kN] IV_ZI; // samples, gliding toward the target
    float     maxLen_ IV_ZI;
    // Modulation floor, per line: a random target, smoothed twice.
    float     floorTarget_[kN] IV_ZI, floorA_[kN] IV_ZI, floorB_[kN] IV_ZI;
    float     floorCountdown_[kN] IV_ZI;
    uint32_t  rng_ = 1;
    // Per-sample read position for the current control block.
    float     readNow_[kN] IV_ZI, readStep_[kN] IV_ZI;
    float     tapLen_[kN] IV_ZI, tapStep_[kN] IV_ZI; // the early taps' line length, gliding per sample
    float     interp_[kN] IV_ZI; // allpass-interpolation state per read
    // Absorbent filters: y = g (1 − b) x + b y.
    float     gain_[kN] IV_ZI, pole_[kN] IV_ZI, absorbY_[kN] IV_ZI;
    float     b1_[kN] IV_ZI, absorbX_[kN] IV_ZI, shelfK_ IV_ZI; // lossShelfK(sampleRate_)
    float     modDepth_ = 1.0f, air_ IV_ZI;
    float     decayRate_ = 0.2f, highRatio_ = voicing::kDampingHighT60Ratio;
    LoopEra   era_[kN];
    Drift     driftGen_[kN];
    DriftAmounts drift_;
    float     extraRate_ IV_ZI;
    DelayLine apLine_[kN][2];
    uint32_t  apLen_[kN][2] IV_ZI;
    uint32_t  apLenNow_[kN][2] IV_ZI; // apLen_ × apScale_ (HALL scales its allpasses with SIZE): where each length is going
    // Each allpass's length as it glides there (SIZE's semitone slew), read between samples while it moves:
    // jumping with the knob clicked loudly in a small HALL (soak audit). Still, it is the exact integer read.
    // Bypassed allpasses (no in-loop diffusion) have length 0: they join and leave the loop from there.
    float     apLenGlide_[kN][2] IV_ZI, apLenStep_[kN][2] IV_ZI; // per sample
    float     apScale_ = 1.0f;
    static constexpr int kEarlyTaps = voicing::kEarlyTaps;
    float     earlyFrac_[kN][kEarlyTaps] IV_ZI, earlyGain_[kN][kEarlyTaps] IV_ZI;
    int       earlyCount_ = kEarlyTaps; // taps per line in use (setEarlyTapCount, a prototype)
    float     earlyTarget_ IV_ZI, earlyNow_ IV_ZI;
    float     inLoopOld_ IV_ZI; // the in-loop amount fading out during a SIZE crossfade
    float     inLoopTarget_ IV_ZI, inLoopNow_ IV_ZI, outputTrim_ = 1.0f, levelGain_ = 1.0f;
    float     outputTrimNow_ = 1.0f; // glides to outputTrim_ (~50 ms): it follows SIZE and DECAY, which can jump (soak audit)
    FloorGovernor governor_;
    const Fdn* leader_ IV_ZI;
    float     externalPower_ IV_ZI;
    // SIZE by crossfade (Fdn::control, ADR 0025): the old lengths, the fade (old/new gains stepped by
    // a rotation), the rest before it starts, the trip's level corrections (per block), the held trim.
    float     xfDelta_[kN] IV_ZI, xfLastTarget_[kN] IV_ZI, interpOld_[kN] IV_ZI, apOld_[kN][2] IV_ZI;
    float     xfLnGain_[kN] IV_ZI, xfHold_[kN] IV_ZI, xfLineCorr_[kN] IV_ZI;
    float     xfTapCorr_[kN][kEarlyTaps] IV_ZI, tapGainNow_[kN][kEarlyTaps] IV_ZI; // tapGainNow_: earlyGain_ x the correction
    float     xfRest_ IV_ZI, xfTrim_ = 1.0f, xfOld_ IV_ZI, xfNew_ = 1.0f, xfRotC_ = 1.0f, xfRotS_ IV_ZI;
    float     xfOO_[kN] IV_ZI, xfNN_[kN] IV_ZI, xfON_[kN] IV_ZI, xfK_[kN] IV_ZI; // the reads' powers, their agreement, the fade's gain
    AbsorbMemo absorbMemo_[kN]; // each line's loss filter, worked out again only when its inputs change
    Memo1     floorCoeffMemo_, glide80Memo_, glideXfMemo_; // per-block smoothing coefficients, worked out once per dt
    float     xfLastInLoop_ IV_ZI, xfAbsOld_[kN][3] IV_ZI; // the loss filter (b1, gain, pole) at the fade's start
    float     xfCos_ = -1.0f, xfSin_ IV_ZI; // the fade's angle (twice it): xfNew_ = (1 - cos) / 2
    int       xfLen_ = 1, xfLeft_ IV_ZI, xfSince_ = -1; // samples; xfSince_: since the start, -1 when no correction runs
#ifndef IV_SIZE_GROUPS
#define IV_SIZE_GROUPS 2 // firmware trial (integration dry run): SIZE_GROUPS=8 in firmware/Makefile
#endif
    int       xfStagger_ = IV_SIZE_GROUPS, xfGroupNext_ IV_ZI; // setStagger; 2 since v1.0.39
    // Staggered (v1.0.39), all set by reset() before any use (no initialisers: flash, the constructor):
    bool      xfLine_[kN];   // the line fades in this crossfade
    float     xfOff_[kN];    // samples since the line's own trip began, ahead of xfSince_
    float     apAmtNow_[kN], apAmtOld_[kN]; // each line's in-loop amount (it changes with its own fade)
    bool      ready_ IV_ZI;
};

} // namespace iv::dsp
