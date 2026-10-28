#include "dsp/Engine.h"
#include "dsp/ProfileHook.h"

#include "dsp/SizeOpt.h"
#include "params/Mappings.h"

#include <cmath>
#include <cstddef>
#include <cstring>


namespace iv {

namespace {

// The same bits in every field of an ERA mix as the bytes of one kept (so +0 and -0 differ): then
// LoopEra::set would store what it holds.
static_assert(sizeof(dsp::EraMix) == sizeof(float), "sameEra compares EraMix as floats with no padding");
bool sameEra(const unsigned char* kept, const dsp::EraMix& m) { return std::memcmp(kept, &m, sizeof m) == 0; }

// One-pole smoothing coefficient for a time constant, per control chunk.
float chunkCoeff(float ms, float sampleRate, int n)
{
    if (ms <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-float(n) / (0.001f * ms * sampleRate));
}

// One-pole coefficient for a time constant in seconds, per block of n: out of line, so its six callers
// share one copy (flash, v1.0.51).
IV_CTL_NOINLINE IV_CTL_OPT float blockCoeff(float seconds, float sampleRate, int n)
{
    return 1.0f - std::exp(-float(n) / (seconds * sampleRate));
}

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Moves `now` toward `target` by at most `step` (linear glides).
float approach(float now, float target, float step)
{
    const float d = target - now;
    return d > step ? now + step : (d < -step ? now - step : target);
}

} // namespace

// Early taps per line by SPACE (v1.0.36, ADR 0026; owner's pick B on renders/early_hits): HALL 2,
// MASSIVE 4, of the six laid out (one per line opened MASSIVE's dip before a hit's tail back up:
// 16.9 dB at SIZE full; four 2.9, six 1.9). The twin follows its space.
IV_SIZE_OPT void Engine::setEarlyTapsForSpace()
{
    int n = activeSpace_ == 2 ? voicing::kMassiveEarlyTaps : voicing::kHallEarlyTaps;
#ifndef IV_FIXED_VOICINGS
    if (fitEarlyTaps_ > 0) n = fitEarlyTaps_;
#endif
    fdn_.setEarlyTapCount(n);
    lowFdn_.setEarlyTapCount(n);
}

#ifndef IV_FIXED_VOICINGS
void Engine::setEarlyTapPrototype(int earlyTaps)
{
    fitEarlyTaps_ = earlyTaps;
    prepare(sampleRate_, maxBlockSize_);
}

void Engine::setSwellSeedPrototype(uint32_t seed)
{
    fitSwellSeed_ = seed;
    prepare(sampleRate_, maxBlockSize_);
}

void Engine::setLowDecimatePrototype(int d)
{
    lowDecimate_ = d < kLowDecimate ? kLowDecimate : (d > kLowDecimateMax ? kLowDecimateMax : d);
    prepare(sampleRate_, maxBlockSize_);
}

void Engine::setSwellTapsPrototype(int taps)
{
    fitSwellTaps_ = taps;
    prepare(sampleRate_, maxBlockSize_);
}

#endif

IV_SIZE_OPT void Engine::attachMemory(float* large, size_t largeCount, float* fast, size_t fastCount, float* axi,
                                      size_t axiCount)
{
    attachedLarge_ = large;
    attachedLargeCount_ = largeCount;
    attachedFast_ = fast;
    attachedFastCount_ = fastCount;
    attachedAxi_ = axi;
    attachedAxiCount_ = axiCount;
}

IV_SIZE_OPT void Engine::prepare(float sampleRate, int maxBlockSize)
{
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;
    for (const auto& p : kParams) {
        values_[static_cast<size_t>(p.id)] = p.defaultValue;
        smooth_[static_cast<size_t>(p.id)] = p.defaultValue;
    }

    float* large = attachedLarge_;
    float* fast = attachedFast_;
    float* axi = attachedAxi_;
    size_t largeCount = attachedLargeCount_, fastCount = attachedFastCount_, axiCount = attachedAxiCount_;
#if !defined(IV_NO_HEAP)
    if (!large) {
        ownLarge_.assign(largeFloats(sampleRate), 0.0f);
        large = ownLarge_.data();
        largeCount = ownLarge_.size();
    }
    if (!fast) {
        ownFast_.assign(fastFloats(sampleRate), 0.0f);
        fast = ownFast_.data();
        fastCount = ownFast_.size();
    }
    if (!axi) {
        ownAxi_.assign(axiFloats(sampleRate), 0.0f);
        axi = ownAxi_.data();
        axiCount = ownAxi_.size();
    }
#endif
    large_.reset(large, largeCount, true); // SDRAM: each line on its own cache set (dsp/Memory.h)
    fast_.reset(fast, fastCount);
    axi_.reset(axi, axiCount, true); // AXI SRAM is cached too: the same stagger

    ready_ = true;
    for (auto& p : predelay_) ready_ &= p.init(large_, size_t(voicing::kPredelayMaxSeconds * sampleRate));
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            const float s = voicing::kDiffuserSeconds[k] * (c ? voicing::kDiffuserRightScale : 1.0f);
            ready_ &= diffuser_[c].stage[k].init(fast_, uint32_t(s * sampleRate), voicing::kDiffuserCoeff[k]);
        }
#ifndef IV_FIXED_VOICINGS
    swell_.setSeed(fitSwellSeed_); // before prepare(): it lays the taps out from the seed
    twinSwell_.setSeed(fitSwellSeed_);
    swell_.setTapCount(fitSwellTaps_); // likewise the count (0 = as built)
    twinSwell_.setTapCount(fitSwellTaps_);
#endif
    ready_ &= swell_.prepare(large_, sampleRate);
    ready_ &= twinSwell_.prepare(large_, sampleRate); // PLATE's twin's (HALL's and MASSIVE's room has none)
    twinSwell_.setSCurve(true); // eases in, then settles (owner, renders/swell_curve)
    // The tank and its twin at half the rate (v1.0.35, ADR 0026; dsp/HalfRate.h).
    ready_ &= fdn_.prepare(large_, 0.5f * sampleRate, 0x1D5A7u, voicing::kFdnMaxLineSeconds, &axi_);
    ready_ &= plate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x9A7Eu);
    ready_ &= shimmer_.prepare(large_, sampleRate);
    ready_ &= lowFdn_.prepare(large_, sampleRate / float(lowDecimate_), 0x10F7u, voicing::kFdnMaxLineSeconds, &axi_);
    // HALL's and MASSIVE's twin, the small room (v1.0.43), at the tank's rate; its rings in AXI SRAM, one slice.
    ready_ &= room_.prepare(0.5f * sampleRate, 0x5A11u, axi_.take(dsp::TwinRoom::memoryFloats(0.5f * sampleRate)));
    roomGain_[0] = std::pow(10.0f, voicing::kTwinRoomHallDb / 20.0f);
    roomGain_[1] = std::pow(10.0f, voicing::kTwinRoomMassiveDb / 20.0f);
    roomLevel_ = 0.0f;
    roomStepN_ = -1;
    lowFdn_.setGovernorLeader(&fdn_);
    {
        // The split: a 2nd-order Butterworth pair (bilinear). The two tanks' outputs are uncorrelated,
        // so they add in power, and a Butterworth pair is power complementary (|L|² + |H|² = 1).
        auto butter = [this](float f, float* lp, float* hp) {
            const float k = dsp::bilinearK(f, sampleRate_);
            const float nrm = 1.0f / (1.0f + 1.41421356f * k + k * k);
            lp[0] = k * k * nrm; lp[1] = 2.0f * lp[0]; lp[2] = lp[0];
            hp[0] = nrm; hp[1] = -2.0f * nrm; hp[2] = nrm;
            lp[3] = hp[3] = 2.0f * (k * k - 1.0f) * nrm;
            lp[4] = hp[4] = (1.0f - 1.41421356f * k + k * k) * nrm;
        };
        float unused[5];
        butter(voicing::kStillLowsHz, xoL_, xoH_);
        butter(kLowAntiAliasHz, aaL_, unused); // flat below the split; with it, −70 dB at 6 kHz
    }
    ready_ &= twinPlate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x7B2Fu);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            const float s = voicing::kDiffuserSeconds[k] * (c ? voicing::kDiffuserRightScale : 1.0f) * 1.13f;
            ready_ &= twinDiffuser_[c].stage[k].init(fast_, uint32_t(s * sampleRate), voicing::kDiffuserCoeff[k]);
        }
    gesture_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
    toneBell_.prepare(sampleRate);
    transport_.prepare(sampleRate, 0x7a9e5u);
    limiter_.prepare(sampleRate);
    reset();
}

