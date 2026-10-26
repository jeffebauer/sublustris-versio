#pragma once
// ERA's bucket-brigade and AM radio (ADR 0021; docs/research/new-eras.md, docs/research/radio-artifacts.md),
// both on the wet after the loop, so neither can build up in the tank. Each takes its intensity t (0 = its
// sweet end, 1 = its gritty end, across its stretch of the ERA knob) once per block.
//   BbdStage:   a bucket-brigade line's sound without its delay (Raffel & Smith, DAFx-10): a low-pass at a
//               third of the clock, the clock's own sample-and-hold (its aliasing) behind a weak anti-alias
//               filter, the NE570 compander 2:1 / 1:2 with noise between (its breathing), the chip's soft
//               2nd harmonic. Its grit grows across the stretch (kBbdGritAt). No clock whine (tonal).
//   RadioStage: AM / shortwave as a real set behaves: the station fades on its own path (Watterson, ITU-R
//               F.1487: a Rician gain) while the receiver's noise stays put, then the AGC lifts both, so the
//               static swells as the station dips. A wandering dip (selective fading), soft coloured hiss,
//               static in three layers (storm activity, Pareto crackle, lightning flashes of 1-6 strokes),
//               a slow detune, and at the gritty end rare drift-offs and a faint mains hum. No whistles or
//               fast flutter (owner, era_v2c: they "read as slow rhythmic delays rather than dropouts").
//               All added noise is mono and follows the tail, so silence stays silent.
#include "dsp/SizeOpt.h"
#include "params/EraVoicing.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace iv::dsp {

// Simper's trapezoidal state-variable filter (low, band, high from one state).
struct EraSvf {
    float ic1 IV_ZI, ic2 IV_ZI, a1 = 1, a2 IV_ZI, a3 IV_ZI, k = 1.4f;
    IV_SIZE_OPT void set(float fc, float q, float sr) { setGain(gain(fc, sr), q); }
    // set() in two halves, so filters at one frequency share the sin and cos (the same bits).
    IV_SIZE_OPT static float gain(float fc, float sr)
    {
        const float w = 3.14159265f * std::fmin(fc, 0.45f * sr) / sr;
        return std::sin(w) / std::cos(w); // not std::tan: it pulled 2.7 KB of maths into the firmware
    }
    IV_SIZE_OPT void setGain(float g, float q)
    {
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // Another filter's coefficients (its state stays).
    void copyCoefficients(const EraSvf& o)
    {
        a1 = o.a1;
        a2 = o.a2;
        a3 = o.a3;
        k = o.k;
    }
    void reset() { ic1 = ic2 = 0.0f; }
    // Returns the low-pass; band and high through the references.
    float tick(float x, float& band, float& high)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        band = v1;
        high = x - k * v1 - v2;
        return v2;
    }
    float lp(float x) { float b, h; return tick(x, b, h); }
    float hp(float x) { float b, h; tick(x, b, h); return h; }
};

// The same bits (not ==, which takes -0 for +0), for the set() early-outs below.
inline bool sameBits(float a, float b)
{
    uint32_t x, y;
    std::memcpy(&x, &a, sizeof x);
    std::memcpy(&y, &b, sizeof y);
    return x == y;
}

struct EraRng {
    uint32_t s = 0x2545F491u;
    float next() { s = s * 1664525u + 1013904223u; return float(int32_t(s)) * (1.0f / 2147483648.0f); } // -1..1
};

