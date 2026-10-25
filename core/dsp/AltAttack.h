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

// One channel's delay-line pitch shifter: `heads` read heads evenly spaced through a window, each
// faded by sin² of its own phase so it's silent when it jumps; evenly spaced sin² windows sum to a
// constant (heads / 2). Round 1 used 2 heads, 43 ms, each picking a fresh length (±15 %) at its jump:
// the windows then no longer summed to a constant and the octave fluttered 14-25 dB at the 23 Hz
// splice rate (heard as "bit crushing"). 4 heads of 100 ms with fixed lengths: 0.8 dB on a sine,
// 1.6 dB on a noise-like tail (below the unshifted tail's own 2.4).
class PitchShifter {
public:
    static constexpr int kMaxHeads = 8;
    void prepare(float ratio, float windowSamples, uint32_t seed, int heads = 2, float jitter = 0.15f)
    {
        ratio_ = ratio;
        w0_ = windowSamples;
        heads_ = heads;
        jitter_ = jitter;
        buf_.assign(1u << 16, 0.0f); // up to ~1.3 s of window
        mask_ = buf_.size() - 1;
        seed_ = seed;
        for (int k = 0; k < heads_; ++k) {
            win_[k] = w0_;
            phase_[k] = float(k) / float(heads_);
        }
        pos_ = 0;
    }
    void setRatio(float r) { ratio_ = r; } // small moves only (detune); the windows stay
    // The heads' reads wander together, slowly, by up to ±samples (a new target every few jumps,
    // glided over ~1.5 s): a few cents of slow movement, as BigSky's Mod and Valhalla's random
    // modulators. A new target every ~30 jumps (~3 s); together: heads drifting apart played at slightly different pitches and beat
    // (the splice-rate roughness came back, +0.6 dB fast-against-slow movement vs -10 smooth).
    void setDrift(float samples, float sampleRate)
    {
        drift_ = samples;
        driftCoeff_ = 1.0f - std::exp(-1.0f / (2.5f * sampleRate));
    }
    float process(float x)
    {
        buf_[pos_ & mask_] = x;
        driftNow_ += driftCoeff_ * (driftTarget_ - driftNow_);
        float y = 0.0f;
        for (int k = 0; k < heads_; ++k) {
            // Up: the delay shrinks (phase 1 → 0); down: it grows (0 → 1).
            const float step = std::fabs(1.0f - ratio_) / win_[k];
            phase_[k] += ratio_ > 1.0f ? -step : step;
            if (phase_[k] < 0.0f || phase_[k] >= 1.0f) {
                phase_[k] = ratio_ > 1.0f ? phase_[k] + 1.0f : phase_[k] - 1.0f;
                if (jitter_ > 0.0f) win_[k] = w0_ * (1.0f - jitter_ + 2.0f * jitter_ * rnd01(seed_));
                if (drift_ > 0.0f && k == 0 && ++jumps_ >= 30) { // a new target every ~3 s: slower moves wobbled at 10 Hz
                    jumps_ = 0;
                    driftTarget_ = drift_ * (2.0f * rnd01(seed_) - 1.0f);
                }
            }
            const float d = 2.0f + drift_ + driftNow_ + win_[k] * phase_[k];
            const auto  di = uint32_t(d);
            const float t = d - float(di);
            const float a = buf_[(pos_ - di) & mask_], b = buf_[(pos_ - di - 1) & mask_];
            const float s = std::sin(3.14159265f * phase_[k]);
            y += s * s * (a + t * (b - a));
        }
        ++pos_;
        return y * (2.0f / float(heads_));
    }

private:
    std::vector<float> buf_;
    size_t   mask_ = 0, pos_ = 0;
    int      heads_ = 2;
    float    ratio_ = 2.0f, w0_ = 2048.0f, jitter_ = 0.15f, win_[kMaxHeads] = {}, phase_[kMaxHeads] = {};
    float    drift_ = 0.0f, driftCoeff_ = 0.0f, driftTarget_ = 0.0f, driftNow_ = 0.0f;
    int      jumps_ = 0;
    uint32_t seed_ = 1;
};

struct OnePole { // y += a (x - y)
    float a = 1.0f, y = 0.0f;
    void set(float hz, float sr) { a = 1.0f - std::exp(-6.2831853f * hz / sr); }
    float lp(float x) { return y += a * (x - y); }
    float hp(float x) { return x - lp(x); }
};

// Butterworth low-pass, 8th order (4 biquads, RBJ), for the shimmer's feed (round 3). Shifting up an
// octave doubles every frequency; above 12 kHz that passes 24 kHz and folds back as inharmonic tones,
// what a sample-rate reducer does (owner: "different flavours of bit crushing"; measured round 1-2:
// 13 / 16 / 19 kHz came out at 22 / 16 / 10 kHz, only 8-16 dB down, and again on every trip). At
// 6 kHz: -48 dB at 12 kHz, -68 dB at 16 kHz.
class SteepLowPass {
public:
    void set(float hz, float sampleRate)
    {
        static constexpr float kQ[4] = {0.5098f, 0.6013f, 0.9000f, 2.5629f};
        const float w = 6.2831853f * hz / sampleRate, cw = std::cos(w), sw = std::sin(w);
        for (int k = 0; k < 4; ++k) {
            const float al = sw / (2.0f * kQ[k]), a0 = 1.0f + al;
            b_[k][0] = b_[k][2] = (1.0f - cw) * 0.5f / a0;
            b_[k][1] = (1.0f - cw) / a0;
            a_[k][0] = -2.0f * cw / a0;
            a_[k][1] = (1.0f - al) / a0;
            z_[k][0] = z_[k][1] = 0.0f;
        }
    }
    float process(float x)
    {
        for (int k = 0; k < 4; ++k) { // transposed direct form II
            const float y = b_[k][0] * x + z_[k][0];
            z_[k][0] = b_[k][1] * x - a_[k][0] * y + z_[k][1];
            z_[k][1] = b_[k][2] * x - a_[k][1] * y;
            x = y;
        }
        return x;
    }

private:
    float b_[4][3] = {}, a_[4][2] = {}, z_[4][2] = {};
};

