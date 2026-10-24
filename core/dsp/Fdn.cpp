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
    shelfK_ = lossShelfK(sampleRate);
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
    // Early taps: fixed points along each line, louder the later they sit, so the first sound is
    // quiet and the tank swells from there. The 8 lines' taps interleave (each line offset by its
    // own share of a slot, shorter lines first) instead of bunching (v1.0.21; the seeded jitter
    // they replace still draws, so every build's signs are the same).
    uint32_t es = 0xEA51u;
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < kEarlyTaps; ++k) {
            (void)uniform(es);
            earlyFrac_[i][k] = (float(k) + (float(i) + 0.5f) / float(kN)) / float(kEarlyTaps);
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
        for (int k = 0; k < 2; ++k) {
            apLenNow_[i][k] = apTargetLength(i, k);
            apLenGlide_[i][k] = float(apLenNow_[i][k]);
            apLenStep_[i][k] = 0.0f;
        }
    }
    outputTrimNow_ = outputTrim_;
    externalPower_ = 0.0f; // a low tank's level from before (soak audit: a stale one read as a phantom level after a flip)
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

// Where an in-loop allpass's length is going: its own length × the scale, or nothing when the
// diffusion is going out.
IV_SIZE_OPT uint32_t Fdn::apTargetLength(int i, int k) const
{
    if (inLoopTarget_ <= 0.0f) return 0u;
    const float len = float(apLen_[i][k]) * apScale_;
    return len < 1.0f ? 1u : uint32_t(len);
}

// In-loop diffusion in and out (HALL's small rooms, v1.0.17): the allpasses join the loop at no
// length at all and grow from there (SIZE's glide); going out, they shrink back to nothing before
// they're bypassed. Switched in or out at full length, the lines' lengths jumped and the bypassed
// allpasses replayed stale sound: a click and a dropout whenever SIZE crossed noon in a HALL tail
// (soak audit). Each allpass's length glides at SIZE's semitone slew, stepped per sample.
IV_SIZE_OPT void Fdn::controlInLoop(float dt, int n, float slew)
{
    const bool wasBypassed = inLoopNow_ == 0.0f;
    inLoopNow_ += (1.0f - std::exp(-dt / 0.08f)) * (inLoopTarget_ - inLoopNow_);
    bool shrunk = true;
    for (int i = 0; i < kN; ++i) shrunk &= apLenGlide_[i][0] <= 0.0f && apLenGlide_[i][1] <= 0.0f;
    if (inLoopNow_ < 1e-4f && inLoopTarget_ == 0.0f && shrunk) inLoopNow_ = 0.0f;
    const bool joining = wasBypassed && inLoopNow_ > 0.0f;
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < 2; ++k) {
            apLenNow_[i][k] = apTargetLength(i, k);
            float& glide = apLenGlide_[i][k];
            if (joining) glide = 0.0f;
            const float d = float(apLenNow_[i][k]) - glide;
            if (std::fabs(d) < 1e-3f) { // there: the exact integer read again
                glide = float(apLenNow_[i][k]);
                apLenStep_[i][k] = 0.0f;
            } else {
                apLenStep_[i][k] = (d > slew ? slew : (d < -slew ? -slew : d)) / float(n);
            }
            if (joining) glide = -apLenStep_[i][k]; // the first sample reads no delay: the buffer holds nothing fresh yet
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
    const FloorGovernor& gov = leader_ ? leader_->governor_ : governor_; // a low tank follows its leader
    const float governRate = gov.extraRate;

    controlInLoop(dt, n, slew);
    earlyNow_ += (1.0f - std::exp(-dt / 0.08f)) * (earlyTarget_ - earlyNow_);
    if (earlyNow_ < 1e-4f && earlyTarget_ == 0.0f) earlyNow_ = 0.0f;
    float meanLen = 0.0f;
    for (int i = 0; i < kN; ++i) meanLen += lenNow_[i];
    meanLen /= float(kN) * sampleRate_;
    outputTrimNow_ += (1.0f - std::exp(-dt / 0.05f)) * (outputTrim_ - outputTrimNow_);
    if (std::fabs(outputTrim_ - outputTrimNow_) < 1e-6f) outputTrimNow_ = outputTrim_;
    levelGain_ = outputTrimNow_ * std::sqrt(meanLen / voicing::kFdnLevelRefSeconds);
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
        if (vintage_ > 0.0f) {
            // E4: the modulation is noisy (Costello: the PCM70's "noisy as all get out"): a fresh random
            // offset each control block, smoothed once (a coarse, steppy read wander).
            jitter_[i] += 0.5f * (voicing::kVintageJitterSamples * uniform(rng_) - jitter_[i]);
            target += vintage_ * jitter_[i];
        }
        if (target < 4.0f) target = 4.0f;
        if (target > maxLen_) target = maxLen_;
        readStep_[i] = (target - readNow_[i]) / float(n);

        // Loss per pass from this line's own length (Jot): the low end
        // decays at decayRate (+ the governor), the high end at that /
        // highRatio; at infinite the highs fade until the tail reaches its
        // floor, then it holds (ADR 0008).
        const float apSeconds = inLoopNow_ > 0.0f ? (apLenGlide_[i][0] + apLenGlide_[i][1]) / sampleRate_ : 0.0f;
        const Absorb a = absorbent(lenNow_[i] / sampleRate_ + apSeconds, decayRate_ + governRate + extraRate_, highRatio_,
                                   gov.darken, shelfK_);
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
                if (vintage_ > 0.0f) {
                    // E4: linear interpolation with the fraction quantised (the 224's multiplier: 32 or 64 steps).
                    const float d = readNow_[i];
                    const auto di = uint32_t(d);
                    const float f = float(int32_t((d - float(di)) * voicing::kVintageSteps)) * (1.0f / voicing::kVintageSteps);
                    const float a0 = line_[i].read(di), a1 = line_[i].read(di + 1);
                    x += vintage_ * (a0 + f * (a1 - a0) - x);
                }
                if (apG > 0.0f)
                    for (int k = 0; k < 2; ++k) { // in-loop diffusion (MASSIVE, a small HALL)
                        float d;
                        const float glide = (apLenGlide_[i][k] += apLenStep_[i][k]);
                        if (glide == float(apLenNow_[i][k]) && glide >= 1.0f) d = apLine_[i][k].read(apLenNow_[i][k]);
                        else if (glide < 1.0f) { // joining or leaving the loop: between no delay (the input) and one sample
                            d = glide > 0.0f ? x + glide * (apLine_[i][k].read(1) - x) : x;
                        } else { // gliding: linear between samples (the buffer has room for the longest)
                            const auto  di = uint32_t(glide);
                            const float t = glide - float(di);
                            const float a0 = apLine_[i][k].read(di), a1 = apLine_[i][k].read(di + 1);
                            d = a0 + t * (a1 - a0);
                        }
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
            governor_.add(0.5f * outGain * outGain * (l * l + r * r) + externalPower_); // the tank's own level (+ a low tank's), before the taps
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
