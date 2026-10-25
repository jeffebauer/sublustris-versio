#pragma once
// PROTOTYPES (desktop only, never in the firmware: Engine includes this under !IV_FIXED_VOICINGS):
// replacements for REVERSE on ATTACK's third position, for the owner to hear (25 Oct 2026).
//   SHIMMER: the wet, low-passed, shifted an octave up, high-passed, sent back into the tank's input,
//            so every trip climbs an octave (Valhalla Shimmer, Strymon BigSky's regenerating tank
//            shimmer; the two-tap shifter of Barr's FV-1 notes, Clouds' pitch_shifter.h, DAFx 2026's
//            FDN shimmer). TIME: the send, a faint sheen to cascading octaves.
//   SUB:     the same an octave down, darker filters and a steeper high-pass so the octaves below
//            fall away instead of piling up (Clouds high-passes its feedback for the same reason).
//   GRAINS:  overlapping Hann grains (80-200 ms, ~40 a second, random onsets, ±8 cents) read from
//            the last 2.5 s of the diffused input feed the tank, which smooths them (Clouds, Beads,
//            Microcosm put the reverb after the grains; Ervik & Brandtsegg: ~3 overlaps and random
//            offsets avoid the periodic grains' sidebands). TIME: how far back the grains reach.
// Written from those designs (no code copied).
#include <cmath>
#include <cstdint>
#include <vector>

