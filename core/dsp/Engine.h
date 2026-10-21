#pragma once
// The Engine: everything between the inputs and the outputs (CONTEXT.md:
// big tank, twin, loop chain, Era, output stage). Every Host drives it
// the same way: prepare(), setParam() with Normalised values, button() and
// gate() edges at a sample offset in the next block, process().
//
// M5: the three SPACEs (PLATE on the Plate, HALL and MASSIVE on the FDN),
// ATTACK, and the freeze: In → predelay → input diffusion → swell → big
// tank → (REVERSE) → MIX (SPEC §4.1). The button and the gate feed one
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
#include "dsp/Fdn.h"
#include "dsp/Memory.h"
#include "dsp/Output.h"
#include "dsp/Plate.h"
#include "dsp/Reverse.h"
#include "dsp/Swell.h"
#include "params/GestureReader.h"
#include "params/ParamSpec.h"

#include <array>
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
        return dsp::Fdn::memoryFloats(sampleRate) + dsp::Swell::memoryFloats(sampleRate)
             + dsp::Reverse::memoryFloats(sampleRate) + 2 * dsp::Plate::memoryFloats(sampleRate, voicing::kPlateMaxScale, true)
             + dsp::Fdn::memoryFloats(sampleRate) // the freeze twin
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
    // PROTOTYPE, renders/reverse_options: 0 A pieces, 1 B smooth pieces, 2 C swell to a cut,
    // 3 D the input reversed into the tank.
    void  setReverseMode(int m) { reverseMode_ = m; reverse_.setSmooth(m == 1 || m == 3); }
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
    static constexpr int kMaxEdges = 16;

    void queueEdge(bool isButton, bool level, int offset);
    void handleEdges(int chunkStart, int chunkEnd);
    void onGesture(GestureReader::Event e);
    void controlTick(int n);
    void applySpace(bool snap);
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
    dsp::Fdn          fdn_;
    dsp::Plate        plate_;
    // The freeze twin: the frozen tank's space, with its own decay.
    dsp::Fdn          twinFdn_;
    dsp::Plate        twinPlate_;
    dsp::Diffuser<4>  twinDiffuser_[2];
    float             twinMean_ = -1.0f, twinSpread_ = -1.0f;
    float             twinIn_ = 0.0f, twinInStep_ = 0.0f;
    bool              twinActive_ = false;
    dsp::Reverse      reverse_;
    dsp::MuLawOut     muLaw_;
    float             eraMakeup_ = 1.0f;
    // Ducking (ADR 0014) and the limiter (SPEC §4.10).
    dsp::Tilt         tilt_;
    dsp::Darken       darken_;
    dsp::Limiter      limiter_;
    float             duckEnv_ = 0.0f, duck_ = 0.0f, duckAttack_ = 0.0f, duckRelease_ = 0.0f;
    // ERA's sources into the tank's input (SPEC §4.7): cassette hiss, Midiverb crackle.
    uint32_t          noiseSeed_ = 0xA11CEu;
    float             hissLp_[2] = {}, hissGain_ = 0.0f, crackleChance_ = 0.0f, crackleGain_ = 0.0f;
    float             lastMean_ = -1.0f, lastSpread_ = -1.0f;

    // SPACE flips (ADR 0013): the wet and the tank's input fade out, the
    // tank restarts in the new voicing, and both fade back in.
    int   activeSpace_ = 1, flipPhase_ = 0; // 0 steady, 1 fading out, 2 fading in
    float flipGain_ = 1.0f, flipStep_ = 0.0f;
    // ATTACK = REVERSE: crossfade between the forward and the reversed wet.
    float reverseMix_ = 0.0f, reverseTarget_ = 0.0f;
    int   reverseMode_ = 0; // PROTOTYPE (setReverseMode)
    float cutGain_ = 1.0f, cutGainTarget_ = 1.0f; // PROTOTYPE C: makeup for the short tail

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
    uint64_t quietSince_ = 0;
    bool     tailCaptured_ = false;
    float    tailCeilingDb_ = -24.0f;
    float tankInTarget_ = 1.0f;

    // MIX, ramped per sample across each control chunk.
    float dryNow_ = 1.0f, wetNow_ = 0.0f, dryStep_ = 0.0f, wetStep_ = 0.0f;

    // Scratch for one chunk.
    float bufL_[kChunk] = {}, bufR_[kChunk] = {}, wetL_[kChunk] = {}, wetR_[kChunk] = {};
    float twinInL_[kChunk] = {}, twinInR_[kChunk] = {}, twinL_[kChunk] = {}, twinR_[kChunk] = {};
};

} // namespace iv
