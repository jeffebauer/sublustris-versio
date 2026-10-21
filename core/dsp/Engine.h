#pragma once
// The Engine: everything between the inputs and the outputs (CONTEXT.md:
// big tank, companion, loop chain, Era, output stage). Every Host drives it
// the same way: prepare(), setParam() with Normalised values, button() and
// gate() edges at a sample offset in the next block, process().
//
// M1: the HALL. In → predelay (TIME, NORMAL) → input diffusion → FDN →
// MIX. Every SPACE position plays the Hall until the Plate (M5) and
// Massive (M4) land. Button and gate edges are recorded; the gesture reader
// (ADR 0009–0011) lands at M5.
//
// Memory (SPEC §5.2): the firmware calls attachMemory() with its SDRAM and
// DTCM arrays before prepare(). Desktop Hosts may skip it: the Engine then
// owns vectors (not in the firmware, which builds with IV_NO_HEAP).
#include "dsp/Diffuser.h"
#include "dsp/Fdn.h"
#include "dsp/Memory.h"
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
        return dsp::Fdn::memoryFloats(sampleRate)
             + 2 * dsp::nextPow2(size_t(voicing::kPredelayMaxSeconds * sampleRate) + 4);
    }
    static constexpr size_t fastFloats(float sampleRate)
    {
        size_t n = 0;
        for (int c = 0; c < 2; ++c)
            for (float s : voicing::kDiffuserSeconds)
                n += dsp::nextPow2(size_t(s * (c ? voicing::kDiffuserRightScale : 1.0f) * sampleRate) + 4);
        return n;
    }

    void attachMemory(float* large, size_t largeCount, float* fast, size_t fastCount);

    void prepare(float sampleRate, int maxBlockSize);
    void reset();

    void setParam(ParamId id, float normalised);
    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Button press / release at a sample offset within the next process() block.
    void button(bool pressed, int /*sampleOffset*/) { buttonDown_ = pressed; }
    // Gate edge: high = a press, low = a release (ADR 0011). Unpatched reads low.
    void gate(bool high, int /*sampleOffset*/) { gateHigh_ = high; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    bool  frozen() const { return frozen_; }
    bool  painting() const { return false; }
    bool  buttonDown() const { return buttonDown_; }
    bool  gateHigh() const { return gateHigh_; }
    // Gain the output limiter is applying now (1 = not limiting), for the LEDs.
    float limiterGain() const { return 1.0f; }
    // Bytes of delay memory the Engine uses (firmware serial report).
    size_t memoryBytes() const { return (large_.used + fast_.used) * sizeof(float); }
    // False when the attached memory was too small: the Engine then passes
    // the dry only.
    bool  ready() const { return ready_; }

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }

    // Read-only views for tests.
    const dsp::Fdn& fdn() const { return fdn_; }

private:
    static constexpr int kChunk = dsp::Fdn::kControlBlock;

    void controlTick(int n);
    float smoothed(ParamId id) const { return smooth_[static_cast<size_t>(id)]; }

    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smooth_{};
    bool  buttonDown_ = false, gateHigh_ = false, frozen_ = false;
    bool  ready_ = false;

    dsp::Arena large_, fast_;
#if !defined(IV_NO_HEAP)
    std::vector<float> ownLarge_, ownFast_;
#endif
    float* attachedLarge_ = nullptr;
    float* attachedFast_ = nullptr;
    size_t attachedLargeCount_ = 0, attachedFastCount_ = 0;

    dsp::DelayLine    predelay_[2];
    float             predelayNow_ = 0.0f, predelayStep_ = 0.0f; // samples
    dsp::Diffuser<4>  diffuser_[2];
    dsp::Fdn          fdn_;
    float             lastMean_ = -1.0f;

    // MIX, ramped per sample across each control chunk.
    float dryNow_ = 1.0f, wetNow_ = 0.0f, dryStep_ = 0.0f, wetStep_ = 0.0f;

    // Scratch for one chunk.
    float bufL_[kChunk] = {}, bufR_[kChunk] = {}, wetL_[kChunk] = {}, wetR_[kChunk] = {};
};

} // namespace iv