class BbdStage {
public:
    IV_SIZE_OPT void reset() { for (int c = 0; c < 2; ++c) { aa_[c].reset(); rc1_[c].reset(); rc2_[c].reset(); p1_[c] = p2_[c] = envY_[c] = envZ_[c] = held_[c] = 0; } }
    // Everything here follows t and the rate alone, so it is worked out again only when they move
    // (overnight review, 26 Oct 2026: 6 sin + 6 cos, 3 exp, 2 pow a chunk for a value that rests).
    IV_SIZE_OPT void set(float t, float sr)
    {
        if (setOk_ && sameBits(t, setT_) && sameBits(sr, setSr_)) return;
        setOk_ = true;
        setT_ = t;
        setSr_ = sr;
        const float g = t / voicing::kBbdGritAt; // grit: 1 at two-thirds, 1.5 at the end
        aliasMix_ = std::fmin(1.0f, g);
        t *= 0.5f / voicing::kBbdGritAt;          // the line's darkness and drive grow more slowly
        const float fc = 7000.0f * std::pow(2200.0f / 7000.0f, t);
        // All six filters sit at fc: one sin and cos; aa_ and rc2_ are the same filter.
        const float gf = EraSvf::gain(fc, sr);
        aa_[0].setGain(gf, 1.0f);
        rc1_[0].setGain(gf, 0.707f);
        for (int c = 0; c < 2; ++c) {
            aa_[c].copyCoefficients(aa_[0]);
            rc1_[c].copyCoefficients(rc1_[0]);
            rc2_[c].copyCoefficients(aa_[0]);
        }
        wp_ = 1.0f - std::exp(-6.2831853f * fc / sr);
        a_ = (0.02f + 0.06f * t) * (1.0f + g);
        b_ = (0.01f + 0.03f * t) * (1.0f + g);
        noise_ = std::pow(10.0f, (-66.0f + 12.0f * t + 6.0f * g) / 20.0f);
        wc_ = 1.0f - std::exp(-1.0f / (0.010f * sr));
        we_ = 1.0f - std::exp(-1.0f / ((0.012f + 0.013f * t) * (1.0f + 0.5f * g) * sr));
        clockInc_ = (2.6f - 0.4f * std::fmin(1.0f, std::fmax(0.0f, g - 1.0f))) * fc / sr;
    }
    float process(float x, int c)
    {
        constexpr float ref = 0.126f, floor = 1e-4f; // -18 dBFS: the compander's unity point
        const float two = aa_[c].lp(x);              // a weak anti-alias filter, then the clock's sample
        const float smooth = (p1_[c] += wp_ * (two - p1_[c]));
        if (c == 0) { clock_ += clockInc_; tick_ = clock_ >= 1.0f; if (tick_) clock_ -= 1.0f; }
        if (tick_) held_[c] = two;
        const float in = smooth + aliasMix_ * (held_[c] - smooth);
        const float y = in * ref / std::fmax(envY_[c], floor); // 2:1, rectifier on its own output
        envY_[c] += wc_ * (std::fabs(y) - envY_[c]);
        const float u = std::fmin(2.0f, std::fmax(-2.0f, y / ref * 0.5f)); // the compressor overshoots on onsets
        float z = (u - a_ * u * u - b_ * u * u * u) * 2.0f * ref;          // the line's soft 2nd harmonic
        z += noise_ * ref * rng_.next();
        envZ_[c] += we_ * (std::fabs(z) - envZ_[c]);
        const float e = z * std::fmax(envZ_[c], floor) / ref; // 1:2: the noise breathes with the level
        p2_[c] += wp_ * (rc1_[c].lp(e) - p2_[c]);
        return rc2_[c].lp(p2_[c]);
    }

private:
    EraSvf aa_[2], rc1_[2], rc2_[2];
    float  p1_[2] IV_ZI, p2_[2] IV_ZI, envY_[2] IV_ZI, envZ_[2] IV_ZI, held_[2] IV_ZI;
    float  wp_ = 1, a_ IV_ZI, b_ IV_ZI, noise_ IV_ZI, wc_ IV_ZI, we_ IV_ZI, clock_ IV_ZI, clockInc_ = 0.5f, aliasMix_ IV_ZI;
    bool   tick_ IV_ZI;
    EraRng rng_;
    bool   setOk_ IV_ZI; // set()'s last t and rate (set while setOk_)
    float  setT_, setSr_;
};

