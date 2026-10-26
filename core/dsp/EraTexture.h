#pragma once
// PROTOTYPE (renders/era_rethink; the owner, 26 Oct 2026: keep ERA's grit, not its darkening or its
// wobble): ERA as texture only, on the wet after the loop, one stereo chain whose stages the three
// layouts (Mappings.h eraLayoutMix: A grit path, B saturation then texture, C one wear amount) blend in.
// No stage low-passes the signal (TONE owns the highs) and none moves its pitch (DRIFT owns movement);
// the Neve's warmth stays in the loop as the base (dsp/Era.h). docs/research/era-rethink.md.
//
//   tape:    record pre-emphasis around a soft clip, then the exact de-emphasis (Strymon's and
//            Chowdhury's tape models); a little bias for even harmonics. The clip is a clamped cubic at
//            48 kHz, no oversampling: it makes only a third harmonic below its ceiling, and the wet of a
//            24 kHz tank has little above 8 kHz to fold (antiderivative anti-aliasing, Parker et al.
//            DAFx-16, was tried: twice the instructions for what the wet barely has).
//   breath:  the BBD's NE570 compander, 2:1 into the line and 1:2 out, its noise between and the chip's
//            soft 2nd harmonic (Raffel & Smith, DAFx-10), without the line's filters: the noise and the
//            level breathe with the playing. Its gains follow its envelopes a block late (1 ms against
//            the chip's 10-30 ms), so no divide per sample.
//   clock:   the BBD's sample-and-hold at its clock, with no anti-alias or reconstruction filter: the
//            reverb's top folds back (aliasing) and the steps add a fizzy image.
//   grain:   magnitude truncation to a step that follows the level (a floating-point converter, like
//            μ-law's 8 bits): the grain stays the same share of the tail all the way down instead of
//            buzzing and gating as a fixed step does on a fade; a floor keeps silence silent.
//   band:    the AM radio's band (one two-pole band-pass, ~300 Hz-3 kHz, gentle at the top so it doesn't
//            do TONE's job), its over-modulation (flattened troughs) and its wandering dip (selective
//            fading: a slow random notch, like a phaser's), without the station's fades, detune or static.
// Each stage: a weight w (its blend, 0 = bypassed and not run) and an intensity t (its own setting).
// A level hold keeps the chain's loudness at its input's, within -6 / +12 dB: the tape's squash and the
// band's loss depend on how hot the wet is (a pushed send lost 4 dB more than the skank). It compares the
// two sides' 0.1 s levels and glides its gain over ~0.5 s, too slowly to undo a transient's squash, quickly
// enough that a fading tail (no longer squashed) is back at unity within a second: measured over 4 s, it
// kept a loud passage's make-up and left tails 3-5 dB over the Neve's. It measures both sides above
// ~200 Hz (one pole), as loudness does, so the band's lost lows don't read as level to make up.
#include "dsp/EraStages.h" // EraSvf, EraRng, sameBits
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

struct TexMix {
    float tapeW = 0, tapeT = 0;
    float breathW = 0, breathT = 0;
    float clockW = 0, clockT = 0;
    float grainW = 0, grainT = 0;
    float bandW = 0, bandT = 0;
    float makeup = 1.0f; // the chain's level against the Neve base on music (Mappings.h)
};

namespace tex {
constexpr float kTapeDriveDb = 15.0f, kTapeEmphDb = 6.0f, kTapeEmphHz = 2000.0f, kTapeBias = 0.12f;
constexpr float kClockHzHi = 32000.0f, kClockHzLo = 14000.0f; // the BBD's clock across its intensity (no lower: unfiltered, it turns metallic)
// At infinite DECAY the band keeps this share of its blend: a held tail darkens to its warm floor (ADR 0008),
// mostly under 100 Hz, where the band is ~20 dB down; whole, it took 4-6 dB off a 10-minute hold.
constexpr float kBandAtInfinite = 0.5f;
constexpr float kGrainBitsHi = 9.0f, kGrainBitsLo = 5.0f;     // the grain's bits under the level, across its intensity
} // namespace tex

class EraTexture {
public:
    IV_SIZE_OPT void reset()
    {
        for (int c = 0; c < 2; ++c) {
            pre_[c] = de_[c] = dc_[c] = 0.0f;
            envY_[c] = envZ_[c] = held_[c] = gEnv_[c] = gSum_[c] = 0.0f;
            bp_[c].reset();
            dip_[c].reset();
        }
        clock_ = hpIn_ = hpOut_ = inPow_ = outPow_ = holdStep_ = 0.0f;
        hold_ = holdAim_ = 1.0f;
        for (float& w : wander_) w = 0.0f;
    }

