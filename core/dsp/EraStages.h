#pragma once
// era_v2 prototype: three candidate eras for the redesigned ERA knob (docs/research/new-eras.md), each on
// the wet after the loop, so none can build up in the tank. Each takes an intensity t (0 = its sweet end,
// 1 = its gritty end) once per block.
//   Bbd:     a bucket-brigade line's sound without its delay (Raffel & Smith, DAFx-10): steep low-pass at a
//            third of the clock, the NE570 compander 2:1 / 1:2 with noise between (its breathing), a soft
//            2nd harmonic. No clock whine (tonal, a metallic risk).
//   Sampler: an early sampler (Fairlight / Mirage / SP-1200): a weak pre-filter, a sample-and-hold at its
//            own rate (no anti-aliasing to speak of, so it folds), 12 → 8 bits without dither, then a
//            resonant 3-pole low-pass that follows the rate (the SSM2044 / CEM3328 stand-in).
//   Radio:   AM / shortwave: over-modulation clip, a 200-400 Hz high-pass and a 4.5-2.4 kHz 4th-order
//            low-pass (receiver bandwidth), one slowly wandering dip (selective fading; one dip, not a
//            comb, which would flange), flat fades, an AGC, and crackle and hiss that follow the tail.
#include <cmath>
#include <cstdint>

