#pragma once
// PROTOTYPES (renders/twin_alternatives; owner, 26 Oct 2026: "a page with today's twin, blur and width, small
// room, and the twin without its swell, all at -8 dB"): two cheaper things to play over a frozen pad instead of
// the twin's second big tank, sized for on-chip memory (AXI SRAM / DTCM) on the module. Desktop only
// (Engine::setTwinAltPrototype); ideas from the literature, no code taken:
//
// TwinBlur: no loop and no tail of its own. Three allpasses a side (20-55 ms, different lengths left and right;
// the first one's read wanders slowly, the "smear" that keeps a chain of allpasses from ringing metallic, as
// in Mutable Instruments' reverbs and Dattorro's modulated input allpass), then a velvet-noise decorrelator a
// side (Alary, Politis and Välimäki, "Velvet-noise decorrelator", DAFx-17: ~24 sparse +/-1 taps over 25 ms,
// decaying by 20 dB) for width. At 48 kHz.
//
// TwinRoom: a small room (after Undulae Music's Tinyspace: short lines, allpasses at the input, one feedback,
// the sides crossfed): two allpasses a side at the input, then eight short lines (SIZE: 8-50 ms at the
// longest) mixed by a Hadamard matrix, each with its own Jot loss for the twin's T60 (at most 8 s), a one-pole
// lowpass and a slow wander of its read. At the rate it's run at (24 kHz inside the twin's HalfRate).
//
// Both add their output into outL / outR (the HalfRate "tank" contract) and take n <= 64 samples a call.
#include <cmath>
#include <cstdint>

namespace iv::dsp {

namespace twinalt {
inline float rnd(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return float(s >> 8) * (1.0f / 16777216.0f); // 0..1
}
// An exact-length ring: read d samples back (d >= 1, fractional by linear interpolation).
template <int kCap>
struct Ring {
    float buf[kCap] = {};
    int   pos = 0, len = kCap;
    void  clear()
    {
        for (auto& v : buf) v = 0.0f;
        pos = 0;
    }
    float at(int d) const
    {
        int i = pos - d;
        if (i < 0) i += len;
        return buf[i];
    }
    float read(float d) const
    {
        const int di = int(d);
        const float t = d - float(di);
        return at(di) + t * (at(di + 1) - at(di));
    }
    void write(float x)
    {
        buf[pos] = x;
        if (++pos == len) pos = 0;
    }
};
} // namespace twinalt

class TwinBlur {
public:
    static constexpr int kStages = 3, kTaps = 24;
    void prepare(float sampleRate, uint32_t seed)
    {
        sr_ = sampleRate;
        const float secs[2][kStages] = {{0.0231f, 0.0377f, 0.0533f}, {0.0269f, 0.0413f, 0.0479f}};
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < kStages; ++k) {
                len_[c][k] = float(int(secs[c][k] * sampleRate));
                ap_[c][k].len = int(len_[c][k]) + 8 + (k == 0 ? int(2.0f * kSmearSeconds * sampleRate) + 2 : 0); // the smear's reach
            }
            velvet_[c].len = int(kVelvetSeconds * sampleRate) + 2;
            uint32_t s = seed + 977u * uint32_t(c);
            float norm = 0.0f;
            for (int k = 0; k < kTaps; ++k) {
                const float slot = kVelvetSeconds * sampleRate / float(kTaps);
                tapPos_[c][k] = 1 + int((float(k) + 0.5f + 0.8f * (rnd(s) - 0.5f)) * slot);
                const float g = std::pow(10.0f, -1.0f * float(k) / float(kTaps)); // -20 dB across
                tapGain_[c][k] = rnd(s) < 0.5f ? -g : g;
                norm += g * g;
            }
            for (auto& g : tapGain_[c]) g /= std::sqrt(norm);
        }
        reset();
    }
    void reset()
    {
        for (auto& side : ap_)
            for (auto& a : side) a.clear();
        for (auto& v : velvet_) v.clear();
        phase_ = 0.0f;
    }
    void setGain(float g) { gain_ = g; }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float* in[2] = {inL, inR};
        float* out[2] = {outL, outR};
        const float dPhase = 6.2831853f * kSmearHz / sr_, depth = kSmearSeconds * sr_;
        for (int i = 0; i < n; ++i) {
            phase_ += dPhase;
            if (phase_ > 6.2831853f) phase_ -= 6.2831853f;
            const float wob = std::sin(phase_);
            for (int c = 0; c < 2; ++c) {
                float x = in[c][i];
                for (int k = 0; k < kStages; ++k) {
                    const float dl = len_[c][k] + (k == 0 ? depth * (1.0f + (c ? -wob : wob)) : 0.0f);
                    const float d = ap_[c][k].read(dl);
                    const float w = x + kCoeff * d;
                    x = d - kCoeff * w;
                    ap_[c][k].write(w);
                }
                velvet_[c].write(x);
                float y = 0.0f;
                for (int k = 0; k < kTaps; ++k) y += tapGain_[c][k] * velvet_[c].at(tapPos_[c][k]);
                out[c][i] += gain_ * y;
            }
        }
    }

