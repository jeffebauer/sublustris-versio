#include "dsp/Fdn.h"
#include "dsp/ProfileHook.h"

#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstring>

// The tank's rare paths (lineSlow, tapsMoving, tapsOld) are built for size, with the small DelayLine and
// LoopEra steps inlined into them: called out of line from -Os code, every read and write paid a call
// (~5 more a line-sample while SIZE crossfades). +160 B of flash for the three.
#if defined(__GNUC__)
#define IV_FLATTEN __attribute__((flatten))
#else
#define IV_FLATTEN
#endif

// The crossfade's faster paths (lineXf, the carried old taps; the SIZE-spike hunt, 27 Oct 2026); the profile
// firmware can switch them off to time v1.0.48's (prof::xfFast).
#if defined(IV_PROFILE_HOOKS)
#define IV_XF_FAST (prof::xfFast)
#else
#define IV_XF_FAST true
#endif

namespace iv::dsp {

namespace {

// Hadamard rows used to feed the inputs in and take the outputs out: four
// mutually orthogonal sign patterns, so L and R reach every line differently
// (true stereo in, ADR 0013) and the two outputs are decorrelated.
constexpr float kInL[8]  = {1, 1, 1, 1, -1, -1, -1, -1};
constexpr float kInR[8]  = {1, -1, 1, -1, 1, -1, 1, -1};
constexpr float kOutL[8] = {1, 1, -1, -1, 1, 1, -1, -1};
constexpr float kOutR[8] = {1, -1, -1, 1, 1, -1, -1, 1};

// Trial division over 2, 3 and then 6k ± 1 only: the same answers, ~6x fewer divisions (a SIZE move
// asks for 16 lines' primes every block, up to ~300 divisions each on the M7's slow integer divide).
bool isPrime(uint32_t n)
{
    if (n < 4) return n >= 2;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (uint32_t d = 5; d * d <= n; d += 6)
        if (n % d == 0 || n % (d + 2) == 0) return false;
    return true;
}

// Which odd numbers are prime, up to every line a 24 kHz tank can have at a 96 kHz host (two such
// rates' worth), sieved once at the first prepare(). The search by trial division ran for 16 lines
// at every SIZE change and SPACE flip: blocks of 150-210 % on the module, drop-outs (M3 round 9b).
// Odd n <-> bit n / 2; beyond the table, trial division as before. The same primes either way.
constexpr uint32_t kSieveMax = uint32_t(voicing::kFdnMaxLineSeconds * 48000.0f) + 64u;
// In the firmware's D2 SRAM (IV_D2_BSS, SizeOpt.h; CPU hunt, 28 Oct 2026: AXI SRAM is full), not zeroed at start-up.
IV_D2_BSS uint32_t gOddComposite[(kSieveMax / 2 + 31) / 32];
bool               gSieved = false;

IV_NOINLINE IV_SIZE_OPT void sievePrimes()
{
    for (auto& w : gOddComposite) w = 0u;
    for (uint32_t p = 3; p * p < kSieveMax; p += 2)
        if (!(gOddComposite[(p / 2) >> 5] & (1u << ((p / 2) & 31u))))
            for (uint32_t q = p * p; q < kSieveMax; q += 2 * p) gOddComposite[(q / 2) >> 5] |= 1u << ((q / 2) & 31u);
    gSieved = true;
}

bool isPrimeFast(uint32_t n)
{
    if (n >= kSieveMax || !gSieved) return isPrime(n);
    if (n < 3) return n == 2;
    if (!(n & 1u)) return false;
    return !(gOddComposite[(n / 2) >> 5] & (1u << ((n / 2) & 31u)));
}

// The nearest prime at or above n not already taken by a lower line. Built for size (a SIZE move's 16 lines).
IV_SIZE_OPT uint32_t primeAtOrAbove(uint32_t n, const float* taken, int count)
{
    for (;; ++n) {
        if (!isPrimeFast(n)) continue;
        bool clash = false;
        for (int i = 0; i < count; ++i) clash |= uint32_t(taken[i]) == n;
        if (!clash) return n;
    }
}

// The 80 ms glides' one-pole coefficient for a step of t seconds (Memo1, one copy for its two users).
float glide80(float t) { return 1.0f - std::exp(-t / 0.08f); }

constexpr float kXfAgreeA = 0.004f; // SIZE's crossfade: the reads' agreement follower (Fdn::xfMix), ~5 ms

float uniform(uint32_t& s) // −1..1
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (1.0f / 2147483648.0f);
}

} // namespace

IV_SIZE_OPT bool Fdn::prepare(Arena& arena, float sampleRate, uint32_t seed, float maxLineSeconds, Arena* apArena)
{
    constexpr float kApMax0 = apMaxSeconds(0), kApMax1 = apMaxSeconds(1); // (worked out here, not in the firmware)
    Arena& ap = apArena ? *apArena : arena;
    if (!gSieved) sievePrimes();
    sampleRate_ = sampleRate;
    shelfK_ = lossShelfK(sampleRate);
    maxLen_ = maxLineSeconds * sampleRate - 4.0f;
    ready_ = true;
    for (auto& l : line_) ready_ &= l.init(arena, size_t(maxLineSeconds * sampleRate));
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < 2; ++k) {
            apLen_[i][k] = uint32_t(voicing::kInLoopSeconds[i][k] * sampleRate);
            ready_ &= apLine_[i][k].init(ap, size_t((k ? kApMax1 : kApMax0) * sampleRate));
        }
    rng_ = seed ? seed : 1u;
    for (int i = 0; i < kN; ++i) {
        era_[i].prepare(sampleRate);
        driftGen_[i].prepare(sampleRate, seed * 2654435761u + uint32_t(i) * 40503u + 1u, float(i) / float(kN - 1));
    }
    { // both SPACEs' layouts ready (layoutEarly()), then the one in use
        const int keep = earlyCount_;
        earlyCount_ = voicing::kHallEarlyTaps;
        layoutEarly();
        earlyCount_ = voicing::kMassiveEarlyTaps;
        layoutEarly();
        earlyCount_ = keep;
    }
    layoutEarly();
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
    for (int i = 0; i < kN; ++i) { // setStagger's per-line state
        apAmtNow_[i] = apAmtOld_[i] = inLoopNow_;
        xfLine_[i] = false;
        xfOff_[i] = 0.0f;
    }
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

