#pragma once
// The wet's last stages before MIX (SPEC §4.9, §4.10): TONE's tilt and the
// soft limiter (endless has no duck since v1.0.61, ADR 0008 amendment). Both act on the wet only, so MIX fully
// CCW stays a clean passthrough.
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"
#include "dsp/EraStages.h" // ToneBell's SVF

#include <cmath>

namespace iv::dsp {

// TONE's tilt (M1 notes: dark and bright "don't feel like extreme ends...
// lower lows, higher highs"). TONE's loop damping (the highs' T60) barely
// shows on a short tail or the first sound, so the wet is also tilted round
// kToneTiltHz: CCW more lows and fewer highs, CW the opposite, noon flat.
// One one-pole split per side: y = low·lp + high·(x − lp).
class Tilt {
public:
    void prepare(float sampleRate)
    {
        coeff_ = 1.0f - std::exp(-6.2831853f * voicing::kToneTiltHz / sampleRate);
        reset();
    }
    void reset() { lp_[0] = lp_[1] = 0.0f; }
    // TONE 0..1, once per control chunk.
    void set(float tone)
    {
        if (!(tone == setTone_)) { // its two std::pow only when TONE moves (CPU save 2, 28 Oct 2026)
            setTone_ = tone;
            const float d = tone < 0.5f ? (0.5f - tone) * 2.0f : 0.0f, b = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
            setLow_ = std::pow(10.0f, (d * voicing::kToneDarkLowDb + b * voicing::kToneBrightLowDb) / 20.0f);
            setHigh_ = std::pow(10.0f, (d * voicing::kToneDarkHighDb + b * voicing::kToneBrightHighDb) / 20.0f);
        }
        lowTarget_ = setLow_;
        highTarget_ = setHigh_;
    }
    void snap() { low_ = lowTarget_; high_ = highTarget_; }
#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (Engine::setHallTailPrototype): after set(), the targets moved by these dB.
    void trimPrototype(float lowDb, float highDb)
    {
        lowTarget_ *= std::pow(10.0f, lowDb / 20.0f);
        highTarget_ *= std::pow(10.0f, highDb / 20.0f);
    }
#endif
    void process(float& l, float& r)
    {
        low_ += kGlide * (lowTarget_ - low_); // per sample, so a fast TONE move can't step
        high_ += kGlide * (highTarget_ - high_);
        lp_[0] += coeff_ * (l - lp_[0]);
        lp_[1] += coeff_ * (r - lp_[1]);
        l = low_ * lp_[0] + high_ * (l - lp_[0]);
        r = low_ * lp_[1] + high_ * (r - lp_[1]);
    }

private:
    static constexpr float kGlide = 0.002f; // ~10 ms at 48 kHz
    float coeff_ = 0.1f, low_ = 1.0f, high_ = 1.0f, lowTarget_ = 1.0f, highTarget_ = 1.0f, lp_[2] IV_ZI;
    float setTone_ = -1.0f, setLow_ = 1.0f, setHigh_ = 1.0f; // set()'s last TONE and its targets
};

// TONE's resonant tilt (v1.0.44; owner's pick A, renders/tone_character: "adds a little character without creating
// too much resonance"; docs/research/tone-character.md). After the Tilt, one trapezoidal SVF a side as a bell riding
// its bend (Pultec / 1073 idea): out = x + (A - 1) k band, A at the corner. The corner moves with the knob (CCW
// 3 kHz -> 600 Hz, a warm swell under the darkening; CW 1 -> 5 kHz, a sheen on top), Q 0.707 at noon -> 1.3 and A
// 0 -> +4.5 dB at the ends; a make-up keeps music's loudness (LUFS) where today's tilt had it (kMakeupDb). The loop's
// TONE share is untouched (holds and freeze don't change). Coefficients once per control chunk (EraSvf::gain: sin /
// cos, no tan), ramped per sample over it; across noon the old side's bell fades out before the new side's starts.
// Noon is flat: out = mk x + 0 + 0. The table carries the wet's +3 dB (v1.0.73, ADR 0049, the owner's picks on
// renders/wet_loudness: lfad's "quieter on the output than my other reverbs"), so the limiter still has the last word.
class ToneBell {
public:
    void prepare(float sampleRate)
    {
        sr_ = sampleRate;
        memoTone_ = -1.0f; // set()'s kept coefficients were this rate's
        reset();
    }
    void reset()
    {
        svf_[0].reset();
        svf_[1].reset();
    }
    // TONE 0..1, once per control chunk of n samples.
    IV_SIZE_OPT void set(float tone, int n)
    {
        const float d = tone < 0.5f ? (0.5f - tone) * 2.0f : 0.0f, b = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
        const int side = d > 0.0f ? -1 : (b > 0.0f ? 1 : 0);
        const bool ccw = side < 0 || (side == 0 && side_ <= 0); // at noon, the side it was last on
        float t[kN];
        // The coefficients follow TONE (and, at noon, the side) alone: worked out again only when they move (CPU save 2,
        // 28 Oct 2026: two std::exp, two std::pow, a sin and a cos a chunk). The same bits.
        if (!(tone == memoTone_ && ccw == memoCcw_)) {
            memoTone_ = tone;
            memoCcw_ = ccw;
            const float u = d > b ? d : b;
            const float fc = ccw ? 3000.0f * std::exp(d * -1.6094379f) : 1000.0f * std::exp(b * 1.6094379f); // /5, x5
            const float q = 0.70710678f + (1.3f - 0.70710678f) * u;
            const float gb = std::pow(10.0f, 4.5f * u / 20.0f) / q, k = 1.0f / q;
            const float m = u * 10.0f < 1.0f ? u * 10.0f : 1.0f; // in over the first tenth of the knob's travel each side
            const float x = tone * 8.0f;                         // the make-up, from the table
            const int i = x >= 8.0f ? 7 : int(x);
            const float mk = std::pow(10.0f, (kMakeupDb[i] * (float(i + 1) - x) + kMakeupDb[i + 1] * (x - float(i))) / 20.0f);
            // out = mk ((1 - m) x + m (low + gb band + high)) = c0 x + c1 low + c2 band (high = x - k band - low); c1 is 0,
            // kept so the sums (and a desktop build's fused multiply-adds) are the ones the owner heard on the page
            const float w = mk * m;
            memoT_[kG] = EraSvf::gain(fc, sr_);
            memoT_[kK] = k;
            memoT_[kC0] = mk * (1.0f - m) + w;
            memoT_[kC1] = w * 0.0f;
            memoT_[kC2] = w * (gb - k);
            memoMk_ = mk;
        }
        const float mk = memoMk_;
        for (int j = 0; j < kN; ++j) t[j] = memoT_[j];
        if (side != side_ && side != 0) {
            if (side_ != 0 && !leaving_) { // across noon: first fade the old side's bell out over this chunk
                leaving_ = true;
                t[kG] = cur_[kG];
                t[kK] = cur_[kK];
                t[kC0] = mk;
                t[kC1] = t[kC2] = 0.0f;
            } else { // then the new side's, from silence, at its own corner (faded out: nothing to hear)
                leaving_ = false;
                reset();
                side_ = side;
                cur_[kG] = t[kG];
                cur_[kK] = t[kK];
            }
        } else
            leaving_ = false;
        bool still = true;
        for (int j = 0; j < kN; ++j) still = still && cur_[j] == t[j];
        const float inv = 1.0f / float(n > 0 ? n : 1);
        for (int j = 0; j < kN; ++j) {
            tgt_[j] = t[j];
            step_[j] = (t[j] - cur_[j]) * inv;
        }
        left_ = still ? 0 : n; // a still knob: nothing to ramp
    }
    void snap()
    {
        for (int j = 0; j < kN; ++j) {
            cur_[j] = tgt_[j];
            step_[j] = 0.0f;
        }
        left_ = 0;
    }
    void process(float& l, float& r)
    {
        if (left_ > 0) {
            if (--left_ == 0) {
                for (int j = 0; j < kN; ++j) cur_[j] = tgt_[j];
            } else {
                for (int j = 0; j < kN; ++j) cur_[j] += step_[j];
            }
            aG_ = -1.0f; // (the a's below follow)
        }
        const float g = cur_[kG], k = cur_[kK], c0 = cur_[kC0], c1 = cur_[kC1], c2 = cur_[kC2];
        if (!(g == aG_ && k == aK_)) { // the divide only when g or k moved (CPU save 2: one a sample)
            aG_ = g;
            aK_ = k;
            a1_ = 1.0f / (1.0f + g * (g + k));
            a2_ = g * a1_;
            a3_ = g * a2_;
        }
        const float a1 = a1_, a2 = a2_, a3 = a3_;
        l = tick(svf_[0], l, a1, a2, a3, c0, c1, c2);
        r = tick(svf_[1], r, a1, a2, a3, c0, c1, c2);
    }

private:
    enum { kG, kK, kC0, kC1, kC2, kN };
    // dB against the Tilt alone at TONE 0, 1/8, ... 1: the mean over HALL and MASSIVE x skank, vibes, pad (LUFS;
    // renders/tone_character, tools/pages/tone_character.py at 5e2562f).
    static constexpr float kMakeupDb[9] = {0.74f, 1.83f, 2.48f, 2.83f, 3.0f, 2.63f, 2.55f, 2.65f, 2.80f};
    static float tick(EraSvf& f, float x, float a1, float a2, float a3, float c0, float c1, float c2)
    {
        const float v3 = x - f.ic2, v1 = a1 * f.ic1 + a2 * v3, v2 = f.ic2 + a2 * f.ic1 + a3 * v3;
        f.ic1 = 2.0f * v1 - f.ic1;
        f.ic2 = 2.0f * v2 - f.ic2;
        return c0 * x + c1 * v2 + c2 * v1;
    }

