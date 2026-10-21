#pragma once
// The Engine: everything between the inputs and the outputs (CONTEXT.md:
// big tank, companion, loop chain, Era, output stage). Every Host drives it
// the same way: prepare(), setParam() with Normalised values, button() and
// gate() edges at a sample offset in the next block, process().
//
// M0: passthrough only. Proves the Core builds and runs identically in every
// Host. Button and gate edges are recorded so Hosts can be wired and tested
// now; the gesture reader (tap / hold, ADR 0009–0011) lands at M5.
#include "params/ParamSpec.h"

#include <array>
#include <cstddef>

namespace iv {

class Engine {
public:
    // Output ceiling of the soft limiter (SPEC §4.10), linear.
    static constexpr float kLimitThreshold = 0.891f; // −1 dBFS

    void prepare(float sampleRate, int maxBlockSize)
    {
        sampleRate_   = sampleRate;
        maxBlockSize_ = maxBlockSize;
        for (const auto& p : kParams)
            values_[static_cast<size_t>(p.id)] = p.defaultValue;
        reset();
    }

    void reset()
    {
        buttonDown_ = false;
        gateHigh_   = false;
        frozen_     = false;
    }

    void setParam(ParamId id, float normalised)
    {
        if (normalised < 0.0f) normalised = 0.0f;
        if (normalised > 1.0f) normalised = 1.0f;
        values_[static_cast<size_t>(id)] = normalised;
    }

    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Button press / release at a sample offset within the next process() block.
    void button(bool pressed, int /*sampleOffset*/) { buttonDown_ = pressed; }
    // Gate edge: high = a press, low = a release (ADR 0011). Unpatched reads low.
    void gate(bool high, int /*sampleOffset*/) { gateHigh_ = high; }

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
    }

    bool  frozen() const { return frozen_; }
    bool  painting() const { return false; }
    bool  buttonDown() const { return buttonDown_; }
    bool  gateHigh() const { return gateHigh_; }
    // Gain the output limiter is applying now (1 = not limiting), for the LEDs.
    float limiterGain() const { return 1.0f; }
    // Bytes of delay memory the Engine uses (firmware serial report).
    size_t memoryBytes() const { return 0; }

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }

private:
    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    bool  buttonDown_ = false, gateHigh_ = false, frozen_ = false;
};

} // namespace iv
