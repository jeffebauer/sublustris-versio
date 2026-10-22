#include "dsp/Fdn.h"

#include "dsp/SizeOpt.h"

#include <cmath>

namespace iv::dsp {

namespace {

// Hadamard rows used to feed the inputs in and take the outputs out: four
// mutually orthogonal sign patterns, so L and R reach every line differently
// (true stereo in, ADR 0013) and the two outputs are decorrelated.
constexpr float kInL[8]  = {1, 1, 1, 1, -1, -1, -1, -1};
constexpr float kInR[8]  = {1, -1, 1, -1, 1, -1, 1, -1};
constexpr float kOutL[8] = {1, 1, -1, -1, 1, 1, -1, -1};
constexpr float kOutR[8] = {1, -1, -1, 1, 1, -1, -1, 1};

bool isPrime(uint32_t n)
{
    if (n < 2) return false;
    for (uint32_t d = 2; d * d <= n; ++d)
        if (n % d == 0) return false;
    return true;
}

// The nearest prime at or above n not already taken by a lower line.
uint32_t primeAtOrAbove(uint32_t n, const float* taken, int count)
{
    for (;; ++n) {
        if (!isPrime(n)) continue;
        bool clash = false;
        for (int i = 0; i < count; ++i) clash |= uint32_t(taken[i]) == n;
        if (!clash) return n;
    }
}

float uniform(uint32_t& s) // −1..1
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (1.0f / 2147483648.0f);
}

} // namespace

IV_SIZE_OPT bool Fdn::prepare(Arena& arena, float sampleRate, uint32_t seed, float maxLineSeconds)
{
    sampleRate_ = sampleRate;
    maxLen_ = maxLineSeconds * sampleRate - 4.0f;
    ready_ = true;
    for (auto& l : line_) ready_ &= l.init(arena, size_t(maxLineSeconds * sampleRate));
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < 2; ++k) {
            apLen_[i][k] = uint32_t(voicing::kInLoopSeconds[i][k] * sampleRate);
            ready_ &= apLine_[i][k].init(arena, size_t(voicing::kInLoopMaxSeconds * sampleRate));
        }
    rng_ = seed ? seed : 1u;
    for (int i = 0; i < kN; ++i) {
        era_[i].prepare(sampleRate);
        driftGen_[i].prepare(sampleRate, seed * 2654435761u + uint32_t(i) * 40503u + 1u, float(i) / float(kN - 1));
    }
    // Early taps: fixed, staggered points along each line (seeded, so every
    // build is the same), louder the later they sit, so the first sound is
    // quiet and the tank swells from there.
    uint32_t es = 0xEA51u;
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < kEarlyTaps; ++k) {
            const float u = 0.5f + 0.35f * uniform(es); // 0.15..0.85 of a slot
            earlyFrac_[i][k] = (float(k) + u) / float(kEarlyTaps);
            earlyGain_[i][k] = voicing::kEarlyTapLow + (1.0f - voicing::kEarlyTapLow) * earlyFrac_[i][k];
            if (uniform(es) < 0.0f) earlyGain_[i][k] = -earlyGain_[i][k];
        }
    for (auto& t : lenTarget_) t = 0.0f;
    setMeanSeconds(0.1f, 1.0f, true);
    reset();
    return ready_;
}

IV_SIZE_OPT void Fdn::reset()
{
    for (int i = 0; i < kN; ++i) {
        line_[i].clear();
        apLine_[i][0].clear();
        apLine_[i][1].clear();
        absorbY_[i] = absorbX_[i] = 0.0f;
        interp_[i] = 0.0f;
        era_[i].reset();
        driftGen_[i].reset();
        floorTarget_[i] = floorA_[i] = floorB_[i] = 0.0f;
        floorCountdown_[i] = 0.0f;
        governor_.reset();
        lenNow_[i] = lenTarget_[i];
        readNow_[i] = lenNow_[i];
        inLoopNow_ = inLoopTarget_;
        earlyNow_ = earlyTarget_;
        readStep_[i] = 0.0f;
    }
}

IV_SIZE_OPT void Fdn::setMeanSeconds(float meanSeconds, float spread, bool snap)
{
    for (int i = 0; i < kN; ++i) {
        const float ratio = 1.0f + spread * (voicing::kFdnRatios[i] - 1.0f);
        float len = ratio * meanSeconds * sampleRate_;
        if (len > maxLen_) len = maxLen_;
        if (len < 8.0f) len = 8.0f;
        lenTarget_[i] = float(primeAtOrAbove(uint32_t(len), lenTarget_, i));
        if (snap) lenNow_[i] = lenTarget_[i];
    }
}