IV_SIZE_OPT void Engine::reset()
{
    eraSentOk_ = 0; // the tanks were prepared or reset: send them their ERA afresh
    buttonDown_ = false;
    gateHigh_   = false;
    edgeCount_  = 0;
    clock_      = 0;
    gesture_.reset();
    freezeMix_ = 0.0f;
    tankIn_ = 1.0f;
    tankInTarget_ = 1.0f;
    paint_ = 0.0f;
    paintRoom_ = 1.0f;
    tankInStep_ = 0.0f;
    twinActive_ = false;
    twinIn_ = twinInStep_ = 0.0f;
    room_.reset();
    roomLevel_ = 0.0f;
    twinSized_ = false;
    resetHalf(twinHalf_);
    twinPlate_.reset();
    twinSwell_.reset();
    for (auto& d : twinDiffuser_) d.clear();
    inputPower_ = inputSlow_ = duckSustained_ = 0.0f;
    tankInPower_ = 0.0f;
    quietSince_ = 0;
    tailCaptured_ = false;
    smooth_ = values_;
    for (auto& p : predelay_) p.clear();
    for (auto& d : diffuser_) d.clear();
    activeSpace_ = normalisedToSwitch(values_[static_cast<size_t>(ParamId::Space)]);
    setEarlyTapsForSpace();
    flipPhase_ = 0;
    flipGain_ = 1.0f;
    flipStep_ = 0.0f;
    onsetDip_ = 0.0f;
    applySpace(true);
    swell_.reset();
    fdn_.reset();
    lowFdn_.reset();
    for (auto& c : xoZ_) for (auto& z : c) z[0] = z[1] = 0.0f;
    lowPhase_ = 0;
    lowPrev_[0] = lowPrev_[1] = lowCur_[0] = lowCur_[1] = 0.0f;
    resetHalf(tankHalf_);
    resetHalf(twinHalf_);
    plate_.reset();
    shimmer_.reset();
    tilt_.reset();
    tilt_.set(smoothed(ParamId::Tone));
    tilt_.snap();
    toneBell_.reset();
    toneBell_.set(smoothed(ParamId::Tone), kChunk);
    toneBell_.snap();
    transport_.reset(transportOn_); // (applySpace(true) above set it)
    limiter_.reset();
    texture_.reset();
    duck_ = duckGrDb_ = duckGainStep_ = 0.0f;
    duckGain_ = 1.0f;
    hissLp_[0] = hissLp_[1] = 0.0f;
    noiseSeed_ = 0xA11CEu;
    shimmerMix_ = shimmerTarget_;
    pdXfLeft_ = 0;
    pdOldG_ = 0.0f;
    pdNewG_ = 1.0f;
    pdRest_ = 0.0f;
    const auto g = map::mixGains(smoothed(ParamId::Mix));
    dryNow_ = g.dry;
    wetNow_ = g.wet;
    dryStep_ = wetStep_ = 0.0f;
}

void Engine::setParam(ParamId id, float normalised)
{
    if (normalised < 0.0f) normalised = 0.0f;
    if (normalised > 1.0f) normalised = 1.0f;
    values_[static_cast<size_t>(id)] = normalised;
}

// ---- Button and gate (ADR 0011) ---------------------------------------------

IV_SIZE_OPT void Engine::queueEdge(bool isButton, bool level, int offset) // once per edge: built for size (flash)
{
    if (edgeCount_ >= kMaxEdges) return; // more than 16 edges in one block: drop the rest
    // Keep the queue in time order (hosts send each source in order).
    int i = edgeCount_++;
    while (i > 0 && edges_[i - 1].offset > offset) {
        edges_[i] = edges_[i - 1];
        --i;
    }
    edges_[i] = {offset, isButton, level};
}

IV_SIZE_OPT void Engine::handleEdges(int chunkStart, int chunkEnd) // once per block: built for size (flash)
{
    int used = 0;
    while (used < edgeCount_ && edges_[used].offset < chunkEnd) {
        const Edge& e = edges_[used++];
        const bool wasDown = buttonDown_ || gateHigh_;
        (e.isButton ? buttonDown_ : gateHigh_) = e.level;
        const bool down = buttonDown_ || gateHigh_;
        if (down != wasDown) {
            const int at = e.offset < chunkStart ? chunkStart : e.offset;
            onGesture(gesture_.edge(down, clock_ + uint64_t(at - chunkStart)));
        }
    }
    for (int i = used; i < edgeCount_; ++i) edges_[i - used] = edges_[i];
    edgeCount_ -= used;
    onGesture(gesture_.tick(clock_));
}

