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

#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (renders/size_stagger, 26 Oct 2026: on the module a SIZE crossfade in MASSIVE costs up to
    // ~110 % of the CPU, every line read twice for 0.3 s): with 2, a SIZE move crossfades lines 0, 2, 4, 6
    // first and 1, 3, 5, 7 straight after (two fades of kSizeCrossfadeSeconds back to back), so only half
    // the lines read twice at once. 1 = as built.
    void setStaggerPrototype(int groups) { xfStagger_ = groups == 2 ? 2 : 1; }
#endif

private:
#ifndef IV_FIXED_VOICINGS
    bool staggered() const { return xfStagger_ == 2; }
#endif
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
    float     lenTarget_[kN] = {}; // samples
    float     lenNow_[kN]    = {}; // samples, gliding toward the target
    float     maxLen_        = 0.0f;
    // Modulation floor, per line: a random target, smoothed twice.
    float     floorTarget_[kN] = {}, floorA_[kN] = {}, floorB_[kN] = {};
    float     floorCountdown_[kN] = {};
    uint32_t  rng_ = 1;
    // Per-sample read position for the current control block.
    float     readNow_[kN] = {}, readStep_[kN] = {};
    float     tapLen_[kN] = {}, tapStep_[kN] = {}; // the early taps' line length, gliding per sample
    float     interp_[kN] = {}; // allpass-interpolation state per read
    // Absorbent filters: y = g (1 − b) x + b y.
    float     gain_[kN] = {}, pole_[kN] = {}, absorbY_[kN] = {};
    float     b1_[kN] = {}, absorbX_[kN] = {}, shelfK_ = 0.0f; // lossShelfK(sampleRate_)
    float     modDepth_ = 1.0f, air_ = 0.0f;
    float     decayRate_ = 0.2f, highRatio_ = voicing::kDampingHighT60Ratio;
    LoopEra   era_[kN];
    Drift     driftGen_[kN];
    DriftAmounts drift_;
    float     extraRate_ = 0.0f;
    DelayLine apLine_[kN][2];
    uint32_t  apLen_[kN][2] = {};
    uint32_t  apLenNow_[kN][2] = {}; // apLen_ × apScale_ (HALL scales its allpasses with SIZE): where each length is going
    // Each allpass's length as it glides there (SIZE's semitone slew), read between samples while it moves:
    // jumping with the knob clicked loudly in a small HALL (soak audit). Still, it is the exact integer read.
    // Bypassed allpasses (no in-loop diffusion) have length 0: they join and leave the loop from there.
    float     apLenGlide_[kN][2] = {}, apLenStep_[kN][2] = {}; // per sample
    float     apScale_ = 1.0f;
    static constexpr int kEarlyTaps = voicing::kEarlyTaps;
    float     earlyFrac_[kN][kEarlyTaps] = {}, earlyGain_[kN][kEarlyTaps] = {};
    int       earlyCount_ = kEarlyTaps; // taps per line in use (setEarlyTapCount, a prototype)
    float     earlyTarget_ = 0.0f, earlyNow_ = 0.0f;
    float     inLoopOld_ = 0.0f; // the in-loop amount fading out during a SIZE crossfade
    float     inLoopTarget_ = 0.0f, inLoopNow_ = 0.0f, outputTrim_ = 1.0f, levelGain_ = 1.0f;
    float     outputTrimNow_ = 1.0f; // glides to outputTrim_ (~50 ms): it follows SIZE and DECAY, which can jump (soak audit)
    FloorGovernor governor_;
    const Fdn* leader_ = nullptr;
    float     externalPower_ = 0.0f;
    // SIZE by crossfade (Fdn::control, ADR 0025): the old lengths, the fade (old/new gains stepped by
    // a rotation), the rest before it starts, the trip's level corrections (per block), the held trim.
    float     xfDelta_[kN] = {}, xfLastTarget_[kN] = {}, interpOld_[kN] = {}, apOld_[kN][2] = {};
    float     xfLnGain_[kN] = {}, xfHold_[kN] = {}, xfLineCorr_[kN] = {};
    float     xfTapCorr_[kN][kEarlyTaps] = {}, tapGainNow_[kN][kEarlyTaps] = {}; // tapGainNow_: earlyGain_ x the correction
    float     xfRest_ = 0.0f, xfTrim_ = 1.0f, xfOld_ = 0.0f, xfNew_ = 1.0f, xfRotC_ = 1.0f, xfRotS_ = 0.0f;
    float     xfOO_[kN] = {}, xfNN_[kN] = {}, xfON_[kN] = {}, xfK_[kN] = {}; // the reads' powers, their agreement, the fade's gain
    AbsorbMemo absorbMemo_[kN]; // each line's loss filter, worked out again only when its inputs change
    Memo1     floorCoeffMemo_, glide80Memo_, glideXfMemo_; // per-block smoothing coefficients, worked out once per dt
    float     xfLastInLoop_ = 0.0f, xfAbsOld_[kN][3] = {}; // the loss filter (b1, gain, pole) at the fade's start
    float     xfCos_ = -1.0f, xfSin_ = 0.0f; // the fade's angle (twice it): xfNew_ = (1 - cos) / 2
    int       xfLen_ = 1, xfLeft_ = 0, xfSince_ = -1; // samples; xfSince_: since the start, -1 when no correction runs
#ifndef IV_FIXED_VOICINGS
    int       xfStagger_ = 1, xfGroupNext_ = 0; // setStaggerPrototype
    bool      xfLine_[kN] = {};  // staggered: the line fades in this crossfade
    float     xfOff_[kN] = {};   // staggered: samples since the line's own trip began, ahead of xfSince_
    float     apAmtNow_[kN] = {}, apAmtOld_[kN] = {}; // staggered: each line's in-loop amount (it changes with its own fade)
#endif
    bool      ready_ = false;
};

} // namespace iv::dsp
