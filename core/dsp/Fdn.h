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
#include "dsp/Drift.h"
#include "dsp/Era.h"
#include "dsp/Governor.h"
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
    // maxLineSeconds: the longest a line may get (REVERSE's short reverb uses far less than the tank).
    static constexpr size_t memoryFloats(float sampleRate, float maxLineSeconds = voicing::kFdnMaxLineSeconds)
    {
        return size_t(kN) * (nextPow2(size_t(maxLineSeconds * sampleRate) + 4)
                             + 2 * nextPow2(size_t(voicing::kInLoopMaxSeconds * sampleRate) + 4));
    }

    bool prepare(Arena& arena, float sampleRate, uint32_t seed, float maxLineSeconds = voicing::kFdnMaxLineSeconds);
    void reset();

    // Targets, read at the next control tick. meanSeconds: the lines' mean
    // length; decayRate: 1/T60 in 1/s (0 = infinite); highT60Ratio: the
    // high end's T60 over the low end's (TONE). snap = jump straight there
    // (at prepare/reset), else SIZE glides (kSizeSlewPerSecond).
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
    // The modulation floor's depth × modDepth (Engine::setModulationScale, a test hook).
    void setModDepth(float k) { modDepth_ = k; }
    // E4 PROTOTYPE (DSP audit, 24 Oct 2026): 0..1, the reads crossfade to the Lexicon 224 / PCM70 way
    // (Costello): linear interpolation with its fraction quantised to kVintageSteps, and a noisy read.
    void setVintage(float v) { vintage_ = v; }
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
    float     interp_[kN] = {}; // allpass-interpolation state per read
    // Absorbent filters: y = g (1 − b) x + b y.
    float     gain_[kN] = {}, pole_[kN] = {}, absorbY_[kN] = {};
    float     b1_[kN] = {}, absorbX_[kN] = {}, shelfK_ = 0.0f; // lossShelfK(sampleRate_)
    float     modDepth_ = 1.0f, vintage_ = 0.0f, jitter_[kN] = {};
    float     decayRate_ = 0.2f, highRatio_ = voicing::kDampingHighT60Ratio;
    LoopEra   era_[kN];
    Drift     driftGen_[kN];
    DriftAmounts drift_;
    float     extraRate_ = 0.0f;
    DelayLine apLine_[kN][2];
    uint32_t  apLen_[kN][2] = {};
    uint32_t  apLenNow_[kN][2] = {}; // apLen_ × apScale_ (HALL scales its allpasses with SIZE)
    float     apScale_ = 1.0f;
    static constexpr int kEarlyTaps = voicing::kEarlyTaps;
    float     earlyFrac_[kN][kEarlyTaps] = {}, earlyGain_[kN][kEarlyTaps] = {};
    float     earlyTarget_ = 0.0f, earlyNow_ = 0.0f;
    float     inLoopTarget_ = 0.0f, inLoopNow_ = 0.0f, outputTrim_ = 1.0f, levelGain_ = 1.0f;
    FloorGovernor governor_;
    const Fdn* leader_ = nullptr;
    float     externalPower_ = 0.0f;
    bool      ready_ = false;
};

} // namespace iv::dsp