IV_SIZE_OPT void Engine::onGesture(GestureReader::Event e) // once per gesture: built for size (flash)
{
    const float twinStep = 1.0f / (voicing::kTwinFadeSeconds * sampleRate_);
    const float tankStep = 1.0f / (voicing::kFreezeInputSeconds * sampleRate_);
    switch (e) {
    case GestureReader::Event::Freeze:
        // The big tank keeps the SIZE and DECAY it had; the knobs move to
        // the twin. Its input closes, its colour glides out.
        heldSize_ = smoothed(ParamId::Size);
        if (sizeHyst_ > 0.0f && sizeHeld_ >= 0.0f) heldSize_ = sizeHeld_; // the SIZE the tank has (hysteresisSize)
        heldDecay_ = smoothed(ParamId::Decay);
        padFloorDb_ = 0.0f; // no cap until the pad has settled (controlTick)
        padCaptured_ = false;
        frozeAt_ = clock_;
#ifndef IV_FIXED_VOICINGS
        duckAtFreeze_ = duckGrDb_; // setFreezeDuckPrototype 2: the glide's start
#endif
        fdn_.resyncHoldLevel();
        lowFdn_.resyncHoldLevel();
        plate_.resyncHoldLevel();
        tankInTarget_ = 0.0f;
        tankInStep_ = tankStep;
#ifndef IV_FIXED_VOICINGS
        if (!twinActive_ && !twinOff_) { // setTwinGainPrototype: no twin at all
#else
        if (!twinActive_) {
#endif
            resetHalf(twinHalf_);
            twinPlate_.reset();
            for (auto& d : twinDiffuser_) d.clear();
            twinSwell_.setAttackSeconds(0.0f, true); // PLATE's twin: no attack
            twinSwell_.reset();
            room_.reset();
            roomLevel_ = 0.0f;
            twinSized_ = false;
            twinActive_ = true;
        }
        twinInStep_ = twinStep;
        break;
    case GestureReader::Event::Unfreeze:
        tankInTarget_ = 1.0f;
        tankInStep_ = tankStep;
        twinInStep_ = -twinStep;
        break;
    case GestureReader::Event::PaintStart:
        // Room to grow while painting: the pad's level or the floor, + kPaintHeadroomDb.
        padFloorDb_ = (tankSlowLevelDb() > floorDbfs() ? tankSlowLevelDb() : floorDbfs()) + voicing::kPaintHeadroomDb;
        break;
    case GestureReader::Event::PaintStop:
        // The painted pad settles like a fresh one: no ceiling until its slow level has caught up,
        // then its own level (controlTick). Held to the paint headroom instead, the pad's makeup
        // lifted it on up after the paint (owner: once painted "it becomes dominant").
        padFloorDb_ = 0.0f;
        padCaptured_ = false;
        frozeAt_ = clock_;
        break;
    case GestureReader::Event::None:
        break;
    }
}

// ---- Voicing -------------------------------------------------------------------

// SIZE's hysteresis (v1.0.37; setSizeHysteresis): in HALL and MASSIVE, SIZE moves on (held) only once the mean line length it means
// would move by more than sizeHyst_ (relative, in log terms); everything SIZE sets follows the held
// value, so nothing in the tank sees the knob's noise (the lines' targets, MASSIVE's in-loop diffusion
// and pre-delay, which each restarted a rest timer). SIZE sets the mean exponentially (expLerp), so
// log(mean) moves by log(hi / lo) per unit of SIZE: the test is on SIZE itself (no maths a chunk).
// snap or held < 0: take SIZE as it is. The PLATE glides its scale (no rest timer): SIZE as it is.
float Engine::hysteresisSize(float size, float& held, bool snap) const
{
    if (sizeHyst_ <= 0.0f) return size;
    const float perUnit = activeSpace_ == 2 ? std::log(2.0f / 0.35f) // map::massiveMeanSeconds
                                            : std::log(voicing::kHallMeanMaxMs / voicing::kHallMeanMinMs); // map::hallMeanSeconds
    if (snap || held < 0.0f || activeSpace_ == 0 || std::fabs(size - held) * perUnit > sizeHyst_) held = size;
    return held;
}

// DECAY's loss rate for the SPACE in use (MASSIVE's shortest tail follows SIZE, v1.0.21).
IV_CTL_OPT float Engine::decayRate(float decay, float size) const
{
    return activeSpace_ == 2 ? map::massiveDecayRate(decay, size) : map::decayRate(decay, activeSpace_);
}

// Sets every voicing target for the SPACE in use (activeSpace_). While
// frozen, the big tank keeps the SIZE and DECAY it froze with and its loss
// glides to none (freezeMix_); the knobs play the twin. snap: jump
// straight there (reset, a SPACE flip).
IV_SIZE_OPT void Engine::applySpace(bool snap, int n, bool snapInput) // once per block: built for size (flash)
{
    const bool frozen = gesture_.frozen();
    const float size = frozen ? heldSize_ : hysteresisSize(smoothed(ParamId::Size), sizeHeld_, snap);
    // HALL's SIZE also sets its T60 scale, output trim and in-loop diffusion, which act at once (the
    // lines' lengths glide on their own): they follow a ~80 ms glide of SIZE, so a SIZE jump (CV, or an
    // unfreeze after SIZE moved) doesn't step the loop's gains (soak audit).
    if (snap) hallSize_ = size;
    else {
        hallSize_ += blockCoeff(0.08f, sampleRate_, n) * (size - hallSize_);
        if (std::fabs(size - hallSize_) < 1e-6f) hallSize_ = size;
    }
    const float decay = frozen ? heldDecay_ : smoothed(ParamId::Decay);
    const float time = smoothed(ParamId::Time), tone = smoothed(ParamId::Tone);
    const int attack = normalisedToSwitch(smoothed(ParamId::Attack));
    const bool massive = activeSpace_ == 2, plate = activeSpace_ == 0;

    // Freezing: loss → 0, highs kept (ratio → 1), the governor caps paint.
    // ERA's darkening scales the high end's T60 share with TONE (Era.h).
    const map::EraWear wear = map::eraMix(smoothed(ParamId::Era)); // once a chunk
    const float toneRatio = map::toneHighT60Ratio(tone) * voicing::kNeveHighRatio;
    const float sizeT60 = activeSpace_ == 1 ? map::hallT60Scale(hallSize_) : 1.0f;
    const float rate = decayRate(decay, size) / sizeT60 * (1.0f - freezeMix_);
    const float spaceRatio = massive ? map::massiveHighRatio(toneRatio, decay) : toneRatio;
    const float highRatio = spaceRatio + freezeMix_ * (1.0f - spaceRatio);
    // Frozen: full weight (only the pad's own hold acts, there's no floor). Not frozen: DECAY's own
    // weight at once, not glided with freezeMix_: gliding it pulled a fed pad (above the floor) down
    // hard on unfreeze, "a pronounced immediate drop" and a too-steep decay (owner, freeze options).
    // Freezing glides the weight up with freezeMix_ (a jump to full changed a finite tail's loss in
    // one sample: a click).
    const float governor = frozen ? std::fmax(map::governorWeight(decay), freezeMix_) : map::governorWeight(decay);
    // DECAY keeps the loudness (v1.0.28): a shorter tail is made up to the finite top's level, on the
    // wet and the twin alike. A frozen pad keeps heldDecay_, so freezing doesn't move it.
    decayGain_ = std::pow(map::decayLevelMakeup(decay, activeSpace_), decayMakeup_);
    if (snap) {
        decayGainNow_ = decayGain_;
    }
    // The cap: a still pad's own level (or + headroom while painting); an
    // infinite tail's settled level once the input has gone quiet; else the
    // floor. Darkening always stops at the floor (ADR 0008).
    // A pad isn't pulled to the floor: the floor glides away with freezeMix_ (and back on unfreeze).
    // Switched in one sample, a fed infinite tail's governor loss vanished at once and the wet
    // stepped (up to 20x a sample's normal change, PLATE; heard once the dark tilt took the highs out).
    // HALL's endless tail settles voicing::kHallTailDb higher (v1.0.52, ADR 0033: the owner's pick C on renders/hall_level).
    const float floorDb = (activeSpace_ == 1 ? floorDbfs() + hallTailDb() : floorDbfs()) * (1.0f - freezeMix_);
    float holdDb = 0.0f;
    if (frozen) holdDb = padFloorDb_;
    else if (tailCaptured_) holdDb = tailCeilingDb_;

    if (plate) {
        plate_.setScale(map::plateScale(size), snap);
        plate_.setDecayRate(rate);
        plate_.setHighT60Ratio(highRatio);
        plate_.setGovernor(governor);
        plate_.setFloorDb(floorDb);
        plate_.setModDepth(1.0f - freezeMix_ * (1.0f - voicing::kFrozenPlateModDepth));
        plate_.setLongDelayMod(1.0f - freezeMix_);
        // A frozen PLATE's chorus and floor glide out too, so a held chord stays pure (v1.0.22, the
        // DSP audit's A3; it kept 0.15 of the chorus and slowly shimmered out of tune).
        plate_.setModScale(modScale_ * (1.0f - freezeMix_));
        plate_.setHoldDb(holdDb);
        plate_.setAir(voicing::kAirPlateDbPerSecond / 60.0f); // v1.0.25
    } else {
        const float mean = massive ? map::massiveMeanSeconds(size) : map::hallMeanSeconds(size);
        const float spread = massive ? map::massiveSpread(size) : 1.0f;
        if (snap || std::fabs(mean - lastMean_) > 1e-6f || std::fabs(spread - lastSpread_) > 1e-6f) {
            lastMean_ = mean;
            lastSpread_ = spread;
            fdn_.setMeanSeconds(mean, spread, snap);
            lowFdn_.setMeanSeconds(mean, spread, snap);
        }
        fdn_.setDecayRate(rate);
        fdn_.setHighT60Ratio(highRatio);
        fdn_.setGovernor(governor);
        fdn_.setFloorDb(floorDb);
        fdn_.setHoldDb(holdDb);
        // A frozen MASSIVE goes to full in-loop diffusion: its long lines otherwise replay painted
        // material as audible repeats (owner, M5 listening).
        const float diff = massive ? map::massiveDiffusion(size) : 0.0f;
        // HALL's in-loop diffusion is for the small rooms (v1.0.17): full at SIZE fully CCW, gone by
        // noon, so medium and large HALLs stay as they were (with it they measured ~2 dB quieter at
        // full SIZE and no less grainy).
        const float hallDiff = hallInLoop_ * std::fmax(0.0f, 1.0f - hallSize_ / voicing::kHallInLoopUntilSize);
        fdn_.setInLoopDiffusion(massive ? diff + freezeMix_ * (1.0f - diff) : hallDiff);
        // HALL's allpasses scale with its mean line, so a small
        // room stays small (MASSIVE's lengths at a 0.2 s mean and up).
        fdn_.setInLoopScale(massive ? 1.0f : std::fmin(1.0f, map::hallMeanSeconds(hallSize_) / 0.2f));
        fdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim * map::massiveDecayLevelTrim(decay, size)
                                   : voicing::kHallOutputTrim * map::hallSizeLevelTrim(hallSize_, decay));
        fdn_.setEarlyTaps(1.0f); // HALL too (v1.0.25: no gap after the note, its "slap-back")
        fdn_.setAir(massive ? 0.0f : voicing::kAirHallDbPerSecond / 60.0f); // v1.0.25
        // The floor glides out with the freeze (v1.0.22, A3; Costello: mod depth to 0 for a freeze).
        fdn_.setModDepth(modScale_ * (massive ? 1.0f : voicing::kHallFloorScale) * (1.0f - freezeMix_));
        // Still lows: the low tank follows, unmodulated.
        lowFdn_.setDecayRate(rate);
        lowFdn_.setHighT60Ratio(highRatio);
        lowFdn_.setGovernor(governor);
        lowFdn_.setFloorDb(floorDb);
        lowFdn_.setHoldDb(holdDb);
        lowFdn_.setInLoopDiffusion(massive ? diff + freezeMix_ * (1.0f - diff) : hallDiff);
        lowFdn_.setInLoopScale(massive ? 1.0f : std::fmin(1.0f, map::hallMeanSeconds(hallSize_) / 0.2f));
        lowFdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim * map::massiveDecayLevelTrim(decay, size)
                                      : voicing::kHallOutputTrim * map::hallSizeLevelTrim(hallSize_, decay));
        lowFdn_.setEarlyTaps(1.0f);
        lowFdn_.setModDepth(0.0f);
    }

    // ERA in the loop and DRIFT (SPEC §4.6, §4.7). Freezing glides the
    // in-loop colour out (the pad holds still, ADR 0009); DRIFT stays.
    // ERA's wear on the wet (dsp/EraTexture.h), run only while it's in. At infinite DECAY the band keeps
    // a share of its blend (tex::kBandAtInfinite).
    {
        dsp::TexMix t = wear.tex;
        t.bandW *= 1.0f - map::governorWeight(decay) * (1.0f - dsp::tex::kBandAtInfinite);
        texOn_ = t.tapeW > 0.0f || t.breathW > 0.0f || t.clockW > 0.0f || t.grainW > 0.0f || t.bandW > 0.0f
              || !texture_.settled(); // every stage out: runs on (the hold alone) until its gain is back at 1
        texture_.set(t, sampleRate_, n);
    }
    const float keep = 1.0f - freezeMix_;
    // Neve's even harmonics add energy in proportion to the level: harmless
    // while DECAY loses energy, a runaway in a lossless loop (measured: the
    // tail jumping 2-3 dB, then the governor overshooting into dips). So
    // they fade out across DECAY's infinite crossover.
    dsp::EraMix loopEra;
    loopEra.even = voicing::kNeveEven;
    loopEra.even *= keep * (1.0f - map::governorWeight(decay));
    const dsp::DriftAmounts drift = map::driftAmounts(smoothed(ParamId::Drift));
    // MOTION's left side: the tape transport on the wet (v1.0.46); off it, the transport glides out and rests.
    transportOn_ = smoothed(ParamId::Drift) < voicing::kDriftDeadLo;
    if (transportOn_ || !transport_.idle()) transportAmt_ = map::transportAmounts(smoothed(ParamId::Drift));
    // The tape's slow 2:1 compression: more loss above −18 dBFS.
    // Judged like the floor (the lower of its 1.5 s and 50 ms levels): on the 1.5 s level alone it
    // kept compressing for seconds after a loud input stopped, and an infinite tail plunged 20+ dB
    // under the floor (soak audit). A steady tail measures the same.
    const float tankNowDb = activeSpace_ == 0 ? plate_.fastLevelDb() : fdn_.fastLevelDb();
    const float over = std::fmin(tankLevelDb(), tankNowDb) - voicing::kCompressThresholdDb;
    const float compress = over > 0.0f ? keep * wear.compress * voicing::kCompressRatePerDb * over : 0.0f;
    if (plate) {
        if (eraChanged(EraSentPlate, loopEra)) plate_.setEra(loopEra);
        plate_.setDrift(drift);
        plate_.setExtraRate(compress);
    } else {
        if (eraChanged(EraSentFdn, loopEra)) {
            fdn_.setEra(loopEra);
            lowFdn_.setEra(loopEra);
        }
        fdn_.setDrift(drift);
        fdn_.setExtraRate(compress);
        lowFdn_.setDrift(dsp::DriftAmounts{}); // still: no wow, flutter or Wander
        lowFdn_.setExtraRate(compress);
    }
    eraMakeup_ = wear.makeup;

    tilt_.set(smoothed(ParamId::Tone));
