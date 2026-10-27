#pragma once
// MOTION's left side as tape (v1.0.46; owner's picks on renders/drift_rethink: round 3's E, the confirm page's B;
// docs/research/drift-rethink.md): a tape transport on the wet, after the tank and outside the loop. One short delay
// per channel, both read at the same moving point, so the whole tail bends together as one tape speed would (the
// tank's per-line wow averages out in a diffuse tail, so the tank keeps only a third of it on this side).
//   wow:      mostly random (two smoothed one-poles) over a sine whose rate drifts, 0.5 -> 1.2 Hz, ±12 c, and over
//             the side's last half ~±20 c with "variance" (ChowTape's Drift, Kinotone Ribbons' wow ceiling): the
//             speed re-picked every 0.5-2 s from 40 % slower to 2x quicker, a slow drift under it, now and then a
//             swing half as big again;
//   flutter:  two sines at 6 -> 10 Hz, ±3 -> 4.5 c, moving the level ±0.3 dB too;
//   crinkle:  as Strymon Magneto's (friction and creases): a bed of scrape roughness (fast random pitch around
//             100-200 Hz) building over the last 40 % of the side, and bursts of 0.1-0.6 s of jittery pitch (~45 Hz,
//             up to ±12 c); a counted share of the bursts (0.43: about a third of round 2's crinkle dips, 15 a minute
//             at full left) also bring round 2's dip: the level rough by 1-3.5 dB (median -1.6), the highs toward
//             ~5 kHz, 50-300 ms, on random numbers of their own.
// Everything moves once per control chunk, ramped across it and smoothed at 150 Hz per sample.
// The read point's centre is kBase (1000 samples, 20.8 ms at 48 kHz): the wet arrives that much later on the left
// side. Off the left side the transport is out (the wet bit for bit as without it); crossing in or out, the centre
// glides between kMin and kBase over kGlideSeconds (a smoothstep: a slow speed change of at most ~11 c, as a tape
// coming up to speed) and the transport crossfades with the direct wet at kMin over ~5 ms.
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstdint>

namespace iv::dsp {

struct TransportAmounts {
    float wowCents = 0, wowHz = 0.8f;      // the capstan's wow: a wandering sine plus a random part
    float wowSine = 0.3f;                  // the sine's share of it
    float flutterCents = 0, flutterHz = 8; // flutter: two sines
    float crinkleRate = 0;                 // crinkle bursts a second (jittered-regular)
    float variance = 0;                    // 0..1: the wow's rate wanders, a slow drift, bigger swings
    float crinkleDip = 0;                  // the share of bursts that bring a dip (counted)
};

class Transport {
public:
    static constexpr int   kSize = 2048;    // samples a channel: 43 ms at 48 kHz
    static constexpr float kBase = 1000.0f; // the read point's centre on the left side, samples: ±20 c at 0.3 Hz swings ±6 ms
    static constexpr float kMin = 8.0f;     // the centre while gliding in or out (the cubic read's room, and the jitter's)
    static constexpr float kGlideSeconds = 5.0f;
    static constexpr int   kFadeChunks = 8; // the crossfade with the direct wet, chunks (~5 ms)

    void prepare(float sampleRate, uint32_t seed)
    {
        sampleRate_ = sampleRate;
        seed_ = seed ? seed : 1u;
        smooth_ = 1.0f - std::exp(-6.2831853f * 150.0f / sampleRate); // the read point's 150 Hz smoothing
        reset(false);
    }
    // on: MOTION on the left side now (the transport in, at its centre, as if it had always been).
    IV_SIZE_OPT void reset(bool on)
    {
        for (int i = 0; i < kSize; ++i) bufL_[i] = bufR_[i] = 0.0f;
        w_ = 0;
        rng_ = seed_;
        wowPhase_ = flPhase_ = 0.0f;
        wowRnd_ = wowRndA_ = wowRndT_ = wowDrift_ = 0.0f;
        wowCount_ = 0.0f;
        base_ = glideFrom_ = glideTo_ = on ? kBase : kMin;
        glide_ = 1.0f;
        delayNow_ = delaySm_ = base_;
        gainSm_ = lpSm_ = 1.0f;
        gainNow_ = 1.0f;
        lpNow_ = 1.0f;
        lpL_ = lpR_ = 0.0f;
        dStep_ = gStep_ = lStep_ = 0.0f;
        crinkleLeft_ = crinkleLen_ = 0.0f;
        jitA_ = jitB_ = roughA_ = 0.0f;
        crinkles_ = 0;
        dipRng_ = seed_ ^ 0x9e3779b9u;
        dipAcc_ = 0.5f; // the first dip comes on the first or second burst, not the share's last
        dipThis_ = false;
        dips_ = 0;
        dipLen_ = dipLeft_ = 0.0f;
        crinkleDue_ = 0.0f;
        crinkleNext_ = 0.2f + 0.6f * uniform01();
        (void)uniform01(); // (the picked renders drew a dropout's first wait here: kept, so they render as picked)
        rateMul_ = rateT_ = rateCount_ = slow_ = slowA_ = slowT_ = slowCount_ = 0.0f;
        scrape_ = scrapeAmt_ = scrapeNow_ = 0.0f;
        mixNow_ = mixT_ = on ? 1.0f : 0.0f;
        mStep_ = 0.0f;
        idle_ = !on;
    }

