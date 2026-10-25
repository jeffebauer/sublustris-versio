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
        tapLen_[i] = lenNow_[i];
        inLoopNow_ = inLoopTarget_;
        earlyNow_ = earlyTarget_;
        readStep_[i] = 0.0f;
        for (int k = 0; k < 2; ++k) {
            apLenNow_[i][k] = apTargetLength(i, k);
            apLenGlide_[i][k] = float(apLenNow_[i][k]);
            apLenStep_[i][k] = 0.0f;
        }
    }
    outputTrimNow_ = xfTrim_ = outputTrim_;
    { // the level that follows SIZE starts where it belongs (it's on the input: gliding in, it stayed in the tail)
        float meanLen = 0.0f;
        for (int i = 0; i < kN; ++i) meanLen += lenNow_[i];
        levelGain_ = outputTrim_ * std::sqrt(meanLen / (float(kN) * sampleRate_) / voicing::kFdnLevelRefSeconds);
    }
    xfLeft_ = 0;
    xfSince_ = -1;
    xfOld_ = 0.0f;
    xfNew_ = 1.0f;
    xfRest_ = 0.0f;
    for (int i = 0; i < kN; ++i) {
        xfLastTarget_[i] = lenTarget_[i];
        xfLineCorr_[i] = 1.0f;
        for (int k = 0; k < kEarlyTaps; ++k) {
            xfTapCorr_[i][k] = 1.0f;
            tapGainNow_[i][k] = earlyGain_[i][k];
        }
    }
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
IV_SIZE_OPT void Fdn::controlInLoop(float dt, int n, float slew, bool pending)
{
    const bool wasBypassed = inLoopNow_ == 0.0f;
    // While a SIZE move waits for its crossfade the amount holds too, and jumps with the lengths at its
    // start, the old amount fading out with the old lengths (followed on its own, it disagreed with
    // the held lengths: a small HALL jumped across noon clicked at the crossfade's start).
    if (!pending) inLoopNow_ += (1.0f - std::exp(-dt / 0.08f)) * (inLoopTarget_ - inLoopNow_);
    bool shrunk = true;
    for (int i = 0; i < kN; ++i) shrunk &= apLenGlide_[i][0] <= 0.0f && apLenGlide_[i][1] <= 0.0f;
    // Out of the loop: at once when a SIZE crossfade has taken them out (their coefficient faded with it).
    if ((inLoopNow_ < 1e-4f || xfLeft_ <= 0) && inLoopTarget_ == 0.0f && shrunk) inLoopNow_ = 0.0f;
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
            } else if (pending && !joining) { // held: they jump with the lines at a SIZE crossfade's start (gliding in
                apLenStep_[i][k] = 0.0f; // the loop smeared every note: flicks across noon -6.7 dB off the note)
            } else {
                apLenStep_[i][k] = (d > slew ? slew : (d < -slew ? -slew : d)) / float(n);
            }
            if (joining) glide = -apLenStep_[i][k]; // the first sample reads no delay: the buffer holds nothing fresh yet
        }
}

// SIZE's crossfade (ADR 0025), only while one runs: the old length faded out against the new read x.
// Out of line, so the everyday per-sample loop stays as tight as before (inlined, -O3 copied these
// into every unrolled pass: +2 KB of flash).
IV_NOINLINE IV_SIZE_OPT void Fdn::xfLines(float* xs, float* xo)
{
    for (int i = 0; i < kN; ++i) {
        readNow_[i] += readStep_[i];
        xs[i] = line_[i].readAllpass(readNow_[i], interp_[i]) * xfLineCorr_[i];
        // The old length, read the way it was before (allpass, its own state: a cubic read there differed
        // by a hair from the sample before, a click at the start), on the old timeline (no correction).
        xo[i] = line_[i].readAllpass(readNow_[i] + xfDelta_[i], interpOld_[i]);
    }
}

// The new read and the old mixed, with the gain for their agreement (control). The old one first runs
// through the in-loop allpasses it had when they're leaving (gOld > 0).
IV_NOINLINE IV_SIZE_OPT float Fdn::xfMix(int i, float nw, float od, float gOld)
{
    constexpr float kA = 0.004f; // the agreement's follower: ~5 ms
    if (gOld > 0.0f) od = xfAllpass(i, od, gOld, true);
    xfOO_[i] += kA * (od * od - xfOO_[i]);
    xfNN_[i] += kA * (nw * nw - xfNN_[i]);
    xfON_[i] += kA * (od * nw - xfON_[i]);
    return xfK_[i] * (xfNew_ * nw + xfOld_ * od);
}

