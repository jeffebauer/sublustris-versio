#pragma once
// The wet's last stages before MIX (SPEC §4.9, §4.10): TONE's tilt and the
// soft limiter (the ducking is a gain in Engine, ADR 0014). Both act on the wet only, so MIX fully
// CCW stays a clean passthrough.
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"
#if !defined(IV_FIXED_VOICINGS) || defined(IV_TONE_CHAR_SHIP)
#include "dsp/EraStages.h" // ToneCharacter's SVF
#endif

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
        const float d = tone < 0.5f ? (0.5f - tone) * 2.0f : 0.0f, b = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
        lowTarget_ = std::pow(10.0f, (d * voicing::kToneDarkLowDb + b * voicing::kToneBrightLowDb) / 20.0f);
        highTarget_ = std::pow(10.0f, (d * voicing::kToneDarkHighDb + b * voicing::kToneBrightHighDb) / 20.0f);
    }
    void snap() { low_ = lowTarget_; high_ = highTarget_; }
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
};

#if !defined(IV_FIXED_VOICINGS) || defined(IV_TONE_CHAR_SHIP)
// PROTOTYPE (renders/tone_character; owner, 27 Oct 2026: "add more character to the tone control - maybe some soft,
// sweet resonance ... a balance between performance and quality"; docs/research/tone-character.md). The wet's
// tilt with a soft resonance, three ways; the loop keeps its TONE share (holds and freeze don't change):
//   1 A  resonant tilt: today's tilt, then one trapezoidal SVF a side as a bell (low + A k band + high) riding its
//        bend: the corner moves with the knob (CCW 3 kHz -> 600 Hz, CW 1 -> 5 kHz), Q 0.707 at noon -> 1.3 and the
//        bell 0 -> +4.5 dB at the ends (a Pultec-like lift on today's EQ; a one-SVF shelf mix with today's amounts
//        and the corner up there lost 6 dB on music CW: the wet's loudness sits at 250 Hz-1 kHz).
//   2 B  SEM sweep (the Oberheim 12 dB filter): CCW a low-pass 16 kHz -> 350 Hz, CW a high-pass 20 -> 280 Hz plus
//        a +3 dB air shelf (one-pole, 6 kHz); Q 0.707 -> 1.4 at the ends.
//   3 C  today's tilt, then CCW a ZDF 4-pole ladder 16 kHz -> 500 Hz (bass-compensated, feedback to 1.5 of 4, a
//        cubic soft clip in the feedback); CW a resonant air shelf (corner 2 -> 8 kHz, the top +3 dB, Q to 1.3).
// Coefficients once per control chunk (EraSvf::gain: sin / cos, no tan), every value ramped per sample over the
// chunk; a level make-up per version follows the knob (kMakeupDb, matched to today's tilt on music). At noon the
// filter is out (the Engine plays today's Tilt there, so noon is today's, bit for bit).
class ToneCharacter {
public:
#if defined(IV_TONE_CHAR_SHIP)
    static constexpr int mode_ = IV_TONE_CHAR_SHIP;
    void setMode(int) {}
#else
    void setMode(int m) { mode_ = m; }
    int  mode() const { return mode_; }
#endif
    void prepare(float sampleRate)
    {
        sr_ = sampleRate;
        airCoeff_ = 1.0f - std::exp(-6.2831853f * 6000.0f / sampleRate);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) {
            svf_[c].reset();
            air_[c] = 0.0f;
            for (int i = 0; i < 4; ++i) lad_[c][i] = 0.0f;
        }
    }
    // TONE 0..1, once per control chunk of n samples.
    IV_SIZE_OPT void set(float tone, int n)
    {
        const float d = tone < 0.5f ? (0.5f - tone) * 2.0f : 0.0f, b = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
        const float u = d > b ? d : b;
        const int side = d > 0.0f ? -1 : (b > 0.0f ? 1 : 0);
        const bool ccw = side < 0 || (side == 0 && side_ <= 0); // at noon, the side it was last on
        float gl = 1.0f, gb = 1.41421356f, gh = 1.0f, air = 0.0f, k = 0.0f; // the identity: x = low + k band + high
        float fc = 1000.0f, q = 0.70710678f;
        if (mode_ == 1) {
            fc = ccw ? 3000.0f * std::exp(d * -1.6094379f) : 1000.0f * std::exp(b * 1.6094379f); // /5, x5
            q += (1.3f - 0.70710678f) * u;
            gb = std::pow(10.0f, 4.5f * u / 20.0f) / q; // a bell on today's tilt: x + (A - 1) k band, A at fc
        } else if (mode_ == 2) {
            q += (1.4f - 0.70710678f) * u;
            if (ccw) {
                fc = 16000.0f * std::exp(d * -3.8210720f); // -> 350 Hz
                gb = gh = 0.0f;
            } else {
                fc = 20.0f * std::exp(b * 2.6390573f); // -> 280 Hz
                gl = gb = 0.0f;
                air = std::pow(10.0f, 3.0f * b / 20.0f) - 1.0f;
            }
        } else if (mode_ == 3) {
            if (ccw) {
                fc = 16000.0f * std::exp(d * -3.4657359f); // -> 500 Hz
                k = 1.5f * d;
            } else {
                fc = 2000.0f * std::exp(b * 1.3862944f); // -> 8 kHz
                q += (1.3f - 0.70710678f) * b;
                gh = std::pow(10.0f, 3.0f * b / 20.0f); // on today's tilt: the top +3 dB more, a bump at the corner
                gb = 1.41421356f * std::pow(10.0f, 1.5f * b / 20.0f);
            }
        }
        const bool ladder = mode_ == 3 && ccw;
        if (!ladder) k = 1.0f / q; // the SVF's damping (the ladder's feedback otherwise)
        const float m = u * 10.0f < 1.0f ? u * 10.0f : 1.0f; // in over the first tenth of the knob's travel each side
        const float x = tone * 8.0f;                          // the make-up, from the table
        const int i = x >= 8.0f ? 7 : int(x), row = mode_ < 1 || mode_ > 3 ? 0 : mode_ - 1;
        const float mk = std::pow(10.0f, (kMakeupDb[row][i] * (float(i + 1) - x) + kMakeupDb[row][i + 1] * (x - float(i))) / 20.0f);
        // out = mk ((1 - m) x + m F), F = (1 + air) Fs - air lp6k(Fs), Fs = gl low + gb band + gh high
        //     = c0 x + c1 low + c2 band + c3 lp6k (high = x - k band - low); the ladder: c0 x + c1 y4.
        const float w = mk * m * (1.0f + air);
        float t[kN];
        t[kG] = EraSvf::gain(fc, sr_);
        t[kK] = k;
        t[kC0] = ladder ? mk * (1.0f - m) : mk * (1.0f - m) + w * gh;
        t[kC1] = ladder ? mk * m : w * (gl - gh);
        t[kC2] = ladder ? 0.0f : w * (gb - gh * k);
        t[kC3] = -mk * m * air;
        if (side != side_ && side != 0) {
            if (side_ != 0 && !leaving_) { // across noon: first fade the old side's filter out over this chunk
                leaving_ = true;
                t[kG] = cur_[kG];
                t[kK] = cur_[kK];
                t[kC0] = mk;
                t[kC1] = t[kC2] = t[kC3] = 0.0f;
            } else { // then the new side's filter, from silence, at its own corner (faded out: nothing to hear)
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
        flatTarget_ = side == 0 && still; // flat once a chunk starts at noon with nothing left to move
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
        flatTarget_ = side_ == 0 || (tgt_[kC0] == 1.0f && tgt_[kC1] == 0.0f && tgt_[kC2] == 0.0f && tgt_[kC3] == 0.0f);
    }
    // Noon, settled: the filter is out (the Engine plays today's Tilt).
    bool flat() const { return flatTarget_; }
    // A and C take today's tilted wet (they add to today's EQ); B the wet as it comes (it replaces the tilt).
    bool onTilt() const { return mode_ == 1 || mode_ == 3; }
    void process(float& l, float& r)
    {
        if (left_ > 0) {
            if (--left_ == 0) {
                for (int j = 0; j < kN; ++j) cur_[j] = tgt_[j];
            } else {
                for (int j = 0; j < kN; ++j) cur_[j] += step_[j];
            }
        }
        const float g = cur_[kG], k = cur_[kK], c0 = cur_[kC0], c1 = cur_[kC1];
        if (mode_ == 3 && side_ <= 0) { // the ladder: one divide for 1 / (1 + g) and 1 / (1 + k G^4)
            const float p = 1.0f + g, p2 = p * p, p4 = p2 * p2, g2 = g * g, q4 = p4 + k * g2 * g2;
            const float inv = 1.0f / (p * q4), G = g * q4 * inv, dn = p4 * p * inv, G2 = G * G, G4 = G2 * G2;
            l = c0 * l + c1 * ladderTick(lad_[0], l, G, G4, dn, k);
            r = c0 * r + c1 * ladderTick(lad_[1], r, G, G4, dn, k);
            return;
        }
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2, c2 = cur_[kC2];
        float xs[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            EraSvf& f = svf_[c];
            const float x = xs[c], v3 = x - f.ic2, v1 = a1 * f.ic1 + a2 * v3, v2 = f.ic2 + a2 * f.ic1 + a3 * v3;
            f.ic1 = 2.0f * v1 - f.ic1;
            f.ic2 = 2.0f * v2 - f.ic2;
            float y = c0 * x + c1 * v2 + c2 * v1;
            if (mode_ == 2 && side_ > 0) { // B's air: a one-pole at 6 kHz on the high-pass
                air_[c] += airCoeff_ * (x - k * v1 - v2 - air_[c]);
                y += cur_[kC3] * air_[c];
            }
            xs[c] = y;
        }
        l = xs[0];
        r = xs[1];
    }

private:
    enum { kG, kK, kC0, kC1, kC2, kC3, kN };
    // dB against today's tilt at TONE 0, 1/8, ... 1: the mean over HALL and MASSIVE x skank, vibes, pad (LUFS;
    // tools/pages/tone_character.py calibrate).
    static constexpr float kMakeupDb[3][9] = {{-2.26f, -1.17f, -0.52f, -0.17f, 0, -0.37f, -0.45f, -0.35f, -0.20f},
                                              {2.76f, 0.32f, 0.17f, -0.02f, 0, 0.06f, 0.30f, 0.60f, 1.34f},
                                              {-1.47f, -1.02f, -0.23f, 0, 0, -0.23f, -0.25f, -0.18f, -0.09f}};
    // Zavalishin's ZDF ladder (The Art of VA Filter Design, 2.1, ch. 5): the linear solve for the 4th stage,
    // the soft clip on the feedback it gives. Bass-compensated (input x (1 + k)).
    static float ladderTick(float* s, float x, float G, float G4, float dn, float k)
    {
        const float b = 1.0f - G, S = b * (G * (G * (G * s[0] + s[1]) + s[2]) + s[3]), c = 1.0f + k;
        float fb = k * (G4 * c * x + S) * dn;
        fb = fb > 1.8f ? 1.8f : (fb < -1.8f ? -1.8f : fb);
        fb *= 1.0f - 0.10288066f * fb * fb; // a cubic soft clip: slope 1 at 0, flat at +-1.8 (1.2)
        float u = c * x - fb;
        for (int i = 0; i < 4; ++i) {
            const float v = G * (u - s[i]), y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u;
    }

#if !defined(IV_TONE_CHAR_SHIP)
    int mode_ = 0;
#endif
    float sr_ = 48000.0f, airCoeff_ = 0.5f;
    EraSvf svf_[2];
    float air_[2] IV_ZI, lad_[2][4] IV_ZI;
    float cur_[kN] IV_ZI, tgt_[kN] IV_ZI, step_[kN] IV_ZI;
    int   left_ IV_ZI, side_ IV_ZI;
    bool  flatTarget_ = true, leaving_ IV_ZI;
};
#endif

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
