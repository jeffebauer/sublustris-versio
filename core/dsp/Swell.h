#pragma once
// The swell stage (SPEC §4.5, ADR 0007, 0015): makes the tank's attack a
// slab instead of a hit. The input reaches the tank through 12 taps of a
// long delay, spread over the attack time A with rising gains, so the tank
// is fed a little, then more, then most: the reverb builds over A and then
// decays. Used for BLOOM (A = TIME) in every space and for MASSIVE's gentle
// built-in mass in NORMAL.
//
// The taps span kSpanPerAttack × A, so the wet's 10 → 90 % rise lasts about
// A (SPEC §7 M4; measured: a span of A gives a rise of ~0.8 A). Tap times
// are fixed fractions of the span, jittered so they never line up into
// a rhythm, and each side has its own jitter and signs (decorrelated L/R).
// Total tap energy is 1, so a swell is about as loud as no swell. Below
// kDirectBelowSeconds the taps would bunch into a comb, so the stage hands
// over to the direct path (crossfaded up to kTapsFromSeconds).
#include "dsp/Memory.h"
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

class Swell {
public:
    // 12 taps per side (v1.0.35, ADR 0026; owner's pick on renders/fit_cpu, was 24): half the SDRAM
    // reads, ~12 % of the module's CPU with BLOOM. Their jitter comes from the same draws as the 24 did
    // (kDraws a side, the first kTaps used), so it is the version the owner heard.
    static constexpr int   kTaps = 12;
    static constexpr int   kDraws = 24;
#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (overnight review, 26 Oct 2026): the points per side, kTaps (as built) .. kDraws (24:
    // the version before v1.0.35, the same draws and spacing), set before prepare() lays them out.
    static constexpr int kTapSlots = kDraws;
    void setTapCount(int n) { taps_ = n < 1 ? kTaps : (n > kDraws ? kDraws : n); }
    int  tapCount() const { return taps_; }
#else
    static constexpr int kTapSlots = kTaps;
#endif
    static constexpr float kMaxSeconds = 7.6f; // BLOOM's 4 s × MASSIVE's mass 1.5 × kSpanPerAttack, plus jitter
    static constexpr float kDirectBelowSeconds = 0.04f;
    static constexpr float kTapsFromSeconds = 0.12f;
    static constexpr float kSpanPerAttack = 1.25f;

    // BLOOM's smear (the audit's S1; owner's pick on renders/swell_smear, v1.0.16): three allpasses
    // per side after the taps, each a multiple of the tap spacing, so every tap is smeared across
    // the gap to the next instead of returning as a ghost hit.
    static constexpr int   kSmearStages = 3;
    static constexpr float kSmearRatio[kSmearStages] = {0.53f, 0.79f, 1.17f};
    static constexpr float kSmearMaxSeconds = kMaxSeconds / kTaps * 1.2f;
    static constexpr float kSmearSpanScale = 0.95f;
    static constexpr float kSmearGain = 0.5f;
    static constexpr size_t memoryFloats(float sampleRate)
    {
        return 2 * nextPow2(size_t(kMaxSeconds * sampleRate) + 4)
             + 2 * kSmearStages * nextPow2(size_t(kSmearMaxSeconds * sampleRate) + 4);
    }