#ifndef IV_FIXED_VOICINGS
    // setHallTailPrototype (renders/hall_level): HALL at infinite DECAY tilted brighter (lows / highs dB), as far as
    // DECAY's governor weight reaches (0 below 0.85, full at 1) and not while frozen.
    if (activeSpace_ == 1 && (hallTiltLowDb_ != 0.0f || hallTiltHighDb_ != 0.0f)) {
        const float w = map::governorWeight(smoothed(ParamId::Decay)) * (1.0f - freezeMix_);
        tilt_.trimPrototype(w * hallTiltLowDb_, w * hallTiltHighDb_);
    }
#endif
    toneBell_.set(smoothed(ParamId::Tone), n);
    // Hiss follows the input (none in silence, so an infinite tail doesn't
    // fill with noise, ADR 0005); crackle the same.
    const float inDb = 10.0f * std::log10(inputPower_ + 1e-20f);
    const float follow = map::clamp01((inDb - (voicing::kHissFollowFromDbfs - 30.0f)) / 30.0f);
    hissGain_ = wear.hiss * follow * std::pow(10.0f, voicing::kHissDbfs / 20.0f);
    crackleChance_ = wear.crackle * follow * voicing::kCracklePerSecond / sampleRate_;
    crackleGain_ = std::pow(10.0f, voicing::kCrackleDbfs / 20.0f);

    // The input diffusers' amount glides (~50 ms): it changes with a SPACE flip and with MASSIVE's SIZE,
    // and a jump stepped the input into the tank (soak audit).
    const float diffusion = massive ? map::massiveDiffusion(size) : 1.0f;
    const float glide = snap && snapInput ? 1.0f : blockCoeff(0.05f, sampleRate_, n);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            float& g = diffuser_[c].stage[k].coeff;
            const float target = voicing::kDiffuserCoeff[k] * diffusion;
            g += glide * (target - g);
            if (std::fabs(target - g) < 1e-6f) g = target;
        }

    // ATTACK (ADR 0015): what TIME means, and the swell stage's attack.
    float predelay = massive ? map::massivePredelaySeconds(size) : 0.0f;
    float attackSeconds = massive ? voicing::kMassiveSwellScale * map::massiveNormalAttackSeconds(decay, size) : 0.0f;
    if (attack == 0)
        predelay += massive ? map::massiveNormalPredelaySeconds(time) : map::normalPredelaySeconds(time);
    if (attack == 1)
        attackSeconds = map::bloomSeconds(time) * (massive ? map::massiveMass(size) : 1.0f);
    swell_.setAttackSeconds(attackSeconds, snap);
    swell_.setSmear(attack == 1); // BLOOM only (owner, renders/swell_smear)
    // An ONSET change (what the swell is: BLOOM's attack is TIME, the others' MASSIVE's own or none): the
    // swell's input and output dip out over kSpaceFlipSeconds, its taps and smear held; it restarts from
    // silence in its new voicing there and dips back in, the tank's tail ringing on. Glided, its taps swept
    // across seconds of their line in ~0.3 s, re-set once a chunk: scrambled, sped-up sound into the tank,
    // 50-80 dB over the tail's highs for its whole length (owner: "a slight burst of noise", SHIMMER to
    // BLOOM; every switch into or out of BLOOM measured it, and NATURAL to SHIMMER in MASSIVE).
    if (snap) swellOnset_ = attack;
    if (attack != swellOnset_ && onsetDip_ >= 1.0f) {
        swellOnset_ = attack;
        swell_.reset(); // its attack jumps to the one just set, and its smear
        swell_.snapSmear();
    }
    // Out while the swell isn't the ONSET's, back in (and resting at 0, exactly) once it is.
    onsetStep_ = (attack != swellOnset_ ? 1.0f : -1.0f) / (voicing::kSpaceFlipSeconds * sampleRate_);
    // SHIMMER (ADR 0024): TIME is how much; it rests while frozen (as REVERSE did, ADR 0015).
    shimmerAmount_ = time;
    shimmerTarget_ = attack == 2 && !frozen ? 1.0f : 0.0f;
#ifndef IV_FIXED_VOICINGS
    shimmerArmed_ = attack == 2; // setShimmerLazyPrototype: recording while SHIMMER is selected