private:
    static float rnd(uint32_t& s) { return twinalt::rnd(s); }
    static constexpr float kCoeff = 0.6f, kSmearHz = 0.31f, kSmearSeconds = 0.0012f, kVelvetSeconds = 0.025f;
    float sr_ = 48000.0f, phase_ = 0.0f, gain_ = 1.0f;
    float len_[2][kStages] = {};
    twinalt::Ring<2700> ap_[2][kStages]; // the longest, 55 ms at 48 kHz, + the smear
    twinalt::Ring<1280> velvet_[2];
    int   tapPos_[2][kTaps] = {};
    float tapGain_[2][kTaps] = {};
};

class TwinRoom {
public:
    static constexpr int kN = 8;
    void prepare(float sampleRate, uint32_t seed)
    {
        sr_ = sampleRate;
        const float apSecs[2][2] = {{0.0033f, 0.0059f}, {0.0039f, 0.0067f}};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) {
                apLen_[c][k] = int(apSecs[c][k] * sampleRate);
                inAp_[c][k].len = apLen_[c][k] + 2;
            }
        uint32_t s = seed;
        for (int i = 0; i < kN; ++i) {
            line_[i].len = int(kMaxSeconds * sampleRate) + 16;
            modPhase_[i] = 6.2831853f * twinalt::rnd(s);
            modStep_[i] = 6.2831853f * (0.21f + 0.37f * twinalt::rnd(s)) / sampleRate;
        }
        reset();
        set(0.5f, 1.0f / 8.0f);
    }
    void reset()
    {
        for (auto& side : inAp_)
            for (auto& a : side) a.clear();
        for (auto& l : line_) l.clear();
        for (auto& z : lp_) z = 0.0f;
    }
    void setGain(float g) { gain_ = g; }
    // size 0..1 (SIZE), rate 1/T60 in 1/s (the twin's: at least 1/8 s).
    void set(float size, float rate)
    {
        const float longest = (0.008f + 0.042f * size) * sr_;
        for (int i = 0; i < kN; ++i) {
            len_[i] = std::fmax(8.0f, kRatio[i] * longest) + float(i); // distinct even when tiny
            loopGain_[i] = std::pow(10.0f, -3.0f * len_[i] * rate / sr_);
        }
    }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        constexpr float kNorm = 0.35355339f;
        constexpr float kInS[2][kN] = {{1, -1, 1, -1, 1, -1, 1, -1}, {1, 1, -1, -1, 1, 1, -1, -1}};
        constexpr float kOutS[2][kN] = {{1, 1, -1, -1, 1, 1, -1, -1}, {1, -1, -1, 1, 1, -1, -1, 1}};
        const float depth = kModSeconds * sr_;
        for (int i = 0; i < n; ++i) {
            float x[2] = {inL[i], inR[i]};
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 2; ++k) {
                    const float d = inAp_[c][k].at(apLen_[c][k]);
                    const float w = x[c] + kApCoeff * d;
                    x[c] = d - kApCoeff * w;
                    inAp_[c][k].write(w);
                }
            float y[kN];
            for (int j = 0; j < kN; ++j) {
                modPhase_[j] += modStep_[j];
                if (modPhase_[j] > 6.2831853f) modPhase_[j] -= 6.2831853f;
                const float r = loopGain_[j] * line_[j].read(len_[j] + depth * (1.0f + std::sin(modPhase_[j])));
                lp_[j] += (1.0f - kDamp) * (r - lp_[j]);
                y[j] = lp_[j];
            }
            float l = 0.0f, rr = 0.0f;
            for (int j = 0; j < kN; ++j) {
                l += kOutS[0][j] * y[j];
                rr += kOutS[1][j] * y[j];
            }
            outL[i] += gain_ * kNorm * l;
            outR[i] += gain_ * kNorm * rr;
            const float a0 = y[0] + y[1], a1 = y[0] - y[1], a2 = y[2] + y[3], a3 = y[2] - y[3];
            const float a4 = y[4] + y[5], a5 = y[4] - y[5], a6 = y[6] + y[7], a7 = y[6] - y[7];
            const float b0 = a0 + a2, b2 = a0 - a2, b1 = a1 + a3, b3 = a1 - a3;
            const float b4 = a4 + a6, b6 = a4 - a6, b5 = a5 + a7, b7 = a5 - a7;
            const float h[kN] = {b0 + b4, b1 + b5, b2 + b6, b3 + b7, b0 - b4, b1 - b5, b2 - b6, b3 - b7};
            for (int j = 0; j < kN; ++j) line_[j].write(kNorm * h[j] + kNorm * (kInS[0][j] * x[0] + kInS[1][j] * x[1]));
        }
    }

private:
    static constexpr float kMaxSeconds = 0.052f, kApCoeff = 0.6f, kDamp = 0.35f, kModSeconds = 0.00015f;
    static constexpr float kRatio[kN] = {0.31f, 0.42f, 0.51f, 0.60f, 0.70f, 0.79f, 0.90f, 1.0f};
    float sr_ = 24000.0f, gain_ = 1.0f;
    twinalt::Ring<1360> line_[kN]; // 52 ms at 24 kHz + the wander
    twinalt::Ring<200>  inAp_[2][2];
    int   apLen_[2][2] = {};
    float len_[kN] = {}, loopGain_[kN] = {}, lp_[kN] = {}, modPhase_[kN] = {}, modStep_[kN] = {};
};

} // namespace iv::dsp
