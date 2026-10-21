#pragma once
// REVERSE, the hardware reverse reverb (SPEC §4.11, ADR 0015, amended
// 22 Oct 2026): each note blooms into a swell that rises over TIME, then
// stops. Nothing is played backwards; the reverb's *envelope* is reversed.
//
// The established recipe (AMS RMX16 "Reverse", Lexicon and Midiverb reverse
// programs): a long delay with many taps that start quiet and get louder,
// into a short, dense reverb (the Engine's reverseFdn_). The taps give the
// rising envelope and its sudden end; the short reverb smears them into one
// smooth swell. Tap spacing, the rise and the short reverb's decay are tuned
// together (voicing::kReverse*).
//
// kTaps taps per side spread over TIME, jittered so they never line up into
// a rhythm, with their own jitter and signs per side (decorrelated L/R). The
// gains rise kReverseRiseDb from the first tap to the last, in dB per second
// (a reversed exponential decay), and their total energy is 1. TIME glides
// over 150 ms, like the swell stage's attack.
#include "dsp/Memory.h"
#include "params/SpaceVoicing.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

class Reverse {
public:
    static constexpr int   kTaps = voicing::kReverseTaps;
    static constexpr float kMaxSeconds = 2.0f;

    static constexpr size_t memoryFloats(float sampleRate)
    {
        return 2 * nextPow2(size_t((kMaxSeconds + 0.05f) * sampleRate) + 4);
    }

    bool prepare(Arena& arena, float sampleRate)
    {
        sampleRate_ = sampleRate;
        const size_t n = size_t((kMaxSeconds + 0.05f) * sampleRate);
        const bool ok = line_[0].init(arena, n) && line_[1].init(arena, n);
        uint32_t s = 0x2E7E5u;
        float energy = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k) {
                s = s * 1664525u + 1013904223u;
                const float jitter = float(s >> 8) * (1.0f / 16777216.0f) - 0.5f; // ±half a slot
                frac_[c][k] = (float(k) + 0.5f + 0.8f * jitter) / float(kTaps);
                const float g = std::pow(10.0f, -voicing::kReverseRiseDb * (1.0f - frac_[c][k]) / 20.0f);
                gain_[c][k] = (s & 0x80000000u) ? -g : g;
                if (c == 0) energy += g * g;
            }
        const float norm = 1.0f / std::sqrt(energy);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k) gain_[c][k] *= norm;
        reset();
        return ok;
    }

    void reset()
    {
        line_[0].clear();
        line_[1].clear();
        lengthNow_ = lengthTarget_;
    }

    // The swell's length in seconds (TIME); glides unless snap.
    void setSeconds(float s, bool snap = false)
    {
        if (s > kMaxSeconds) s = kMaxSeconds;
        if (s < 0.05f) s = 0.05f;
        lengthTarget_ = s;
        if (snap) lengthNow_ = s;
    }

    // Once per control chunk of n samples.
    void control(int n)
    {
        lengthNow_ += (1.0f - std::exp(-float(n) / (0.15f * sampleRate_))) * (lengthTarget_ - lengthNow_);
        const float span = lengthNow_ * sampleRate_;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k) tapDelay_[c][k] = 1.0f + frac_[c][k] * span;
    }

    // Always records; with tap, also returns the tapped swell in l and r.
    void process(float& l, float& r, bool tap)
    {
        line_[0].write(l);
        line_[1].write(r);
        if (!tap) {
            l = r = 0.0f;
            return;
        }
        float sum[2] = {0.0f, 0.0f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k) {
                const float d  = tapDelay_[c][k];
                const auto  di = uint32_t(d);
                const float t  = d - float(di);
                const float a0 = line_[c].read(di < 1 ? 1 : di), a1 = line_[c].read(di + 1);
                sum[c] += gain_[c][k] * (a0 + t * (a1 - a0));
            }
        l = sum[0];
        r = sum[1];
    }

private:
    float     sampleRate_ = 48000.0f;
    DelayLine line_[2];
    float     frac_[2][kTaps] = {}, gain_[2][kTaps] = {}, tapDelay_[2][kTaps] = {};
    float     lengthNow_ = 0.5f, lengthTarget_ = 0.5f;
};

} // namespace iv::dsp