#endif

    const float pd = predelay * sampleRate_;
    if (snap) predelayNow_ = pd;
    predelayTarget_ = pd;

    // The twin (ADR 0009): the frozen tank's space and the knobs' colour, never frozen. SIZE and
    // DECAY play it while frozen (at the freeze they're where the pad froze, so it starts as the
    // pad's twin); it always fades within kTwinMaxT60. Full in-loop diffusion in MASSIVE, like the pad.
    if (twinActive_) {
        const float tSize = hysteresisSize(smoothed(ParamId::Size), twinSizeHeld_, snap || !twinSized_);
        const float tDecay = smoothed(ParamId::Decay);
        twinSwell_.setAttackSeconds(0.0f, snap); // PLATE's twin: no attack
        const float tT60 = activeSpace_ == 1 ? map::hallT60Scale(tSize) : 1.0f;
        const float tankRate = decayRate(tDecay, tSize) / tT60;
        const float tRate = std::fmax(tankRate, 1.0f / voicing::kTwinMaxT60);
        dsp::EraMix tEra;
        tEra.even = voicing::kNeveEven;
        tEra.even *= 1.0f - map::governorWeight(tDecay);
        if (plate) {
            twinPlate_.setScale(map::plateScale(tSize), snap);
            twinPlate_.setDecayRate(tRate);
            twinPlate_.setHighT60Ratio(toneRatio);
            twinPlate_.setGovernor(0.0f);
            if (eraChanged(EraSentTwinPlate, tEra)) twinPlate_.setEra(tEra);
            twinPlate_.setDrift(drift);
        } else { // HALL and MASSIVE: the small room (v1.0.43) plays SIZE, DECAY and TONE, as the twin tank did
            room_.set(tSize, tRate, massive ? map::massiveHighRatio(toneRatio, tDecay) : toneRatio);
            // Its level follows the pad (v1.0.43, the owner's pick B on renders/room_level: "follows pad B is the
            // winner"): the page's gain (renders/small_room E) at SIZE 0.9 with DECAY infinite, and elsewhere as the
            // twin tank was: v1.0.41's lift for a twin cut short (up to kTwinRoomLift), its level trims (MASSIVE's
            // DECAY, HALL's SIZE), and the room's line length against SIZE 0.9 (shorter lines, more passes: louder).
            const float over = tankRate * (voicing::kTwinRoomLift * voicing::kTwinRoomLift) / tRate;
            const float trim = map::governorWeight(tDecay) >= 1.0f ? 1.0f
                             : massive ? map::massiveDecayLevelTrim(tDecay, tSize) : map::hallSizeLevelTrim(tSize, tDecay);
            const float dens = std::sqrt((0.008f + 0.042f * tSize) / (0.008f + 0.042f * 0.9f));
            room_.setGain(roomGain_[massive ? 1 : 0] * (trim * dens * (over > 1.0f ? 1.0f / std::sqrt(over) : 1.0f)));
            // The level the owner heard B at on renders/room_motion (v1.0.47), on its output (Paint sends as before).
            const float* k = voicing::kTwinRoomTrimDb;
            const float trimDb = tSize >= 0.9f ? k[2] : tSize >= 0.5f ? k[1] + (k[2] - k[1]) * ((tSize - 0.5f) / 0.4f)
                                                                       : k[0] + (k[1] - k[0]) * (tSize / 0.5f);
            roomTrim_ = std::pow(10.0f, trimDb / 20.0f);
            twinSized_ = true; // (PLATE's twin takes SIZE as it comes, as before)
        }
    }
}

// LoopEra::set works out two constants per line (an exp2 and an exp: ~16 libm calls a chunk per
// tank) and stores the mix; with the same mix it would store the same again. So each tank is sent
// its ERA only when it changed (overnight review, 26 Oct 2026; bit-identical).
IV_SIZE_OPT bool Engine::eraChanged(int which, const dsp::EraMix& m)
{
    const unsigned bit = 1u << which;
    if ((eraSentOk_ & bit) && sameEra(eraSent_[which], m)) return false;
    std::memcpy(eraSent_[which], &m, sizeof m);
    eraSentOk_ = uint8_t(eraSentOk_ | bit);
    return true;
}

// PROTOTYPE (setDiffuserIndependent): lengths of their own for the right side, each shorter
// than the 1.071x copy the buffers were sized for, mutually prime in samples at 48 kHz.
void Engine::setDiffuserIndependent(bool on)
{
    constexpr float kRight[4] = {0.004937f, 0.003437f, 0.013208f, 0.008687f};
    for (int k = 0; k < 4; ++k) {
        const float s = on ? kRight[k] : voicing::kDiffuserSeconds[k] * voicing::kDiffuserRightScale;
        diffuser_[1].stage[k].length = uint32_t(s * sampleRate_);
        twinDiffuser_[1].stage[k].length = uint32_t(s * sampleRate_);
    }
}

// TIME's pre-delay crossfade (ADR 0025), only while one runs: the old delay fading out (gains stepped
// by a rotation). Out of line, so the per-sample input loop stays as before (flash).
IV_NOINLINE IV_SIZE_OPT void Engine::predelayCrossfade(float l, float r, float& pl, float& pr)
{
    const float d = std::fmax(pdOld_, 2.0f);
    const float ol = pdOld_ > 0.0f ? predelay_[0].readCubic(d) : l, orr = pdOld_ > 0.0f ? predelay_[1].readCubic(d) : r;
    pl = pdOldG_ * ol + pdNewG_ * pl;
    pr = pdOldG_ * orr + pdNewG_ * pr;
    const float o = pdOldG_;
    pdOldG_ = o * pdRotC_ - pdNewG_ * pdRotS_;
    pdNewG_ = pdNewG_ * pdRotC_ + o * pdRotS_;
    if (--pdXfLeft_ == 0) {
        pdOldG_ = 0.0f;
        pdNewG_ = 1.0f;
    }
}