    // Once a block. Coefficients that follow an intensity alone are worked out again only when it moves.
    IV_SIZE_OPT void set(const TexMix& m, float sr, int n)
    {
        m_ = m;
        if (m.tapeW > 0.0f && (!sameBits(m.tapeT, tapeT_) || !sameBits(sr, sr_))) {
            tapeT_ = m.tapeT;
            gEmph_ = std::pow(10.0f, tex::kTapeEmphDb * m.tapeT / 20.0f);
            drive_ = std::pow(10.0f, tex::kTapeDriveDb * m.tapeT / 20.0f);
            wPre_ = 1.0f - std::exp(-6.2831853f * tex::kTapeEmphHz / sr);
            wDe_ = 1.0f - std::exp(-6.2831853f * tex::kTapeEmphHz / gEmph_ / sr);
            wDc_ = 6.2831853f * 5.0f / sr;
            bias_ = tex::kTapeBias * m.tapeT;
            fBias_ = clip(bias_);
            invDrive_ = 1.0f / drive_; // the M7 divides slowly (~14 cycles): reciprocals once, not per sample
            deGain_ = 1.0f / gEmph_ - 1.0f;
        }
        if (m.breathW > 0.0f && (!sameBits(m.breathT, breathT_) || !sameBits(sr, sr_))) {
            const float t = breathT_ = m.breathT;
            a2_ = 0.04f + 0.10f * t;
            a3_ = 0.02f + 0.05f * t;
            noise_ = 0.126f * std::pow(10.0f, (-60.0f + 18.0f * t) / 20.0f);
            wc_ = 1.0f - std::exp(-1.0f / (0.010f * sr));
            we_ = 1.0f - std::exp(-1.0f / ((0.012f + 0.02f * t) * sr));
        }
        if (m.breathW > 0.0f)
            for (int c = 0; c < 2; ++c) { // the compander's gains from its envelopes, once a block
                gComp_[c] = 0.126f / std::fmax(envY_[c], 1e-4f);
                gExp_[c] = std::fmax(envZ_[c], 1e-4f) * (1.0f / 0.126f);
            }
        if (m.clockW > 0.0f) clockInc_ = tex::kClockHzHi * std::pow(tex::kClockHzLo / tex::kClockHzHi, m.clockT) / sr;
        if (m.grainW > 0.0f) { // the step follows the level a block late (~5 ms), worked out once a block
            const float step = std::exp2(-(tex::kGrainBitsHi + (tex::kGrainBitsLo - tex::kGrainBitsHi) * m.grainT));
            const float w = 1.0f - std::exp(-float(n) / (0.005f * sr)), inv = 1.0f / float(n > 0 ? n : 1);
            for (int c = 0; c < 2; ++c) {
                gEnv_[c] += w * (gSum_[c] * inv - gEnv_[c]);
                gSum_[c] = 0.0f;
                q_[c] = std::fmax(gEnv_[c] * step, 1.0f / 65536.0f); // the floor: 16 bits
                invQ_[c] = 1.0f / q_[c];
            }
        }
        if (m.bandW > 0.0f) {
            const float t = m.bandT;
            if (!sameBits(t, bandT_) || !sameBits(sr, sr_)) {
                bandT_ = t;
                // The old radio's 200-400 Hz to 4.5-2.4 kHz: centred ~1.1 kHz, narrowing with t.
                bp_[0].set(1000.0f + 200.0f * t, 0.45f + 0.35f * t, sr);
                bp_[1].copyCoefficients(bp_[0]);
                mEff_ = (0.9f + 0.5f * t) * 4.0f; // over a -12 dBFS carrier
                invMEff_ = 1.0f / mEff_;
                dipDb_ = -4.0f - 14.0f * t;
                dipHz_ = 0.1f + 0.4f * t;
            }
            // The wandering dip (as the radio's): a new random target every ~1/hz s, smoothed twice.
            const float dt = float(n) / sr;
            wander_[3] -= dt;
            if (wander_[3] <= 0.0f) {
                wander_[2] = rng_.next();
                wander_[3] = (0.7f + 0.6f * (0.5f + 0.5f * rng_.next())) / dipHz_;
            }
            const float w = 1.0f - std::exp(-6.2831853f * dipHz_ * dt);
            wander_[0] += w * (wander_[2] - wander_[0]);
            wander_[1] += w * (wander_[0] - wander_[1]);
            dip_[0].set(300.0f * std::pow(10.0f, 0.5f + 0.5f * wander_[1]), 1.5f + 1.5f * t, sr);
            dip_[1].copyCoefficients(dip_[0]);
            dipGain_ = std::pow(10.0f, dipDb_ / 20.0f);
        }
        // The hold's gain for this block, from the last one's powers.
        wHold_ = 1.0f - std::exp(-1.0f / (0.1f * sr));
        wHp_ = 1.0f - std::exp(-6.2831853f * 200.0f / sr);
        const float want = std::fmin(4.0f, std::fmax(0.5f, std::sqrt((inPow_ + 1e-20f) / (outPow_ + 1e-20f))));
        holdAim_ += (1.0f - std::exp(-float(n) / (0.5f * sr))) * (want - holdAim_);
        holdStep_ = (holdAim_ - hold_) / float(n > 0 ? n : 1);
        sr_ = sr;
    }