// In-loop diffusion joining or leaving with a SIZE crossfade: it runs on its own read, the new one
// (joining) or the old (leaving), each a whole allpass. Blended inside the allpass between straight
// through and delayed, mid-fade it passed 14 %, every trip: a small HALL 13 dB down in 0.25 s.
IV_NOINLINE IV_SIZE_OPT float Fdn::xfAllpass(int i, float x, float g, bool old)
{
    for (int k = 0; k < 2; ++k) {
        const float len = old ? apOld_[i][k] : apLenGlide_[i][k];
        float d = x;
        if (len >= 1.0f) {
            const auto  li = uint32_t(len);
            const float t = len - float(li);
            const float a0 = apLine_[i][k].read(li), a1 = apLine_[i][k].read(li + 1);
            d = a0 + t * (a1 - a0);
        }
        const float v = x + g * d;
        apLine_[i][k].write(v);
        x = d - g * v;
    }
    return x;
}

IV_NOINLINE IV_SIZE_OPT float Fdn::xfOldAllpass(int i, int k, float x, float d)
{
    const float ol = apOld_[i][k];
    float od = x;
    if (ol >= 1.0f) {
        const auto  oi = uint32_t(ol);
        const float ot = ol - float(oi);
        const float o0 = apLine_[i][k].read(oi), o1 = apLine_[i][k].read(oi + 1);
        od = o0 + ot * (o1 - o0);
    }
    return xfNew_ * d + xfOld_ * od;
}

IV_NOINLINE IV_SIZE_OPT void Fdn::xfOldTaps(float& el, float& er)
{
    float ol = 0.0f, orr = 0.0f;
    for (int i = 0; i < kN; ++i) {
        const int j = (i + 3) & (kN - 1);
        float e = 0.0f;
        for (int k = 0; k < kEarlyTaps; ++k) { // the old positions, on the old timeline (no correction)
            const float od = 1.0f + earlyFrac_[i][k] * (tapLen_[i] + xfDelta_[i]);
            const auto  oi = uint32_t(od);
            const float ot = od - float(oi);
            const float b0 = line_[i].read(oi), b1 = line_[i].read(oi + 1);
            e += earlyGain_[i][k] * (b0 + ot * (b1 - b0));
        }
        ol += kOutL[j] * e;
        orr += kOutR[j] * e;
    }
    el = xfNew_ * el + xfOld_ * ol;
    er = xfNew_ * er + xfOld_ * orr;
}

