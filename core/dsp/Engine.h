#pragma once
// The Engine: everything between the inputs and the outputs (CONTEXT.md:
// big tank, twin, loop chain, Era, output stage). Every Host drives it
// the same way: prepare(), setParam() with Normalised values, button() and
// gate() edges at a sample offset in the next block, process().
//
// M5: the three SPACEs (PLATE on the Plate, HALL and MASSIVE on the FDN),
// ATTACK, and the freeze: In → predelay → input diffusion → swell → big
// tank → MIX (SPEC §4.1); in REVERSE the diffused input's rising taps feed
// a short reverb of its own, which replaces the big tank's wet. The button and the gate feed one
// gesture reader (ADR 0011): a tap freezes the big tank into a still pad and
// moves the input to the twin, a second tank of the same space whose sound
// fades back out over the pad (SIZE and DECAY play it); holding while frozen
// paints a little of the twin into the pad (ADR 0010); a tap unfreezes and
// the big tank glides back to the knobs (ADR 0009).
//
// Memory (SPEC §5.2): the firmware calls attachMemory() with its SDRAM and
// DTCM arrays before prepare(). Desktop Hosts may skip it: the Engine then
// owns vectors (not in the firmware, which builds with IV_NO_HEAP).
#include "dsp/Diffuser.h"
#include "dsp/EraStages.h"
#include "dsp/Fdn.h"
#include "dsp/Memory.h"
#include "dsp/Output.h"
#include "dsp/Oversampler.h"
#include "dsp/Plate.h"
#include "dsp/Reverse.h"
#include "dsp/Swell.h"
#include "dsp/Tape.h"
#include "params/GestureReader.h"
#include "params/ParamSpec.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#if !defined(IV_NO_HEAP)
#include <vector>
#endif

namespace iv {

class Engine {
public:
    // Output ceiling of the soft limiter (SPEC §4.10), linear.
    static constexpr float kLimitThreshold = 0.891f; // −1 dBFS

    // Floats of each memory kind the Engine needs at this sample rate.
    static constexpr size_t largeFloats(float sampleRate)
    {
        return dsp::Fdn::memoryFloats(sampleRate) + 2 * dsp::Swell::memoryFloats(sampleRate) // the tank's swell and the twin's
             + dsp::Reverse::memoryFloats(sampleRate) + 2 * dsp::Plate::memoryFloats(sampleRate, voicing::kPlateMaxScale, true)
             + dsp::Fdn::memoryFloats(sampleRate) // the freeze twin
             + dsp::Fdn::memoryFloats(sampleRate / float(kLowDecimate)) // still lows' low tank
             + dsp::Fdn::memoryFloats(sampleRate, voicing::kReverseMaxLineSeconds) // REVERSE's short reverb
             + 2 * dsp::nextPow2(size_t(voicing::kPredelayMaxSeconds * sampleRate) + 4);
    }
    static constexpr size_t fastFloats(float sampleRate)
    {
        size_t n = 0;
        for (int twin = 0; twin < 2; ++twin) // the input diffusers, and the twin's (13 % longer)
            for (int c = 0; c < 2; ++c)
                for (float s : voicing::kDiffuserSeconds)
                    n += dsp::nextPow2(size_t(s * (c ? voicing::kDiffuserRightScale : 1.0f) * (twin ? 1.13f : 1.0f) * sampleRate) + 4);
        return n;
    }

    void attachMemory(float* large, size_t largeCount, float* fast, size_t fastCount);

    void prepare(float sampleRate, int maxBlockSize);
    void reset();

    void setParam(ParamId id, float normalised);
    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Button press / release at a sample offset within the next process() block.
    void button(bool pressed, int sampleOffset) { queueEdge(true, pressed, sampleOffset); }
    // Gate edge: high = a press, low = a release (ADR 0011). Unpatched reads low.
    void gate(bool high, int sampleOffset) { queueEdge(false, high, sampleOffset); }

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    bool  frozen() const { return gesture_.frozen(); }
    bool  painting() const { return gesture_.painting(); }
    // Test hook: scales the tanks' own modulation (0 = none; DRIFT is separate), to check a
    // structure isn't metallic without it (Costello: tune it to sound good unmodulated).
    void  setModulationScale(float k) { modScale_ = k; }
    // Test hook: how much of DECAY's level make-up applies (1 = v1.0.28; 0 = none, shorter tails
    // quieter, as v1.0.27), for demos.
    void  setDecayMakeup(float k) { decayMakeup_ = k; }
    // Test hook: false = no tape out (ERA's cassette stage without its output tape), for measuring.
    void  setTapeOut(bool on) { tapeOn_ = on; }
    // Test hook: false = one tank, the lows moving too (before v1.0.18), for demos.
    void  setStillLows(bool on)
    {
        lowTank_ = on;
        fdn_.setExternalPower(0.0f);
    }
    // PROTOTYPE (Mutable Instruments idea 4, no measurable gain; kept for a later listen): the
    // right input diffuser gets its own lengths instead of a 1.071x copy of the left.
    void  setDiffuserIndependent(bool on);
    // Test/demo override of HALL's in-loop diffusion amount (default kHallInLoopDiffusion, v1.0.17;
    // 0..1 × kInLoopCoeff, fading out by SIZE noon).
    void  setHallInLoopDiffusion(float a) { hallInLoop_ = a; }
    bool  buttonDown() const { return buttonDown_; }
    bool  gateHigh() const { return gateHigh_; }
    // Gain the output limiter is applying now (1 = not limiting), for the LEDs.
    float limiterGain() const { return limiter_.gain(); }
    float duckAmount() const { return duck_; }
    // Bytes of delay memory the Engine uses (firmware serial report).
    size_t memoryBytes() const { return (large_.used + fast_.used) * sizeof(float); }
    // False when the attached memory was too small: the Engine then passes
    // the dry only.
    bool  ready() const { return ready_; }

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }

