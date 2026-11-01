#pragma once
// ERA's wear (P4, SPEC §4.7; v1.0.42, the owner's pick C on renders/era_rethink, "one wear amount"): on the
// wet after the loop, one stereo chain whose stages all come in near the start of the knob and deepen
// together (Mappings.h eraMix). It keeps the grit the owner likes, the Midiverb's and the BBD's, and tape
// saturation; none moves its pitch (DRIFT owns movement). The Neve's warmth stays in the loop as the base
// (dsp/Era.h). docs/research/era-rethink.md. Its last stage, the radio's band, is MOTION's right side since
// v1.0.60 (Mappings.h radioBand; ADR 0036, the owner's pick F on renders/dip_motion), the one stage that
// narrows the tone.
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
//            do TONE's job), its over-modulation (flattened troughs, a soft knee since v1.0.60) and its
//            wandering dip (selective fading: a slow random notch, like a phaser's), without the station's
//            fades, detune or static. On MOTION's right side since v1.0.60 (Mappings.h radioBand, ADR 0036).
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
};

namespace tex {
// The tape's drive at full, 11.25 dB (v1.0.49: 15 x 0.75, the owner's pick E on renders/wear_saturation).
constexpr float kTapeDriveDb = 11.25f, kTapeEmphDb = 6.0f, kTapeEmphHz = 2000.0f, kTapeBias = 0.12f;
// The BBD's clock across its intensity: 28 kHz at full (v1.0.49, the owner's pick F on renders/wear_fizz; 14 kHz
// before: unfiltered, lower turns metallic, and 14 kHz fizzed).
constexpr float kClockHzHi = 32000.0f, kClockHzLo = 28000.0f;
// The level hold's ceiling, +15 dB (10^(15/20); v1.0.49, renders/wear_fizz): at +12 the band took a low chord's or
// pad's fundamentals down and WEAR 1 lost 1.1-1.8 dB on held chords and pads; now within +-1 dB of WEAR 0.
constexpr float kHoldMax = 5.62341309f;
// At infinite DECAY the band keeps this share of its blend: a held tail darkens to its warm floor (ADR 0008),
// mostly under 100 Hz, where the band is ~20 dB down; whole, it took 4-6 dB off a 10-minute hold.
constexpr float kBandAtInfinite = 0.5f;
// The over-modulation's soft knee (v1.0.60): linear down to -kOverModKnee of the carrier, then curving towards
// -kOverModFloor (the hard clamp it replaces sat at -1). Measured on renders/dip_motion's material against the page's
// F: a still PLATE's tick +13.9 -> +11.0 dB (A, WEAR's band, +10.1-10.4), keys in MASSIVE at full right +21.1 -> +15.7,
// the sound at most 19.6 dB under F's (a knee from 0.75 left the tick at +12.8; one from 0 reached +13.5 on the keys
// but sat 11-13.5 dB under F).
constexpr float kOverModKnee = 0.5f, kOverModFloor = 1.25f;
constexpr float kGrainBitsHi = 9.0f, kGrainBitsLo = 5.0f;     // the grain's bits under the level, across its intensity
} // namespace tex