IV_SIZE_OPT void Fdn::control(int n) // once per block: built for size (flash)
{
    const float dt = float(n) / sampleRate_;
    const float slew = voicing::kSizeSlewPerSecond * float(n); // samples of length per block
    const float floorDepth = voicing::kFloorDepthSeconds * sampleRate_ * modDepth_;
    const float floorCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 0.3f * dt);

    // The floor governor's extra loss (ADR 0008).
    governor_.control(n, sampleRate_);
    const FloorGovernor& gov = leader_ ? leader_->governor_ : governor_; // a low tank follows its leader
    const float governRate = gov.extraRate;

    // A SIZE move waiting for its crossfade (below): new lengths, or the in-loop diffusion joining or
    // leaving (it started after a crossfade that had already gone, and joined on its own: a dropout).
    bool pending = (inLoopTarget_ > 0.0f) != (inLoopNow_ > 0.0f);
    for (int i = 0; i < kN; ++i) pending |= std::fabs(lenTarget_[i] - lenNow_[i]) > 0.5f;
    controlInLoop(dt, n, slew, pending);
    earlyNow_ += (1.0f - std::exp(-dt / 0.08f)) * (earlyTarget_ - earlyNow_);
    if (earlyNow_ < 1e-4f && earlyTarget_ == 0.0f) earlyNow_ = 0.0f;
    float meanLen = 0.0f;
    for (int i = 0; i < kN; ++i) meanLen += lenNow_[i];
    meanLen /= float(kN) * sampleRate_;
    // SIZE by crossfade (v1.0.34, ADR 0025; owner's pick C on renders/knob_moves): a glide in a loop
    // retunes everything circulating by the length ratio for the rest of its tail (PASP "Delay-line
    // interpolation": crossfade between the two delay settings rather than glide). Once SIZE has rested
    // kSizeRestSeconds (back to back, each crossfade's small dip stayed in the loop: an infinite MASSIVE
    // drained 45 dB in a minute of sweeping), every line, early tap and in-loop allpass jumps to its new
    // length and the old one fades out over kSizeCrossfadeSeconds, the gains summing to one.
    float still = 0.0f;
    for (int i = 0; i < kN; ++i) {
        still = std::fmax(still, std::fabs(lenTarget_[i] - xfLastTarget_[i]));
        xfLastTarget_[i] = lenTarget_[i];
    }
    still = std::fmax(still, 1e4f * std::fabs(inLoopTarget_ - xfLastInLoop_)); // and the diffusion settled
    xfLastInLoop_ = inLoopTarget_;
    xfRest_ = still > 0.5f ? 0.0f : xfRest_ + dt;
    // The trim that follows SIZE (hallSizeLevelTrim, massiveDecayLevelTrim) waits for the lines while a
    // move is pending (following the knob, a flicked HALL's level flickered); with the length-following
    // level it glides over the crossfade.
    if (!pending) xfTrim_ = outputTrim_;
    const float glideK = 1.0f - std::exp(-dt / (0.5f * voicing::kSizeCrossfadeSeconds));
    outputTrimNow_ += glideK * (xfTrim_ - outputTrimNow_);
    if (std::fabs(outputTrim_ - outputTrimNow_) < 1e-6f) outputTrimNow_ = outputTrim_;
    levelGain_ += glideK * (outputTrimNow_ * std::sqrt(meanLen / voicing::kFdnLevelRefSeconds) - levelGain_);
    const float rate = decayRate_ + governRate + extraRate_;
    if (pending && xfLeft_ <= 0 && xfRest_ >= voicing::kSizeRestSeconds) {
        xfLen_ = xfLeft_ = int(voicing::kSizeCrossfadeSeconds * sampleRate_);
        xfSince_ = 0;
        xfTrim_ = outputTrim_;
        // The fade's angle per sample: a rotation, no cos per sample. The gains sum to one, sin² and cos²
        // (equal power summed the old and new reads to 1.41x wherever they still agree, the lows: inside
        // the loop that grew every trip, a bass swell to a DC offset of -0.4 on SIZE flicks).
        const float step = 3.1415927f / float(xfLen_);
        xfRotC_ = std::cos(step);
        xfRotS_ = std::sin(step);
        xfCos_ = 1.0f;
        xfSin_ = 0.0f;
        xfOld_ = 1.0f;
        xfNew_ = 0.0f;
        inLoopOld_ = inLoopNow_; // the in-loop diffusion's amount jumps too, the old one fading out
        inLoopNow_ = inLoopTarget_;
        for (int i = 0; i < kN; ++i) {
            xfDelta_[i] = lenNow_[i] - lenTarget_[i]; // old minus new
            // For one trip after the jump the new read hears sound written before it: from further back
            // (louder, the tail was younger) when longer, nearer (quieter) when shorter. Its level is
            // corrected by the tail's own decay over the difference for exactly that trip, then eased
            // back over the crossfade (owner: "some jumps in amplitude with the crossfades").
            xfLnGain_[i] = 6.9077553f * xfDelta_[i] / sampleRate_ * rate;
            xfHold_[i] = lenTarget_[i];
            lenNow_[i] = lenTarget_[i];
            readNow_[i] -= xfDelta_[i];
            interpOld_[i] = interp_[i]; // the old read carries on exactly as it was (allpass, its own state)
            xfAbsOld_[i][0] = b1_[i];
            xfAbsOld_[i][1] = gain_[i];
            xfAbsOld_[i][2] = pole_[i];
            xfOO_[i] = xfNN_[i] = xfON_[i] = 0.0f;
            xfK_[i] = 1.0f;
            // ...and the new read starts from what it would have given a sample ago at its new length (its
            // state is the previous output): the old position's state there rang at Nyquist.
            interp_[i] = line_[i].readCubic(readNow_[i] + 1.0f);
            for (int k = 0; k < 2; ++k) { // HALL's in-loop allpasses jump too, their old length fading out
                apOld_[i][k] = apLenGlide_[i][k];
                apLenGlide_[i][k] = float(apLenNow_[i][k]);
            }
            tapLen_[i] = lenNow_[i]; // the early taps too (slid over 0.3 s they read 3x real time: a crackle)
        }
    }
    // The fade's gain, once per block for each line (Bristow-Johnson's crossfade for any correlation): the
    // level of the powers' own fade over that of the mix. Old and new reads that agree sum to one (equal
    // power there grew the lows every trip, a swell to DC); reads that differ keep their power (summing
    // to one, they dipped 3 dB mid-fade, and in the loop every trip: a small HALL 25 dB down).
    if (xfLeft_ > 0)
        for (int i = 0; i < kN; ++i) {
            const float a = xfOld_, b = xfNew_, on = std::fmax(0.0f, xfON_[i]);
            const float want = a * xfOO_[i] + b * xfNN_[i], mix = a * a * xfOO_[i] + b * b * xfNN_[i] + 2.0f * a * b * on;
            xfK_[i] = mix > 1e-24f ? std::fmin(1.42f, std::sqrt(want / mix)) : 1.0f;
        }
    // The trip's level correction, once per block (for each line's read and each early tap).
    if (xfSince_ >= 0) {
        const float s = float(xfSince_);
        auto corr = [&](float lnGain, float hold) {
            if (s < hold) return std::exp(lnGain);
            const float r = (s - hold) / float(xfLen_);
            return r >= 1.0f ? 1.0f : std::exp(lnGain * (1.0f - r));
        };
        for (int i = 0; i < kN; ++i) {
            xfLineCorr_[i] = corr(xfLnGain_[i], xfHold_[i]);
            for (int k = 0; k < kEarlyTaps; ++k) xfTapCorr_[i][k] = corr(earlyFrac_[i][k] * xfLnGain_[i], earlyFrac_[i][k] * xfHold_[i]);
        }
        if (s > maxLen_ + float(xfLen_)) { // every line's trip done
            xfSince_ = -1;
            for (int i = 0; i < kN; ++i) {
                xfLineCorr_[i] = 1.0f;
                for (int k = 0; k < kEarlyTaps; ++k) xfTapCorr_[i][k] = 1.0f;
            }
        }
    }
    for (int i = 0; i < kN; ++i) {
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
        tapStep_[i] = (lenNow_[i] - tapLen_[i]) / float(n); // 0 but after a snap (the taps jump with a crossfade)

        // Loss per pass from this line's own length (Jot): the low end
        // decays at decayRate (+ the governor), the high end at that /
        // highRatio; at infinite the highs fade until the tail reaches its
        // floor, then it holds (ADR 0008).
        const float apSeconds = inLoopNow_ > 0.0f ? (apLenGlide_[i][0] + apLenGlide_[i][1]) / sampleRate_ : 0.0f;
        const Absorb a = absorbent(lenNow_[i] / sampleRate_ + apSeconds, rate, highRatio_,
                                   gov.darken, shelfK_, air_);
        // While SIZE crossfades the loss filter goes from the old length's to the new one's with the fade
        // (switched at its start, the old read, all of the mix then, stepped to the new trip's loss: a click).
        const float o = xfLeft_ > 0 ? xfOld_ : 0.0f, w = 1.0f - o;
        b1_[i] = o * xfAbsOld_[i][0] + w * a.b1;
        gain_[i] = o * xfAbsOld_[i][1] + w * a.gain;
        // The trip's level correction after a SIZE jump rides on the early taps' gains (they hold no state,
        // so it's exact); the line's own read takes it per sample (on the loss filter, the in-loop
        // allpasses' memory sat between: the old read entered them x5 at a jump, a click).
        for (int k = 0; k < kEarlyTaps; ++k) tapGainNow_[i][k] = earlyGain_[i][k] * xfTapCorr_[i][k];
        pole_[i] = o * xfAbsOld_[i][2] + w * a.pole;
    }
}