// Shimmer, up or down: in = the tank's wet; out = what goes back into the tank's input.
// Styles (round 2, owner: round 1 "too static ... almost like bit crushing"):
//   Round1:   2 heads, 43 ms, ±15 % jitter (fluttered 14-25 dB at 23 Hz).
//   Smooth:   4 heads, 100 ms, fixed: a clean octave (0.8 dB swing on a sine).
//   Moving:   Smooth + the heads drifting together ±3 ms (slow) + left/right detuned ±2 cents (±6
//             beat ~21 times a second at 3 kHz: roughness again) (BigSky's Mod,
//             Valhalla's random modulators; the Eno/Lanois chain into a modulated Lexicon hall).
//   Evolving: Moving + a darker feed (4.5 kHz, so each higher octave fades faster) and the send
//             breathing slowly (±35 % at ~0.07 Hz) (Costello: loop gain and EQ take it from
//             "steel drum-esque" to "slow attack string pads").
class Shimmer {
public:
    // Round 3: Clean (Smooth's shifter behind SteepLowPass at 6 kHz, then a 9 kHz one-pole so each
    // octave up is a little softer) and CleanMoving (Clean + the slow shared drift). Fed from the wet
    // (each trip climbs) or from the input (one octave, no feedback), Engine decides.
    // CleanDark: Clean at 3.5 kHz into the shifter and 5 kHz after: the splices' smear (each trip
    // spreads every harmonic into its neighbours, worst up high) stays where it reads as a wash.
    enum class Style { Round1, Smooth, Moving, Evolving, Clean, CleanMoving, CleanDark };
    // Clean styles' shifter shape (heads, window seconds), set before prepare (round 3 trials).
    void setShape(int heads, float windowSeconds) { heads_ = heads; window_ = windowSeconds; }
    void prepare(float sampleRate, bool down, Style style = Style::Round1)
    {
        down_ = down;
        style_ = style;
        sr_ = sampleRate;
        const bool r1 = style == Style::Round1;
        clean_ = style == Style::Clean || style == Style::CleanMoving || style == Style::CleanDark;
        const bool dark = style == Style::CleanDark;
        const float lp = down ? 3000.0f : (style == Style::Evolving ? 4500.0f : (dark ? 5000.0f : (clean_ ? 9000.0f : 7000.0f)));
        for (int c = 0; c < 2; ++c) {
            float ratio = down ? 0.5f : 2.0f;
            if (style == Style::Moving || style == Style::Evolving) ratio *= std::exp2((c == 0 ? 2.0f : -2.0f) / 1200.0f);
            const float win = r1 ? (down ? 0.085f : 0.043f) : (clean_ ? window_ : 0.1f);
            shift_[c].prepare(ratio, win * sampleRate, 0x5A1u + 77u * uint32_t(c) + (down ? 9u : 0u),
                              r1 ? 2 : (clean_ ? heads_ : 4), r1 ? 0.15f : 0.0f);
            if (style == Style::Moving || style == Style::Evolving || style == Style::CleanMoving)
                shift_[c].setDrift(0.003f * sampleRate, sampleRate);
            steep_[c].set(dark ? 3500.0f : 6000.0f, sampleRate);
            pre_[c].set(lp, sampleRate);
            pre2_[c].set(lp, sampleRate);
            for (auto& h : post_[c]) h.set(down ? 45.0f : 80.0f, sampleRate);
        }
    }
    void process(float& l, float& r)
    {
        float* io[2] = {&l, &r};
        float breathe = 1.0f;
        if (style_ == Style::Evolving) {
            lfo_ += 0.07f / sr_;
            if (lfo_ >= 1.0f) lfo_ -= 1.0f;
            breathe = 1.0f + 0.35f * std::sin(6.2831853f * lfo_);
        }
        for (int c = 0; c < 2; ++c) {
            float x = *io[c];
            if (clean_) x = pre_[c].lp(shift_[c].process(steep_[c].process(x))); // the 9 kHz one-pole after
            else        x = shift_[c].process(pre2_[c].lp(pre_[c].lp(x)));
            x = post_[c][0].hp(x);
            if (down_) x = post_[c][1].hp(x); // steeper: the second octave down falls away
            *io[c] = breathe * x;
        }
    }

private:
    bool         down_ = false, clean_ = false;
    int          heads_ = 4;
    float        window_ = 0.1f;
    Style        style_ = Style::Round1;
    SteepLowPass steep_[2];
    float        sr_ = 48000.0f, lfo_ = 0.0f;
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