class RadioStage {
public:
    IV_SIZE_OPT void reset()
    {
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); lp1_[c].reset(); lp2_[c].reset(); dip_[c].reset(); }
        crashBp_.reset();
        agc_ = 1.0f;
        inPow_ = outPow_ = 0.0f;
        slow_ = hissLp_ = clickLp_ = clickEnv_ = strokeEnv_ = nfast_ = phHum_ = 0.0f;
    }
    IV_SIZE_OPT void set(float t, float sr, int blockLen)
    {
        // What follows t and the rate alone is worked out again only when they move (overnight review,
        // 26 Oct 2026; the same bits): 6 pow, 6 exp, 2 sqrt and the high-pass's sin and cos a chunk.
        // The wanders, the drift-offs and what follows them still run every chunk.
        if (!setOk_ || !sameBits(t, t_) || !sameBits(sr, sr_)) {
            setOk_ = true;
            const float kDb = 10.0f - 7.0f * t;
            steady_ = std::sqrt(std::pow(10.0f, kDb / 10.0f) * (2.0f / 3.0f));
            gNorm_ = std::sqrt(steady_ * steady_ + 2.0f / 3.0f);
            gFloor_ = std::pow(10.0f, (-12.0f - 8.0f * t) / 20.0f);
            hum_ = t > 0.5f ? 0.08f * (t - 0.5f) : 0.0f; // 0 -> 4 %
            humStep_ = 100.0f / sr;
            lpTop_ = 4500.0f * std::pow(2400.0f / 4500.0f, t);
            hp_[0].set(200.0f + 200.0f * t, 0.707f, sr);
            hp_[1].copyCoefficients(hp_[0]);
            hiss_ = std::pow(10.0f, (-52.0f + 14.0f * t) / 20.0f);
            wHiss_ = 1.0f - std::exp(-6.2831853f * (1200.0f + 1000.0f * t) / sr);
            crackleLvl_ = std::pow(10.0f, (-48.0f + 16.0f * t) / 20.0f);
            alpha_ = 2.5f - t;
            wNfast_ = 1.0f - std::exp(-6.2831853f * (3.0f + 9.0f * t) / sr);
            wa_ = 1.0f - std::exp(-1.0f / (0.005f * sr));
            wd_ = 1.0f - std::exp(-1.0f / ((0.5f - 0.25f * t) * sr));
            ws_ = 1.0f - std::exp(-1.0f / (0.5f * sr));
            maxBoost_ = std::pow(10.0f, (6.0f + 9.0f * t) / 20.0f);
            makeup_ = std::pow(10.0f, kMakeupDb / 20.0f);
            wHold_ = 1.0f - std::exp(-1.0f / (8.0f * sr));
        }
        sr_ = sr;
        t_ = t;
        const float dt = float(blockLen) / sr;
        wander(storm_[0], storm_[1], storm_[2], storm_[3], 0.02f, dt); // storm activity, about a minute
        const float storm = 0.65f + 0.35f * storm_[1];
        // The station's fade (Rician): K 10 -> 3 dB, spread 0.08 -> 0.5 Hz, floor -12 -> -20 dB.
        const float sigma = 0.08f + 0.42f * t;
        wander(fi_[0], fi_[1], fi_[2], fi_[3], sigma, dt);
        wander(fq_[0], fq_[1], fq_[2], fq_[3], sigma, dt);
        const float steady = steady_;
        float g = std::sqrt((steady + fi_[1]) * (steady + fi_[1]) + fq_[1] * fq_[1]) / gNorm_;
        g = std::fmax(g, gFloor_);
        // Detune: a slow wander, plus at the gritty end rare drift-offs (raised-cosine bumps).
        wander(dt_[0], dt_[1], dt_[2], dt_[3], 0.03f + 0.07f * t, dt);
        float detune = (0.075f + 0.175f * t) * (1.0f + dt_[1]);
        if (t > 0.5f) {
            driftGap_ -= dt;
            if (driftGap_ <= 0.0f && slow_ > 0.003f) {
                driftLen_ = 0.7f + 1.4f * (rng_.next() + 1.0f);
                driftPeak_ = 0.7f + 0.3f * rng_.next();
                driftPos_ = 0.0f;
                driftGap_ = std::fmax(15.0f, expo(25.0f));
            }
        }
        float bump = 0.0f;
        if (driftPos_ >= 0.0f) {
            driftPos_ += dt / driftLen_;
            if (driftPos_ >= 1.0f) driftPos_ = -1.0f;
            else bump = driftPeak_ * 0.5f * (1.0f - std::cos(6.2831853f * driftPos_));
        }
        detune = std::fmin(1.0f, detune + bump);
        gStep_ = (g * (1.0f - 0.8f * bump) - gNow_) / float(blockLen > 0 ? blockLen : 1);
        mEff_ = (0.9f + 0.5f * t) * (1.0f + 0.6f * detune) * (1.0f + 0.5f * (1.0f - g)); // distortion breathes with the fade
        fl_ = (0.02f + 0.13f * t) * detune;                                               // slope flutter on the skirt
        const float lpHz = lpTop_ * (1.0f - (0.1f + 0.25f * t) * detune);
        // Both sides' low-passes at lpHz, both dips at one frequency: one sin and cos each.
        const float gl = EraSvf::gain(lpHz, sr);
        lp1_[0].setGain(gl, 0.54f);
        lp2_[0].setGain(gl, 1.31f);
        lp1_[1].copyCoefficients(lp1_[0]);
        lp2_[1].copyCoefficients(lp2_[0]);
        wander(notch_[0], notch_[1], notch_[2], notch_[3], 0.1f + 0.4f * t, dt); // the wandering dip
        dip_[0].set(300.0f * std::pow(10.0f, 0.5f + 0.5f * notch_[1]), 1.5f + 1.5f * t, sr);
        dip_[1].copyCoefficients(dip_[0]);
        dipGain_ = std::pow(10.0f, (-4.0f - 14.0f * t) * (1.0f + 0.5f * (1.0f - g)) / 20.0f);
        crackleP_ = (3.0f + 37.0f * t) * storm / sr;
        crashMean_ = (25.0f - 20.0f * t) / storm;
    }
    void process(float& l, float& r)
    {
        constexpr float carrier = 0.25f; // -12 dBFS is full modulation
        slow_ += ws_ * (0.5f * (std::fabs(l) + std::fabs(r)) - slow_); // the station, measured before any fade
        gNow_ += gStep_;
        phHum_ += humStep_;
        phHum_ -= float(int(phHum_));
        // The mains hum rides on the signal. Without it (t up to 0.5) humMod is exactly 1 (1 + ±0), so
        // the sin is skipped (overnight review, 26 Oct 2026; the same bits).
        const float humMod = hum_ != 0.0f ? 1.0f + hum_ * std::sin(6.2831853f * phHum_) : 1.0f;
        nfast_ += wNfast_ * (rng_.next() - nfast_);
        const float prop = gNow_ * (1.0f + fl_ * nfast_ * 8.0f);
        // The receiver's noise (mono), not faded with the station.
        hissLp_ += wHiss_ * (0.5f * (rng_.next() + rng_.next()) - hissLp_);
        float n = hiss_ * hissLp_ * 2.0f;
        if (rng_.next() * 0.5f + 0.5f < crackleP_) { // a click: heavy-tailed level, its own colour and decay
            const float u = 0.5f * rng_.next() + 0.5f + 1e-3f;
            clickEnv_ = crackleLvl_ * std::fmin(20.0f, std::pow(u, -1.0f / alpha_)) * (rng_.next() > 0.0f ? 1.0f : -1.0f);
            clickDecay_ = std::exp(-1.0f / ((0.0002f + 0.0009f * (rng_.next() + 1.0f)) * sr_));
            wClick_ = 1.0f - std::exp(-6.2831853f * (700.0f + 1150.0f * (rng_.next() + 1.0f)) / sr_);
        }
        clickLp_ += wClick_ * (clickEnv_ * rng_.next() - clickLp_);
        clickEnv_ *= clickDecay_;
        n += clickLp_;
        if (--crashWait_ <= 0) { // lightning: a flash of 1-6 strokes, each its own level, one colour per flash
            if (strokesLeft_ <= 0) {
                strokesLeft_ = 1 + int(2.5f * (rng_.next() + 1.0f));
                crashBp_.set(500.0f * std::pow(5.0f, 0.5f + 0.5f * rng_.next()), 0.9f, sr_);
                flashLvl_ = std::pow(10.0f, (-42.0f + 14.0f * t_ + 4.0f * (rng_.next() + rng_.next())) / 20.0f);
            }
            strokeEnv_ = flashLvl_ * (0.5f + 0.5f * (rng_.next() + 1.0f));
            strokeDecay_ = std::exp(-1.0f / ((0.008f + 0.026f * (rng_.next() + 1.0f)) * sr_));
            --strokesLeft_;
            crashWait_ = strokesLeft_ > 0 ? int((0.02f + 0.05f * (rng_.next() + 1.0f)) * sr_) : int(expo(crashMean_) * sr_);
        }
        float b, h;
        crashBp_.tick(strokeEnv_ * rng_.next(), b, h);
        n += 3.0f * b; // the flash's own band
        strokeEnv_ *= strokeDecay_;
        n *= slow_ * 4.0f;
        float x[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            float v = x[c] * humMod;
            v = std::fmax(v / carrier * mEff_, -1.0f) / mEff_ * carrier; // over-modulation flattens the troughs
            v *= prop;
            dip_[c].tick(v, b, h);
            v += (dipGain_ - 1.0f) * dip_[c].k * b;
            v = hp_[c].hp(v + n);
            x[c] = lp2_[c].lp(lp1_[c].lp(v));
        }
        // The AGC follows the carrier, as a real set's does: it gives back the station's fade (quickly when the
        // station comes back, slowly when it goes), lifting the receiver's noise with it, so the static swells
        // as the station dips. It never lifts the music as a whole (aiming at a fixed level made sparse music
        // 6 dB louder), and the band-limiting's level is a fixed trim (makeup_).
        const float want = std::fmin(maxBoost_, 1.0f / std::fmax(gNow_, 1e-3f));
        agc_ += (want < agc_ ? wa_ : wd_) * (want - agc_);
        // A slow level hold (~8 s): the receiver's band keeps the output's level tracking the input's, within
        // ±12 dB, too slowly to undo the fades. Without it a held infinite tail, which darkens towards its warm
        // floor (ADR 0008), lost more and more of itself below the band: 6 dB over six minutes.
        inPow_ += wHold_ * (0.5f * (l * l + r * r) - inPow_);
        outPow_ += wHold_ * (0.5f * (x[0] * x[0] + x[1] * x[1]) * agc_ * agc_ - outPow_); // after the AGC: the band's loss only
        const float hold = std::fmin(4.0f, std::fmax(0.25f, std::sqrt((inPow_ + 1e-20f) / (outPow_ + 1e-20f))));
        l = x[0] * agc_ * hold * makeup_;
        r = x[1] * agc_ * hold * makeup_;
    }