// Early taps: fixed points along each line, louder the later they sit, so the first sound is quiet and
// the tank swells from there. The 8 lines' taps interleave (each line offset by its own share of a slot,
// shorter lines first) instead of bunching (v1.0.21; the seeded jitter they replace still draws, so
// every build's signs are the same). earlyCount_ of the kEarlyTaps laid out (v1.0.36, ADR 0026: HALL 2,
// MASSIVE 4, the owner's pick on renders/early_hits), at the same early energy as all six.
IV_SIZE_OPT void Fdn::layoutEarly()
{
    // HALL's and MASSIVE's layouts are worked out once (prepare()) and copied at a SPACE flip, whose restart
    // block paid ~200 divides a tank for them (CPU save 2, 28 Oct 2026). The same values.
    if (lay_)
        for (int c = 0; c < 2; ++c)
            if (lay_->count[c] == earlyCount_) {
                std::memcpy(earlyFrac_, lay_->frac[c], sizeof earlyFrac_);
                std::memcpy(earlyGain_, lay_->gain[c], sizeof earlyGain_);
                return;
            }
    uint32_t es = 0xEA51u;
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < kEarlyTaps; ++k) {
            (void)uniform(es);
            earlyFrac_[i][k] = (float(k) + (float(i) + 0.5f) / float(kN)) / float(earlyCount_);
            earlyGain_[i][k] = voicing::kEarlyTapLow + (1.0f - voicing::kEarlyTapLow) * earlyFrac_[i][k];
            if (uniform(es) < 0.0f) earlyGain_[i][k] = -earlyGain_[i][k];
            if (k >= earlyCount_) earlyFrac_[i][k] = earlyGain_[i][k] = 0.0f;
        }
    if (earlyCount_ < kEarlyTaps) { // the same early energy from fewer taps
        float full = 0.0f, used = 0.0f;
        for (int i = 0; i < kN; ++i)
            for (int k = 0; k < kEarlyTaps; ++k) {
                const float fr = (float(k) + (float(i) + 0.5f) / float(kN)) / float(kEarlyTaps);
                const float g = voicing::kEarlyTapLow + (1.0f - voicing::kEarlyTapLow) * fr;
                full += g * g;
                used += earlyGain_[i][k] * earlyGain_[i][k];
            }
        const float norm = std::sqrt(full / used);
        for (auto& row : earlyGain_) for (auto& g : row) g *= norm;
    }
    const int c = earlyCount_ == voicing::kMassiveEarlyTaps ? 1 : (earlyCount_ == voicing::kHallEarlyTaps ? 0 : -1);
    if (lay_ && c >= 0) { // (another count, a prototype's, isn't kept)
        lay_->count[c] = earlyCount_;
        std::memcpy(lay_->frac[c], earlyFrac_, sizeof earlyFrac_);
        std::memcpy(lay_->gain[c], earlyGain_, sizeof earlyGain_);
    }
}