    IV_SIZE_OPT bool prepare(Arena& arena, float sampleRate) // set-up: built for size (flash)
    {
        sampleRate_ = sampleRate;
        bool ok = line_[0].init(arena, size_t(kMaxSeconds * sampleRate)) && line_[1].init(arena, size_t(kMaxSeconds * sampleRate));
        for (auto& side : smear_)
            for (auto& ap : side) ok = ok && ap.init(arena, size_t(kSmearMaxSeconds * sampleRate));
        // Fixed jitter and signs from a seeded generator (deterministic).
        uint32_t s = seed_;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kDraws; ++k) {
                s = s * 1664525u + 1013904223u;
                if (k >= taps_) continue;
                const float jitter = float(s >> 8) * (1.0f / 16777216.0f) - 0.5f; // ±half a slot
                frac_[c][k] = (float(k) + 0.5f + 0.8f * jitter) / float(taps_);
                sign_[c][k] = (s & 0x80000000u) ? -1.0f : 1.0f;
            }
        setSCurve(sCurve_);
        reset();
        return ok;
    }

    // The shape of the taps' rise, x = 0..1 over the span: linear (the tank's), or an
    // S-curve (raised cosine) that eases in and settles into place, MASSIVE's twin
    // (owner, renders/swell_curve: B on all 18 cards over linear, crescendo, RC).
    IV_SIZE_OPT void setSCurve(bool on)
    {
        sCurve_ = on;
        float energy = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < taps_; ++k) {
                const float x = (float(k) + 1.0f) / float(taps_);
                const float g = sCurve_ ? 0.5f - 0.5f * std::cos(3.14159265f * x) : x; // rising: the slab builds
                gain_[c][k] = g;
                if (c == 0) energy += g * g;
            }
        const float norm = 1.0f / std::sqrt(energy);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < taps_; ++k) gain_[c][k] *= norm * sign_[c][k];
    }

    void reset()
    {
        line_[0].clear();
        line_[1].clear();
        for (auto& side : smear_)
            for (auto& ap : side) ap.clear();
        attackNow_ = attackTarget_;
    }

    // PROTOTYPE (renders/swell_seed): the jitter and signs' seed, before prepare() lays the taps out.
    void setSeed(uint32_t s) { seed_ = s ? s : 0x5EE11u; }

    // The attack time in seconds; glides (so TIME moves don't jump the taps).
    void setAttackSeconds(float a, bool snap = false)
    {
        const float maxA = (kMaxSeconds - 0.1f) / kSpanPerAttack;
        attackTarget_ = a < 0.0f ? 0.0f : (a > maxA ? maxA : a);
        if (snap) attackNow_ = attackTarget_;
    }

    // Once per control chunk of n samples (built for size: flash).
    IV_SIZE_OPT void control(int n)
    {
        const float c = 1.0f - std::exp(-float(n) / (0.15f * sampleRate_)); // 150 ms glide
        attackNow_ += c * (attackTarget_ - attackNow_);
        smearG_ += c * (smearTarget_ - smearG_); // glides too, so switching it never clicks
        if (smearTarget_ == 0.0f && smearG_ < 1e-4f) smearG_ = 0.0f;
        const float a = attackNow_;
        tapMix_ = a <= kDirectBelowSeconds ? 0.0f
                : (a >= kTapsFromSeconds ? 1.0f : (a - kDirectBelowSeconds) / (kTapsFromSeconds - kDirectBelowSeconds));
        // The smear's allpasses add about a tenth of the span of their own delay, so the taps
        // close up by that much and the swell lands when it did unsmeared (owner: "the attack is a
        // little slower").
        const float span = kSpanPerAttack * a * sampleRate_ * (1.0f - (1.0f - kSmearSpanScale) * smearG_ / kSmearGain);
        const float maxLen = kSmearMaxSeconds * sampleRate_;
        for (int j = 0; j < kSmearStages; ++j) {
            const float len = span / float(taps_) * kSmearRatio[j];
            smearLen_[j] = len < 1.0f ? 1u : uint32_t(len > maxLen ? maxLen : len);
        }
        for (int c2 = 0; c2 < 2; ++c2)
            for (int k = 0; k < taps_; ++k) {
                const float d = 1.0f + frac_[c2][k] * span;
                tapDelay_[c2][k] = d;
            }
    }

    static constexpr int kMaxBlock = 64;

    // A block of n <= kMaxBlock samples in place, the same as n calls of process() (the same sums in
    // the same order, so the same output to the bit), but tap by tap: each tap reads its n samples in
    // a row instead of every tap hopping to its own place in SDRAM each sample. Per sample, the 48
    // taps' hops were ~47 % of the module's CPU (M3 round 2, 25 Oct 2026). The taps' delays only
    // change at control(), once a chunk.
    void processBlock(float* l, float* r, int n)
    {
        for (int s = 0; s < n; ++s) {
            line_[0].write(l[s]);
            line_[1].write(r[s]);
        }
        if (tapMix_ <= 0.0f) return;
        float sum[2][kMaxBlock];
        float* io[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            for (int s = 0; s < n; ++s) sum[c][s] = 0.0f;
            const DelayLine& line = line_[c];
            for (int k = 0; k < taps_; ++k) {
                const float    d  = tapDelay_[c][k];
                const auto     di = uint32_t(d);
                const float    t  = d - float(di), g = gain_[c][k];
                // Sample s was written n-1-s writes before the last: its read sits that much further back.
                const uint32_t i0 = line.pos - (di < 1 ? 1u : di) - uint32_t(n - 1), i1 = line.pos - (di + 1) - uint32_t(n - 1);
                if (line.full())
                    for (int s = 0; s < n; ++s) {
                        const float a0 = line.buf[(i0 + uint32_t(s)) & line.mask], a1 = line.buf[(i1 + uint32_t(s)) & line.mask];
                        sum[c][s] += g * (a0 + t * (a1 - a0));
                    }
                else // since a clear(): what's older reads as silence (DelayLine::ago)
                    for (int s = 0; s < n; ++s) {
                        const uint32_t back = uint32_t(n - 1 - s);
                        const float a0 = line.ago((di < 1 ? 1u : di) + back), a1 = line.ago(di + 1 + back);
                        sum[c][s] += g * (a0 + t * (a1 - a0));
                    }
            }
        }
        for (int s = 0; s < n; ++s) {
            float v2[2] = {sum[0][s], sum[1][s]};
            if (smearG_ > 0.0f)
                for (int c = 0; c < 2; ++c)
                    for (int j = 0; j < kSmearStages; ++j) {
                        const float d = smear_[c][j].read(smearLen_[j]);
                        const float v = v2[c] + smearG_ * d;
                        smear_[c][j].write(v);
                        v2[c] = d - smearG_ * v;
                    }
            for (int c = 0; c < 2; ++c) io[c][s] += tapMix_ * (v2[c] - io[c][s]);
        }
    }

    void process(float& l, float& r)
    {
        line_[0].write(l);
        line_[1].write(r);
        if (tapMix_ <= 0.0f) return;
        float sum[2] = {0.0f, 0.0f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < taps_; ++k) {
                const float d  = tapDelay_[c][k];
                const auto  di = uint32_t(d);
                const float t  = d - float(di);
                const float a0 = line_[c].read(di < 1 ? 1 : di), a1 = line_[c].read(di + 1);
                sum[c] += gain_[c][k] * (a0 + t * (a1 - a0));
            }
        if (smearG_ > 0.0f)
            for (int c = 0; c < 2; ++c)
                for (int j = 0; j < kSmearStages; ++j) {
                    const float d = smear_[c][j].read(smearLen_[j]);
                    const float v = sum[c] + smearG_ * d;
                    smear_[c][j].write(v);
                    sum[c] = d - smearG_ * v;
                }
        l += tapMix_ * (sum[0] - l);
        r += tapMix_ * (sum[1] - r);
    }

private:
    float     sampleRate_ = 48000.0f;
    DelayLine line_[2];
    float     frac_[2][kTapSlots] = {}, sign_[2][kTapSlots] = {}, gain_[2][kTapSlots] = {}, tapDelay_[2][kTapSlots] = {};
#ifndef IV_FIXED_VOICINGS
    int       taps_ = kTaps; // setTapCount (a prototype)
#else
    static constexpr int taps_ = kTaps;
#endif
    uint32_t  seed_ = 0x5EE11u; // the jitter and signs' generator (setSeed, a prototype)
    float     attackNow_ = 0.0f, attackTarget_ = 0.0f, tapMix_ = 0.0f;
    bool      sCurve_ = false;
    DelayLine smear_[2][kSmearStages];
    uint32_t  smearLen_[kSmearStages] = {1, 1, 1};
    float     smearG_ = 0.0f, smearTarget_ = 0.0f;

public:
    // BLOOM smears its taps (kSmearGain); MASSIVE's mass and the twin's swell don't.
    void setSmear(bool on) { smearTarget_ = on ? kSmearGain : 0.0f; }
};

} // namespace iv::dsp