IV_SIZE_OPT void Engine::controlTick(int n) // once per block: built for size (flash)
{
    // Ducking (ADR 0014, amended v1.0.17 and v1.0.51): 2:1 compression of the wet, keyed by the input's
    // ~10 ms power, × how far into infinite DECAY is; not while frozen (playing over a pad
    // mustn't duck it; fades with the freeze glide). The reduction moves with its own attack
    // and release once per control block; the wet gain ramps across the block.
    {
        const float levelDb = 10.0f * std::log10(inputPower_ + 1e-12f);
        const float over = levelDb - voicing::kDuckThresholdDbfs;
        float target = (over > 0.0f ? duckSlope() * over : 0.0f)
                     * map::governorWeight(smoothed(ParamId::Decay)) * (1.0f - freezeMix_);
        // v1.0.51 (renders/level_glue E): held playing is ducked less and let go faster when it stops, so the
        // tail arrives under the dry without a dip; short sounds keep the full reduction and the slow release.
        // "Held": the 10 ms power within 3 dB of its 0.5 s power, judged only while over the threshold.
        float release = duckRelease();
        if (duckFastRelease() > 0.0f || duckSustainCut() > 0.0f) {
            inputSlow_ += blockCoeff(voicing::kDuckSlowSeconds, sampleRate_, n) * (inputPower_ - inputSlow_);
            if (over > 0.0f) {
                const float held = inputPower_ < voicing::kDuckSustainCrest * inputSlow_ ? 1.0f : 0.0f;
                duckSustained_ += blockCoeff(voicing::kDuckSustainSeconds, sampleRate_, n) * (held - duckSustained_);
            }
            if (duckFastRelease() > 0.0f) release += duckSustained_ * (duckFastRelease() - release);
            target *= 1.0f - duckSustainCut() * duckSustained_;
        }
#ifndef IV_FIXED_VOICINGS
        if (freezeDuck_ == 1 && gesture_.frozen()) {
            // setFreezeDuckPrototype 1: the pad keeps the reduction it had at the tap
        } else if (freezeDuck_ == 2 && gesture_.frozen()) { // 2: let go in a straight line (dB) over the glide
            duckGrDb_ = std::fmax(0.0f, duckGrDb_ - duckAtFreeze_ * float(n) / (freezeDuckSeconds_ * sampleRate_));
        } else {
            const float secs = target > duckGrDb_ ? voicing::kDuckAttackSeconds
                             : (freezeMix_ > 0.0f ? (freezeDuck_ == 1 ? freezeDuckSeconds_ : 0.05f) : release);
            duckGrDb_ += blockCoeff(secs, sampleRate_, n) * (target - duckGrDb_);
        }
#else
        // A freeze lets go quickly (50 ms), so a fresh pad doesn't swell up over the release.
        const float secs = target > duckGrDb_ ? voicing::kDuckAttackSeconds
                         : (freezeMix_ > 0.0f ? 0.05f : release);
        duckGrDb_ += blockCoeff(secs, sampleRate_, n) * (target - duckGrDb_);
#endif
        if (target == 0.0f && duckGrDb_ < 0.05f) duckGrDb_ = 0.0f; // fully let go (and exact unity)
        duckGainStep_ = (std::pow(10.0f, -duckGrDb_ / 20.0f) - duckGain_) / float(n);
        duck_ = map::clamp01(duckGrDb_ / voicing::kDuckFullDb);
    }
    // The PLATE's first input allpass wanders (Mutable Instruments' Clouds and Beads; owner's pick
    // on renders/diffuser_motion, v1.0.16), so repeated hits don't enter the tank identically.
    // Its depth glides in and out with SPACE, so a flip never jumps the read.
    const float motionTarget = activeSpace_ == 0 ? voicing::kDiffuserMotionSeconds * sampleRate_ : 0.0f;
    diffMotionNow_ += blockCoeff(0.1f, sampleRate_, n) * (motionTarget - diffMotionNow_);
    if (diffMotionNow_ > 1e-3f || motionTarget > 0.0f) {
        diffMotionPhase_ += voicing::kDiffuserMotionHz * float(n) / sampleRate_;
        if (diffMotionPhase_ >= 1.0f) diffMotionPhase_ -= 1.0f;
        float depth = diffMotionNow_;
        if (depth > 8.0f) depth = 8.0f; // the buffers' spare room (nextPow2 of the 1.071x lengths)
        const float m = depth * std::sin(6.2831853f * diffMotionPhase_);
        diffuser_[0].stage[0].mod = m;
        diffuser_[1].stage[0].mod = -m;
    } else {
        diffuser_[0].stage[0].mod = diffuser_[1].stage[0].mod = 0.0f; // still: the plain read
    }
    // Smooth every knob toward its value (ParamSpec tiers, ADR 0012).
    for (const auto& p : kParams) {
        const size_t i = static_cast<size_t>(p.id);
        if (p.kind == ParamKind::Switch3 || p.smoothing == Smoothing::Slewed) smooth_[i] = values_[i];
        else {
            smooth_[i] += chunkCoeff(p.smoothingMs, sampleRate_, n) * (values_[i] - smooth_[i]);
            if (std::fabs(values_[i] - smooth_[i]) < 1e-5f) smooth_[i] = values_[i]; // land exactly (MIX 0 = bit-exact dry)
        }
    }

    // Freeze glides (ADR 0009): colour out over 0.2 s, back over 1 s.
    const float dt = float(n) / sampleRate_;
    freezeMix_ = gesture_.frozen() ? approach(freezeMix_, 1.0f, dt / voicing::kFreezeColourSeconds)
                                   : approach(freezeMix_, 0.0f, dt / voicing::kUnfreezeSeconds);
    // A still pad's ceiling: its own level once settled (kPadSettleSeconds
    // after the freeze), so it can't creep up afterwards.
    if (gesture_.frozen() && !gesture_.painting() && !padCaptured_ && freezeMix_ >= 1.0f && tankIn_ <= 0.0f
        && clock_ - frozeAt_ > uint64_t(voicing::kPadSettleSeconds * sampleRate_)) {
        // Judged on the 8 s level, which averages a MASSIVE pad's early swings; unless that's still
        // well above the pad's level now (50 ms), as when a fast-dying tail (a short DECAY) was frozen:
        // the slower levels still remember the tail before the freeze (it can fall 130 dB in a second),
        // and the makeup swelled the pad up towards it for minutes, 25-55 dB, even a finished tail from
        // -260 dBFS (soak audit). Then the 8 s level starts over from now (the hold judges on it).
        // Above the 1.5 s level too, so a pad whose level swings (a 50 ms dip) isn't misjudged.
        const float nowDb = activeSpace_ == 0 ? plate_.fastLevelDb() : fdn_.fastLevelDb();
        if (tankSlowLevelDb() > std::fmax(nowDb, tankLevelDb()) + 3.0f) {
            fdn_.resyncHoldLevelToFast();
            lowFdn_.resyncHoldLevelToFast();
            plate_.resyncHoldLevelToFast();
        }
        padFloorDb_ = tankSlowLevelDb() + voicing::kPadCeilingDb;
        padCaptured_ = true;
    }
    // An infinite tail holds where it settled once the input goes quiet: the big tank's own input,
    // after the swell, so a long BLOOM (which feeds the tank for seconds after the input stops)
    // isn't judged while it's still rising (soak audit: its ceiling was taken too low and the tail
    // was pulled 5-6 dB under it).
    const bool tankQuiet = 10.0f * std::log10(tankInPower_ + 1e-20f) <= voicing::kQuietDbfs;
    if (!tankQuiet) {
        quietSince_ = clock_;
        tailCaptured_ = false;
    } else if (map::governorWeight(smoothed(ParamId::Decay)) >= 1.0f
               && clock_ - quietSince_ > uint64_t(voicing::kQuietSeconds * sampleRate_)) {
        // Ratchets down with the tail while it settles, then stays put (the
        // hold makeup brings a sagging tail back up to it).
        const float c = tankSlowLevelDb() + voicing::kPadCeilingDb;
        const bool settling = clock_ - quietSince_ < uint64_t((voicing::kQuietSeconds + voicing::kTailSettleSeconds) * sampleRate_);
        if (!tailCaptured_ || (settling && c < tailCeilingDb_)) tailCeilingDb_ = c;
        tailCaptured_ = true;
    }
    if (map::governorWeight(smoothed(ParamId::Decay)) < 1.0f) tailCaptured_ = false;

    // Paint (ADR 0010): builds over ~3 s while held, lets go over 0.2 s.
    paint_ = gesture_.painting() ? paint_ + (1.0f - paint_) * (1.0f - std::exp(-dt / (voicing::kPaintSeconds / 3.0f)))
                                 : approach(paint_, 0.0f, dt / voicing::kPaintReleaseSeconds);
    // Painting adds less and less as the pad nears its ceiling, so the pad's own layers stay on top
    // (owner: once painted "it becomes dominant"; the slow ceiling let a pad grow 7 dB).
    // Only while painting: at the release the pad's ceiling is lifted (PaintStop), and the room jumped
    // to full while the paint was still letting go, a burst of the twin into the pad (soak audit).
    if (gesture_.painting()) paintRoom_ = clamp01((padFloorDb_ - tankLevelDb()) / voicing::kPaintFadeDb);

    // SPACE flip (ADR 0013): fade out, restart the tank in the new voicing,
    // fade back in. Not while frozen: the pad waits for the unfreeze.
    const int space = normalisedToSwitch(smoothed(ParamId::Space));
    const float flipChunk = float(n) / (voicing::kSpaceFlipSeconds * sampleRate_);
    if (flipPhase_ == 0 && space != activeSpace_ && !gesture_.frozen() && freezeMix_ == 0.0f) flipPhase_ = 1;
    if (flipPhase_ == 1) {
        flipStep_ = -flipChunk / float(n);
        if (gesture_.frozen()) {
            // A freeze inside the fade-out keeps the old space: it fades back in as the pad, and the
            // flip waits for the unfreeze like any other (soak audit: the tank used to restart under
            // the freeze, so the pad came out silent).
            flipPhase_ = 2;
        } else if (flipGain_ <= 0.0f) {
            activeSpace_ = space;
            setEarlyTapsForSpace(); // the tanks restart here, so their early taps re-lay out unheard
            // Targets first, so the resets below land on them (the FDN's output trim). The predelay
            // jumps too, here where the tank's input is silent; the input diffusers glide
            // (a jump in their amount rang on in them for ~100 ms, past the fade back in).
            applySpace(true, n, false);
            fdn_.reset();
            resetHalf(tankHalf_);
            lowFdn_.reset();
            plate_.reset();
            swell_.reset();
            // The shimmer doesn't depend on SPACE: it keeps recording and its send fades with the flip.
            // A twin still fading after an unfreeze belongs to the old space: it has faded out with
            // the wet, so it stops here and the new space's twin starts clean at the next freeze
            // (soak audit: it was cut off mid-sound, a click, and the other twin replayed stale sound).
            twinActive_ = false;
            twinIn_ = twinInStep_ = 0.0f;
            room_.reset();
            roomLevel_ = 0.0f;
            twinSized_ = false;
            resetHalf(twinHalf_);
            twinPlate_.reset();
            twinSwell_.reset();
            for (auto& d : twinDiffuser_) d.clear();
            // The input side fades with the flip (process(): before the diffusers), so the predelay's jump
            // here is silent and the diffusers and swell start from silence. Faded only at the tank (after
            // them), that jump and the swell's cleared line under the unfaded input rang into the new space
            // as clicks, hiss over its tail's highs for seconds (owner: a burst switching MASSIVE and HALL).
            for (auto& d : diffuser_) d.clear();
            flipPhase_ = 2;
        }
    }
    if (flipPhase_ == 2) {
        flipStep_ = flipChunk / float(n);
        if (flipGain_ >= 1.0f) {
            flipPhase_ = 0;
            flipStep_ = 0.0f;
        }
    }
    if (flipPhase_ == 0) flipStep_ = 0.0f;

    applySpace(false, n);
    if (onsetStep_ < 0.0f) swell_.control(n); // dipping out for an ONSET change (applySpace), its taps and smear stay put
    twinSwell_.control(n);
    // TIME's pre-delay by crossfade (v1.0.34, ADR 0025; owner's pick C on renders/knob_moves): it jumps
    // and the old delay fades out over kSizeCrossfadeSeconds, equal power, instead of sliding, which swept
    // the input by up to an octave ("a large wash of noise"; a held note's energy away from its pitch -24
    // dB steady, -1 dB while TIME moved). Once TIME has rested kSizeRestSeconds: back to back, each flick
    // jumped the reverb's input back and forth in time, a stutter. (Sliding, it was capped at half a
    // sample per sample after the soak audit: a jump in its target swept it across in one block.)
    pdRest_ = std::fabs(predelayTarget_ - pdLastTarget_) > 0.5f ? 0.0f : pdRest_ + float(n) / sampleRate_;
    pdLastTarget_ = predelayTarget_;
    if (pdXfLeft_ <= 0 && pdRest_ >= voicing::kSizeRestSeconds && std::fabs(predelayTarget_ - predelayNow_) > 0.5f) {
        pdOld_ = predelayNow_;
        predelayNow_ = predelayTarget_;
        pdXfLeft_ = int(voicing::kSizeCrossfadeSeconds * sampleRate_);
        const float step = 1.5707963f / float(pdXfLeft_);
        pdRotC_ = std::cos(step);
        pdRotS_ = std::sin(step);
        pdOldG_ = 1.0f;
        pdNewG_ = 0.0f;
    }

    const auto g = map::mixGains(smoothed(ParamId::Mix));
    dryStep_ = (g.dry - dryNow_) / float(n);
    wetStep_ = (g.wet - wetNow_) / float(n);
}