namespace iv::dsp {

// Simper's trapezoidal state-variable filter (low, band, high from one state).
struct EraSvf {
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1.4f;
    void set(float fc, float q, float sr)
    {
        const float g = std::tan(3.14159265f * std::fmin(fc, 0.45f * sr) / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
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

struct EraRng {
    uint32_t s = 0x2545F491u;
    float next() { s = s * 1664525u + 1013904223u; return float(int32_t(s)) * (1.0f / 2147483648.0f); } // -1..1
};

class BbdStage {
public:
    void reset() { for (int c = 0; c < 2; ++c) { aa_[c].reset(); rc1_[c].reset(); rc2_[c].reset(); p1_[c] = p2_[c] = envY_[c] = envZ_[c] = 0; } }
    // grit (round 2, owner: "a little more grit"): 0 = round 1; 1, 2 = the chip driven harder (x2, x3 its
    // distortion), its compander noise 6 / 12 dB up and breathing slower, and the clock's own aliasing: the
    // line holds each sample for a clock period (2.6 / 2.2 x the cutoff) behind a weaker anti-alias filter
    // (2-pole / 1-pole), as a cheap long-delay BBD does.
    // grit 3 (round 3, owner: "BBD more is preferred but... make that the 0.66 position"): grit grows across
    // the stretch, reaching round 2's B (grit 1 halfway in) at two-thirds, and half as much again by the end;
    // the clock's aliasing fades in with it.
    void set(float t, float sr, int grit = 0)
    {
        float g = float(grit);
        aliasMix_ = grit ? 1.0f : 0.0f;
        if (grit == 3) {
            g = t / 0.66f;
            aliasMix_ = std::fmin(1.0f, g);
            t *= 0.5f / 0.66f;
        }
        const float fc = 7000.0f * std::pow(2200.0f / 7000.0f, t);
        for (int c = 0; c < 2; ++c) { aa_[c].set(fc, 1.0f, sr); rc1_[c].set(fc, 0.707f, sr); rc2_[c].set(fc, 1.0f, sr); }
        wp_ = 1.0f - std::exp(-6.2831853f * fc / sr);
        a_ = (0.02f + 0.06f * t) * (1.0f + g);
        b_ = (0.01f + 0.03f * t) * (1.0f + g);
        noise_ = std::pow(10.0f, (-66.0f + 12.0f * t + 6.0f * g) / 20.0f);
        wc_ = 1.0f - std::exp(-1.0f / (0.010f * sr));
        we_ = 1.0f - std::exp(-1.0f / ((0.012f + 0.013f * t) * (1.0f + 0.5f * g) * sr));
        grit_ = grit;
        clockInc_ = (g <= 1.0f ? 2.6f : 2.6f - 0.4f * std::fmin(1.0f, g - 1.0f)) * fc / sr;
    }
    float process(float x, int c)
    {
        constexpr float ref = 0.126f, floor = 1e-4f; // -18 dBFS: the compander's unity point
        float in;
        if (grit_ == 0) in = (p1_[c] += wp_ * (aa_[c].lp(x) - p1_[c])); // anti-alias: 2-pole + 1-pole
        else if (grit_ == 3) {
            const float two = aa_[c].lp(x); // the weaker anti-alias filter, sampled by the clock
            const float smooth = (p1_[c] += wp_ * (two - p1_[c]));
            if (c == 0) { clock_ += clockInc_; tick_ = clock_ >= 1.0f; if (tick_) clock_ -= 1.0f; }
            if (tick_) held_[c] = two;
            in = smooth + aliasMix_ * (held_[c] - smooth);
        } else {
            in = grit_ == 1 ? aa_[c].lp(x) : (p1_[c] += wp_ * (x - p1_[c])); // weaker: 2-pole / 1-pole
            if (c == 0) { clock_ += clockInc_; tick_ = clock_ >= 1.0f; if (tick_) clock_ -= 1.0f; }
            if (tick_) held_[c] = in; // the clock's sample, held until the next
            in = held_[c];
        }
        const float y = in * ref / std::fmax(envY_[c], floor); // 2:1, rectifier on its own output
        envY_[c] += wc_ * (std::fabs(y) - envY_[c]);
        const float u = std::fmin(2.0f, std::fmax(-2.0f, y / ref * 0.5f)); // the compressor overshoots on onsets
        float z = (u - a_ * u * u - b_ * u * u * u) * 2.0f * ref; // the line's soft 2nd harmonic
        z += noise_ * ref * rng_.next();
        envZ_[c] += we_ * (std::fabs(z) - envZ_[c]);
        const float e = z * std::fmax(envZ_[c], floor) / ref; // 1:2: the noise breathes with the level
        p2_[c] += wp_ * (rc1_[c].lp(e) - p2_[c]);
        return rc2_[c].lp(p2_[c]);
    }

private:
    EraSvf aa_[2], rc1_[2], rc2_[2];
    float  p1_[2] = {}, p2_[2] = {}, envY_[2] = {}, envZ_[2] = {};
    float  wp_ = 1, a_ = 0, b_ = 0, noise_ = 0, wc_ = 0, we_ = 0, clock_ = 0, clockInc_ = 0.5f, held_[2] = {};
    int    grit_ = 0;
    float  aliasMix_ = 0;
    bool   tick_ = false;
    EraRng rng_;
};

class SamplerStage {
public:
    void reset() { for (int c = 0; c < 2; ++c) { pre_[c].reset(); post_[c].reset(); pole_[c] = held_[c] = 0; } phase_ = 0; }
    void set(float t, float sr)
    {
        const float rate = 31250.0f * std::pow(12000.0f / 31250.0f, t);
        inc_ = rate / sr;
        step_ = 2.0f / std::exp2(12.0f - 4.0f * t);
        drive_ = std::pow(10.0f, 12.0f * t / 20.0f);
        for (int c = 0; c < 2; ++c) { pre_[c].set(0.5f * rate, 0.6f, sr); post_[c].set(0.42f * rate, 0.9f + 0.4f * t, sr); }
        wp_ = 1.0f - std::exp(-6.2831853f * 0.42f * rate / sr);
    }
    void process(float& l, float& r)
    {
        float x[2] = {pre_[0].lp(l * drive_), pre_[1].lp(r * drive_)};
        phase_ += inc_;
        if (phase_ >= 1.0f) { // a new sample: hold it, coarsely, until the next
            phase_ -= 1.0f;
            for (int c = 0; c < 2; ++c) held_[c] = std::round(x[c] / step_) * step_;
        }
        for (int c = 0; c < 2; ++c) {
            pole_[c] += wp_ * (post_[c].lp(held_[c]) - pole_[c]);
            x[c] = pole_[c] / drive_;
        }
        l = x[0];
        r = x[1];
    }

private:
    EraSvf pre_[2], post_[2];
    float  pole_[2] = {}, held_[2] = {}, phase_ = 0, inc_ = 0.6f, step_ = 1e-3f, drive_ = 1, wp_ = 1;
};

class RadioStage {
public:
    void reset()
    {
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); lp1_[c].reset(); lp2_[c].reset(); dip_[c].reset(); }
        agc_ = slow_ = 0; fadeA_ = fadeB_ = notchA_ = notchB_ = 0; fadeCount_ = notchCount_ = 0;
    }
    // noise (round 2, owner: "some light noise, hiss, or even radio artifacts"): 0 = round 1; 1 = audible
    // band-limited hiss, more crackle, and now and then a burst of static (atmospherics); 2 = that plus a faint
    // heterodyne whistle drifting slowly (two stations beating), the classic shortwave artifact. All of it
    // follows the tail, so silence stays silent.
    void set(float t, float sr, int blockLen, int noise = 0)
    {
        noiseMode_ = noise;
        sr_ = sr;
        m_ = 0.9f + 0.5f * t;
        dipDb_ = -4.0f - 14.0f * t;
        dipQ_ = 1.5f + 1.5f * t;
        notchHz_ = 0.1f + 0.4f * t;
        fadeDb_ = 1.0f + 5.0f * t;
        maxBoost_ = std::pow(10.0f, (6.0f + 9.0f * t) / 20.0f);
        crackle_ = (noise ? 6.0f + 24.0f * t : 2.0f + 18.0f * t) / sr;
        crackleAmp_ = noise ? 0.15f : 0.06f;
        hiss_ = std::pow(10.0f, ((noise ? -36.0f : -60.0f) + (noise ? 6.0f : 20.0f) * t) / 20.0f);
        for (int c = 0; c < 2; ++c) { hp_[c].set(200.0f + 200.0f * t, 0.707f, sr); lp1_[c].set(4500.0f * std::pow(2400.0f / 4500.0f, t), 0.54f, sr); lp2_[c].set(4500.0f * std::pow(2400.0f / 4500.0f, t), 1.31f, sr); }
        // The selective fade's centre and the flat fade wander on smoothed random targets.
        const float dt = float(blockLen) / sr;
        wander(notchA_, notchB_, notchTarget_, notchCount_, notchHz_, dt);
        wander(fadeA_, fadeB_, fadeTarget_, fadeCount_, 0.05f, dt);
        if (noise) {
            // Static bursts: every 3-8 s, 0.15-0.4 s long.
            burstCount_ -= dt;
            if (burstCount_ <= 0.0f) { burstCount_ = 3.0f + 2.5f * (rng_.next() + 1.0f); burstLeft_ = 0.15f + 0.125f * (rng_.next() + 1.0f); }
            burstLeft_ -= dt;
            burstGain_ += 0.3f * ((burstLeft_ > 0.0f ? 0.1f : 0.0f) - burstGain_);
        }
        if (noise == 2) {
            wander(whA_, whB_, whTarget_, whCount_, 0.05f, dt);
            whInc_ = (1500.0f + 900.0f * whB_) / sr; // 600 Hz - 2.4 kHz
        }
        const float centre = 300.0f * std::pow(10.0f, 0.5f + 0.5f * notchB_);
        for (int c = 0; c < 2; ++c) dip_[c].set(centre, dipQ_, sr);
        dipGain_ = std::pow(10.0f, dipDb_ / 20.0f);
        flat_ = std::pow(10.0f, fadeDb_ * fadeB_ / 20.0f);
        wa_ = 1.0f - std::exp(-1.0f / (0.005f * sr));
        wd_ = 1.0f - std::exp(-1.0f / (0.25f * sr));
        ws_ = 1.0f - std::exp(-1.0f / (0.5f * sr));
    }
    void process(float& l, float& r)
    {
        constexpr float carrier = 0.25f, target = 0.1f; // -12 dBFS is full modulation; the AGC aims at -20
        slow_ += ws_ * (0.5f * (std::fabs(l) + std::fabs(r)) - slow_);
        float n = hiss_ * rng_.next() + (rng_.next() * 0.5f + 0.5f < crackle_ ? crackleAmp_ * rng_.next() : 0.0f);
        if (noiseMode_) n += burstGain_ * rng_.next();
        if (noiseMode_ == 2) {
            whPhase_ += whInc_;
            whPhase_ -= float(int(whPhase_));
            n += 0.012f * std::sin(6.2831853f * whPhase_);
        }
        n *= slow_ * 4.0f;
        float x[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            float v = std::fmax(x[c] / carrier * m_, -1.0f) / m_ * carrier; // over-modulation flattens the troughs
            v = hp_[c].hp(v + n);
            v = lp2_[c].lp(lp1_[c].lp(v));
            float b, h;
            dip_[c].tick(v, b, h);
            x[c] = (v + (dipGain_ - 1.0f) * dip_[c].k * b) * flat_;
        }
        const float lev = 0.5f * (std::fabs(x[0]) + std::fabs(x[1]));
        agc_ += (lev > agc_ ? wa_ : wd_) * (lev - agc_);
        const float g = std::fmin(maxBoost_, std::fmax(1.0f, target / std::fmax(agc_, 1e-6f)));
        l = x[0] * g;
        r = x[1] * g;
    }

private:
    void wander(float& a, float& b, float& target, float& count, float hz, float dt)
    {
        count -= dt;
        if (count <= 0.0f) { target = rng_.next(); count = (0.7f + 0.6f * (0.5f + 0.5f * rng_.next())) / hz; }
        const float w = 1.0f - std::exp(-6.2831853f * hz * dt);
        a += w * (target - a);
        b += w * (a - b);
    }
    EraSvf hp_[2], lp1_[2], lp2_[2], dip_[2];
    EraRng rng_;
    float  sr_ = 48000, m_ = 1, dipDb_ = 0, dipQ_ = 1, notchHz_ = 0.1f, fadeDb_ = 0, maxBoost_ = 1, crackle_ = 0, hiss_ = 0;
    float  agc_ = 0, slow_ = 0, dipGain_ = 1, flat_ = 1, wa_ = 0, wd_ = 0, ws_ = 0;
    float  fadeA_ = 0, fadeB_ = 0, fadeTarget_ = 0, fadeCount_ = 0, notchA_ = 0, notchB_ = 0, notchTarget_ = 0, notchCount_ = 0;
    float  crackleAmp_ = 0.06f, burstCount_ = 2.0f, burstLeft_ = 0, burstGain_ = 0;
    float  whA_ = 0, whB_ = 0, whTarget_ = 0, whCount_ = 0, whInc_ = 0.03f, whPhase_ = 0;
    int    noiseMode_ = 0;
};


// Radio, round 3 (owner, 25 Oct 2026: softer hiss; static too uniform in colour, interval and length; the
// whistle too steady; docs/research/radio-artifacts.md). The order of a real set: the station fades on its
// own path while the receiver's noise stays put, then the AGC lifts both, so hiss and static swell as the
// station dips. Station fading is Watterson's (ITU-R F.1487): a Rician gain from slowly wandering I and Q.
// Static is three layers (storm activity over minutes, Pareto crackle, lightning flashes of 1-6 strokes,
// each with its own colour); whistles live 4-30 s at a nearly fixed pitch with a Doppler wobble and their
// own fading, some as sub-audio throbs; a slow detune dulls, distorts and flutters on the filter's skirt;
// at the gritty end, rare drift-offs (with a whoop through zero beat), flutter episodes and a faint
// mains hum. The wandering dip is round 1's, untouched. All added noise is mono and follows the tail.
class RadioStage2 {
public:
    void reset()
    {
        for (int c = 0; c < 2; ++c) { hp_[c].reset(); lp1_[c].reset(); lp2_[c].reset(); dip_[c].reset(); }
        crashBp_.reset();
        agc_ = slow_ = hissLp_ = clickLp_ = clickEnv_ = strokeEnv_ = nfast_ = 0.0f;
        phHum_ = phWh_ = phThrob_ = phWhoop_ = 0.0f;
    }
    void set(float t, float sr, int blockLen, bool noNoise = false)
    {
        sr_ = sr;
        noNoise_ = noNoise;
        const float dt = float(blockLen) / sr;
        t_ = t;
        // Storm activity: comes and goes over about a minute.
        wander(stormA_, stormB_, stormT_, stormC_, 0.02f, dt);
        storm_ = 0.65f + 0.35f * stormB_;
        // Station fade (Rician): K 10 -> 3 dB, spread 0.08 -> 0.5 Hz; flutter episodes at the gritty end.
        float sigma = 0.08f + 0.42f * t, kDb = 10.0f - 7.0f * t;
        if (t > 0.5f) {
            flutterGap_ -= dt;
            if (flutterGap_ <= 0.0f) { flutterLeft_ = 2.0f + 4.0f * (rng_.next() + 1.0f); flutterGap_ = expo(45.0f) + flutterLeft_; }
        }
        if (flutterLeft_ > 0.0f) { flutterLeft_ -= dt; sigma = 5.0f + 20.0f * (t - 0.5f); kDb = 3.0f; } // 5-15 Hz: polar flutter
        wander(fiA_, fiB_, fiT_, fiC_, sigma, dt);
        wander(fqA_, fqB_, fqT_, fqC_, sigma, dt);
        const float steady = std::sqrt(std::pow(10.0f, kDb / 10.0f) * (2.0f / 3.0f));
        float g = std::sqrt((steady + fiB_) * (steady + fiB_) + fqB_ * fqB_) / std::sqrt(steady * steady + 2.0f / 3.0f);
        g = std::fmax(g, std::pow(10.0f, (-12.0f - 8.0f * t) / 20.0f));
        // Detune: a slow wander plus, at the gritty end, rare drift-offs (raised-cosine bumps).
        wander(dtA_, dtB_, dtT_, dtC_, 0.03f + 0.07f * t, dt);
        float detune = (0.075f + 0.175f * t) * (1.0f + dtB_);
        if (t > 0.5f) {
            driftGap_ -= dt;
            if (driftGap_ <= 0.0f && slow_ > 0.003f) {
                driftLen_ = 0.7f + 1.4f * (rng_.next() + 1.0f);
                driftPeak_ = 0.7f + 0.3f * rng_.next();
                driftPos_ = 0.0f;
                driftGap_ = std::fmax(15.0f, expo(25.0f));
                whooped_ = false;
            }
        }
        float bump = 0.0f;
        if (driftPos_ >= 0.0f) {
            driftPos_ += dt / driftLen_;
            if (driftPos_ >= 1.0f) driftPos_ = -1.0f;
            else bump = driftPeak_ * 0.5f * (1.0f - std::cos(6.2831853f * driftPos_));
            if (bump > 0.5f && !whooped_) { whooped_ = true; whoopLeft_ = 0.4f + 0.55f * (rng_.next() + 1.0f); whoopLen_ = whoopLeft_; }
        }
        detune = std::fmin(1.0f, detune + bump);
        // Whistle voice: off / on with lifetimes, nearly fixed pitch, Doppler wobble, own fading.
        if (whLeft_ > 0.0f) {
            whLeft_ -= dt;
            whAge_ += dt;
            wander(wdA_, wdB_, wdT_, wdC_, 0.2f + 0.3f * t, dt);
            wander(wfiA_, wfiB_, wfiT_, wfiC_, 0.1f + 0.3f * t, dt);
            wander(wfqA_, wfqB_, wfqT_, wfqC_, 0.1f + 0.3f * t, dt);
            const float ks = std::sqrt(std::pow(10.0f, (6.0f - 6.0f * t) / 10.0f) * (2.0f / 3.0f));
            const float wg = std::sqrt((ks + wfiB_) * (ks + wfiB_) + wfqB_ * wfqB_) / std::sqrt(ks * ks + 2.0f / 3.0f);
            const float edge = std::fmin(1.0f, std::fmin(whAge_, whLeft_) / whFade_);
            whPeak_ = std::pow(10.0f, (-42.0f + 14.0f * t) / 20.0f);
            whGain_ = whPeak_ * wg * std::fmax(0.0f, edge);
            whHz_ = whF0_ + (0.5f + 2.5f * t) * wdB_ + whDrift_ * whAge_ / whLife_;
        } else {
            whGain_ = 0.0f;
            whGap_ -= dt;
            if (whGap_ <= 0.0f) {
                whLife_ = 4.0f * std::pow(7.5f, 0.5f + 0.5f * rng_.next()) * (0.6f + 0.4f * t + 0.4f);
                whLeft_ = whLife_;
                whAge_ = 0.0f;
                whFade_ = 1.0f + (rng_.next() + 1.0f);
                whThrob_ = rng_.next() * 0.5f + 0.5f < 0.5f - 0.2f * t;
                whF0_ = whThrob_ ? 2.0f + 6.5f * (rng_.next() + 1.0f) : 600.0f * std::pow(3.0f + 1.2f * t, 0.5f + 0.5f * rng_.next());
                whDrift_ = (5.0f + 15.0f * t) * rng_.next();
                whGap_ = expo(60.0f - 45.0f * t) + whLife_;
            }
        }
        // Coefficients.
        gTarget_ = g * (1.0f - 0.8f * bump);
        mEff_ = (0.9f + 0.5f * t) * (1.0f + 0.6f * detune) * (1.0f + 0.5f * (1.0f - g));
        fl_ = (0.02f + 0.13f * t) * detune;
        hum_ = t > 0.5f ? 0.04f * (t - 0.5f) * 2.0f : 0.0f;
        const float lpHz = 4500.0f * std::pow(2400.0f / 4500.0f, t) * (1.0f - (0.1f + 0.25f * t) * detune);
        for (int c = 0; c < 2; ++c) { hp_[c].set(200.0f + 200.0f * t, 0.707f, sr); lp1_[c].set(lpHz, 0.54f, sr); lp2_[c].set(lpHz, 1.31f, sr); }
        wander(notchA_, notchB_, notchT_, notchC_, 0.1f + 0.4f * t, dt); // round 1's wandering dip
        for (int c = 0; c < 2; ++c) dip_[c].set(300.0f * std::pow(10.0f, 0.5f + 0.5f * notchB_), 1.5f + 1.5f * t, sr);
        dipGain_ = std::pow(10.0f, (-4.0f - 14.0f * t) * (1.0f + 0.5f * (1.0f - g)) / 20.0f);
        hiss_ = std::pow(10.0f, (-52.0f + 14.0f * t) / 20.0f); // softer than round 2 (owner)
        wHiss_ = 1.0f - std::exp(-6.2831853f * (1200.0f + 1000.0f * t) / sr);
        crackleP_ = (3.0f + 37.0f * t) * storm_ / sr;
        crackleLvl_ = std::pow(10.0f, (-48.0f + 16.0f * t) / 20.0f);
        alpha_ = 2.5f - t;
        crashMean_ = (25.0f - 20.0f * t) / storm_;
        wNfast_ = 1.0f - std::exp(-6.2831853f * (3.0f + 9.0f * t) / sr);
        wa_ = 1.0f - std::exp(-1.0f / (0.005f * sr));
        wd_ = 1.0f - std::exp(-1.0f / ((0.5f - 0.25f * t) * sr));
        ws_ = 1.0f - std::exp(-1.0f / (0.5f * sr));
        gStep_ = (gTarget_ - gNow_) / float(blockLen > 0 ? blockLen : 1);
        whStep_ = (whGain_ - whNow_) / float(blockLen > 0 ? blockLen : 1);
        if (whoopLeft_ > 0.0f) whoopLeft_ -= dt;
    }
    void process(float& l, float& r)
    {
        constexpr float carrier = 0.25f, target = 0.1f;
        slow_ += ws_ * (0.5f * (std::fabs(l) + std::fabs(r)) - slow_); // the station, measured before any fade
        gNow_ += gStep_;
        whNow_ += whStep_;
        // Mains hum rides on the signal (silent with it), then the transmitter's over-modulation.
        phHum_ += 100.0f / sr_;
        phHum_ -= float(int(phHum_));
        const float humMod = 1.0f + hum_ * std::sin(6.2831853f * phHum_);
        nfast_ += wNfast_ * (rng_.next() - nfast_);
        float prop = gNow_ * (1.0f + fl_ * nfast_ * 8.0f);
        if (whThrob_ && whNow_ > 0.0f) { // a co-channel beat below hearing: a warble of the level
            phThrob_ += whF0_ / sr_;
            phThrob_ -= float(int(phThrob_));
            prop *= 1.0f + (0.1f + 0.2f * t_) * (whNow_ / whPeak_) * std::sin(6.2831853f * phThrob_); // 10-30 %
        }
        // Receiver noise (mono), not faded with the station.
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
        if (!whThrob_ && whNow_ > 0.0f) { // a whistle: nearly fixed pitch, a little phase noise, a faint 2nd
            phWh_ += whHz_ * (1.0f + 0.002f * rng_.next()) / sr_;
            phWh_ -= float(int(phWh_));
            n += whNow_ * (std::sin(6.2831853f * phWh_) + 0.15f * std::sin(12.566371f * phWh_));
        }
        if (whoopLeft_ > 0.0f) { // tuning through zero beat: about 2.5 kHz down to nothing and back up
            const float x = 1.0f - whoopLeft_ / whoopLen_, hz = 2500.0f * std::fabs(1.0f - 1.6f * x) + 5.0f;
            phWhoop_ += hz / sr_;
            phWhoop_ -= float(int(phWhoop_));
            n += 0.03f * std::sin(6.2831853f * phWhoop_);
        }
        n *= noNoise_ ? 0.0f : slow_ * 4.0f;
        float x[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            float v = x[c] * humMod;
            v = std::fmax(v / carrier * mEff_, -1.0f) / mEff_ * carrier;
            v *= prop;
            dip_[c].tick(v, b, h);
            v += (dipGain_ - 1.0f) * dip_[c].k * b;
            v = hp_[c].hp(v + n);
            x[c] = lp2_[c].lp(lp1_[c].lp(v));
        }
        const float lev = 0.5f * (std::fabs(x[0]) + std::fabs(x[1]));
        agc_ += (lev > agc_ ? wa_ : wd_) * (lev - agc_);
        const float g = std::fmin(maxBoost(), std::fmax(1.0f, target / std::fmax(agc_, 1e-6f)));
        l = x[0] * g;
        r = x[1] * g;
    }

private:
    float maxBoost() const { return std::pow(10.0f, (6.0f + 9.0f * t_) / 20.0f); }
    float expo(float mean) { return -mean * std::log(0.5f * rng_.next() + 0.5f + 1e-4f); }
    void wander(float& a, float& b, float& target, float& count, float hz, float dt)
    {
        count -= dt;
        if (count <= 0.0f) { target = rng_.next(); count = (0.7f + 0.6f * (0.5f + 0.5f * rng_.next())) / hz; }
        const float w = 1.0f - std::exp(-6.2831853f * hz * dt);
        a += w * (target - a);
        b += w * (a - b);
    }
    EraSvf hp_[2], lp1_[2], lp2_[2], dip_[2], crashBp_;
    EraRng rng_;
    float  sr_ = 48000, t_ = 0, agc_ = 0, slow_ = 0, hissLp_ = 0, wHiss_ = 0.1f, hiss_ = 0;
    float  clickLp_ = 0, clickEnv_ = 0, clickDecay_ = 0, wClick_ = 0.1f, crackleP_ = 0, crackleLvl_ = 0, alpha_ = 2;
    float  strokeEnv_ = 0, strokeDecay_ = 0, flashLvl_ = 0, crashMean_ = 20;
    int    crashWait_ = 48000, strokesLeft_ = 0;
    float  stormA_ = 0, stormB_ = 0, stormT_ = 0, stormC_ = 0, storm_ = 1;
    float  fiA_ = 0, fiB_ = 0, fiT_ = 0, fiC_ = 0, fqA_ = 0, fqB_ = 0, fqT_ = 0, fqC_ = 0, flutterGap_ = 20, flutterLeft_ = 0;
    float  dtA_ = 0, dtB_ = 0, dtT_ = 0, dtC_ = 0, driftGap_ = 8, driftLen_ = 1, driftPeak_ = 0, driftPos_ = -1;
    bool   whooped_ = false, whThrob_ = false;
    float  whoopLeft_ = 0, whoopLen_ = 1, phWhoop_ = 0;
    float  whPeak_ = 0.01f, whLeft_ = 0, whAge_ = 0, whGap_ = 6, whLife_ = 10, whFade_ = 2, whF0_ = 1000, whDrift_ = 0, whHz_ = 1000, whGain_ = 0;
    float  wdA_ = 0, wdB_ = 0, wdT_ = 0, wdC_ = 0, wfiA_ = 0, wfiB_ = 0, wfiT_ = 0, wfiC_ = 0, wfqA_ = 0, wfqB_ = 0, wfqT_ = 0, wfqC_ = 0;
    float  gTarget_ = 1, gNow_ = 1, gStep_ = 0, whNow_ = 0, whStep_ = 0, mEff_ = 1, fl_ = 0, hum_ = 0, nfast_ = 0, wNfast_ = 0;
    float  dipGain_ = 1, wa_ = 0, wd_ = 0, ws_ = 0, phHum_ = 0, phWh_ = 0, phThrob_ = 0;
    float  notchA_ = 0, notchB_ = 0, notchT_ = 0, notchC_ = 0;
    bool   noNoise_ = false; // measuring: round 3 without its added noise
};

} // namespace iv::dsp