void Fdn::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (!ready_) return;
    constexpr float kNorm = 0.35355339f; // 1 / sqrt(8): keeps the Hadamard mix energy-preserving
    for (int start = 0; start < n; start += kControlBlock) {
        const int m = (n - start) < kControlBlock ? (n - start) : kControlBlock;
        control(m);
        // The level that follows SIZE (a longer loop holds the same energy over more samples) and the
        // trims go in with the input, so what's already in the loop comes out as it went in (on the
        // output, a SIZE jump stepped every held tail: an infinite MASSIVE +6 dB for SIZE 0 -> 0.7).
        const float outGain = voicing::kFdnOutputGain * kNorm;
        const float inGain = kNorm * levelGain_;
        // ERA's in-loop stages (bits, saturation) aren't linear: they hear the loop at the level they did
        // with the gain on the output (in, it shortened a Midiverb HALL's T60 9 %).
        const float eraIn = 1.0f / levelGain_, eraOut = levelGain_;
        const float apG = voicing::kInLoopCoeff * inLoopNow_;
        const float apGOld = xfLeft_ > 0 ? voicing::kInLoopCoeff * inLoopOld_ : 0.0f;
        const float earlyG = voicing::kEarlyTapLevel * earlyNow_;
        for (int s = start; s < start + m; ++s) {
            float y[kN], xs[kN], xo[kN];
            if (xfLeft_ > 0) xfLines(xs, xo); // one call a sample while SIZE crossfades (nothing writes these lines before)
            for (int i = 0; i < kN; ++i) {
                float x;
                if (xfLeft_ > 0) {
                    x = xs[i];
                    if (apG > 0.0f && apGOld == 0.0f) x = xfAllpass(i, x, apG, false); // joining: on the new read
                    x = xfMix(i, x, xo[i], apG == 0.0f ? apGOld : 0.0f);               // leaving: on the old
                } else {
                    readNow_[i] += readStep_[i];
                    x = line_[i].readAllpass(readNow_[i], interp_[i]) * xfLineCorr_[i];
                }
                if (apG > 0.0f && (xfLeft_ <= 0 || apGOld > 0.0f))
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
                        float g = apG;
                        if (xfLeft_ > 0) { // in the loop before and after: its length and amount crossfade
                            if (apOld_[i][k] != glide) d = xfOldAllpass(i, k, x, d);
                            g = xfNew_ * apG + xfOld_ * apGOld;
                        }
                        const float v = x + g * d;
                        apLine_[i][k].write(v);
                        x = d - g * v;
                    }
                absorbY_[i] = gain_[i] * x + b1_[i] * absorbX_[i] + pole_[i] * absorbY_[i];
                absorbX_[i] = x;
                y[i] = eraOut * era_[i].process(eraIn * absorbY_[i]);
            }
            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < kN; ++i) {
                l += kOutL[i] * y[i];
                r += kOutR[i] * y[i];
            }
            governor_.add(0.5f * outGain * outGain * (l * l + r * r) + externalPower_); // the tank's own level (+ a low tank's), before the taps
            for (int i = 0; i < kN; ++i) tapLen_[i] += tapStep_[i]; // v1.0.25: no per-block steps on SIZE moves
            if (earlyG > 0.0f) {
                float el = 0.0f, er = 0.0f;
                for (int i = 0; i < kN; ++i) {
                    // Each line's taps go out with another line's signs, so they decorrelate from the ends.
                    const int j = (i + 3) & (kN - 1);
                    float e = 0.0f;
                    for (int k = 0; k < kEarlyTaps; ++k) {
                        const float d  = 1.0f + earlyFrac_[i][k] * tapLen_[i];
                        const auto  di = uint32_t(d);
                        const float t  = d - float(di);
                        const float a0 = line_[i].read(di), a1 = line_[i].read(di + 1);
                        e += tapGainNow_[i][k] * (a0 + t * (a1 - a0)); // the tap's gain x the trip's correction (per block)
                    }
                    el += kOutL[j] * e;
                    er += kOutR[j] * e;
                }
                if (xfLeft_ > 0) xfOldTaps(el, er); // one call a sample while SIZE crossfades
                l += earlyG * el;
                r += earlyG * er;
            }
            outL[s] += outGain * l;
            outR[s] += outGain * r;
            if (xfLeft_ > 0) { // the fade's next step (a rotation by the angle per sample), exact at the end
                const float c = xfCos_;
                xfCos_ = c * xfRotC_ - xfSin_ * xfRotS_;
                xfSin_ = xfSin_ * xfRotC_ + c * xfRotS_;
                xfNew_ = 0.5f - 0.5f * xfCos_;
                xfOld_ = 1.0f - xfNew_;
                if (--xfLeft_ == 0) {
                    xfOld_ = 0.0f;
                    xfNew_ = 1.0f;
                }
            }
            if (xfSince_ >= 0) ++xfSince_;

            // Fast Walsh–Hadamard transform: 24 adds for the 8×8 mix.
            for (int span = 1; span < kN; span <<= 1)
                for (int i = 0; i < kN; i += 2 * span)
                    for (int j = i; j < i + span; ++j) {
                        const float a = y[j], b = y[j + span];
                        y[j] = a + b;
                        y[j + span] = a - b;
                    }
            const float xl = inL[s] * inGain, xr = inR[s] * inGain;
            for (int i = 0; i < kN; ++i) line_[i].write(y[i] * kNorm + kInL[i] * xl + kInR[i] * xr);
        }
    }
}

} // namespace iv::dsp