namespace iv::dsp::proto {

inline float rnd01(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return float(s >> 8) * (1.0f / 16777216.0f);
}

// One channel's two-tap delay pitch shifter: two heads half a window apart, each faded by
// sin² of its own phase so it's silent when it jumps; each head picks a fresh window length
// (±15 %) when it jumps, so the crossfade rate never settles into a steady flutter.
class PitchShifter {
public:
    void prepare(float ratio, float windowSamples, uint32_t seed)
    {
        ratio_ = ratio;
        w0_ = windowSamples;
        buf_.assign(1u << 15, 0.0f);
        mask_ = buf_.size() - 1;
        seed_ = seed;
        for (int k = 0; k < 2; ++k) {
            win_[k] = w0_;
            phase_[k] = 0.5f * float(k);
        }
        pos_ = 0;
    }
    float process(float x)
    {
        buf_[pos_ & mask_] = x;
        float y = 0.0f;
        for (int k = 0; k < 2; ++k) {
            // Up: the delay shrinks (phase 1 → 0); down: it grows (0 → 1).
            const float step = std::fabs(1.0f - ratio_) / win_[k];
            phase_[k] += ratio_ > 1.0f ? -step : step;
            if (phase_[k] < 0.0f || phase_[k] >= 1.0f) {
                phase_[k] = ratio_ > 1.0f ? phase_[k] + 1.0f : phase_[k] - 1.0f;
                win_[k] = w0_ * (0.85f + 0.3f * rnd01(seed_));
            }
            const float d = 2.0f + win_[k] * phase_[k];
            const auto  di = uint32_t(d);
            const float t = d - float(di);
            const float a = buf_[(pos_ - di) & mask_], b = buf_[(pos_ - di - 1) & mask_];
            const float s = std::sin(3.14159265f * phase_[k]);
            y += s * s * (a + t * (b - a));
        }
        ++pos_;
        return y;
    }

private:
    std::vector<float> buf_;
    size_t   mask_ = 0, pos_ = 0;
    float    ratio_ = 2.0f, w0_ = 2048.0f, win_[2] = {}, phase_[2] = {};
    uint32_t seed_ = 1;
};

struct OnePole { // y += a (x - y)
    float a = 1.0f, y = 0.0f;
    void set(float hz, float sr) { a = 1.0f - std::exp(-6.2831853f * hz / sr); }
    float lp(float x) { return y += a * (x - y); }
    float hp(float x) { return x - lp(x); }
};

// Shimmer, up or down: in = the tank's wet; out = what goes back into the tank's input.
class Shimmer {
public:
    void prepare(float sampleRate, bool down)
    {
        down_ = down;
        for (int c = 0; c < 2; ++c) {
            shift_[c].prepare(down ? 0.5f : 2.0f, (down ? 0.085f : 0.043f) * sampleRate, 0x5A1u + 77u * uint32_t(c) + (down ? 9u : 0u));
            pre_[c].set(down ? 3000.0f : 7000.0f, sampleRate);
            pre2_[c].set(down ? 3000.0f : 7000.0f, sampleRate);
            for (auto& h : post_[c]) h.set(down ? 45.0f : 80.0f, sampleRate);
        }
    }
    void process(float& l, float& r)
    {
        float* io[2] = {&l, &r};
        for (int c = 0; c < 2; ++c) {
            float x = pre2_[c].lp(pre_[c].lp(*io[c]));
            x = shift_[c].process(x);
            x = post_[c][0].hp(x);
            if (down_) x = post_[c][1].hp(x); // steeper: the second octave down falls away
            *io[c] = x;
        }
    }

private:
    bool         down_ = false;
    PitchShifter shift_[2];
    OnePole      pre_[2], pre2_[2], post_[2][2];
};

// Granular feed: records the diffused input, plays overlapping grains of it.
class Grains {
public:
    static constexpr int kGrains = 16;
    void prepare(float sampleRate)
    {
        sr_ = sampleRate;
        for (auto& b : buf_) b.assign(1u << 17, 0.0f); // 2.7 s at 48 kHz
        mask_ = buf_[0].size() - 1;
        for (auto& g : g_) g.left = 0;
    }
    // spraySeconds: how far back a grain may start (TIME).
    void process(float inL, float inR, float& outL, float& outR, float spraySeconds)
    {
        buf_[0][pos_ & mask_] = inL;
        buf_[1][pos_ & mask_] = inR;
        if (--untilNext_ <= 0) {
            spawn(spraySeconds);
            untilNext_ = int(sr_ / kRate * (0.5f + rnd01(seed_))); // ~40 a second, never periodic
        }
        float l = 0.0f, r = 0.0f;
        for (auto& g : g_) {
            if (g.left <= 0) continue;
            const float x = 1.0f - float(g.left) / float(g.len); // 0 → 1 across the grain
            const float s = std::sin(3.14159265f * x);
            const float w = s * s;
            const float d = g.delay;
            const auto  di = uint32_t(d);
            const float t = d - float(di);
            for (int c = 0; c < 2; ++c) {
                const float a = buf_[c][(pos_ - di) & mask_], b = buf_[c][(pos_ - di - 1) & mask_];
                const float v = w * (a + t * (b - a));
                if (c == 0) l += v * g.panL;
                else        r += v * g.panR;
            }
            g.delay += 1.0f - g.ratio; // a detuned grain drifts through the recording
            --g.left;
        }
        // ~6 grains overlap: their energy sums, so scale by 1/sqrt(overlap).
        const float norm = 1.0f / std::sqrt(kRate * 0.14f);
        outL = norm * l;
        outR = norm * r;
        ++pos_;
    }

private:
    static constexpr float kRate = 40.0f;
    struct Grain {
        int   left = 0, len = 1;
        float delay = 0.0f, ratio = 1.0f, panL = 1.0f, panR = 1.0f;
    };
    void spawn(float spray)
    {
        for (auto& g : g_) {
            if (g.left > 0) continue;
            g.len = g.left = int(sr_ * (0.08f + 0.12f * rnd01(seed_)));
            const float cents = 16.0f * (rnd01(seed_) - 0.5f); // ±8 cents
            g.ratio = std::exp2(cents / 1200.0f);
            // Start far enough back that the whole grain (and its drift) is already recorded.
            g.delay = float(g.len) * 1.05f + 4.0f + spray * sr_ * rnd01(seed_);
            const float p = rnd01(seed_); // a random place in the stereo field
            g.panL = std::sqrt(1.0f - 0.5f * p);
            g.panR = std::sqrt(0.5f + 0.5f * p);
            return;
        }
    }
    float    sr_ = 48000.0f;
    std::vector<float> buf_[2];
    size_t   mask_ = 0, pos_ = 0;
    Grain    g_[kGrains];
    int      untilNext_ = 0;
    uint32_t seed_ = 0x6A41u;
};

} // namespace iv::dsp::proto