    // Once a chunk of n samples (built for size: flash). on: MOTION on the left side.
    IV_SIZE_OPT void control(int n, const TransportAmounts& a, bool on)
    {
        if (idle_ && !on) return;
        const float dt = float(n) / sampleRate_;
        // The centre: a smoothstep glide to kBase (on) or kMin (off), restarted from where it is if the side changes.
        const float target = on ? kBase : kMin;
        if (glideTo_ != target) {
            glideTo_ = target;
            glideFrom_ = base_;
            glide_ = 0.0f;
        }
        if (glide_ < 1.0f) {
            glide_ += dt / kGlideSeconds;
            if (glide_ >= 1.0f) {
                glide_ = 1.0f;
                base_ = target;
            } else {
                base_ = glideFrom_ + (target - glideFrom_) * glide_ * glide_ * (3.0f - 2.0f * glide_);
            }
        }
        // The crossfade with the direct wet: in at once, out only once the centre is back at kMin.
        mixNow_ = mixT_;
        const float mixTo = on || base_ > kMin ? 1.0f : 0.0f;
        const float fade = 1.0f / float(kFadeChunks);
        if (mixTo > mixT_) mixT_ = mixT_ + fade < 1.0f ? mixT_ + fade : 1.0f;
        else if (mixTo < mixT_) mixT_ = mixT_ - fade > 0.0f ? mixT_ - fade : 0.0f;
        mStep_ = (mixT_ - mixNow_) / float(n);
        // Variance: the wow's speed re-picked every 0.5-2 s, -40 to +100 % at full, glided over ~0.4 s; a slow
        // random drift (~0.15 Hz) under it; now and then a wow swing half as big again.
        float slowD = 0.0f;
        if (a.variance > 0.0f) {
            rateCount_ -= dt;
            if (rateCount_ <= 0.0f) {
                rateT_ = a.variance * (1.4f * uniform01() - 0.4f);
                rateCount_ = 0.5f + 1.5f * uniform01();
            }
            rateMul_ += 2.5f * dt * (rateT_ - rateMul_);
            slowCount_ -= dt;
            if (slowCount_ <= 0.0f) {
                slowT_ = uniform();
                slowCount_ = 3.0f + 5.0f * uniform01();
            }
            slowA_ += dt * (slowT_ - slowA_);
            slow_ += dt * (slowA_ - slow_);
            slowD = samples(6.0f * a.variance, 0.15f) * 2.0f * slow_;
        }
        const float hz = a.wowHz * (1.0f + rateMul_);
        // Wow: a smoothed random line (two one-poles), with a sine whose rate wanders ±20 % under it.
        wowCount_ -= dt;
        if (wowCount_ <= 0.0f) {
            wowRndT_ = uniform();
            if (a.variance > 0.0f && uniform01() < 0.3f * a.variance) wowRndT_ *= 1.5f; // a bigger swing
            wowCount_ = (0.6f + 0.4f * (0.5f + 0.5f * uniform())) / hz;
        }
        const float k = 1.0f - std::exp(-6.2831853f * hz * dt);
        wowRndA_ += k * (wowRndT_ - wowRndA_);
        wowRnd_ += k * (wowRndA_ - wowRnd_);
        wowDrift_ += 0.02f * (0.2f * uniform() - wowDrift_);
        wowPhase_ += hz * (1.0f + wowDrift_) * dt;
        wowPhase_ -= float(int(wowPhase_));
        flPhase_ += a.flutterHz * dt;
        flPhase_ -= float(int(flPhase_));
        const float wow = a.wowSine * std::sin(6.2831853f * wowPhase_) + (1.0f - a.wowSine) * 1.6f * wowRnd_;
        const float fl = 0.8f * std::sin(6.2831853f * flPhase_) + 0.2f * std::sin(6.2831853f * 2.37f * flPhase_);
        float d;
        if (base_ >= kBase) {
            d = base_ + samples(a.wowCents, hz) * wow + samples(a.flutterCents, a.flutterHz) * fl + slowD; // as picked
        } else { // gliding: the movement grows with the centre, so the read never piles up against kMin
            const float m = (base_ - kMin) / (kBase - kMin);
            d = base_ + m * (samples(a.wowCents, hz) * wow + samples(a.flutterCents, a.flutterHz) * fl + slowD);
        }
        if (d > float(kSize) - 16.0f) d = float(kSize) - 16.0f; // the buffer's room (never reached at ±20 c, 0.3 Hz)
        if (d < kMin) d = kMin;

        float gain = 1.0f - 0.035f * (a.flutterCents / 3.0f) * fl, lp = 1.0f; // flutter moves the level ±0.3 dB at full
        // Crinkle bursts come jittered-regular, not Poisson: each waits 0.5-1.5 of its mean interval (the first
        // 0.2-0.8), counted in expected events, so a short phrase reliably hears its share.
        crinkleDue_ += a.crinkleRate * dt;
        if (crinkleLeft_ <= 0.0f && crinkleDue_ >= crinkleNext_) {
            crinkleDue_ = 0.0f;
            crinkleNext_ = 0.5f + uniform01();
            // Every 1/share-th burst carries the dip, counted, not drawn, so the share holds in every phrase.
            dipThis_ = false;
            if (a.crinkleDip > 0.0f) {
                dipAcc_ += a.crinkleDip;
                if (dipAcc_ >= 0.999f) {
                    dipAcc_ -= 1.0f;
                    dipThis_ = true;
                    ++dips_;
                    dipRng_ = dipRng_ * 1664525u + 1013904223u; // the dip's length, 50-300 ms
                    dipLen_ = dipLeft_ = 0.05f + 0.25f * (0.5f + 0.5f * float(int32_t(dipRng_)) * (1.0f / 2147483648.0f));
                }
            }
            crinkleLen_ = crinkleLeft_ = 0.1f + 0.5f * uniform01(); // a crease's friction
            ++crinkles_;
        }
        // In a burst, jittery pitch (noise smoothed to ~45 Hz, up to ±12 c); no level loss of its own.
        const float e = crinkleLeft_ > 0.0f ? raised(1.0f - crinkleLeft_ / crinkleLen_) : 0.0f;
        if (crinkleLeft_ > 0.0f) {
            crinkleLeft_ -= dt;
            const float kj = 1.0f - std::exp(-6.2831853f * 45.0f * dt);
            jitA_ += kj * (uniform() - jitA_);
            jitB_ += kj * (jitA_ - jitB_);
            d += e * samples(12.0f, 45.0f) * 2.5f * jitB_;
            if (d < kMin) d = kMin;
        }
        if (dipThis_ && dipLeft_ > 0.0f) { // the dip, its own length from the burst's start, its own random numbers
            dipLeft_ -= dt;
            const float ed = raised(1.0f - dipLeft_ / dipLen_);
            const float kr = 1.0f - std::exp(-6.2831853f * 40.0f * dt);
            dipRng_ = dipRng_ * 1664525u + 1013904223u;
            roughA_ += kr * (float(int32_t(dipRng_)) * (1.0f / 2147483648.0f) - roughA_);
            gain *= 1.0f - ed * (0.1f + 0.5f * std::fabs(roughA_)); // -1 to -3.5 dB, rough
            lp = 1.0f - 0.52f * ed;                                   // the highs toward ~5 kHz
        }
        scrapeAmt_ = (a.crinkleRate / 0.6f) * (0.4f + 0.6f * e); // the scrape bed, heavier in a burst
        dStep_ = (d - delayNow_) / float(n);
        gStep_ = (gain - gainNow_) / float(n);
        lStep_ = (lp - lpNow_) / float(n);
        idle_ = !on && mixT_ <= 0.0f && mixNow_ <= 0.0f;
    }