    void process(float& l, float& r)
    {
        float x[2] = {l, r};
        hpIn_ += wHp_ * (0.5f * (l + r) - hpIn_);
        const float in = 0.5f * (l + r) - hpIn_;
        inPow_ += wHold_ * (in * in - inPow_);
        if (m_.clockW > 0.0f) { // one clock for both sides
            clock_ += clockInc_;
            tick_ = clock_ >= 1.0f;
            if (tick_) clock_ -= 1.0f;
        }
        for (int c = 0; c < 2; ++c) {
            float v = x[c];
            if (m_.tapeW > 0.0f) v += m_.tapeW * (tape(v, c) - v);
            if (m_.breathW > 0.0f) v += m_.breathW * (breath(v, c) - v);
            if (m_.clockW > 0.0f) {
                if (tick_) held_[c] = v;
                v += m_.clockW * (held_[c] - v);
            }
            if (m_.grainW > 0.0f) {
                gSum_[c] += std::fabs(v);
                v += m_.grainW * (float(int32_t(v * invQ_[c])) * q_[c] - v);
            }
            if (m_.bandW > 0.0f) v += m_.bandW * (band(v, c) - v);
            x[c] = v;
        }
        hpOut_ += wHp_ * (0.5f * (x[0] + x[1]) - hpOut_);
        const float out = 0.5f * (x[0] + x[1]) - hpOut_;
        outPow_ += wHold_ * (out * out - outPow_);
        hold_ += holdStep_;
        const float g = hold_ * m_.makeup;
        l = x[0] * g;
        r = x[1] * g;
    }

private:
    // The soft clip: x - x³/3 up to ±1, then ±2/3 (slope 1 at 0, 0 at the ceiling), without a branch.
    static float clip(float x)
    {
        x = std::fmin(1.0f, std::fmax(-1.0f, x));
        return x - x * x * x * (1.0f / 3.0f);
    }
    float tape(float x, int c)
    {
        pre_[c] += wPre_ * (x - pre_[c]);
        const float u = drive_ * (x + (gEmph_ - 1.0f) * (x - pre_[c])) + bias_;
        float y = (clip(u) - fBias_) * invDrive_;
        de_[c] += wDe_ * (y - de_[c]);
        y += deGain_ * (y - de_[c]);
        dc_[c] += wDc_ * (y - dc_[c]);
        return y - dc_[c];
    }
    float breath(float x, int c)
    {
        constexpr float ref = 0.126f;                                  // -18 dBFS: the compander's unity point
        const float y = x * gComp_[c];                                 // 2:1, its rectifier on its own output
        envY_[c] += wc_ * (std::fabs(y) - envY_[c]);
        const float u = std::fmin(2.0f, std::fmax(-2.0f, y * (0.5f / ref))); // overshoots on onsets
        float z = (u - a2_ * u * u - a3_ * u * u * u) * (2.0f * ref); // the chip's soft 2nd harmonic
        z += noise_ * rng_.next();
        envZ_[c] += we_ * (std::fabs(z) - envZ_[c]);
        return z * gExp_[c];                                           // 1:2: the noise breathes with the level
    }
    float band(float v, int c)
    {
        v = std::fmax(v * mEff_, -1.0f) * invMEff_;                    // over-modulation flattens the troughs
        float b, h;
        dip_[c].tick(v, b, h);
        v += (dipGain_ - 1.0f) * dip_[c].k * b;
        bp_[c].tick(v, b, h);
        return bp_[c].k * b; // the band-pass at unity in its middle
    }

    TexMix m_;
    float  sr_ = 0.0f, tapeT_ = -1.0f, breathT_ = -1.0f, bandT_ = -1.0f;
    float  gEmph_ = 1, drive_ = 1, wPre_ = 0, wDe_ = 0, wDc_ = 0, bias_ = 0, fBias_ = 0;
    float  pre_[2] = {}, de_[2] = {}, dc_[2] = {};
    float  a2_ = 0, a3_ = 0, noise_ = 0, wc_ = 0, we_ = 0, envY_[2] = {}, envZ_[2] = {}, gComp_[2] = {1, 1}, gExp_[2] = {1, 1};
    float  clock_ = 0, clockInc_ = 1, held_[2] = {};
    bool   tick_ = false;
    float  gSum_[2] = {}, gEnv_[2] = {}, q_[2] = {1.0f, 1.0f}, invQ_[2] = {1.0f, 1.0f};
    float  invDrive_ = 1, deGain_ = 0, invMEff_ = 1;
    EraSvf bp_[2], dip_[2];
    float  mEff_ = 1, dipDb_ = 0, dipHz_ = 0.1f, dipGain_ = 1, wander_[4] = {};
    float  wHold_ = 0, wHp_ = 0, hpIn_ = 0, hpOut_ = 0, inPow_ = 0, outPow_ = 0, hold_ = 1, holdAim_ = 1, holdStep_ = 0;
    EraRng rng_;
};

} // namespace iv::dsp