private:
    static constexpr float kMakeupDb = 0.0f; // music LUFS against the Neve base (era tests)
    IV_NOINLINE float expo(float mean) { return -mean * std::log(0.5f * rng_.next() + 0.5f + 1e-4f); }
    // A smooth random line: a new target every ~1/hz seconds, smoothed twice (s = {a, b, target, countdown}).
    IV_NOINLINE IV_SIZE_OPT void wander(float& a, float& b, float& target, float& count, float hz, float dt)
    {
        count -= dt;
        if (count <= 0.0f) { target = rng_.next(); count = (0.7f + 0.6f * (0.5f + 0.5f * rng_.next())) / hz; }
        const float w = 1.0f - std::exp(-6.2831853f * hz * dt);
        a += w * (target - a);
        b += w * (a - b);
    }
    EraSvf hp_[2], lp1_[2], lp2_[2], dip_[2], crashBp_;
    EraRng rng_;
    float  sr_ = 48000, t_ IV_ZI, agc_ = 1, slow_ IV_ZI, hissLp_ IV_ZI, wHiss_ = 0.1f, hiss_ IV_ZI, maxBoost_ = 1, makeup_ = 1, inPow_ IV_ZI, outPow_ IV_ZI, wHold_ IV_ZI;
    float  clickLp_ IV_ZI, clickEnv_ IV_ZI, clickDecay_ IV_ZI, wClick_ = 0.1f, crackleP_ IV_ZI, crackleLvl_ IV_ZI, alpha_ = 2;
    float  strokeEnv_ IV_ZI, strokeDecay_ IV_ZI, flashLvl_ IV_ZI, crashMean_ = 20;
    int    crashWait_ = 48000, strokesLeft_ IV_ZI;
    float  storm_[4] IV_ZI, fi_[4] IV_ZI, fq_[4] IV_ZI, dt_[4] IV_ZI, notch_[4] IV_ZI;
    float  driftGap_ = 8, driftLen_ = 1, driftPeak_ IV_ZI, driftPos_ = -1;
    float  gNow_ = 1, gStep_ IV_ZI, mEff_ = 1, fl_ IV_ZI, hum_ IV_ZI, nfast_ IV_ZI, wNfast_ IV_ZI;
    float  dipGain_ = 1, wa_ IV_ZI, wd_ IV_ZI, ws_ IV_ZI, phHum_ IV_ZI;
    bool   setOk_ IV_ZI; // set()'s t-and-rate part is in place for t_ and sr_ (and these set)
    float  steady_, gNorm_, gFloor_, humStep_, lpTop_;
};

} // namespace iv::dsp