    // Read-only views for tests.
    const dsp::Fdn&   fdn() const { return fdn_; }
    const dsp::Plate& plate() const { return plate_; }
    int   activeSpace() const { return activeSpace_; }
    float tankLevelDb() const { return activeSpace_ == 0 ? plate_.levelDb() : fdn_.levelDb(); }
    float tankSlowLevelDb() const { return activeSpace_ == 0 ? plate_.slowLevelDb() : fdn_.slowLevelDb(); }

private:
    static constexpr int kChunk = dsp::Fdn::kControlBlock;
    static constexpr int kLowDecimate = 4; // still lows' low tank runs at 12 kHz
    static constexpr int kMaxEdges = 16;

    void queueEdge(bool isButton, bool level, int offset);
    void handleEdges(int chunkStart, int chunkEnd);
    void onGesture(GestureReader::Event e);
    void controlTick(int n);
    // snap: jump to the targets (reset, a SPACE flip); snapInput: the input diffusers' amount and
    // REVERSE too (not on a flip, where they keep running and glide).
    void applySpace(bool snap, int n = kChunk, bool snapInput = true);
    static constexpr float kPredelayMaxSpeed = 0.5f; // samples per sample
    float decayRate(float decay, float size) const;
    float smoothed(ParamId id) const { return smooth_[static_cast<size_t>(id)]; }

    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smooth_{};
    bool  buttonDown_ = false, gateHigh_ = false;
    bool  ready_ = false;

    dsp::Arena large_, fast_;
#if !defined(IV_NO_HEAP)
    std::vector<float> ownLarge_, ownFast_;
#endif
    float* attachedLarge_ = nullptr;
    float* attachedFast_ = nullptr;
    size_t attachedLargeCount_ = 0, attachedFastCount_ = 0;

    dsp::DelayLine    predelay_[2];
    float             predelayNow_ = 0.0f, predelayStep_ = 0.0f, predelayTarget_ = 0.0f; // samples
    dsp::Diffuser<4>  diffuser_[2];
    dsp::Swell        swell_;
    dsp::Swell        twinSwell_; // MASSIVE's twin swells like its tank (owner: no repeats over a frozen MASSIVE)
    dsp::Fdn          fdn_;
    dsp::Plate        plate_;
    // The freeze twin: the frozen tank's space, with its own decay.
    dsp::Fdn          twinFdn_;
    dsp::Fdn          reverseFdn_; // REVERSE's short reverb
    // Still lows (v1.0.18, ADR 0012 amendment; docs/research/still-lows.md): the HALL / MASSIVE tank's
    // input splits at kStillLowsHz (Butterworth); the highs go to fdn_, the lows to lowFdn_, an
    // unmodulated tank at 1/kLowDecimate the rate that follows fdn_'s SIZE, DECAY and governor
    // (the established two-tank design: Keith Barr's dual-band reverb, Eventide's H90 Hall).
    void              processTwoTanks(int n);
    dsp::Fdn          lowFdn_;
    bool              lowTank_ = true;
    static constexpr float kLowAntiAliasHz = 2000.0f;
    float             xoL_[5] = {}, xoH_[5] = {}, aaL_[5] = {}; // biquads b0 b1 b2 a1 a2: crossover LP/HP, 2 kHz LP
    float             xoZ_[2][6][2] = {};              // per channel: in LP, in AA, in HP, (spare), out AA
    int               lowPhase_ = 0;
    float             lowPrev_[2] = {}, lowCur_[2] = {};
    float             lowInL_[kChunk / kLowDecimate + 2] = {}, lowInR_[kChunk / kLowDecimate + 2] = {};
    float             lowOutL_[kChunk / kLowDecimate + 2] = {}, lowOutR_[kChunk / kLowDecimate + 2] = {};
    dsp::Plate        twinPlate_;
    dsp::Diffuser<4>  twinDiffuser_[2];
    float             twinMean_ = -1.0f, twinSpread_ = -1.0f;
    float             twinIn_ = 0.0f, twinInStep_ = 0.0f;
    bool              twinActive_ = false;
    dsp::Reverse      reverse_;
    float             eraMakeup_ = 1.0f;
    // Ducking (ADR 0014) and the limiter (SPEC §4.10).
    dsp::Tilt         tilt_;
    dsp::Limiter      limiter_;
    float             duck_ = 0.0f, duckGrDb_ = 0.0f, duckGain_ = 1.0f, duckGainStep_ = 0.0f; // ducking: LEDs 0..1, reduction dB, wet gain
    // ERA's sources into the tank's input (SPEC §4.7): cassette hiss, Midiverb crackle.
    uint32_t          noiseSeed_ = 0xA11CEu;
    float             hissLp_[2] = {}, hissGain_ = 0.0f, crackleChance_ = 0.0f, crackleGain_ = 0.0f;
    float             lastMean_ = -1.0f, lastSpread_ = -1.0f;
    float             hallSize_ = 0.5f; // HALL's SIZE for its gains, glided (applySpace)