void Engine::process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    if (!ready_) {
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        edgeCount_ = 0;
        return;
    }
    const float attackStep = 1.0f / (voicing::kAttackFadeSeconds * sampleRate_);
    for (int start = 0; start < numSamples; start += kChunk) {
        const int n = (numSamples - start) < kChunk ? (numSamples - start) : kChunk;
        handleEdges(start, start + n);
        controlTick(n);
        prof::mark(prof::kControl);
        // The twin's input side runs only while there is a twin (overnight review, 26 Oct 2026). The bits
        // are the same: without a twin its input is shut (twinIn_ 0: only a freeze opens it, and that
        // starts a twin), a freeze clears its diffusers and swell before it starts one (onGesture), and
        // their output is read only while it runs.
        const bool twinRuns = twinActive_;

        // Input side: predelay, diffusion; the swell feeds the big tank; the
        // twin hears the input through its own diffusers.
        for (int i = 0; i < n; ++i) {
            flipGain_ = clamp01(flipGain_ + flipStep_);
            tankIn_ = tankIn_ < tankInTarget_ ? std::fmin(tankInTarget_, tankIn_ + tankInStep_)
                                              : std::fmax(tankInTarget_, tankIn_ - tankInStep_);
            const float l = inL[start + i], r = inR[start + i];
            inputPower_ += 0.002f * (0.5f * (l * l + r * r) - inputPower_); // ~10 ms follower
            predelay_[0].write(l);
            predelay_[1].write(r);
            float pl = l, pr = r;
            if (predelayNow_ >= 2.0f) {
                pl = predelay_[0].readCubic(predelayNow_);
                pr = predelay_[1].readCubic(predelayNow_);
            } else if (predelayNow_ > 0.0f) { // under 2 samples (the cubic's least): between the input and that
                const float t = 0.5f * predelayNow_;
                pl += t * (predelay_[0].readCubic(2.0f) - pl);
                pr += t * (predelay_[1].readCubic(2.0f) - pr);
            }
            if (pdXfLeft_ > 0) predelayCrossfade(l, r, pl, pr);
            pl *= flipGain_; // a SPACE flip fades the input side too (the predelay jumps at its silent point)
            pr *= flipGain_;
            pl = diffuser_[0].process(pl);
            pr = diffuser_[1].process(pr);
            prof::mark(prof::kInput);
            // SHIMMER: the diffused input, an octave up, joins it into the tank (always recording, so
            // switching in finds the last moments ready). Its recording fades with a SPACE flip as the
            // tank's input does: the predelay jumps at the flip's silent point, and recorded unfaded
            // that edge came back through the heads into the new space as a click (x7 the steady
            // sound's sharpest step, MASSIVE to PLATE; REVERSE's line had the same, soak finding 6).
            shimmerMix_ = approach(shimmerMix_, shimmerTarget_, attackStep);
            {
                float sl = pl, sr = pr; // faded with a SPACE flip (above)
#ifndef IV_FIXED_VOICINGS
                if (shimmerLazy_ && !shimmerArmed_ && shimmerMix_ <= 0.0f) { // the prototype: resting
                    sl = sr = 0.0f;
                    shimmerResting_ = true;
                } else {
                    if (shimmerResting_) shimmer_.restart();
                    shimmerResting_ = false;
                    shimmer_.process(sl, sr, shimmerMix_ > 0.0f);
                }
#else
                shimmer_.process(sl, sr, shimmerMix_ > 0.0f);
#endif
                const float send = shimmerMix_ * voicing::kShimmerSend * shimmerAmount_;
                pl += send * sl;
                pr += send * sr;
            }
            prof::mark(prof::kShimmer);
            // The twin hears the input itself, through its own diffusion: no predelay; in MASSIVE, its own swell.
            twinIn_ = clamp01(twinIn_ + twinInStep_);
            if (twinRuns) { // no twin, nothing to hear: a freeze clears them before it starts (onGesture)
                twinInL_[i] = twinDiffuser_[0].process(twinIn_ * l);
                twinInR_[i] = twinDiffuser_[1].process(twinIn_ * r);
            }
            // MASSIVE's twin swells as its tank does, so what you play over a frozen MASSIVE
            // arrives as a wash, not two distinct repeats first (owner, 22 Oct 2026).
            twinL_[i] = twinR_[i] = 0.0f;
            prof::mark(prof::kTwin);
            onsetDip_ = clamp01(onsetDip_ + onsetStep_);
            const float og = 1.0f - onsetDip_;
            bufL_[i] = og * pl; // the swell's input; it runs over the whole chunk below
            bufR_[i] = og * pr;
            chunkGain_[i] = tankIn_ * flipGain_ * og;
            wetL_[i] = wetR_[i] = 0.0f;
            prof::mark(prof::kInput);
        }
        // Its swell too: after a MASSIVE freeze its attack stayed set, and with no twin it read 24 taps
        // a sample from a ring of silence until power-off (overnight review, 26 Oct 2026).
        if (twinRuns && activeSpace_ == 0) twinSwell_.processBlock(twinInL_, twinInR_, n); // PLATE's twin (the room has no swell)
        prof::mark(prof::kTwin);
        swell_.processBlock(bufL_, bufR_, n);
        prof::mark(prof::kSwell);
        for (int i = 0; i < n; ++i) {
            float pl = bufL_[i], pr = bufR_[i];
            if (hissGain_ > 0.0f || crackleChance_ > 0.0f) {
                // ERA's noise into the tank: soft (3 kHz) hiss, sparse crackle.
                for (int c = 0; c < 2; ++c) {
                    noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
                    const float w = float(int32_t(noiseSeed_)) * (1.0f / 2147483648.0f);
                    hissLp_[c] += 0.33f * (w - hissLp_[c]);
                }
                pl += hissGain_ * 2.0f * hissLp_[0];
                pr += hissGain_ * 2.0f * hissLp_[1];
                noiseSeed_ = noiseSeed_ * 1664525u + 1013904223u;
                if (float(noiseSeed_ >> 8) * (1.0f / 16777216.0f) < crackleChance_) {
                    const float a = crackleGain_ * (0.3f + 0.7f * float((noiseSeed_ >> 4) & 0xFFu) / 255.0f);
                    pl += (noiseSeed_ & 1u) ? a : -a;
                    pr += (noiseSeed_ & 2u) ? a : -a;
                }
            }
            const float g = chunkGain_[i];
            bufL_[i] = g * pl;
            bufR_[i] = g * pr;
            tankInPower_ += 0.002f * (0.5f * (bufL_[i] * bufL_[i] + bufR_[i] * bufR_[i]) - tankInPower_);
            prof::mark(prof::kInput);
        }

        // The twin first, so painting can send it into the pad.
        if (twinActive_) {
            if (activeSpace_ == 0) twinPlate_.process(twinInL_, twinInR_, twinL_, twinR_, n);
            else twinHalf_.process(room_, twinInL_, twinInR_, twinL_, twinR_, n);
            // The twin stops on its 50 ms level (v1.0.37; overnight review, lit-plate 6): ~15-24 s sooner
            // after an unfreeze than on its 1.5 s level, which kept a silent second tank running.
            float lvl = twinPlate_.fastLevelDb();
#ifndef IV_FIXED_VOICINGS
            if (!twinStopFast_) lvl = twinPlate_.levelDb(); // setTwinStopFast(false)
#endif
            if (activeSpace_ != 0) { // the room's own 50 ms power, under -100 dB: no log a block, the step once per block size
                if (n != roomStepN_) {
                    roomStepN_ = n;
                    roomStep_ = blockCoeff(0.05f, sampleRate_, n);
                }
                float p = 0.0f;
                for (int i = 0; i < n; ++i) p += 0.5f * (twinL_[i] * twinL_[i] + twinR_[i] * twinR_[i]);
                roomLevel_ += roomStep_ * (p / float(n) - roomLevel_);
                lvl = roomLevel_ + 1e-30f < 1e-10f ? -200.0f : 0.0f;
            }
            if (!gesture_.frozen() && twinIn_ == 0.0f && lvl < -100.0f) twinActive_ = false;
            // Painting (holding) copies a little of the twin into the pad: what you played becomes a layer of it.
            const float pg = paint_ * paintRoom_ * voicing::kPaintLevel;
            if (pg > 0.0f)
                for (int i = 0; i < n; ++i) {
                    bufL_[i] += pg * twinL_[i];
                    bufR_[i] += pg * twinR_[i];
                }
        }

        prof::mark(prof::kTwin);
        if (activeSpace_ == 0) plate_.process(bufL_, bufR_, wetL_, wetR_, n);
        else if (!lowTank_) tankHalf_.process(fdn_, bufL_, bufR_, wetL_, wetR_, n); // the test hook's one tank
        else processTwoTanks(n);
        prof::mark(prof::kTank);

        transport_.control(n, transportAmt_, transportOn_); // MOTION's tape transport (returns at once when out)
        const bool transportLive = !transport_.idle();
        for (int i = 0; i < n; ++i) {
            // DECAY's level make-up (v1.0.28) on the forward wet and the twin, a ~40 ms glide.
            decayGainNow_ += 0.0005f * (decayGain_ - decayGainNow_);
            // The twin fades with a SPACE flip too (it stops at the flip's silent point).
#ifndef IV_FIXED_VOICINGS
            const float keepTank = layerSolo_ ? 0.0f : 1.0f; // setLayerSoloPrototype: the frozen pad (the tank) out of the wet
            const float fl = keepTank * decayGainNow_ * wetL_[i], fr = keepTank * decayGainNow_ * wetR_[i];
#else
            const float fl = decayGainNow_ * wetL_[i], fr = decayGainNow_ * wetR_[i];
#endif
#ifndef IV_FIXED_VOICINGS
            const float twinK = voicing::kTwinLevel * (activeSpace_ != 0 ? twinGain_ * roomTrim_ : 1.0f); // setTwinGainPrototype (1: exact)
#else
            const float twinK = voicing::kTwinLevel * (activeSpace_ != 0 ? roomTrim_ : 1.0f); // the room's trim (v1.0.47)
#endif
            const float tl = decayGainNow_ * twinK * twinL_[i];
            const float tr = decayGainNow_ * twinK * twinR_[i];
            float wl = (fl + tl) * flipGain_;
            float wr = (fr + tr) * flipGain_;
            wl *= eraMakeup_;
            wr *= eraMakeup_;
            if (transportLive) transport_.process(wl, wr); // MOTION's tape transport (dsp/Transport.h)
            else transport_.write(wl, wr);
            prof::mark(prof::kOutput);
            if (texOn_) texture_.process(wl, wr); // ERA's wear (dsp/EraTexture.h)
            prof::mark(prof::kEraWear);
            tilt_.process(wl, wr);
#ifndef IV_FIXED_VOICINGS
            if (!toneBellOff_)
#endif
                toneBell_.process(wl, wr); // TONE's resonant tilt (v1.0.44)
            duckGain_ += duckGainStep_; // ducking (ADR 0014)
            wl *= duckGain_;
            wr *= duckGain_;
            limiter_.process(wl, wr);
            dryNow_ += dryStep_;
            wetNow_ += wetStep_;
            outL[start + i] = dryNow_ * inL[start + i] + wetNow_ * wl;
            outR[start + i] = dryNow_ * inR[start + i] + wetNow_ * wr;
            prof::mark(prof::kOutput);
        }
        clock_ += uint64_t(n);
    }
    // Edges beyond this block's length (a host's bad offset) apply next block.
    for (int i = 0; i < edgeCount_; ++i) edges_[i].offset -= numSamples;
}