    bool idle() const { return idle_; }
    // Out: only the history, so coming back in reads nothing stale.
    void write(float l, float r)
    {
        bufL_[w_] = l;
        bufR_[w_] = r;
        w_ = (w_ + 1) & (kSize - 1);
    }
    // One sample through, in place; one copy out of line (flash: inlined, it grew Engine::process by 2.5 KB).
    IV_NOINLINE void process(float& l, float& r)
    {
        bufL_[w_] = l;
        bufR_[w_] = r;
        delayNow_ += dStep_;
        gainNow_ += gStep_;
        lpNow_ += lStep_;
        // The per-chunk ramps (read point, level, the highs), smoothed at 150 Hz so nothing steps every chunk.
        // Cubic (Catmull-Rom) read, the same point for both channels.
        delaySm_ += smooth_ * (delayNow_ - delaySm_);
        gainSm_ += smooth_ * (gainNow_ - gainSm_);
        lpSm_ += smooth_ * (lpNow_ - lpSm_);
        float pos = float(w_) - delaySm_;
        if (scrapeAmt_ > 0.0f || scrapeNow_ != 0.0f) { // the scrape bed: noise low-passed at ~200 Hz
            scrapeNow_ += 0.001f * (scrapeAmt_ - scrapeNow_);
            scrape_ += 0.026f * (uniform() - scrape_);
            pos += scrapeNow_ * 6.0f * scrape_; // ~±0.4 samples: ~±10 c of roughness around 100-200 Hz at full
            if (scrapeNow_ < 1e-6f && scrapeAmt_ <= 0.0f) scrapeNow_ = 0.0f;
        }
        if (pos < 0.0f) pos += float(kSize);
        const int i1 = int(pos);
        const float f = pos - float(i1);
        const int i0 = (i1 - 1) & (kSize - 1), i2 = (i1 + 1) & (kSize - 1), i3 = (i1 + 2) & (kSize - 1);
        auto cr = [f](float a, float b, float c, float e) {
            return b + 0.5f * f * (c - a + f * (2.0f * a - 5.0f * b + 4.0f * c - e + f * (3.0f * (b - c) + e - a)));
        };
        const float yl = cr(bufL_[i0], bufL_[i1], bufL_[i2], bufL_[i3]);
        const float yr = cr(bufR_[i0], bufR_[i1], bufR_[i2], bufR_[i3]);
        w_ = (w_ + 1) & (kSize - 1);
        // The head's highs: a one-pole low-pass whose coefficient lpSm_ is 1 (open) to ~0.5 (a dip's ~5 kHz).
        lpL_ += lpSm_ * (yl - lpL_);
        lpR_ += lpSm_ * (yr - lpR_);
        const float ol = gainSm_ * lpL_, orr = gainSm_ * lpR_;
        mixNow_ += mStep_;
        if (mixNow_ >= 1.0f) {
            l = ol;
            r = orr;
        } else { // crossing in or out at kMin: a crossfade with the direct wet
            l += mixNow_ * (ol - l);
            r += mixNow_ * (orr - r);
        }
    }
    int crinkles() const { return crinkles_; } // bursts so far (tests)
    int dips() const { return dips_; }         // of them with a dip
    float centre() const { return base_; }     // the read point's centre, samples (tests)

private:
    float samples(float cents, float hz) const
    {
        return cents > 0.0f ? (std::pow(2.0f, cents / 1200.0f) - 1.0f) * sampleRate_ / (6.2831853f * hz) : 0.0f;
    }
    // 0 -> 1 -> 0 as x goes 0 -> 0.5 -> 1 (a raised cosine).
    static float raised(float x) { return 0.5f - 0.5f * std::cos(6.2831853f * (x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x)); }
    float uniform()
    {
        rng_ = rng_ * 1664525u + 1013904223u;
        return float(int32_t(rng_)) * (1.0f / 2147483648.0f);
    }
    float uniform01() { return 0.5f + 0.5f * uniform(); }