IV_SIZE_OPT void Fdn::setEarlyTapCount(int n)
{
    n = n > 0 && n < kEarlyTaps ? n : kEarlyTaps;
    if (n == earlyCount_) return;
    earlyCount_ = n;
    layoutEarly();
    for (int i = 0; i < kN; ++i)
        for (int k = 0; k < kEarlyTaps; ++k) {
            xfTapCorr_[i][k] = 1.0f;
            tapGainNow_[i][k] = earlyGain_[i][k];
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
    if (!pending) inLoopNow_ += glide80Memo_.get(dt, glide80) * (inLoopTarget_ - inLoopNow_);
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

// The new read and the old mixed, with the gain for their agreement (control). The old one first runs
// through the in-loop allpasses it had when they're leaving (gOld > 0).
IV_NOINLINE IV_SIZE_OPT float Fdn::xfMix(int i, float nw, float od, float gOld)
{
    constexpr float kA = kXfAgreeA;
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

IV_SIZE_OPT void Fdn::control(int n) // once per block: built for size (flash)
{
    const float dt = float(n) / sampleRate_;
    const float slew = voicing::kSizeSlewPerSecond * float(n); // samples of length per block
    const float floorDepth = voicing::kFloorDepthSeconds * sampleRate_ * modDepth_;
    const float floorCoeff = floorCoeffMemo_.get(dt, [](float t) { return 1.0f - std::exp(-2.0f * 3.14159265f * 0.3f * t); });

    // The floor governor's extra loss (ADR 0008).
    governor_.control(n, sampleRate_);
    const FloorGovernor& gov = leader_ ? leader_->governor_ : governor_; // a low tank follows its leader
    const float governRate = gov.extraRate;

    // A SIZE move waiting for its crossfade (below): new lengths, or the in-loop diffusion joining or
    // leaving (it started after a crossfade that had already gone, and joined on its own: a dropout).
    unsigned need = 0; // bit i: line i's length has to move
    for (int i = 0; i < kN; ++i) need |= unsigned(std::fabs(lenTarget_[i] - lenNow_[i]) > 0.5f) << i;
    const bool pending = ((inLoopTarget_ > 0.0f) != (inLoopNow_ > 0.0f)) || need != 0;
    controlInLoop(dt, n, slew, pending);
    if (staggered()) // a line waiting for its fade keeps its amount; every other line is on the shared one
        for (int i = 0; i < kN; ++i)
            if (!((need >> i) & 1u)) apAmtNow_[i] = inLoopNow_;
    earlyNow_ += glide80Memo_.get(dt, glide80) * (earlyTarget_ - earlyNow_);
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
    const float glideK = glideXfMemo_.get(dt, [](float t) { return 1.0f - std::exp(-t / (0.5f * voicing::kSizeCrossfadeSeconds)); });
    outputTrimNow_ += glideK * (xfTrim_ - outputTrimNow_);
    if (std::fabs(outputTrim_ - outputTrimNow_) < 1e-6f) outputTrimNow_ = outputTrim_;
    levelGain_ += glideK * (outputTrimNow_ * std::sqrt(meanLen / voicing::kFdnLevelRefSeconds) - levelGain_);
    const float rate = decayRate_ + governRate + extraRate_;
    if (pending && xfLeft_ <= 0 && xfRest_ >= voicing::kSizeRestSeconds) {
        // Staggered (v1.0.39): this fade takes one group's lines that need it (lines of the other
        // group wait for the next fade, which follows at once: the knob has rested); none, all.
        // (Not when the in-loop diffusion joins or leaves with this move, a small HALL crossing SIZE noon:
        // the lines that waited rejoined it on stale allpass memory, a click; then all fade together.)
        unsigned sw = 0xFFu; // bit i: line i fades now
        if (staggered() && (inLoopTarget_ > 0.0f) == (inLoopNow_ > 0.0f)) {
            // Group g: lines g, g + G, ... (G groups): 0x55 << g for two (0, 2, 4, 6 and 1, 3, 5, 7), 0x11 << g for four.
            const unsigned base = 0xFFu / ((1u << xfStagger_) - 1u);
            for (int t = 0; t < xfStagger_; ++t) {
                const int g = (xfGroupNext_ + t) & (xfStagger_ - 1);
                if (need & (base << g)) {
                    sw = base << g;
                    xfGroupNext_ = (g + 1) & (xfStagger_ - 1);
                    break;
                }
            }
        }
        prof::countXfStart();
        const float elapsed = xfSince_ >= 0 ? float(xfSince_) : -1.0f;
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
            xfLine_[i] = (sw >> i) & 1u;
            if (!xfLine_[i]) { // waits for the next fade: its own trip correction carries on where it was
                xfDelta_[i] = 0.0f;
                if (elapsed >= 0.0f) xfOff_[i] += elapsed;
                else xfLnGain_[i] = xfOff_[i] = 0.0f;
                continue;
            }
            xfOff_[i] = 0.0f;
            apAmtOld_[i] = apAmtNow_[i]; // this line's amount jumps with its lengths, its old one fading out
            apAmtNow_[i] = inLoopTarget_;
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
        const float s0 = float(xfSince_);
        float       s  = s0;
        // Until its hold ends a correction is exp(lnGain), the same as in the trip's first block (s 0: a fade's start),
        // so it is worked out there only (CPU hunt, 28 Oct 2026: every block, every line and tap of both tanks, up to
        // ~160 std::exp a block). A line waiting through a fade keeps its own trip (s > 0) or starts none (lnGain 0).
        auto corr = [&](float lnGain, float hold, float& val) {
            if (s < hold && s > 0.0f) return;
            if (s < hold) {
                val = std::exp(lnGain);
                return;
            }
            const float r = (s - hold) / float(xfLen_);
            val = r >= 1.0f ? 1.0f : std::exp(lnGain * (1.0f - r));
        };
        for (int i = 0; i < kN; ++i) {
            if (staggered()) s = s0 + xfOff_[i]; // staggered: each line's own trip
            corr(xfLnGain_[i], xfHold_[i], xfLineCorr_[i]);
            for (int k = 0; k < earlyCount_; ++k) corr(earlyFrac_[i][k] * xfLnGain_[i], earlyFrac_[i][k] * xfHold_[i], xfTapCorr_[i][k]);
        }
        s = s0;
        if (s > maxLen_ + float(xfLen_)) { // every line's trip done
            xfSince_ = -1;
            for (int i = 0; i < kN; ++i) {
                xfLineCorr_[i] = 1.0f;
                for (int k = 0; k < kEarlyTaps; ++k) xfTapCorr_[i][k] = 1.0f;
            }
        }
    }
    // The loss filters' inputs shared by every line (each line's own: its length): any change, all worked out again.
    bool absChanged = false;
    {
        const float in[5] = {rate, highRatio_, gov.darken, shelfK_, air_};
        for (int k = 0; k < 5; ++k)
            if (!(in[k] == absIn_[k])) {
                absIn_[k] = in[k];
                absChanged = true;
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
        const float amt = staggered() ? apAmtNow_[i] : inLoopNow_;
        const float apSeconds = amt > 0.0f ? (apLenGlide_[i][0] + apLenGlide_[i][1]) / sampleRate_ : 0.0f;
        // Worked out again only when its inputs changed (the two powf, ~210 cycles each): its length here, the
        // rest shared by every line (above).
        const float seconds = lenNow_[i] / sampleRate_ + apSeconds;
        if (absChanged || !(seconds == absSec_[i])) {
            absSec_[i] = seconds;
            prof::mark(prof::kTankCtl);
            absOut_[i] = absorbent(seconds, rate, highRatio_, gov.darken, shelfK_, air_);
            prof::mark(prof::kTankAbs);
        }
        const Absorb& a = absOut_[i];
        // While SIZE crossfades the loss filter goes from the old length's to the new one's with the fade
        // (switched at its start, the old read, all of the mix then, stepped to the new trip's loss: a click).
        const float o = xfLeft_ > 0 && !(staggered() && !xfLine_[i]) ? xfOld_ : 0.0f, w = 1.0f - o; // staggered: its own fade only
        b1_[i] = o * xfAbsOld_[i][0] + w * a.b1;
        gain_[i] = o * xfAbsOld_[i][1] + w * a.gain;
        // The trip's level correction after a SIZE jump rides on the early taps' gains (they hold no state,
        // so it's exact); the line's own read takes it per sample (on the loss filter, the in-loop
        // allpasses' memory sat between: the old read entered them x5 at a jump, a click).
        for (int k = 0; k < earlyCount_; ++k) tapGainNow_[i][k] = earlyGain_[i][k] * xfTapCorr_[i][k];
        pole_[i] = o * xfAbsOld_[i][2] + w * a.pole;
    }
}

// Line by line (overnight review, 26 Oct 2026). Each line runs its whole block in one go (its read, in-loop
// allpasses, loss filter and ERA, its state in registers), then the mix runs sample by sample from the
// lines' outputs, then the writes, then the early taps tap by tap. Sample by sample, every line's state went
// to memory and back every sample, and each line's reads were 8-way interleaved with the others'. Every
// float operation is the one the sample-by-sample loop did, in the same order for each sample (the same
// sums, the same Hadamard butterflies, the governor fed sample by sample), so the output is the same to
// the bit (tools/same_render.py). What makes it legal: a line's read at sample s of a block only hears what
// was written before the block when it sits further back than the block is long (blockFits), and the
// taps, reading after the block's writes, read where they would have.
void Fdn::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (!ready_) return;
    for (int start = 0; start < n; start += kControlBlock) {
        const int m = (n - start) < kControlBlock ? (n - start) : kControlBlock;
        prof::mark(prof::kTank);
        control(m);
        prof::mark(prof::kTankCtl);
        // Any real line fits (the shortest read is ~250 samples); else the block runs a sample at a time.
        const int step = blockFits(m) ? m : 1;
        for (int s = start; s < start + m; s += step) runBlock(inL + s, inR + s, outL + s, outR + s, step);
    }
}

// Every line's reads over the next m samples (and the old ones, while SIZE crossfades) sit at least m + 4
// back, so they only hear what was written before the block; and nothing a read or a tap reaches is
// overwritten by the block's own writes.
IV_SIZE_OPT bool Fdn::blockFits(int m) const
{
    const float fm = float(m);
    for (int i = 0; i < kN; ++i) {
        const float r0 = readNow_[i] + readStep_[i], r1 = readNow_[i] + fm * readStep_[i];
        const float t0 = tapLen_[i] + tapStep_[i], t1 = tapLen_[i] + fm * tapStep_[i];
        float lo = std::fmin(r0, r1), hi = std::fmax(std::fmax(r0, r1), std::fmax(t0, t1));
        if (xfLeft_ > 0) {
            lo = std::fmin(lo, lo + xfDelta_[i]);
            hi = std::fmax(hi, hi + xfDelta_[i]);
        }
        if (lo < fm + 4.0f || hi + fm + 4.0f > float(line_[i].mask)) return false;
    }
    return true;
}

// m samples on this control block's values (m = the block, or 1 when it doesn't fit).
IV_NO_UNSWITCH void Fdn::runBlock(const float* inL, const float* inR, float* outL, float* outR, int m)
{
    constexpr float kNorm = 0.35355339f; // 1 / sqrt(8): keeps the Hadamard mix energy-preserving
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

    // While SIZE crossfades: the fade's gains for the first q samples (a rotation by the angle per
    // sample, exact at the end), stepped ahead here; nothing else in the block moves them.
    float xn[kControlBlock], xo[kControlBlock];
    int q = 0;
    for (; q < m && xfLeft_ > 0; ++q) {
        xn[q] = xfNew_;
        xo[q] = xfOld_;
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
    const float endNew = xfNew_, endOld = xfOld_;

    // The lines, each over the block: the fast path, or (crossfading, an allpass gliding or filling) the
    // sample loop's own steps.
    DelayLine was[kN]; // each line as it stood at the block's start
    float y[kN][kControlBlock];
    for (int i = 0; i < kN; ++i) {
        was[i] = line_[i];
        const int qi = staggered() && !xfLine_[i] ? 0 : q; // staggered: a line waiting for its fade reads once
        // Staggered: each line on its own in-loop amount (one waiting for its fade keeps the old one).
        const float apGi = staggered() ? voicing::kInLoopCoeff * apAmtNow_[i] : apG;
        const float apGOldi = staggered() ? (qi > 0 ? voicing::kInLoopCoeff * apAmtOld_[i] : 0.0f) : apGOld;
        if (qi > 0) prof::countXfLines(this);
        if (qi > 0 ? !(IV_XF_FAST && lineXf(i, m, qi, xn, xo, apGi, apGOldi, eraIn, eraOut, y[i]))
                   : !lineFast(i, m, apGi, eraIn, eraOut, y[i]))
            lineSlow(i, m, qi, xn, xo, apGi, apGOldi, eraIn, eraOut, y[i]);
    }
    xfNew_ = endNew;
    xfOld_ = endOld;
    prof::mark(prof::kTankLoop);

    // The mix, sample by sample: the outputs, the governor (fed in sample order), the Hadamard.
    float in[kN][kControlBlock], ls[kControlBlock], rs[kControlBlock];
    for (int s = 0; s < m; ++s) {
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < kN; ++i) {
            l += kOutL[i] * y[i][s];
            r += kOutR[i] * y[i][s];
        }
        governor_.add(0.5f * outGain * outGain * (l * l + r * r) + externalPower_); // the tank's own level (+ a low tank's), before the taps
        ls[s] = l;
        rs[s] = r;
        // Fast Walsh–Hadamard transform: 24 adds for the 8×8 mix, on locals (as a loop, GCC kept it on the stack).
        const float a0 = y[0][s] + y[1][s], a1 = y[0][s] - y[1][s], a2 = y[2][s] + y[3][s], a3 = y[2][s] - y[3][s];
        const float a4 = y[4][s] + y[5][s], a5 = y[4][s] - y[5][s], a6 = y[6][s] + y[7][s], a7 = y[6][s] - y[7][s];
        const float b0 = a0 + a2, b2 = a0 - a2, b1 = a1 + a3, b3 = a1 - a3;
        const float b4 = a4 + a6, b6 = a4 - a6, b5 = a5 + a7, b7 = a5 - a7;
        const float c[kN] = {b0 + b4, b1 + b5, b2 + b6, b3 + b7, b0 - b4, b1 - b5, b2 - b6, b3 - b7};
        const float xl = inL[s] * inGain, xr = inR[s] * inGain;
        for (int i = 0; i < kN; ++i) in[i][s] = c[i] * kNorm + kInL[i] * xl + kInR[i] * xr;
    }
    for (int i = 0; i < kN; ++i)
        for (int s = 0; s < m; ++s) line_[i].write(in[i][s]);
    prof::mark(prof::kTankMix);

    // The early taps, after the writes: the tap at sample s reads where it did before sample s's write
    // (what it reaches was written before it, and nothing it reaches was overwritten: blockFits).
    float e[kN][kControlBlock], ol[kControlBlock], orr[kControlBlock]; // each line's taps; the old ones' sums
    for (int s = 0; s < q; ++s) ol[s] = orr[s] = 0.0f;
    for (int i = 0; i < kN; ++i) {
        float tl[kControlBlock];
        float len = tapLen_[i];
        const float step = tapStep_[i];
        for (int s = 0; s < m; ++s) tl[s] = (len += step); // v1.0.25: no per-block steps on SIZE moves
        tapLen_[i] = len;
        if (!(earlyG > 0.0f)) continue;
        for (int s = 0; s < m; ++s) e[i][s] = 0.0f;
        const DelayLine& w = was[i];
        if (step == 0.0f) { // still (always, but after a snap): each tap one stream, a0 the next sample's a1
            int k = 0;
            if (w.full()) // two taps a pass, each sample's sum in a register (still in k order)
                for (; k + 1 < earlyCount_; k += 2) {
                    const float    g0 = tapGainNow_[i][k], g1 = tapGainNow_[i][k + 1];
                    const float    d0 = 1.0f + earlyFrac_[i][k] * tl[0], d1 = 1.0f + earlyFrac_[i][k + 1] * tl[0];
                    const auto     i0 = uint32_t(d0), i1 = uint32_t(d1);
                    const float    t0 = d0 - float(i0), t1 = d1 - float(i1);
                    const uint32_t p0 = w.pos - i0, p1 = w.pos - i1;
                    float b0 = w.buf[(p0 - 1u) & w.mask], b1 = w.buf[(p1 - 1u) & w.mask];
                    for (int s = 0; s < m; ++s) {
                        const float a0 = w.buf[(p0 + uint32_t(s)) & w.mask], a1 = w.buf[(p1 + uint32_t(s)) & w.mask];
                        float sum = e[i][s];
                        sum += g0 * (a0 + t0 * (b0 - a0));
                        sum += g1 * (a1 + t1 * (b1 - a1));
                        e[i][s] = sum;
                        b0 = a0;
                        b1 = a1;
                    }
                }
            for (; k < earlyCount_; ++k) {
                const float g  = tapGainNow_[i][k]; // the tap's gain x the trip's correction (per block)
                const float d  = 1.0f + earlyFrac_[i][k] * tl[0];
                const auto  di = uint32_t(d);
                const float t  = d - float(di);
                // DelayLine::read sample by sample: silence until a clear()'s writes reach the tap.
                const int      lag = w.full() ? -1 : int(di) - int(w.fill);
                const uint32_t p   = w.pos - di;
                float a1 = lag < 0 ? w.buf[(p - 1u) & w.mask] : 0.0f;
                for (int s = 0; s < m; ++s) {
                    const float a0 = s >= lag ? w.buf[(p + uint32_t(s)) & w.mask] : 0.0f;
                    e[i][s] += g * (a0 + t * (a1 - a0));
                    a1 = a0;
                }
            }
        } else {
            tapsMoving(i, m, tl, w, e[i]);
        }
        if (q > 0 && staggered() && !xfLine_[i]) { // waiting for its fade: its taps on both sides of the mix
            const int j = (i + 3) & (kN - 1);         // (the fade's gains sum to one, so they pass as they are)
            for (int s = 0; s < q; ++s) {
                ol[s] += kOutL[j] * e[i][s];
                orr[s] += kOutR[j] * e[i][s];
            }
        } else if (q > 0 && step == 0.0f && w.full() && IV_XF_FAST) {
            // The old taps while SIZE crossfades, the everyday case (the taps jumped with the lines, so each old
            // tap sits still all block), as the still taps above: each tap one stream, its b1 the sample before's
            // b0 (tapsOld reads both, every sample, through a copy of the line). The same values read and the same
            // sums in the same order (each sample's taps in k order, then the line's signs), so the same output to
            // the bit, with half the old taps' SDRAM reads (the SIZE-spike hunt, 27 Oct 2026).
            float       eo[kControlBlock];
            const float tlo = tl[0] + xfDelta_[i];
            for (int s = 0; s < q; ++s) eo[s] = 0.0f;
            for (int k = 0; k < earlyCount_; ++k) {
                const float    g  = earlyGain_[i][k];
                const float    od = 1.0f + earlyFrac_[i][k] * tlo;
                const auto     oi = uint32_t(od);
                const float    ot = od - float(oi);
                const uint32_t p  = w.pos - oi;
                float b1 = w.buf[(p - 1u) & w.mask];
                for (int s = 0; s < q; ++s) {
                    const float b0 = w.buf[(p + uint32_t(s)) & w.mask];
                    eo[s] += g * (b0 + ot * (b1 - b0));
                    b1 = b0;
                }
            }
            const int j = (i + 3) & (kN - 1);
            for (int s = 0; s < q; ++s) {
                ol[s] += kOutL[j] * eo[s];
                orr[s] += kOutR[j] * eo[s];
            }
        } else if (q > 0) {
            tapsOld(i, q, tl, w, ol, orr);
        }
    }
    for (int s = 0; s < m; ++s) {
        float l = ls[s], r = rs[s];
        if (earlyG > 0.0f) {
            float tapL = 0.0f, tapR = 0.0f;
            for (int i = 0; i < kN; ++i) {
                // Each line's taps go out with another line's signs, so they decorrelate from the ends.
                const int j = (i + 3) & (kN - 1);
                tapL += kOutL[j] * e[i][s];
                tapR += kOutR[j] * e[i][s];
            }
            if (s < q) { // the old positions fade out with the lines
                tapL = xn[s] * tapL + xo[s] * ol[s];
                tapR = xn[s] * tapR + xo[s] * orr[s];
            }
            l += earlyG * tapL;
            r += earlyG * tapR;
        }
        outL[s] += outGain * l;
        outR[s] += outGain * r;
    }
    if (xfSince_ >= 0) xfSince_ += m;
    prof::mark(prof::kTankOut);
}

// One line over the block, the everyday case: full or filling, its in-loop allpasses (if any) at their
// exact length all block. Its state in locals; ERA after, over the block. False: lineSlow's case.
IV_NO_UNSWITCH bool Fdn::lineFast(int i, int m, float apG, float eraIn, float eraOut, float* y)
{
    float*   apBuf[2];
    uint32_t apPos[2], apMask[2], apL[2];
    for (int k = 0; k < 2; ++k) {
        const DelayLine& a = apLine_[i][k];
        if (apG > 0.0f && !(apLenStep_[i][k] == 0.0f && apLenGlide_[i][k] == float(apLenNow_[i][k])
                            && apLenGlide_[i][k] >= 1.0f && a.full()))
            return false;
        apBuf[k] = a.buf;
        apPos[k] = a.pos;
        apMask[k] = a.mask;
        apL[k] = apLenNow_[i][k];
    }
    const DelayLine& line = line_[i];
    const float*   buf = line.buf;
    const uint32_t mask = line.mask, pos0 = line.pos, fill0 = line.fill;
    const bool     full = line.full();
    const float    step = readStep_[i], corr = xfLineCorr_[i], gain = gain_[i], b1 = b1_[i], pole = pole_[i];
    float read = readNow_[i], state = interp_[i], ay = absorbY_[i], ax = absorbX_[i];
    // A read that holds still (the low tank: no modulation) has one position all block (read + 0 is read),
    // so its coefficient and its divide are worked out once.
    const bool still = step == 0.0f;
    uint32_t   di  = 0;
    float      eta = 0.0f;
    if (still) {
        di = uint32_t(int32_t(read - 0.5f));
        const float f = read - float(di);
        eta = (1.0f - f) / (1.0f + f);
    }
    for (int s = 0; s < m; ++s) {
        // DelayLine::readAllpass as it stood at sample s (filling: through ago(), silence before the clear).
        const uint32_t pos = pos0 + uint32_t(s), fill = fill0 + uint32_t(s);
        if (!still) {
            read += step;
            di = uint32_t(int32_t(read - 0.5f));
            const float f = read - float(di);
            eta = (1.0f - f) / (1.0f + f);
        }
        float a, b;
        if (full || di + 1 <= fill) {
            a = buf[(pos - di) & mask];
            b = buf[(pos - di - 1) & mask];
        } else {
            a = di <= fill ? buf[(pos - di) & mask] : 0.0f;
            b = 0.0f;
        }
        state = b + eta * (a - state);
        float x = state * corr;
        if (apG > 0.0f)
            for (int k = 0; k < 2; ++k) { // in-loop diffusion (MASSIVE, a small HALL): read(L) and write()
                const float d = apBuf[k][(apPos[k] - apL[k]) & apMask[k]];
                const float v = x + apG * d;
                apBuf[k][apPos[k]] = v;
                apPos[k] = (apPos[k] + 1) & apMask[k];
                x = d - apG * v;
            }
        ay = gain * x + b1 * ax + pole * ay;
        ax = x;
        y[s] = ay;
    }
    era_[i].run(y, m, eraIn, eraOut);
    if (apG > 0.0f)
        for (int k = 0; k < 2; ++k) apLine_[i][k].pos = apPos[k];
    readNow_[i] = read;
    interp_[i] = state;
    absorbY_[i] = ay;
    absorbX_[i] = ax;
    return true;
}

// One line over the block while it crossfades (its first q samples), the everyday case: full, and its in-loop
// allpasses (MASSIVE, a small HALL) in the loop at their exact lengths before and after (or in neither: a large
// HALL), or joining or leaving with the fade (a small HALL's SIZE across noon: xfAllpass on the new read or the
// old, CPU hunt 28 Oct 2026: a HALL SIZE sweep's 16 lines in lineSlow took ~30 % of the module). lineSlow's steps
// for those cases with its state in locals, as lineFast's (lineSlow went through a copy of the line, the members
// and an out-of-line xfMix every sample): the same float operations in the same order, so the same output to the
// bit (the SIZE-spike hunt, 27 Oct 2026). Built for size, as lineSlow (flash). False: lineSlow's case (an
// allpass gliding or not yet full, a line filling).
IV_SIZE_OPT bool Fdn::lineXf(int i, int m, int q, const float* xn, const float* xo, float apG, float apGOld,
                                float eraIn, float eraOut, float* y)
{
    const DelayLine& line = line_[i];
    if (!line.full()) return false;
    const bool join = apG > 0.0f && !(apGOld > 0.0f), leave = apGOld > 0.0f && !(apG > 0.0f);
    float*   apBuf[2] = {};
    uint32_t apPos[2] = {}, apMask[2] = {}, apL[2] = {};
    float    apX[2] = {}; // the joining or leaving allpasses' lengths (xfAllpass: the new, or the old)
    if (apG > 0.0f || apGOld > 0.0f)
        for (int k = 0; k < 2; ++k) {
            const DelayLine& a = apLine_[i][k];
            const float      glide = apLenGlide_[i][k];
            apX[k] = leave ? apOld_[i][k] : glide;
            // Leaving, or joining with the fade all block (the allpasses run only through apXf: their length still,
            // where it is, gliding or not): any length from one sample. Else at their exact lengths, still.
            if (!(a.full() && ((leave || (join && q == m)) ? apX[k] >= 1.0f
                                                          : apLenStep_[i][k] == 0.0f && glide == float(apLenNow_[i][k])
                                                                && glide >= 1.0f && (join || apOld_[i][k] == glide))))
                return false;
            apBuf[k] = a.buf;
            apPos[k] = a.pos;
            apMask[k] = a.mask;
            apL[k] = apLenNow_[i][k];
        }
    // xfAllpass: the two allpasses at their joining or leaving lengths (between samples), each read and written.
    auto apXf = [&](float x, float g) {
        for (int k = 0; k < 2; ++k) {
            const auto     li = uint32_t(apX[k]);
            const float    t = apX[k] - float(li);
            const uint32_t at = apPos[k] - li;
            const float    a0 = apBuf[k][at & apMask[k]], a1 = apBuf[k][(at - 1u) & apMask[k]];
            const float    d = a0 + t * (a1 - a0);
            const float    v = x + g * d;
            apBuf[k][apPos[k]] = v;
            apPos[k] = (apPos[k] + 1) & apMask[k];
            x = d - g * v;
        }
        return x;
    };
    const float*   buf = line.buf;
    const uint32_t mask = line.mask, pos0 = line.pos;
    const float    step = readStep_[i], corr = xfLineCorr_[i], gain = gain_[i], b1 = b1_[i], pole = pole_[i];
    const float    delta = xfDelta_[i], K = xfK_[i];
    float read = readNow_[i], state = interp_[i], stateOld = interpOld_[i], ay = absorbY_[i], ax = absorbX_[i];
    float oo = xfOO_[i], nn = xfNN_[i], on = xfON_[i];
    for (int s = 0; s < m; ++s) {
        const uint32_t pos = pos0 + uint32_t(s);
        read += step;
        // DelayLine::readAllpass, the new length, then (crossfading) the old one on its own state.
        const auto  di = uint32_t(int32_t(read - 0.5f));
        const float f = read - float(di);
        const float eta = (1.0f - f) / (1.0f + f);
        const float a = buf[(pos - di) & mask], b = buf[(pos - di - 1) & mask];
        state = b + eta * (a - state);
        float x = state * corr;
        float g = apG;
        if (s < q) {
            const float dO = read + delta;
            const auto  dOi = uint32_t(int32_t(dO - 0.5f));
            const float fO = dO - float(dOi);
            const float etaO = (1.0f - fO) / (1.0f + fO);
            const float aO = buf[(pos - dOi) & mask], bO = buf[(pos - dOi - 1) & mask];
            stateOld = bO + etaO * (aO - stateOld);
            float od = stateOld;
            if (leave) od = apXf(od, apGOld); // xfMix's: the old read through the allpasses it had
            if (join) x = apXf(x, apG);       // the new read through the allpasses it joins
            oo += kXfAgreeA * (od * od - oo); // xfMix
            nn += kXfAgreeA * (x * x - nn);
            on += kXfAgreeA * (od * x - on);
            x = K * (xn[s] * x + xo[s] * od);
            g = xn[s] * apG + xo[s] * apGOld;
        }
        if (apG > 0.0f && !(join && s < q)) // (joining, the fade's samples ran them above)
            for (int k = 0; k < 2; ++k) { // in-loop diffusion: read(L) and write()
                const float d = apBuf[k][(apPos[k] - apL[k]) & apMask[k]];
                const float v = x + g * d;
                apBuf[k][apPos[k]] = v;
                apPos[k] = (apPos[k] + 1) & apMask[k];
                x = d - g * v;
            }
        ay = gain * x + b1 * ax + pole * ay;
        ax = x;
        y[s] = ay;
    }
    era_[i].run(y, m, eraIn, eraOut);
    if (apG > 0.0f || apGOld > 0.0f)
        for (int k = 0; k < 2; ++k) apLine_[i][k].pos = apPos[k];
    readNow_[i] = read;
    interp_[i] = state;
    interpOld_[i] = stateOld;
    absorbY_[i] = ay;
    absorbX_[i] = ax;
    xfOO_[i] = oo;
    xfNN_[i] = nn;
    xfON_[i] = on;
    return true;
}

// One line over the block, the sample loop's own steps: SIZE crossfading (its first q samples), an
// allpass gliding, joining or leaving, or filling after a clear(). Rare, so built for size (flash).
IV_NOINLINE IV_SIZE_OPT IV_FLATTEN void Fdn::lineSlow(int i, int m, int q, const float* xn, const float* xo, float apG,
                                          float apGOld, float eraIn, float eraOut, float* y)
{
    for (int s = 0; s < m; ++s) {
        const DelayLine v = line_[i].ahead(uint32_t(s)); // as it stood at sample s (its writes come after)
        const bool xf = s < q;
        if (xf) {
            xfNew_ = xn[s];
            xfOld_ = xo[s];
        }
        readNow_[i] += readStep_[i];
        float x = v.readAllpass(readNow_[i], interp_[i]) * xfLineCorr_[i];
        if (xf) {
            // The old length, read the way it was before (allpass, its own state: a cubic read there differed
            // by a hair from the sample before, a click at the start), on the old timeline (no correction).
            const float od = v.readAllpass(readNow_[i] + xfDelta_[i], interpOld_[i]);
            if (apG > 0.0f && apGOld == 0.0f) x = xfAllpass(i, x, apG, false); // joining: on the new read
            x = xfMix(i, x, od, apG == 0.0f ? apGOld : 0.0f);                  // leaving: on the old
        }
        if (apG > 0.0f && (!xf || apGOld > 0.0f))
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
                if (xf) { // in the loop before and after: its length and amount crossfade
                    if (apOld_[i][k] != glide) d = xfOldAllpass(i, k, x, d);
                    g = xfNew_ * apG + xfOld_ * apGOld;
                }
                const float v = x + g * d;
                apLine_[i][k].write(v);
                x = d - g * v;
            }
        absorbY_[i] = gain_[i] * x + b1_[i] * absorbX_[i] + pole_[i] * absorbY_[i];
        absorbX_[i] = x;
        y[s] = eraOut * era_[i].process(eraIn * absorbY_[i]);
    }
}

// A line's early taps while they move (the block after a snap), each read as it stood at its sample.
IV_NOINLINE IV_SIZE_OPT IV_FLATTEN void Fdn::tapsMoving(int i, int m, const float* tl, const DelayLine& was, float* e) const
{
    for (int k = 0; k < earlyCount_; ++k)
        for (int s = 0; s < m; ++s) {
            const DelayLine v  = was.ahead(uint32_t(s));
            const float     d  = 1.0f + earlyFrac_[i][k] * tl[s];
            const auto      di = uint32_t(d);
            const float     t  = d - float(di);
            const float a0 = v.read(di), a1 = v.read(di + 1);
            e[s] += tapGainNow_[i][k] * (a0 + t * (a1 - a0));
        }
}

// SIZE's crossfade, its first q samples: a line's early taps at the old positions, on the old timeline
// (no correction), summed into the old taps' output with the line's signs.
IV_NOINLINE IV_SIZE_OPT IV_FLATTEN void Fdn::tapsOld(int i, int q, const float* tl, const DelayLine& was, float* ol, float* orr) const
{
    const int j = (i + 3) & (kN - 1);
    for (int s = 0; s < q; ++s) {
        const DelayLine v = was.ahead(uint32_t(s));
        float e = 0.0f;
        for (int k = 0; k < earlyCount_; ++k) {
            const float od = 1.0f + earlyFrac_[i][k] * (tl[s] + xfDelta_[i]);
            const auto  oi = uint32_t(od);
            const float ot = od - float(oi);
            const float b0 = v.read(oi), b1 = v.read(oi + 1);
            e += earlyGain_[i][k] * (b0 + ot * (b1 - b0));
        }
        ol[s] += kOutL[j] * e;
        orr[s] += kOutR[j] * e;
    }
}

} // namespace iv::dsp