// Still lows: split the tank's input (Butterworth), the highs through fdn_, the lows
// decimated through lowFdn_ (anti-aliased by the crossover's lowpass and a 2 kHz one), then linearly
// interpolated back up and through the 2 kHz lowpass again (anti-image).
void Engine::processTwoTanks(int n)
{
    auto bq = [](const float* c, float* z, float v) {
        const float y = c[0] * v + z[0];
        z[0] = c[1] * v - c[3] * y + z[1];
        z[1] = c[2] * v - c[4] * y;
        return y;
    };
    // The coefficients and states in locals for the chunk (overnight review, 26 Oct 2026): the stores
    // through bufL_ / wetL_ could alias the members, so every sample reloaded all of them. The same
    // sums in the same order (the same bits); the phase's % 4 as & 3 (it is never negative).
    static_assert((kLowDecimate & (kLowDecimate - 1)) == 0, "the phase wraps with & (lowDecimate_ - 1): a power of two (the desktop prototype's 8 too)");
    const int phase0 = lowPhase_;
    int k = 0;
    float cL[5], cA[5], cH[5], z[2][3][2];
    for (int j = 0; j < 5; ++j) {
        cL[j] = xoL_[j];
        cA[j] = aaL_[j];
        cH[j] = xoH_[j];
    }
    for (int c = 0; c < 2; ++c)
        for (int f = 0; f < 3; ++f) {
            z[c][f][0] = xoZ_[c][f][0];
            z[c][f][1] = xoZ_[c][f][1];
        }
    int ph = phase0;
    for (int i = 0; i < n; ++i) {
        float* buf[2] = {bufL_, bufR_};
        for (int c = 0; c < 2; ++c) {
            const float x = buf[c][i];
            const float lo = bq(cA, z[c][1], bq(cL, z[c][0], x));
            buf[c][i] = bq(cH, z[c][2], x);
            if (ph == 0) (c ? lowInR_ : lowInL_)[k] = lo;
        }
        if (ph == 0) ++k;
        ph = (ph + 1) & (lowDecimate_ - 1); // 4 or 8: a power of two
    }
    for (int c = 0; c < 2; ++c)
        for (int f = 0; f < 3; ++f) {
            xoZ_[c][f][0] = z[c][f][0];
            xoZ_[c][f][1] = z[c][f][1];
        }
    tankHalf_.process(fdn_, bufL_, bufR_, wetL_, wetR_, n);
    for (int j = 0; j < k; ++j) lowOutL_[j] = lowOutR_[j] = 0.0f;
    if (k > 0) lowFdn_.process(lowInL_, lowInR_, lowOutL_, lowOutR_, k);
    int j = 0;
    double power = 0.0;
    float zo[2][2] = {{xoZ_[0][4][0], xoZ_[0][4][1]}, {xoZ_[1][4][0], xoZ_[1][4][1]}};
    float prev[2] = {lowPrev_[0], lowPrev_[1]}, cur[2] = {lowCur_[0], lowCur_[1]};
    ph = phase0;
    for (int i = 0; i < n; ++i) {
        if (ph == 0) {
            prev[0] = cur[0]; prev[1] = cur[1];
            cur[0] = lowOutL_[j]; cur[1] = lowOutR_[j];
            ++j;
        }
        const float t = float(ph) / float(lowDecimate_);
        float* wet[2] = {wetL_, wetR_};
        for (int c = 0; c < 2; ++c) {
            const float up = prev[c] + t * (cur[c] - prev[c]);
            const float y = bq(cA, zo[c], up); // anti-image (linear interpolation's own sinc² helps)
            wet[c][i] += y;
            power += 0.5 * double(y) * y;
        }
        ph = (ph + 1) & (lowDecimate_ - 1);
    }
    lowPhase_ = ph;
    lowPrev_[0] = prev[0]; lowPrev_[1] = prev[1];
    lowCur_[0] = cur[0]; lowCur_[1] = cur[1];
    for (int c = 0; c < 2; ++c) {
        xoZ_[c][4][0] = zo[c][0];
        xoZ_[c][4][1] = zo[c][1];
    }
    fdn_.setExternalPower(float(power / double(n)));
}

} // namespace iv
