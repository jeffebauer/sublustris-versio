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
#include "params/SpaceVoicing.h"

#include <cmath>
#include <utility>
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
        glideN_[0] = glideN_[1] = 0;
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

    IV_SIZE_OPT void reset()
    {
        line_[0].clear();
        line_[1].clear();
        for (auto& side : smear_)
            for (auto& ap : side) ap.clear();
        attackNow_ = attackTarget_; // (a TIME crossfade running carries on to the taps' new places)
    }

    // PROTOTYPE (renders/swell_seed): the jitter and signs' seed, before prepare() lays the taps out.
    void setSeed(uint32_t s) { seed_ = s ? s : 0x5EE11u; }

    // The attack time in seconds. It moves by crossfade (ADR 0025's phase 2): once it has rested
    // kSizeRestSeconds the taps jump to their new places one after another, each fading from its old
    // read to its new over a twelfth of kSizeCrossfadeSeconds (equal power), and the smear's allpasses
    // read from their old lengths' to their new ones' over the whole of it. Glided, the taps swept
    // across seconds of their line at up to ~9x speed (a TIME move in BLOOM: scrambled, sped-up sound
    // into the tank, +11-14 dB over 3 kHz while it moved, +14 dB under 100 Hz left in an endless tail;
    // owner, v1.0.52: "noise ... which carries into the tail"), and the smear's lengths stepped once a
    // chunk. One tap at a time: one extra read a side while it runs (all twelve at once would double
    // the swell's reads, ~12 % of the module's CPU).
    void setAttackSeconds(float a, bool snap = false)
    {
        const float maxA = (kMaxSeconds - 0.1f) / kSpanPerAttack;
        attackTarget_ = a < 0.0f ? 0.0f : (a > maxA ? maxA : a);
        if (snap) attackNow_ = attackTarget_; // (a crossfade running carries on to the taps' new places)
    }

    // Once per control chunk of n samples, before processBlock() of the same n (built for size, out of
    // line: the tank's swell and the twin's share one copy; flash).
    IV_NOINLINE IV_SIZE_OPT void control(int n)
    {
        if (n != glideN_[0]) { // 150 ms glide; its std::exp kept for the last two chunk lengths (CPU save 2)
            std::swap(glideN_[0], glideN_[1]);
            std::swap(glideK_[0], glideK_[1]);
            if (n != glideN_[0]) {
                glideN_[0] = n;
                glideK_[0] = 1.0f - std::exp(-float(n) / (0.15f * sampleRate_));
            }
        }
        const float c = glideK_[0];
        smearG_ += c * (smearTarget_ - smearG_); // glides too, so switching it never clicks
        if (smearTarget_ == 0.0f && smearG_ < 1e-4f) smearG_ = 0.0f;
        // The TIME crossfade (setAttackSeconds): the next tap once the last one's fade is done (none after
        // the last); a start once the attack has rested (no move over half a sample at the longest tap
        // for kSizeRestSeconds), none is running, and the taps would move.
        const int seg = int(voicing::kSizeCrossfadeSeconds * sampleRate_) / taps_; // each tap's fade
        int  xk = xfTap_ - 1; // the fading tap (-1: none)
        bool next = xk >= 0 && xfPos_ >= (xk + 1) * seg;
        if (next && ++xk >= taps_) xk = -1, next = false;
        const float halfSample = 0.5f / (kSpanPerAttack * sampleRate_); // of attack, at the longest tap
        restSamples_ = std::fabs(attackTarget_ - attackLast_) > halfSample ? 0 : restSamples_ + n;
        attackLast_ = attackTarget_;
        if (xk < 0 && restSamples_ >= int(voicing::kSizeRestSeconds * sampleRate_) && std::fabs(attackTarget_ - attackNow_) > halfSample) {
            attackNow_ = attackTarget_;
            xfPos_ = xk = 0;
            next = true;
        }
        xfTap_ = xk + 1;
        const bool fading = xk >= 0;
        const float a = attackNow_;
        // The smear's allpasses add about a tenth of the span of their own delay, so the taps
        // close up by that much and the swell lands when it did unsmeared (owner: "the attack is a
        // little slower").
        const float span = kSpanPerAttack * a * sampleRate_ * (1.0f - (1.0f - kSmearSpanScale) * smearG_ / kSmearGain);
        const float maxLen = kSmearMaxSeconds * sampleRate_;
        for (int j = 0; j < kSmearStages; ++j) {
            const float len = span / float(taps_) * kSmearRatio[j];
            smearLen_[j] = len < 1.0f ? 1u : uint32_t(len > maxLen ? maxLen : len);
            if (!fading) smearOld_[j] = smearLen_[j]; // crossfading: from the lengths it started at
        }
        for (int c2 = 0; c2 < 2; ++c2)
            for (int k = 0; k < taps_; ++k) {
                const float d = 1.0f + frac_[c2][k] * span;
                if (!fading) tapDelay_[c2][k] = d; // crossfading: each tap takes its new place in turn
                else if (next && k == xk) {    // the fading tap: its old place into the extra slot
                    tapDelay_[c2][kTapSlots] = tapDelay_[c2][k];
                    tapDelay_[c2][k] = d;
                }
            }
        const float mix = a <= kDirectBelowSeconds ? 0.0f
                        : (a >= kTapsFromSeconds ? 1.0f : (a - kDirectBelowSeconds) / (kTapsFromSeconds - kDirectBelowSeconds));
        if (!fading) tapMix_ = mix;
        else { // this block: the fading tap's progress and the whole fade's (a line across it), the direct path
            const float step = 1.0f / float(seg), d = step / float(taps_) * float(n);
            // Between the direct path and the taps (MASSIVE's shortest swells), across the whole fade.
            tapMix_ = tapMix_ < mix ? std::fmin(mix, tapMix_ + d) : std::fmax(mix, tapMix_ - d);
            xfTapStep_ = step;
            xfTapAt_ = float(xfPos_ - xk * seg) * step;
            xfPos_ += n;
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
        // A TIME crossfade (control()): the fading tap fades in at its new place while its old place, in
        // the extra slot, fades out (addRamped); the other taps as they are.
        const int xk = xfTap_ - 1, last = xk >= 0 ? taps_ + 1 : taps_;
        for (int c = 0; c < 2; ++c) {
            for (int s = 0; s < n; ++s) sum[c][s] = 0.0f;
            const DelayLine& line = line_[c];
            for (int k = 0; k < last; ++k) {
                if (k == taps_ || k == xk) { // the fading tap: its old place (the extra slot), its new
                    addRamped(sum[c], line, tapDelay_[c][k == taps_ ? kTapSlots : k], gain_[c][xk], k == taps_, n);
                    continue;
                }
                // The everyday case, two taps at a time (CPU save 2, 28 Oct 2026): a full line, a tap at least a
                // sample back whose block of reads doesn't wrap the ring. Its two reads a sample are neighbours, so
                // each sample's later one is the next sample's earlier one: one load a sample, kept in a register,
                // and the pair's two terms go onto the sum in tap order before it's stored. The same sums.
                const float* p0 = line.full() ? plainRun(line, tapDelay_[c][k], n) : nullptr;
                if (p0) {
                    const float* p1 = k + 1 < taps_ && k + 1 != xk ? plainRun(line, tapDelay_[c][k + 1], n) : nullptr;
                    const float  d0 = tapDelay_[c][k], t0 = d0 - float(uint32_t(d0)), g0 = gain_[c][k];
                    float        b0 = p0[0];
                    if (p1) {
                        const float d1 = tapDelay_[c][k + 1], t1 = d1 - float(uint32_t(d1)), g1 = gain_[c][k + 1];
                        float       b1 = p1[0];
                        for (int s = 0; s < n; ++s) {
                            const float a0 = p0[s + 1], a1 = p1[s + 1];
                            float       v  = sum[c][s];
                            v += g0 * (a0 + t0 * (b0 - a0));
                            v += g1 * (a1 + t1 * (b1 - a1));
                            sum[c][s] = v;
                            b0 = a0;
                            b1 = a1;
                        }
                        ++k;
                    } else {
                        for (int s = 0; s < n; ++s) {
                            const float a0 = p0[s + 1];
                            sum[c][s] += g0 * (a0 + t0 * (b0 - a0));
                            b0 = a0;
                        }
                    }
                    continue;
                }
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
        // The smear's blend across the whole crossfade: the taps done and the fading one's progress.
        const float xfSmear = (float(xk) + xfTapAt_) / float(taps_), xfSmearStep = xfTapStep_ / float(taps_);
        for (int s = 0; s < n; ++s) {
            float v2[2] = {sum[0][s], sum[1][s]};
            if (smearG_ > 0.0f)
                for (int c = 0; c < 2; ++c)
                    for (int j = 0; j < kSmearStages; ++j) {
                        float d = smear_[c][j].read(smearLen_[j]);
                        if (smearOld_[j] != smearLen_[j]) { // a TIME crossfade: from the old length's read
                            const float o = smear_[c][j].read(smearOld_[j]);
                            d = o + (xfSmear + xfSmearStep * float(s)) * (d - o);
                        }
                        const float v = v2[c] + smearG_ * d;
                        smear_[c][j].write(v);
                        v2[c] = d - smearG_ * v;
                    }
            for (int c = 0; c < 2; ++c) io[c][s] += tapMix_ * (v2[c] - io[c][s]);
        }
    }

    // processBlock()'s plain reads of a tap at delay d over n samples, oldest first: n + 1 floats in a row (the
    // block's earlier read for its first sample, then each sample's later one), or nullptr where they'd wrap the
    // ring or the tap sits under a sample back.
    static const float* plainRun(const DelayLine& line, float d, int n)
    {
        const auto di = uint32_t(d);
        if (di < 1) return nullptr;
        const uint32_t start = (line.pos - (di + 1) - uint32_t(n - 1)) & line.mask;
        return start + uint32_t(n) <= line.mask ? line.buf + start : nullptr;
    }

    // A TIME crossfade's fading tap, at its old place (out) or its new: its read, faded in or out across
    // the block, equal power (the two places hold unrelated sound): sin / cos of a quarter turn as the
    // cubic x (3 - x²) / 2 (within 3 %, no libm). Out of line, built for size (flash; 0.3 s after a move).
    IV_NOINLINE IV_SIZE_OPT void addRamped(float* sum, const DelayLine& line, float d, float gain, bool out, int n) const
    {
        const auto  di = uint32_t(d);
        const float t  = d - float(di), x0 = out ? 1.0f - xfTapAt_ : xfTapAt_, dx = out ? -xfTapStep_ : xfTapStep_;
        for (int s = 0; s < n; ++s) {
            const uint32_t back = uint32_t(n - 1 - s);
            const float a0 = line.ago((di < 1 ? 1u : di) + back), a1 = line.ago(di + 1 + back);
            const float x = x0 + dx * float(s); // (past 1 by under a block at a tap's end: x (3 - x²) / 2 ~ 1)
            sum[s] += gain * (0.5f * x * (3.0f - x * x)) * (a0 + t * (a1 - a0));
        }
    }

    // (One sample, outside a TIME crossfade: processBlock() is the one the Engine runs.)
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
    float     frac_[2][kTapSlots] IV_ZI, sign_[2][kTapSlots] IV_ZI, gain_[2][kTapSlots] IV_ZI, tapDelay_[2][kTapSlots + 1] IV_ZI;
    // The TIME crossfade (setAttackSeconds, control()): the fading tap and the samples since
    // the fade started (tapDelay_'s extra slot: the fading tap's old place), this block's ramp (the
    // fading tap's progress: start and step), the rest timer, the smear's old lengths.
    int       xfTap_ IV_ZI, xfPos_ IV_ZI; // xfTap_: the fading tap + 1 (0: none)
    float     xfTapAt_ IV_ZI, xfTapStep_ IV_ZI;
    float     attackLast_ IV_ZI;
    int       restSamples_ IV_ZI;
    uint32_t  smearOld_[kSmearStages] IV_ZI; // (control() sets it before the first block)
#ifndef IV_FIXED_VOICINGS
    int       taps_ = kTaps; // setTapCount (a prototype)
#else
    static constexpr int taps_ = kTaps;
#endif
    uint32_t  seed_ = 0x5EE11u; // the jitter and signs' generator (setSeed, a prototype)
    float     attackNow_ IV_ZI, attackTarget_ IV_ZI, tapMix_ IV_ZI;
    bool      sCurve_ IV_ZI;
    DelayLine smear_[2][kSmearStages];
    uint32_t  smearLen_[kSmearStages] = {1, 1, 1};
    float     smearG_ IV_ZI, smearTarget_ IV_ZI;
    int       glideN_[2] IV_ZI; // control()'s glide coefficient glideK_[k] for chunks of glideN_[k] (0: none; prepare() clears)
    float     glideK_[2] IV_ZI;

public:
    // BLOOM smears its taps (kSmearGain); MASSIVE's mass and the twin's swell don't.
    void setSmear(bool on) { smearTarget_ = on ? kSmearGain : 0.0f; }
    // The smear straight at its target (a restart: gliding out from a cleared smear, it jumped from the
    // delayed read to the direct one when it let go, a click ~1.5 s after BLOOM was left).
    void snapSmear() { smearG_ = smearTarget_; }
};

} // namespace iv::dsp