    // SPACE flips (ADR 0013): the wet and the tank's input fade out, the
    // tank restarts in the new voicing, and both fade back in.
    int   activeSpace_ = 1, flipPhase_ = 0; // 0 steady, 1 fading out, 2 fading in
    float flipGain_ = 1.0f, flipStep_ = 0.0f;
    // ATTACK = REVERSE: crossfade between the forward wet and the reverse swell.
    float reverseMix_ = 0.0f, reverseTarget_ = 0.0f, reverseFeed_ = 0.0f; // reverseFeed_: the taps into the short reverb
    bool  reverseRunning_ = false;
    float modScale_ = 1.0f;  // test hook (setModulationScale)
    dsp::BbdStage   bbd_;   // ERA's bucket-brigade and radio, on the wet (ADR 0021, dsp/EraStages.h)
    dsp::RadioStage radio_;
    float bbdW_ = 0, radioW_ = 0;
    float decayMakeup_ = 1.0f, decayGain_ = 1.0f, decayGainNow_ = 1.0f; // DECAY's level make-up (v1.0.28)
    float reverseGain_ = 1.0f, reverseGainNow_ = 1.0f;                     // ... and REVERSE's
    float tapeDrive_ = 0.0f;
    bool  tapeOn_ = true; // test hook (setTapeOut)
    float tapeMix_ = 0.0f; // tape out's fade in/out
    dsp::TapeOut tape_[2]; // tape out (v1.0.27, dsp/Tape.h)
    float diffMotionNow_ = 0.0f, diffMotionPhase_ = 0.0f; // the PLATE's wandering input allpass, samples
    float hallInLoop_ = voicing::kHallInLoopDiffusion; // setHallInLoopDiffusion overrides it (renders/hall_diffusion)
    float reverseMean_ = -1.0f;

    // Freeze (ADR 0009–0011). Button and gate edges this block, by offset.
    struct Edge {
        int  offset;
        bool isButton, level;
    };
    Edge          edges_[kMaxEdges] = {};
    int           edgeCount_ = 0;
    GestureReader gesture_;
    uint64_t      clock_ = 0;            // samples since prepare
    float heldSize_ = 0.5f, heldDecay_ = 0.5f; // the big tank's SIZE and DECAY, frozen with it
    float freezeMix_ = 0.0f;             // 0 normal .. 1 a still pad (in-loop colour out, gain 1)
    float tankIn_ = 1.0f, tankInStep_ = 0.0f;
    float paint_ = 0.0f, paintRoom_ = 1.0f; // paintRoom_: 1 → 0 as a painted pad nears its ceiling
    float padFloorDb_ = -24.0f;
    bool  padCaptured_ = false; // the pad's own level has been taken as its ceiling
    uint64_t frozeAt_ = 0;
    // Input quiet detector for holding an infinite tail where it settled.
    float    inputPower_ = 0.0f;
    float    tankInPower_ = 0.0f; // the big tank's input (after the swell), for the quiet detector
    uint64_t quietSince_ = 0;
    bool     tailCaptured_ = false;
    float    tailCeilingDb_ = -24.0f;
    float tankInTarget_ = 1.0f;

    // MIX, ramped per sample across each control chunk.
    float dryNow_ = 1.0f, wetNow_ = 0.0f, dryStep_ = 0.0f, wetStep_ = 0.0f;

    // Scratch for one chunk.
    float bufL_[kChunk] = {}, bufR_[kChunk] = {}, wetL_[kChunk] = {}, wetR_[kChunk] = {};
    float twinInL_[kChunk] = {}, twinInR_[kChunk] = {}, twinL_[kChunk] = {}, twinR_[kChunk] = {};
    float revInL_[kChunk] = {}, revInR_[kChunk] = {}, revL_[kChunk] = {}, revR_[kChunk] = {};
};

} // namespace iv