class EraTexture {
public:
    IV_SIZE_OPT void reset()
    {
        for (int c = 0; c < 2; ++c) {
            pre_[c] = de_[c] = dc_[c] = 0.0f;
            envY_[c] = envZ_[c] = held_[c] = gEnv_[c] = gSum_[c] = 0.0f;
            gCompStep_[c] = gExpStep_[c] = 0.0f;
            gComp_[c] = 0.126f / 1e-4f; // the gains of empty envelopes: the first block starts where it did
            gExp_[c] = 1e-4f * (1.0f / 0.126f);
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
            a2_ = 0.02f + 0.05f * t;  // the compander's 2nd and 3rd harmonics, halved in v1.0.49 (renders/wear_saturation E)
            a3_ = 0.01f + 0.025f * t;
            noise_ = 0.126f * std::pow(10.0f, (-60.0f + 18.0f * t) / 20.0f);
            wc_ = 1.0f - std::exp(-1.0f / (0.010f * sr));
            we_ = 1.0f - std::exp(-1.0f / ((0.012f + 0.02f * t) * sr));
        }
        if (m.breathW > 0.0f) {
            // The compander's gains from its envelopes, worked out once a block and ramped to across it (v1.0.49:
            // stepped, they left small clicks on a pure chord in PLATE once the grit no longer covered them).
            const float inv = 1.0f / float(n > 0 ? n : 1);
            for (int c = 0; c < 2; ++c) {
                gCompStep_[c] = (0.126f / std::fmax(envY_[c], 1e-4f) - gComp_[c]) * inv;
                gExpStep_[c] = (std::fmax(envZ_[c], 1e-4f) * (1.0f / 0.126f) - gExp_[c]) * inv;
            }
        }
        // (CPU save 2, 28 Oct 2026: what follows a setting or the chunk's length alone is worked out again only when
        // that moves; the same expressions, so the same bits.)
        const bool srMoved = !sameBits(sr, sr_);
        int slot = n == memoN_[0] ? 0 : (n == memoN_[1] ? 1 : -1);
        if (slot < 0 || srMoved) {
            slot = slot < 0 ? memoNext_ : slot;
            memoNext_ = slot ^ 1;
            memoN_[slot] = n;
            memoHold_[slot] = 1.0f - std::exp(-float(n) / (0.5f * sr));
            memoGrain_[slot] = 1.0f - std::exp(-float(n) / (0.005f * sr));
            memoBandHz_[slot] = -1.0f;
            if (srMoved) memoN_[slot ^ 1] = 0;
        }
        if (m.clockW > 0.0f && (!sameBits(m.clockT, clockT_) || srMoved)) {
            clockT_ = m.clockT;
            clockInc_ = tex::kClockHzHi * std::pow(tex::kClockHzLo / tex::kClockHzHi, m.clockT) / sr;
        }
        if (m.grainW > 0.0f) { // the step follows the level a block late (~5 ms), worked out once a block
            if (!sameBits(m.grainT, grainT_)) {
                grainT_ = m.grainT;
                grainStep_ = std::exp2(-(tex::kGrainBitsHi + (tex::kGrainBitsLo - tex::kGrainBitsHi) * m.grainT));
            }
            const float step = grainStep_;
            const float w = memoGrain_[slot], inv = 1.0f / float(n > 0 ? n : 1);
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
                mEff_ = (0.9f + 0.5f * t) * 3.0f; // over a -12 dBFS carrier (x4 before v1.0.49: x0.75, renders/wear_saturation E)
                invMEff_ = 1.0f / mEff_;
                dipDb_ = -4.0f - 14.0f * t;
                dipHz_ = 0.1f + 0.4f * t;
                dipGain_ = std::pow(10.0f, dipDb_ / 20.0f);
            }
            // The wandering dip (as the radio's): a new random target every ~1/hz s, smoothed twice.
            const float dt = float(n) / sr;
            wander_[3] -= dt;
            if (wander_[3] <= 0.0f) {
                wander_[2] = rng_.next();
                wander_[3] = (0.7f + 0.6f * (0.5f + 0.5f * rng_.next())) / dipHz_;
            }
            if (!(memoBandHz_[slot] == dipHz_)) {
                memoBandHz_[slot] = dipHz_;
                memoBand_[slot] = 1.0f - std::exp(-6.2831853f * dipHz_ * dt);
            }
            const float w = memoBand_[slot];
            wander_[0] += w * (wander_[2] - wander_[0]);
            wander_[1] += w * (wander_[0] - wander_[1]);
            dip_[0].set(300.0f * std::pow(10.0f, 0.5f + 0.5f * wander_[1]), 1.5f + 1.5f * t, sr);
            dip_[1].copyCoefficients(dip_[0]);
        }
        // The hold's gain for this block, from the last one's powers.
        if (srMoved) {
            wHold_ = 1.0f - std::exp(-1.0f / (0.1f * sr));
            wHp_ = 1.0f - std::exp(-6.2831853f * 200.0f / sr);
        }
        const float want = std::fmin(tex::kHoldMax, std::fmax(0.5f, std::sqrt((inPow_ + 1e-20f) / (outPow_ + 1e-20f))));
        holdAim_ += memoHold_[slot] * (want - holdAim_);
        holdStep_ = (holdAim_ - hold_) / float(n > 0 ? n : 1);
        sr_ = sr;
    }

    // The level hold is back at unity (within -80 dB): with every stage out, the chain may stop running. Turned
    // off while the hold still made up a level, the wet stepped (a click turning ERA down to 0).
    bool settled() const { return std::fabs(hold_ - 1.0f) < 1e-4f && std::fabs(holdAim_ - 1.0f) < 1e-4f; }

    // process() over a block in place (out of line: the Engine's one call, its two sides side by side).
    IV_NOINLINE IV_NO_UNSWITCH void processBlock(float* __restrict l, float* __restrict r, int n)
    {
        for (int i = 0; i < n; ++i) process(l[i], r[i]);
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
        // Stage by stage, both sides in each (CPU save 2, 28 Oct 2026): side by side, the M7 overlaps the two
        // sides' dependent chains; a side's whole chain at a time, each stage waited on the last's result. The
        // same operations for each side, the breath's noise drawn left then right as before: the same bits.
        if (m_.tapeW > 0.0f) {
            const float t0 = tape(x[0], 0), t1 = tape(x[1], 1);
            x[0] += m_.tapeW * (t0 - x[0]);
            x[1] += m_.tapeW * (t1 - x[1]);
        }
        if (m_.breathW > 0.0f) {
            const float b0 = breath(x[0], 0), b1 = breath(x[1], 1);
            x[0] += m_.breathW * (b0 - x[0]);
            x[1] += m_.breathW * (b1 - x[1]);
        }
        if (m_.clockW > 0.0f) {
            if (tick_) {
                held_[0] = x[0];
                held_[1] = x[1];
            }
            x[0] += m_.clockW * (held_[0] - x[0]);
            x[1] += m_.clockW * (held_[1] - x[1]);
        }
        if (m_.grainW > 0.0f)
            for (int c = 0; c < 2; ++c) {
                gSum_[c] += std::fabs(x[c]);
                x[c] += m_.grainW * (float(int32_t(x[c] * invQ_[c])) * q_[c] - x[c]);
            }
        if (m_.bandW > 0.0f) {
            const float b0 = band(x[0], 0), b1 = band(x[1], 1);
            x[0] += m_.bandW * (b0 - x[0]);
            x[1] += m_.bandW * (b1 - x[1]);
        }
        hpOut_ += wHp_ * (0.5f * (x[0] + x[1]) - hpOut_);
        const float out = 0.5f * (x[0] + x[1]) - hpOut_;
        outPow_ += wHold_ * (out * out - outPow_);
        hold_ += holdStep_;
        l = x[0] * hold_;
        r = x[1] * hold_;
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
        gComp_[c] += gCompStep_[c];
        envY_[c] += wc_ * (std::fabs(y) - envY_[c]);
        // Onsets overshoot past ±2: there a soft knee to ±2.5, slope 1 at ±2 (v1.0.52). The hard clamp it replaces
        // put a corner in the wave, heard as a tick on a loud tail's biggest peaks (renders/hall_level C, 17 dB over
        // its highs); every sample within ±2 is as before, to the bit.
        float u = y * (0.5f / ref);
        if (std::fabs(u) > 2.0f) {
            const float d = std::fabs(u) - 2.0f;
            u = std::copysign(2.0f + d / (1.0f + 2.0f * d), u);
        }
        float z = (u - a2_ * u * u - a3_ * u * u * u) * (2.0f * ref); // the chip's soft 2nd harmonic
        z += noise_ * rng_.next();
        envZ_[c] += we_ * (std::fabs(z) - envZ_[c]);
        const float out = z * gExp_[c];                                // 1:2: the noise breathes with the level
        gExp_[c] += gExpStep_[c];
        return out;
    }
    float band(float v, int c)
    {
        // Over-modulation flattens the troughs: a soft knee from -kOverModKnee towards -kOverModFloor (v1.0.60; slope 1
        // at the knee, as the breath's in v1.0.52). The hard clamp at -1 it replaces put a corner in the wave, heard as a
        // tick on a clean, still PLATE once the band moved to MOTION without WEAR's tape and grit (renders/dip_motion F).
        float u = v * mEff_;
        if (u < -tex::kOverModKnee) {
            const float d = -tex::kOverModKnee - u;
            u = -tex::kOverModKnee - d / (1.0f + d * (1.0f / (tex::kOverModFloor - tex::kOverModKnee)));
        }
        v = u * invMEff_;
        float b, h;
        dip_[c].tick(v, b, h);
        v += (dipGain_ - 1.0f) * dip_[c].k * b;
        bp_[c].tick(v, b, h);
        return bp_[c].k * b; // the band-pass at unity in its middle
    }

    TexMix m_;
    float  sr_ IV_ZI, tapeT_ = -1.0f, breathT_ = -1.0f, bandT_ = -1.0f;
    float  gEmph_ = 1, drive_ = 1, wPre_ IV_ZI, wDe_ IV_ZI, wDc_ IV_ZI, bias_ IV_ZI, fBias_ IV_ZI;
    float  pre_[2] IV_ZI, de_[2] IV_ZI, dc_[2] IV_ZI;
    float  a2_ IV_ZI, a3_ IV_ZI, noise_ IV_ZI, wc_ IV_ZI, we_ IV_ZI, envY_[2] IV_ZI, envZ_[2] IV_ZI, gComp_[2] = {1, 1}, gExp_[2] = {1, 1};
    float  gCompStep_[2] IV_ZI, gExpStep_[2] IV_ZI;
    float  clock_ IV_ZI, clockInc_ = 1, held_[2] IV_ZI;
    bool   tick_ IV_ZI;
    float  gSum_[2] IV_ZI, gEnv_[2] IV_ZI, q_[2] = {1.0f, 1.0f}, invQ_[2] = {1.0f, 1.0f};
    float  invDrive_ = 1, deGain_ IV_ZI, invMEff_ = 1;
    EraSvf bp_[2], dip_[2];
    float  mEff_ = 1, dipDb_ IV_ZI, dipHz_ = 0.1f, dipGain_ = 1, wander_[4] IV_ZI;
    float  wHold_ IV_ZI, wHp_ IV_ZI, hpIn_ IV_ZI, hpOut_ IV_ZI, inPow_ IV_ZI, outPow_ IV_ZI, hold_ = 1, holdAim_ = 1, holdStep_ IV_ZI;
    EraRng rng_;
    // set()'s kept maths (CPU save 2): per chunk length (memoN_, 0 none): the hold's and the grain's one-pole
    // coefficients, the band's wander's for memoBandHz_; and the clock's and the grain's step for their settings.
    int    memoN_[2] IV_ZI, memoNext_ IV_ZI;
    float  memoHold_[2] IV_ZI, memoGrain_[2] IV_ZI, memoBand_[2] IV_ZI, memoBandHz_[2] IV_ZI;
    float  clockT_ = -1.0f, grainT_ = -1.0f, grainStep_ IV_ZI;
};

} // namespace iv::dsp