    float    sampleRate_ = 48000.0f;
    uint32_t seed_ = 1, rng_ = 1;
    float    bufL_[kSize] IV_ZI, bufR_[kSize] IV_ZI;
    int      w_ IV_ZI;
    float    wowPhase_ IV_ZI, flPhase_ IV_ZI, wowRnd_ IV_ZI, wowRndA_ IV_ZI, wowRndT_ IV_ZI, wowDrift_ IV_ZI, wowCount_ IV_ZI;
    float    smooth_ = 0.02f, delaySm_ = kMin, gainSm_ = 1, lpSm_ = 1;
    float    delayNow_ = kMin, dStep_ IV_ZI, gainNow_ = 1, gStep_ IV_ZI, lpNow_ = 1, lStep_ IV_ZI, lpL_ IV_ZI, lpR_ IV_ZI;
    float    base_ = kMin, glideFrom_ = kMin, glideTo_ = kMin, glide_ = 1;
    float    mixNow_ IV_ZI, mixT_ IV_ZI, mStep_ IV_ZI;
    bool     idle_ = true;
    float    crinkleLeft_ IV_ZI, crinkleLen_ IV_ZI;
    float    jitA_ IV_ZI, jitB_ IV_ZI, roughA_ IV_ZI;
    float    crinkleDue_ IV_ZI, crinkleNext_ = 0.5f; // expected bursts so far, the next's due
    int      crinkles_ IV_ZI;
    uint32_t dipRng_ = 1;
    float    dipAcc_ = 0.5f, dipLen_ IV_ZI, dipLeft_ IV_ZI;
    bool     dipThis_ IV_ZI;
    int      dips_ IV_ZI;
    float    rateMul_ IV_ZI, rateT_ IV_ZI, rateCount_ IV_ZI, slow_ IV_ZI, slowA_ IV_ZI, slowT_ IV_ZI, slowCount_ IV_ZI; // variance
    float    scrape_ IV_ZI, scrapeAmt_ IV_ZI, scrapeNow_ IV_ZI;
};

} // namespace iv::dsp