    float sr_ = 48000.0f;
    EraSvf svf_[2];
    float cur_[kN] IV_ZI, tgt_[kN] IV_ZI, step_[kN] IV_ZI;
    int   left_ IV_ZI, side_ IV_ZI;
    bool  leaving_ IV_ZI;
    float memoTone_ = -1.0f, memoT_[kN] IV_ZI, memoMk_ IV_ZI; // set()'s coefficients for memoTone_ / memoCcw_
    bool  memoCcw_ IV_ZI;
    float aG_ = -1.0f, aK_ IV_ZI, a1_ IV_ZI, a2_ IV_ZI, a3_ IV_ZI; // process()'s a1-a3 for g aG_, k aK_
};

// The soft limiter: instant attack on the peak, kLimitReleaseSeconds release,
// so the wet never passes kLimitCeiling; gain() is what it applies (LEDs).
class Limiter {
public:
    void prepare(float sampleRate)
    {
        release_ = 1.0f - std::exp(-1.0f / (voicing::kLimitReleaseSeconds * sampleRate));
        reset();
    }
    void reset() { gain_ = 1.0f; }
    void process(float& l, float& r)
    {
        const float a = std::fmax(std::fabs(l), std::fabs(r));
        const float need = a > voicing::kLimitCeiling ? voicing::kLimitCeiling / a : 1.0f; // the most this sample allows
        if (need < gain_) gain_ = need;                // instant down
        else gain_ = std::fmin(need, gain_ + release_ * (1.0f - gain_)); // slow back up, never past what this sample allows
        l *= gain_;
        r *= gain_;
    }
    float gain() const { return gain_; }

private:
    float release_ = 0.0001f, gain_ = 1.0f;
};

} // namespace iv::dsp