void Fdn::control(int n)
{
    const float dt = float(n) / sampleRate_;
    const float slew = voicing::kSizeSlewPerSecond * float(n); // samples of length per block
    const float floorDepth = voicing::kFloorDepthSeconds * sampleRate_ * modDepth_;
    const float floorCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 0.3f * dt);

    // The floor governor's extra loss (ADR 0008).
    governor_.control(n, sampleRate_);
    const float governRate = governor_.extraRate;

    inLoopNow_ += (1.0f - std::exp(-dt / 0.08f)) * (inLoopTarget_ - inLoopNow_);
    if (inLoopNow_ < 1e-4f && inLoopTarget_ == 0.0f) inLoopNow_ = 0.0f;
    earlyNow_ += (1.0f - std::exp(-dt / 0.08f)) * (earlyTarget_ - earlyNow_);
    if (earlyNow_ < 1e-4f && earlyTarget_ == 0.0f) earlyNow_ = 0.0f;
    float meanLen = 0.0f;
    for (int i = 0; i < kN; ++i) meanLen += lenNow_[i];
    meanLen /= float(kN) * sampleRate_;
    levelGain_ = outputTrim_ * std::sqrt(meanLen / voicing::kFdnLevelRefSeconds);
    for (int i = 0; i < kN; ++i) {
        // SIZE glides toward its target, at most a semitone of bend.
        const float diff = lenTarget_[i] - lenNow_[i];
        lenNow_[i] += diff > slew ? slew : (diff < -slew ? -slew : diff);

        // Modulation floor: a new random target every ~1.7–3.1 s per line.
        floorCountdown_[i] -= dt;
        if (floorCountdown_[i] <= 0.0f) {
            floorTarget_[i] = uniform(rng_);
            floorCountdown_[i] = voicing::kFloorStepSeconds * (1.0f + 0.12f * float(i)) * (0.8f + 0.2f * uniform(rng_));
        }
        floorA_[i] += floorCoeff * (floorTarget_[i] - floorA_[i]);
        floorB_[i] += floorCoeff * (floorA_[i] - floorB_[i]);

        float driftRoom = voicing::kDriftMaxShareOfLine * lenNow_[i];
        if (driftRoom > voicing::kDriftMaxSeconds * sampleRate_) driftRoom = voicing::kDriftMaxSeconds * sampleRate_;
        float target = lenNow_[i] + floorDepth * floorB_[i] + driftGen_[i].control(dt, drift_, driftRoom);
        if (target < 4.0f) target = 4.0f;
        if (target > maxLen_) target = maxLen_;
        readStep_[i] = (target - readNow_[i]) / float(n);

        // Loss per pass from this line's own length (Jot): the low end
        // decays at decayRate (+ the governor), the high end at that /
        // highRatio; at infinite the highs fade until the tail reaches its
        // floor, then it holds (ADR 0008).
        const float apSeconds = inLoopNow_ > 0.0f ? float(apLen_[i][0] + apLen_[i][1]) / sampleRate_ : 0.0f;
        const Absorb a = absorbent(lenNow_[i] / sampleRate_ + apSeconds, decayRate_ + governRate + extraRate_, highRatio_,
                                   governor_.darken, shelfHz_, sampleRate_);
        b1_[i] = a.b1;
        gain_[i] = a.gain;
        pole_[i] = a.pole;
    }
}

void Fdn::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (!ready_) return;
    constexpr float kNorm = 0.35355339f; // 1 / sqrt(8): keeps the Hadamard mix energy-preserving
    for (int start = 0; start < n; start += kControlBlock) {
        const int m = (n - start) < kControlBlock ? (n - start) : kControlBlock;
        control(m);
        const float outGain = voicing::kFdnOutputGain * kNorm * levelGain_;
        const float apG = voicing::kInLoopCoeff * inLoopNow_;
        const float earlyG = voicing::kEarlyTapLevel * earlyNow_;
        for (int s = start; s < start + m; ++s) {
            float y[kN];
            for (int i = 0; i < kN; ++i) {
                readNow_[i] += readStep_[i];
                float x = line_[i].readAllpass(readNow_[i], interp_[i]);
                if (apG > 0.0f)
                    for (int k = 0; k < 2; ++k) { // in-loop diffusion (MASSIVE)
                        const float d = apLine_[i][k].read(apLen_[i][k]);
                        const float v = x + apG * d;
                        apLine_[i][k].write(v);
                        x = d - apG * v;
                    }
                absorbY_[i] = gain_[i] * x + b1_[i] * absorbX_[i] + pole_[i] * absorbY_[i];
                absorbX_[i] = x;
                y[i] = era_[i].process(absorbY_[i]);
            }
            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < kN; ++i) {
                l += kOutL[i] * y[i];
                r += kOutR[i] * y[i];
            }
            governor_.add(0.5f * outGain * outGain * (l * l + r * r)); // the tank's own level, before the taps
            if (earlyG > 0.0f)
                for (int i = 0; i < kN; ++i) {
                    // Each line's taps go out with another line's signs, so they decorrelate from the ends.
                    const int j = (i + 3) & (kN - 1);
                    float e = 0.0f;
                    for (int k = 0; k < kEarlyTaps; ++k) {
                        const float d  = 1.0f + earlyFrac_[i][k] * lenNow_[i];
                        const auto  di = uint32_t(d);
                        const float t  = d - float(di);
                        const float a0 = line_[i].read(di), a1 = line_[i].read(di + 1);
                        e += earlyGain_[i][k] * (a0 + t * (a1 - a0));
                    }
                    l += earlyG * kOutL[j] * e;
                    r += earlyG * kOutR[j] * e;
                }
            outL[s] += outGain * l;
            outR[s] += outGain * r;

            // Fast Walsh–Hadamard transform: 24 adds for the 8×8 mix.
            for (int span = 1; span < kN; span <<= 1)
                for (int i = 0; i < kN; i += 2 * span)
                    for (int j = i; j < i + span; ++j) {
                        const float a = y[j], b = y[j + span];
                        y[j] = a + b;
                        y[j + span] = a - b;
                    }
            const float xl = inL[s] * kNorm, xr = inR[s] * kNorm;
            for (int i = 0; i < kN; ++i) line_[i].write(y[i] * kNorm + kInL[i] * xl + kInR[i] * xr);
        }
    }
}

} // namespace iv::dsp
