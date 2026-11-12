#pragma once
// HALL's and MASSIVE's freeze twin (v1.0.43, the owner's pick E on renders/small_room, "room softer", on every
// card; ADR 0009): a small room in place of a second big tank. What you play over a frozen pad goes into it, heard
// as a quieter layer over the pad, fading within kTwinMaxT60. PLATE keeps its own twin (a second Plate).
//
// After Undulae Music's Tinyspace (short lines, allpasses at the input, one feedback, the sides crossfed; ideas
// only, no code taken): a 2nd-order Butterworth high-pass at 150 Hz on its input (the room's lows out: the owner
// found the unvoiced room "a little boomy and low-end heavy"), two allpasses a side, then eight short lines (SIZE:
// 8-50 ms at the longest) mixed by a Hadamard matrix. Each line's loss is the twin tank's own (absorbent(): DECAY's
// T60, at most kTwinMaxT60, the highs' T60 TONE's share of the lows', × kTwinRoomBrightness: the top a little
// softer than the tank's), and its read wanders slowly on a rotating phasor (no sin a sample), by 0.5 % of
// the line's length (v1.0.47). Run at 24 kHz,
// inside the twin's HalfRate stage. It keeps the SIZE the pad froze with (the Engine; v1.0.80, ADR 0056): SIZE never
// bends its lines, so they never glide. Its rings (~42 KB at 24 kHz) come from the caller: AXI SRAM on the module.
//
// Adds its output into outL / outR (the HalfRate "tank" contract) and takes n <= 64 samples a call.
#include "dsp/Governor.h"
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace iv::dsp {

class TwinRoom {
public:
    static constexpr int kN = 8;
    static constexpr float kLowCutHz = 150.0f;   // renders/small_room C and E
    static constexpr float kBrightness = 0.6f;   // E: the highs' T60 share × this ("room softer")

    // Floats of ring memory prepare() takes at this rate.
    static constexpr size_t memoryFloats(float sampleRate)
    {
        size_t n = size_t(kN) * lineFloats(sampleRate);
        for (const auto& side : kApSeconds)
            for (float s : side) n += size_t(int(s * sampleRate)) + 2;
        return n;
    }

    // mem: memoryFloats(sampleRate) floats (zeroed here). False (and silent) without it.
    IV_SIZE_OPT bool prepare(float sampleRate, uint32_t seed, float* mem)
    {
        sr_ = sampleRate;
        ok_ = mem != nullptr;
        mem_ = mem;
        memFloats_ = mem ? int(memoryFloats(sampleRate)) : 0;
        clearedTo_ = 0; // (zeroed before the first process(): reset() below)
        float* p = mem;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) {
                apLen_[c][k] = int(kApSeconds[c][k] * sampleRate);
                inAp_[c][k].attach(p, apLen_[c][k] + 2);
                if (p) p += apLen_[c][k] + 2;
            }
        uint32_t s = seed;
        for (int i = 0; i < kN; ++i) {
            line_[i].attach(p, int(lineFloats(sampleRate)));
            if (p) p += lineFloats(sampleRate);
            const float ph = 6.2831853f * rnd(s), w = 6.2831853f * (0.21f + 0.37f * rnd(s)) / sampleRate;
            c0_[i] = std::cos(ph);
            s0_[i] = std::sin(ph);
            rc_[i] = std::cos(w);
            rs_[i] = std::sin(w);
        }
        // The low cut: a 2nd-order Butterworth high-pass (bilinear, as the still lows' split).
        const float k = bilinearK(kLowCutHz, sampleRate), nrm = 1.0f / (1.0f + 1.41421356f * k + k * k);
        hb_ = nrm;
        ha1_ = 2.0f * (k * k - 1.0f) * nrm;
        ha2_ = (1.0f - 1.41421356f * k + k * k) * nrm;
        shelfK_ = lossShelfK(sampleRate);
        reset();
        set(0.5f, 1.0f / 8.0f, voicing::kDampingHighT60Ratio);
        return ok_;
    }
    // The rings restart at 0 and must read as silence. Zeroing them all here (42 KB of D2 SRAM, ~10,500
    // stores, every cache line it passed evicted) was most of a SPACE flip's restart block on the module (the
    // flip section ~23 % of a block, the tank's reads after it missing; CPU save 2, 28 Oct 2026). Now the
    // zeroing waits: clearSome() does it a slice a block while the room rests (Engine), and the first
    // process() after the reset finishes whatever is left. The same zeros before the first read, so the
    // same output to the bit.
    IV_SIZE_OPT void reset()
    {
        for (auto& side : inAp_)
            for (auto& a : side) a.pos = 0;
        for (auto& l : line_) l.pos = 0;
        fresh_ = true;
        for (int i = 0; i < kN; ++i) ay_[i] = ax_[i] = ap_[i] = 0.0f;
        for (auto& c : hz_)
            for (auto& z : c) z = 0.0f;
    }
    void setGain(float g) { gain_ = g; }
    // size 0..1 (SIZE), rate 1/T60 in 1/s (the twin's), highRatio the twin's TONE share (its highs' T60 over its lows').
    IV_SIZE_OPT void set(float size, float rate, float highRatio)
    {
        const float longest = (0.008f + 0.042f * size) * sr_;
        const float ratio = highRatio * kBrightness;
        for (int i = 0; i < kN; ++i) {
            target_[i] = std::fmax(8.0f, kRatio[i] * longest) + float(i);
            len_[i] = target_[i]; // (a new size only ever on the first set() after a reset: the Engine keeps the room's size)
            ab_[i] = absorbent(target_[i] / sr_, rate, ratio < 0.02f ? 0.02f : ratio, 0.0f, shelfK_);
        }
    }
    // Built for size (IV_SIZE_OPT, firmware only): at -O3 it was 2,808 B of flash, at -Os 1.4 KB less (the
    // integration dry run's candidate profile measured its CPU).
    // Zeroes up to `floats` more of the rings' memory (only between process() calls that a reset() separates:
    // the Engine calls it while the twin rests, and a twin starts with a reset()). True once all of it is zero.
    IV_SIZE_OPT bool clearSome(int floats)
    {
        const int end = memFloats_ - clearedTo_ > floats ? clearedTo_ + floats : memFloats_;
        for (int i = clearedTo_; i < end; ++i) mem_[i] = 0.0f;
        clearedTo_ = end;
        return clearedTo_ >= memFloats_;
    }
    bool cleared() const { return clearedTo_ >= memFloats_; }
    float lineLength(int i) const { return len_[i]; } // (tests)

    IV_SIZE_OPT void process(const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        if (!ok_) return;
        if (fresh_) { // the first call since a reset(): the rest of the zeroing (none if the rest did it all)
            clearSome(memFloats_);
            fresh_ = false;
        }
        clearedTo_ = 0; // written from here on
        constexpr float kNorm = 0.35355339f;
        constexpr float kInS[2][kN] = {{1, -1, 1, -1, 1, -1, 1, -1}, {1, 1, -1, -1, 1, 1, -1, -1}};
        constexpr float kOutS[2][kN] = {{1, 1, -1, -1, 1, 1, -1, -1}, {1, -1, -1, 1, 1, -1, -1, 1}};
        int   di[kN];
        float eta[kN], dep[kN];
        for (int i = 0; i < kN; ++i) { // keep the phasors on the unit circle (a first-order step, once a call)
            // The wander's depth, a share of the line's own length (v1.0.47, owner's pick B on renders/room_motion: 0.15 ms
            // on every line bent the short ones most, the bend building up pass after pass; scaled, it stays a few cents
            // at every SIZE). Fixed for the call, as the read.
            dep[i] = kWanderShare * len_[i];
            const float r = 1.5f - 0.5f * (c0_[i] * c0_[i] + s0_[i] * s0_[i]);
            c0_[i] *= r;
            s0_[i] *= r;
            // The read, fixed for the call (n <= 33 samples; the wander moves it ~0.01 sample in that time), through
            // a first-order allpass for its fraction, as the tank's reads: linear interpolation dulled every pass.
            const float d = len_[i] + dep[i] * (1.0f + s0_[i]);
            di[i] = int(d);
            const float t = d - float(di[i]);
            eta[i] = (1.0f - t) / (1.0f + t);
        }
        for (int t = 0; t < n; ++t) {
            float x[2] = {inL[t], inR[t]};
            for (int c = 0; c < 2; ++c) {
                { // the low cut, transposed direct form II
                    const float y = hb_ * x[c] + hz_[c][0];
                    hz_[c][0] = -2.0f * hb_ * x[c] - ha1_ * y + hz_[c][1];
                    hz_[c][1] = hb_ * x[c] - ha2_ * y;
                    x[c] = y;
                }
                for (int k = 0; k < 2; ++k) {
                    const float d = inAp_[c][k].at(apLen_[c][k]);
                    const float w = x[c] + kApCoeff * d;
                    x[c] = d - kApCoeff * w;
                    inAp_[c][k].write(w);
                }
            }
            float y[kN];
            for (int j = 0; j < kN; ++j) {
                const float c = c0_[j];
                c0_[j] = c * rc_[j] - s0_[j] * rs_[j];
                s0_[j] = s0_[j] * rc_[j] + c * rs_[j];
                const float r = eta[j] * line_[j].at(di[j]) + line_[j].at(di[j] + 1) - eta[j] * ap_[j];
                ap_[j] = r;
                const float a = ab_[j].gain * r + ab_[j].b1 * ax_[j] + ab_[j].pole * ay_[j]; // the tank's loss shelf
                ax_[j] = r;
                ay_[j] = a;
                y[j] = a;
            }
            float l = 0.0f, rr = 0.0f;
            for (int j = 0; j < kN; ++j) {
                l += kOutS[0][j] * y[j];
                rr += kOutS[1][j] * y[j];
            }
            outL[t] += gain_ * kNorm * l;
            outR[t] += gain_ * kNorm * rr;
            const float a0 = y[0] + y[1], a1 = y[0] - y[1], a2 = y[2] + y[3], a3 = y[2] - y[3];
            const float a4 = y[4] + y[5], a5 = y[4] - y[5], a6 = y[6] + y[7], a7 = y[6] - y[7];
            const float b0 = a0 + a2, b2 = a0 - a2, b1 = a1 + a3, b3 = a1 - a3;
            const float b4 = a4 + a6, b6 = a4 - a6, b5 = a5 + a7, b7 = a5 - a7;
            const float h[kN] = {b0 + b4, b1 + b5, b2 + b6, b3 + b7, b0 - b4, b1 - b5, b2 - b6, b3 - b7};
            for (int j = 0; j < kN; ++j) line_[j].write(kNorm * h[j] + kNorm * (kInS[0][j] * x[0] + kInS[1][j] * x[1]));
        }
    }

private:
    // An exact-length ring on memory from the caller: read d samples back (1 <= d < len).
    struct Ring {
        float* buf IV_ZI;
        int    pos IV_ZI, len IV_ZI;
        void   attach(float* b, int n)
        {
            buf = b;
            len = n;
            pos = 0;
        }
        float at(int d) const
        {
            int i = pos - d;
            if (i < 0) i += len;
            return buf[i];
        }
        void write(float x)
        {
            buf[pos] = x;
            if (++pos == len) pos = 0;
        }
    };
    static float rnd(uint32_t& s)
    {
        s = s * 1664525u + 1013904223u;
        return float(s >> 8) * (1.0f / 16777216.0f); // 0..1
    }
    static constexpr size_t lineFloats(float sampleRate) { return size_t(int(kMaxSeconds * sampleRate) + 16); }

    static constexpr float kMaxSeconds = 0.052f, kApCoeff = 0.6f;
    static constexpr float kApSeconds[2][2] = {{0.0033f, 0.0059f}, {0.0039f, 0.0067f}};
    static constexpr float kWanderShare = 0.005f; // the wander's depth x the line's length (v1.0.46's 0.15 ms on a SIZE 0.9 line)
    static constexpr float kRatio[kN] = {0.31f, 0.42f, 0.51f, 0.60f, 0.70f, 0.79f, 0.90f, 1.0f};
    // IV_ZI (SizeOpt.h): a member of the Engine only, never a local; prepare() sets the rest.
    float sr_ IV_RI(24000.0f), gain_ = 1.0f, shelfK_ IV_ZI;
    bool  ok_ IV_ZI, fresh_ IV_RI(true);
    float* mem_ IV_ZI; // the rings' memory (prepare()): memFloats_ floats, [0, clearedTo_) zero since the last process()
    int   memFloats_ IV_ZI, clearedTo_ IV_ZI;
    float target_[kN] IV_ZI;
    float hb_ IV_ZI, ha1_ IV_ZI, ha2_ IV_ZI, hz_[2][2] IV_ZI;
    Ring  line_[kN];
    Ring  inAp_[2][2];
    int   apLen_[2][2] IV_ZI;
    float len_[kN] IV_ZI, ay_[kN] IV_ZI, ax_[kN] IV_ZI, ap_[kN] IV_ZI;
    float c0_[kN] IV_ZI, s0_[kN] IV_ZI, rc_[kN] IV_ZI, rs_[kN] IV_ZI;
    Absorb ab_[kN] = {};
};

} // namespace iv::dsp
