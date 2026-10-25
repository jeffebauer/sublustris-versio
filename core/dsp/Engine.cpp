#include "dsp/Engine.h"

#include "dsp/SizeOpt.h"
#include "params/Mappings.h"

#include <cmath>

namespace iv {

namespace {

// One-pole smoothing coefficient for a time constant, per control chunk.
float chunkCoeff(float ms, float sampleRate, int n)
{
    if (ms <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-float(n) / (0.001f * ms * sampleRate));
}

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Moves `now` toward `target` by at most `step` (linear glides).
float approach(float now, float target, float step)
{
    const float d = target - now;
    return d > step ? now + step : (d < -step ? now - step : target);
}

} // namespace

IV_SIZE_OPT void Engine::attachMemory(float* large, size_t largeCount, float* fast, size_t fastCount)
{
    attachedLarge_ = large;
    attachedLargeCount_ = largeCount;
    attachedFast_ = fast;
    attachedFastCount_ = fastCount;
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
    size_t largeCount = attachedLargeCount_, fastCount = attachedFastCount_;
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
#endif
    large_.reset(large, largeCount);
    fast_.reset(fast, fastCount);

    ready_ = true;
    for (auto& p : predelay_) ready_ &= p.init(large_, size_t(voicing::kPredelayMaxSeconds * sampleRate));
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            const float s = voicing::kDiffuserSeconds[k] * (c ? voicing::kDiffuserRightScale : 1.0f);
            ready_ &= diffuser_[c].stage[k].init(fast_, uint32_t(s * sampleRate), voicing::kDiffuserCoeff[k]);
        }
    ready_ &= swell_.prepare(large_, sampleRate);
    ready_ &= twinSwell_.prepare(large_, sampleRate);
    twinSwell_.setSCurve(true); // eases in, then settles (owner, renders/swell_curve)
    ready_ &= fdn_.prepare(large_, sampleRate, 0x1D5A7u);
    ready_ &= plate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x9A7Eu);
    ready_ &= reverse_.prepare(large_, sampleRate);
#ifndef IV_FIXED_VOICINGS
    shimUp_.prepare(sampleRate, false);
    shimDown_.prepare(sampleRate, true);
    shimSmooth_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Smooth);
    shimMoving_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Moving);
    shimEvolving_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Evolving);
    // Fed back, the splices' sidebands (±10, 20, 30 Hz around every note at 100 ms) pile up trip on
    // trip: off-harmonic energy -14 dB (plain reverb -35). 8 heads of 800 ms: -30.
    for (auto* s : {&shimClean_, &shimCleanMoving_, &shimCleanDark_}) s->setShape(8, 0.8f);
    shimClean_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Clean);
    shimCleanMoving_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::CleanMoving);
    shimInput_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Clean);
    shimCleanDark_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::CleanDark);
    for (int c = 0; c < 2; ++c) {
        revA_[c].prepare(sampleRate, 0xA11u + 13u * uint32_t(c), c ? -4.0f : 4.0f);
        revB_[c].prepare(sampleRate, 0xB22u + 17u * uint32_t(c), c ? 4.0f : -4.0f);
    }
    echo_.prepare(sampleRate);
    shimInputR4_.setShape(8, 0.4f);
    shimFeedR4_.setShape(8, 0.8f);
    shimFeedDarkR4_.setShape(8, 0.8f);
    for (auto* s : {&shimInputR4_, &shimFeedR4_, &shimFeedDarkR4_}) s->setScatter(1.0f);
    shimInputR4_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Clean);
    shimFeedR4_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::Clean);
    shimFeedDarkR4_.prepare(sampleRate, false, dsp::proto::Shimmer::Style::CleanDark);
    grains_.prepare(sampleRate);
#endif
    ready_ &= twinFdn_.prepare(large_, sampleRate, 0x7A1Eu);
    ready_ &= lowFdn_.prepare(large_, sampleRate / float(kLowDecimate), 0x10F7u);
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
    // REVERSE's short reverb: short lines, no in-loop diffusion or early taps, never infinite.
    ready_ &= reverseFdn_.prepare(large_, sampleRate, 0x2E7Eu, voicing::kReverseMaxLineSeconds);
    reverseFdn_.setMeanSeconds(map::reverseMeanSeconds(0.5f), 1.0f, true);
    ready_ &= twinPlate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x7B2Fu);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            const float s = voicing::kDiffuserSeconds[k] * (c ? voicing::kDiffuserRightScale : 1.0f) * 1.13f;
            ready_ &= twinDiffuser_[c].stage[k].init(fast_, uint32_t(s * sampleRate), voicing::kDiffuserCoeff[k]);
        }
    gesture_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    reset();
}

IV_SIZE_OPT void Engine::reset()
{
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
    twinMean_ = twinSpread_ = -1.0f;
    twinFdn_.reset();
    twinPlate_.reset();
    twinSwell_.reset();
    for (auto& d : twinDiffuser_) d.clear();
    inputPower_ = 0.0f;
    tankInPower_ = 0.0f;
    quietSince_ = 0;
    tailCaptured_ = false;
    smooth_ = values_;
    for (auto& p : predelay_) p.clear();
    for (auto& d : diffuser_) d.clear();
    activeSpace_ = normalisedToSwitch(values_[static_cast<size_t>(ParamId::Space)]);
    flipPhase_ = 0;
    flipGain_ = 1.0f;
    flipStep_ = 0.0f;
    applySpace(true);
    swell_.reset();
    fdn_.reset();
    lowFdn_.reset();
    for (auto& c : xoZ_) for (auto& z : c) z[0] = z[1] = 0.0f;
    lowPhase_ = 0;
    lowPrev_[0] = lowPrev_[1] = lowCur_[0] = lowCur_[1] = 0.0f;
    plate_.reset();
    reverse_.reset();
    reverseFdn_.reset();
    reverseRunning_ = false;
    tilt_.reset();
    tilt_.set(smoothed(ParamId::Tone));
    tilt_.snap();
    limiter_.reset();
    for (auto& t : tape_) t.reset();
    bbd_.reset();
    radio_.reset();
    tapeMix_ = 0.0f;
    duck_ = duckGrDb_ = duckGainStep_ = 0.0f;
    duckGain_ = 1.0f;
    hissLp_[0] = hissLp_[1] = 0.0f;
    noiseSeed_ = 0xA11CEu;
    reverseMix_ = reverseFeed_ = reverseTarget_;
    predelayStep_ = 0.0f;
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

void Engine::queueEdge(bool isButton, bool level, int offset)
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

void Engine::handleEdges(int chunkStart, int chunkEnd)
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

void Engine::onGesture(GestureReader::Event e)
{
    const float twinStep = 1.0f / (voicing::kTwinFadeSeconds * sampleRate_);
    const float tankStep = 1.0f / (voicing::kFreezeInputSeconds * sampleRate_);
    switch (e) {
    case GestureReader::Event::Freeze:
        // The big tank keeps the SIZE and DECAY it had; the knobs move to
        // the twin. Its input closes, its colour glides out.
        heldSize_ = smoothed(ParamId::Size);
        heldDecay_ = smoothed(ParamId::Decay);
        padFloorDb_ = 0.0f; // no cap until the pad has settled (controlTick)
        padCaptured_ = false;
        frozeAt_ = clock_;
        fdn_.resyncHoldLevel();
        lowFdn_.resyncHoldLevel();
        plate_.resyncHoldLevel();
        tankInTarget_ = 0.0f;
        tankInStep_ = tankStep;
        if (!twinActive_) {
            twinFdn_.setInLoopDiffusion(activeSpace_ == 2 ? 1.0f : 0.0f); // before the reset, so it starts there
            twinFdn_.reset();
            twinPlate_.reset();
            for (auto& d : twinDiffuser_) d.clear();
            twinSwell_.setAttackSeconds(activeSpace_ == 2
                                            ? map::massiveNormalAttackSeconds(smoothed(ParamId::Decay), smoothed(ParamId::Size)) : 0.0f,
                                        true);
            twinSwell_.reset();
            twinMean_ = twinSpread_ = -1.0f;
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
        padFloorDb_ = (tankSlowLevelDb() > voicing::kFloorDbfs ? tankSlowLevelDb() : voicing::kFloorDbfs) + voicing::kPaintHeadroomDb;
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

// DECAY's loss rate for the SPACE in use (MASSIVE's shortest tail follows SIZE, v1.0.21).
float Engine::decayRate(float decay, float size) const
{
    return activeSpace_ == 2 ? map::massiveDecayRate(decay, size) : map::decayRate(decay, activeSpace_);
}

// Sets every voicing target for the SPACE in use (activeSpace_). While
// frozen, the big tank keeps the SIZE and DECAY it froze with and its loss
// glides to none (freezeMix_); the knobs play the twin. snap: jump
// straight there (reset, a SPACE flip).
void Engine::applySpace(bool snap, int n, bool snapInput)
{
    const bool frozen = gesture_.frozen();
    const float size = frozen ? heldSize_ : smoothed(ParamId::Size);
    // HALL's SIZE also sets its T60 scale, output trim and in-loop diffusion, which act at once (the
    // lines' lengths glide on their own): they follow a ~80 ms glide of SIZE, so a SIZE jump (CV, or an
    // unfreeze after SIZE moved) doesn't step the loop's gains (soak audit).
    if (snap) hallSize_ = size;
    else {
        hallSize_ += (1.0f - std::exp(-float(n) / (0.08f * sampleRate_))) * (size - hallSize_);
        if (std::fabs(size - hallSize_) < 1e-6f) hallSize_ = size;
    }
    const float decay = frozen ? heldDecay_ : smoothed(ParamId::Decay);
    const float time = smoothed(ParamId::Time), tone = smoothed(ParamId::Tone);
    const int attack = normalisedToSwitch(smoothed(ParamId::Attack));
    const bool massive = activeSpace_ == 2, plate = activeSpace_ == 0;

    // Freezing: loss → 0, highs kept (ratio → 1), the governor caps paint.
    // ERA's darkening scales the high end's T60 share with TONE (Era.h).
    const float toneRatio = map::toneHighT60Ratio(tone) * map::eraMix(smoothed(ParamId::Era)).highRatioScale;
    const float sizeT60 = activeSpace_ == 1 ? map::hallT60Scale(hallSize_) : 1.0f;
    const float rate = decayRate(decay, size) / sizeT60 * (1.0f - freezeMix_);
    const float spaceRatio = massive ? map::massiveHighRatio(toneRatio, decay) : toneRatio;
    const float highRatio = spaceRatio + freezeMix_ * (1.0f - spaceRatio);
#ifndef IV_FIXED_VOICINGS
    // Prototype: a fed-back shimmer turns the tank's own movement off: re-applied on every trip round
    // the shimmer loop, PLATE's chorus and HALL's deeper floor piled up into a smeared, gritty octave
    // (off-harmonic energy, PLATE -11 dB with it, -26 without; at 30 % still -10.5).
    const float shimMod = altMode_ == 8 || altMode_ == 9 || altMode_ == 11 || altMode_ == 13 || altMode_ == 14 ? 1.0f - altMix_ : 1.0f;
#else
    constexpr float shimMod = 1.0f;
#endif
    // Frozen: full weight (only the pad's own hold acts, there's no floor). Not frozen: DECAY's own
    // weight at once, not glided with freezeMix_: gliding it pulled a fed pad (above the floor) down
    // hard on unfreeze, "a pronounced immediate drop" and a too-steep decay (owner, freeze options).
    // Freezing glides the weight up with freezeMix_ (a jump to full changed a finite tail's loss in
    // one sample: a click).
    const float governor = frozen ? std::fmax(map::governorWeight(decay), freezeMix_) : map::governorWeight(decay);
    // DECAY keeps the loudness (v1.0.28): a shorter tail is made up to the finite top's level, on the
    // wet and the twin alike. A frozen pad keeps heldDecay_, so freezing doesn't move it.
    decayGain_ = std::pow(map::decayLevelMakeup(decay, activeSpace_), decayMakeup_);
    reverseGain_ = std::pow(map::reverseLevelMakeup(smoothed(ParamId::Decay)), decayMakeup_);
    if (snap) {
        decayGainNow_ = decayGain_;
        reverseGainNow_ = reverseGain_;
    }
    // The cap: a still pad's own level (or + headroom while painting); an
    // infinite tail's settled level once the input has gone quiet; else the
    // floor. Darkening always stops at the floor (ADR 0008).
    // A pad isn't pulled to the floor: the floor glides away with freezeMix_ (and back on unfreeze).
    // Switched in one sample, a fed infinite tail's governor loss vanished at once and the wet
    // stepped (up to 20x a sample's normal change, PLATE; heard once the dark tilt took the highs out).
    const float floorDb = voicing::kFloorDbfs * (1.0f - freezeMix_);
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
        plate_.setModScale(modScale_ * shimMod * (1.0f - freezeMix_));
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
        fdn_.setModDepth(modScale_ * shimMod * (massive ? 1.0f : voicing::kHallFloorScale) * (1.0f - freezeMix_));
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
    const dsp::EraMix era = map::eraMix(smoothed(ParamId::Era));
    // The bucket-brigade and the radio on the wet (ADR 0021); each runs only while it's in.
    bbdW_ = era.bbd;
    radioW_ = era.radio;
    if (bbdW_ > 0.0f) bbd_.set(era.bbdT, sampleRate_);
    if (radioW_ > 0.0f) radio_.set(era.radioT, sampleRate_, n);
    dsp::EraMix loopEra = era;
    const float keep = 1.0f - freezeMix_;
    loopEra.midiRound = map::governorWeight(decay) >= 1.0f; // infinite: no systematic loss
    loopEra.midiDark = 1.0f - map::governorWeight(decay) * (1.0f - voicing::kMidiDarkAtInfinite); // v1.0.23
    // Neve's even harmonics add energy in proportion to the level: harmless
    // while DECAY loses energy, a runaway in a lossless loop (measured: the
    // tail jumping 2-3 dB, then the governor overshooting into dips). So
    // they fade out across DECAY's infinite crossover.
    loopEra.even *= keep * (1.0f - map::governorWeight(decay));
    loopEra.drive = 1.0f + keep * (era.drive - 1.0f);
    loopEra.midiEngage *= keep;
    const dsp::DriftAmounts drift = map::driftAmounts(smoothed(ParamId::Drift), era.cassette);
    // The cassette's slow 2:1 compression: more loss above −18 dBFS.
    // Judged like the floor (the lower of its 1.5 s and 50 ms levels): on the 1.5 s level alone it
    // kept compressing for seconds after a loud input stopped, and an infinite tail plunged 20+ dB
    // under the floor (soak audit). A steady tail measures the same.
    const float tankNowDb = activeSpace_ == 0 ? plate_.fastLevelDb() : fdn_.fastLevelDb();
    const float over = std::fmin(tankLevelDb(), tankNowDb) - voicing::kCompressThresholdDb;
    const float compress = over > 0.0f ? keep * era.cassette * voicing::kCompressRatePerDb * over : 0.0f;
    if (plate) {
        plate_.setEra(loopEra);
        plate_.setDrift(drift);
        plate_.setExtraRate(compress);
    } else {
        fdn_.setEra(loopEra);
        fdn_.setDrift(drift);
        fdn_.setExtraRate(compress);
        lowFdn_.setEra(loopEra);
        lowFdn_.setDrift(dsp::DriftAmounts{}); // still: no wow, flutter or Wander
        lowFdn_.setExtraRate(compress);
    }
    eraMakeup_ = era.makeup;
    tapeDrive_ = era.cassette; // tape out (v1.0.22, E5)
    if (snap) tapeMix_ = tapeDrive_ > 0.0f && tapeOn_ ? 1.0f : 0.0f;
    for (auto& t : tape_) t.set(tapeDrive_, sampleRate_);

    tilt_.set(smoothed(ParamId::Tone));
    // Hiss follows the input (none in silence, so an infinite tail doesn't
    // fill with noise, ADR 0005); crackle the same.
    const float inDb = 10.0f * std::log10(inputPower_ + 1e-20f);
    const float follow = map::clamp01((inDb - (voicing::kHissFollowFromDbfs - 30.0f)) / 30.0f);
    hissGain_ = era.cassette * follow * std::pow(10.0f, voicing::kHissDbfs / 20.0f);
    crackleChance_ = era.midiverb * follow * voicing::kCracklePerSecond / sampleRate_;
    crackleGain_ = std::pow(10.0f, voicing::kCrackleDbfs / 20.0f);

    // The input diffusers' amount glides (~50 ms): it changes with a SPACE flip and with MASSIVE's SIZE,
    // and a jump stepped the input into the tank and REVERSE's line (soak audit).
    const float diffusion = massive ? map::massiveDiffusion(size) : 1.0f;
    const float glide = snap && snapInput ? 1.0f : 1.0f - std::exp(-float(n) / (0.05f * sampleRate_));
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
    // REVERSE: rising taps over TIME into the short reverb; DECAY sets how sudden the stop is.
    reverse_.setSeconds(map::reverseSeconds(time), snap && snapInput); // REVERSE runs on through a flip: it glides there
    reverseFdn_.setDecayRate(1.0f / map::reverseTailSeconds(smoothed(ParamId::Decay)));
    const float revMean = map::reverseMeanSeconds(smoothed(ParamId::Size));
    if (snap || std::fabs(revMean - reverseMean_) > 1e-6f) {
        reverseFdn_.setMeanSeconds(revMean, 1.0f, snap && snapInput);
        reverseMean_ = revMean;
    }
    reverseFdn_.setHighT60Ratio(toneRatio);
    reverseFdn_.setEra(loopEra);
    reverseFdn_.setDrift(drift);
#ifndef IV_FIXED_VOICINGS
    // A prototype on ATTACK's third position: REVERSE stays out; the prototype rests while frozen.
    altTarget_ = attack == 2 && altMode_ > 0 && !frozen ? 1.0f : 0.0f;
    reverseTarget_ = attack == 2 && (altMode_ == 0 || altMode_ == 16 || altMode_ == 19) && !frozen ? 1.0f : 0.0f;
    if (altMode_ == 19) altTarget_ = 0.0f; // the gate rides REVERSE's own crossfade
    // Gate length 0.1 -> 0.8 s (RMX16 / FutureVerb / BigSky ranges; 100-700 ms the classic drum gate).
    if (altMode_ == 19) reverse_.setSeconds(map::expLerp(0.1f, 0.8f, time), snap && snapInput);
    if (altMode_ == 16) altTarget_ = 0.0f; // the reversed tail rides REVERSE's own crossfade into the wet
    // Reversed tail: the short reverb rings about as long as a grain (TIME), so a hit's tail reverses whole.
    if (altMode_ == 16) reverseFdn_.setDecayRate(1.0f / map::reverseSeconds(time));
#else
    reverseTarget_ = attack == 2 && !frozen ? 1.0f : 0.0f; // REVERSE rests while frozen (ADR 0015)
#endif

    const float pd = predelay * sampleRate_;
    if (snap) predelayNow_ = pd;
    predelayTarget_ = pd;

    // The twin (ADR 0009): the frozen tank's space and the knobs' colour, never frozen. SIZE and
    // DECAY play it while frozen (at the freeze they're where the pad froze, so it starts as the
    // pad's twin); it always fades within kTwinMaxT60. Full in-loop diffusion in MASSIVE, like the pad.
    if (twinActive_) {
        const float tSize = smoothed(ParamId::Size), tDecay = smoothed(ParamId::Decay);
        twinSwell_.setAttackSeconds(massive ? map::massiveNormalAttackSeconds(tDecay, tSize) : 0.0f, snap);
        const float tT60 = activeSpace_ == 1 ? map::hallT60Scale(tSize) : 1.0f;
        const float tRate = std::fmax(decayRate(tDecay, tSize) / tT60, 1.0f / voicing::kTwinMaxT60);
        dsp::EraMix tEra = era;
        tEra.even *= 1.0f - map::governorWeight(tDecay);
        if (plate) {
            twinPlate_.setScale(map::plateScale(tSize), snap);
            twinPlate_.setDecayRate(tRate);
            twinPlate_.setHighT60Ratio(toneRatio);
            twinPlate_.setGovernor(0.0f);
            twinPlate_.setEra(tEra);
            twinPlate_.setDrift(drift);
        } else {
            const float mean = massive ? map::massiveMeanSeconds(tSize) : map::hallMeanSeconds(tSize);
            const float spread = massive ? map::massiveSpread(tSize) : 1.0f;
            if (std::fabs(mean - twinMean_) > 1e-6f || std::fabs(spread - twinSpread_) > 1e-6f) {
                twinFdn_.setMeanSeconds(mean, spread, twinMean_ < 0.0f);
                twinMean_ = mean;
                twinSpread_ = spread;
            }
            twinFdn_.setDecayRate(tRate);
            twinFdn_.setHighT60Ratio(massive ? map::massiveHighRatio(toneRatio, tDecay) : toneRatio);
            twinFdn_.setGovernor(0.0f);
            twinFdn_.setInLoopDiffusion(massive ? 1.0f : 0.0f);
            twinFdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim * map::massiveDecayLevelTrim(tDecay, tSize)
                                           : voicing::kHallOutputTrim * map::hallSizeLevelTrim(tSize, tDecay));
            twinFdn_.setEarlyTaps(massive ? 1.0f : 0.0f);
            twinFdn_.setEra(tEra);
            twinFdn_.setDrift(drift);
        }
    }
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

void Engine::controlTick(int n)
{
    // Ducking (ADR 0014, amended v1.0.17): 4:1 compression of the wet, keyed by the input's
    // ~10 ms power, × how far into infinite DECAY is; not while frozen (playing over a pad
    // mustn't duck it; fades with the freeze glide). The reduction moves with its own attack
    // and release once per control block; the wet gain ramps across the block.
    {
        const float levelDb = 10.0f * std::log10(inputPower_ + 1e-12f);
        const float over = levelDb - voicing::kDuckThresholdDbfs;
        const float target = (over > 0.0f ? (1.0f - 1.0f / voicing::kDuckRatio) * over : 0.0f)
                           * map::governorWeight(smoothed(ParamId::Decay)) * (1.0f - freezeMix_);
        // A freeze lets go quickly (50 ms), so a fresh pad doesn't swell up over the release.
        const float secs = target > duckGrDb_ ? voicing::kDuckAttackSeconds
                         : (freezeMix_ > 0.0f ? 0.05f : voicing::kDuckReleaseSeconds);
        duckGrDb_ += (1.0f - std::exp(-float(n) / (secs * sampleRate_))) * (target - duckGrDb_);
        if (target == 0.0f && duckGrDb_ < 0.05f) duckGrDb_ = 0.0f; // fully let go (and exact unity)
        duckGainStep_ = (std::pow(10.0f, -duckGrDb_ / 20.0f) - duckGain_) / float(n);
        duck_ = map::clamp01(duckGrDb_ / voicing::kDuckFullDb);
    }
    // The PLATE's first input allpass wanders (Mutable Instruments' Clouds and Beads; owner's pick
    // on renders/diffuser_motion, v1.0.16), so repeated hits don't enter the tank identically.
    // Its depth glides in and out with SPACE, so a flip never jumps the read.
    const float motionTarget = activeSpace_ == 0 ? voicing::kDiffuserMotionSeconds * sampleRate_ : 0.0f;
    diffMotionNow_ += (1.0f - std::exp(-float(n) / (0.1f * sampleRate_))) * (motionTarget - diffMotionNow_);
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
            // Targets first, so the resets below land on them (the FDN's output trim). The predelay
            // jumps too, here where the tank's input and REVERSE's are silent; the input diffusers glide
            // (a jump in their amount rang on in them for ~100 ms, past the fade back in).
            applySpace(true, n, false);
            fdn_.reset();
            lowFdn_.reset();
            plate_.reset();
            swell_.reset();
            // REVERSE (its recording line and its short reverb) doesn't depend on SPACE: it keeps
            // running and fades with the flip. Cleared here while the input played, its line held a
            // hard edge where the input was cut off and the swell clicked when its taps reached it,
            // up to a TIME after the flip; and its restarted reverb clicked when sound first came
            // round its lines (soak audit).
            // A twin still fading after an unfreeze belongs to the old space: it has faded out with
            // the wet, so it stops here and the new space's twin starts clean at the next freeze
            // (soak audit: it was cut off mid-sound, a click, and the other twin replayed stale sound).
            twinActive_ = false;
            twinIn_ = twinInStep_ = 0.0f;
            twinFdn_.reset();
            twinPlate_.reset();
            twinSwell_.reset();
            for (auto& d : twinDiffuser_) d.clear();
            twinMean_ = twinSpread_ = -1.0f;
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
    swell_.control(n);
    twinSwell_.control(n);
    reverse_.control(n);
    // The predelay moves at most half a sample per sample (the input read at 0.5x-1.5x speed): a jump
    // in its target (ATTACK switched with TIME up, MASSIVE's SIZE jumping, a SPACE flip) used to
    // sweep it across in one control chunk, a burst of sped-up sound (soak audit). TIME's own knob
    // glide stays as it was unless it's faster than that.
    predelayStep_ = (predelayTarget_ - predelayNow_) / float(n);
    if (predelayStep_ > kPredelayMaxSpeed) predelayStep_ = kPredelayMaxSpeed;
    if (predelayStep_ < -kPredelayMaxSpeed) predelayStep_ = -kPredelayMaxSpeed;
#ifndef IV_FIXED_VOICINGS
    // PROTOTYPE (owner, plugin: "changing TIME rapidly also creates a large wash of noise"; measured:
    // a held note's energy away from its pitch -24 dB steady, -1 dB while TIME moves): the pre-delay
    // jumps and the old delay crossfades out (equal power) instead of sliding, which swept the input
    // by up to an octave.
    if (predelayXfade_ > 0.0f) {
        predelayStep_ = 0.0f;
        // Once TIME has rested 50 ms (as SIZE): back to back, each flick jumped the reverb's input
        // back and forth in time, a stutter (25 ms steps 6.8 dB against today's 5.9).
        pdRest_ = std::fabs(predelayTarget_ - pdLastTarget_) > 0.5f ? 0.0f : pdRest_ + float(n) / sampleRate_;
        pdLastTarget_ = predelayTarget_;
        if (pdXfLeft_ <= 0 && pdRest_ >= 0.05f && std::fabs(predelayTarget_ - predelayNow_) > 0.5f) {
            pdOld_ = predelayNow_;
            predelayNow_ = predelayTarget_;
            pdXfLen_ = pdXfLeft_ = int(predelayXfade_ * sampleRate_);
        }
    }
#endif

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
    const float reverseStep = 1.0f / (voicing::kAttackFadeSeconds * sampleRate_);
    const float tapeStep = 1.0f / (0.02f * sampleRate_);
    for (int start = 0; start < numSamples; start += kChunk) {
        const int n = (numSamples - start) < kChunk ? (numSamples - start) : kChunk;
        handleEdges(start, start + n);
        controlTick(n);
#ifndef IV_FIXED_VOICINGS
        altTime_ = smoothed(ParamId::Time);
        // Echo-verb: DECAY sets the repeats (feedback 0.35 -> 0.85, about 2 to 12 audible), capped below 1.
        altFb_ = 0.35f + 0.5f * map::clamp01(smoothed(ParamId::Decay) / 0.8f);
#endif

        // REVERSE runs while it's on or fading; its short reverb restarts clean next time.
        const bool reverseOn = reverseTarget_ > 0.0f || reverseMix_ > 0.0f;
        if (!reverseOn && reverseRunning_) reverseFdn_.reset();
        reverseRunning_ = reverseOn;

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
            predelayNow_ += predelayStep_;
            float pl = l, pr = r;
            if (predelayNow_ >= 2.0f) {
                pl = predelay_[0].readCubic(predelayNow_);
                pr = predelay_[1].readCubic(predelayNow_);
            } else if (predelayNow_ > 0.0f) { // under 2 samples (the cubic's least): between the input and that
                const float t = 0.5f * predelayNow_;
                pl += t * (predelay_[0].readCubic(2.0f) - pl);
                pr += t * (predelay_[1].readCubic(2.0f) - pr);
            }
#ifndef IV_FIXED_VOICINGS
            if (pdXfLeft_ > 0) { // the old pre-delay fading out
                const float d = std::fmax(pdOld_, 2.0f), g = 1.5707963f * float(pdXfLeft_--) / float(pdXfLen_);
                const float ol = pdOld_ > 0.0f ? predelay_[0].readCubic(d) : l, orr = pdOld_ > 0.0f ? predelay_[1].readCubic(d) : r;
                pl = std::sin(g) * ol + std::cos(g) * pl;
                pr = std::sin(g) * orr + std::cos(g) * pr;
            }
#endif
#ifndef IV_FIXED_VOICINGS
            echoTapL_[i] = echoTapR_[i] = 0.0f;
            if (altMode_ == 17 || altMode_ == 18) {
                // The echo before the reverb (replacing the pre-delay's role); the reverb grows from the
                // echoes and the first repeats are also heard directly (-6 dB).
                float m = 0.5f * (pl + pr);
                if (altMode_ == 18) m += 0.15f * 0.5f * (shimSrcL_[i] + shimSrcR_[i]); // the reverb re-echoed
                const float e = altMix_ * echo_.process(m, map::expLerp(0.08f, 1.0f, altTime_), altFb_);
                pl += e;
                pr += e;
                echoTapL_[i] = echoTapR_[i] = 0.35f * e; // -9 dB (at -6 the mode was 2-4.5 dB over NORMAL)
            }
#endif
            pl = diffuser_[0].process(pl);
            pr = diffuser_[1].process(pr);
            // REVERSE's taps hear the same diffused input as the tank (always recording), faded with
            // a SPACE flip as the tank's is: the input side jumps at the flip's silent point.
            revInL_[i] = flipGain_ * pl;
            revInR_[i] = flipGain_ * pr;
#ifndef IV_FIXED_VOICINGS
            if (altMode_ != 16) // the reversed tail hears the input itself, not REVERSE's rising taps
#endif
            reverse_.process(revInL_[i], revInR_[i], reverseOn);
            // The taps fade into the short reverb as REVERSE comes in (an ATTACK switch, an unfreeze):
            // a restarted reverb fed the full swell at once clicked when it first came round its lines,
            // 25-80 ms later, through the crossfade (soak audit).
            reverseFeed_ = approach(reverseFeed_, reverseTarget_, reverseStep);
            revInL_[i] *= reverseFeed_;
            revInR_[i] *= reverseFeed_;
            // The twin hears the input itself, through its own diffusion: no predelay; in MASSIVE, its own swell.
            twinIn_ = clamp01(twinIn_ + twinInStep_);
            twinInL_[i] = twinDiffuser_[0].process(twinIn_ * l);
            twinInR_[i] = twinDiffuser_[1].process(twinIn_ * r);
            // MASSIVE's twin swells as its tank does, so what you play over a frozen MASSIVE
            // arrives as a wash, not two distinct repeats first (owner, 22 Oct 2026).
            twinSwell_.process(twinInL_[i], twinInR_[i]);
            twinL_[i] = twinR_[i] = 0.0f;
#ifndef IV_FIXED_VOICINGS
            if (altMode_ > 0) {
                altMix_ = approach(altMix_, altTarget_, reverseStep);
                if (altMode_ == 15) {
                    // Reverse into the tank: reversed grains of what you play (TIME long) feed the reverb.
                    const float len = map::reverseSeconds(altTime_);
                    const float rl = revA_[0].process(pl, len, 0.25f), rr = revA_[1].process(pr, len, 0.25f);
                    pl += altMix_ * (0.25f * pl + 1.4f * rl - pl);
                    pr += altMix_ * (0.25f * pr + 1.4f * rr - pr);
                } else if (altMode_ == 17 || altMode_ == 18 || altMode_ == 16 || altMode_ == 19 || altMode_ == 20) {
                    // handled before the diffusers (echo) or after the short reverb (reversed tail)
                } else if (altMode_ == 3) {
                    // Grains of the last ~2.7 s feed the tank in place of most of the direct input.
                    float gl = 0.0f, gr = 0.0f;
                    grains_.process(pl, pr, gl, gr, altTime_ * altTime_ * 2.0f + 0.05f);
                    pl += altMix_ * (0.3f * pl + gl - pl);
                    pr += altMix_ * (0.3f * pr + gr - pr);
                } else if (altMode_ == 10 || altMode_ == 12) {
                    // Input shimmer: what you play, an octave up, into the tank; no feedback.
                    float sl = pl, sr2 = pr;
                    (altMode_ == 12 ? shimInputR4_ : shimInput_).process(sl, sr2);
                    const float send = altMix_ * 1.2f * altTime_;
                    pl += send * sl;
                    pr += send * sr2;
                } else if (altMix_ > 0.0f) {
                    // Shimmer: the last chunk's wet, an octave up (or down), back into the tank.
                    float sl = shimSrcL_[i], sr2 = shimSrcR_[i];
                    dsp::proto::Shimmer& sh = altMode_ == 2 ? shimDown_ : (altMode_ == 4 ? shimSmooth_ : (altMode_ == 5 ? shimMoving_
                                       : (altMode_ == 6 ? shimEvolving_ : (altMode_ == 8 ? shimClean_ : (altMode_ == 9 ? shimCleanMoving_ : (altMode_ == 11 ? shimCleanDark_ : (altMode_ == 13 ? shimFeedR4_ : (altMode_ == 14 ? shimFeedDarkR4_ : shimUp_))))))));
                    sh.process(sl, sr2);
                    const float send = altMix_ * (altMode_ == 2 ? 0.45f : 0.6f) * altTime_;
                    pl += send * sl;
                    pr += send * sr2;
                }
            }
#endif
            swell_.process(pl, pr);
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
            const float g = tankIn_ * flipGain_;
            bufL_[i] = g * pl;
            bufR_[i] = g * pr;
            tankInPower_ += 0.002f * (0.5f * (bufL_[i] * bufL_[i] + bufR_[i] * bufR_[i]) - tankInPower_);
            wetL_[i] = wetR_[i] = 0.0f;
        }

        // The twin first, so painting can send it into the pad.
        if (twinActive_) {
            if (activeSpace_ == 0) twinPlate_.process(twinInL_, twinInR_, twinL_, twinR_, n);
            else twinFdn_.process(twinInL_, twinInR_, twinL_, twinR_, n);
            const float lvl = activeSpace_ == 0 ? twinPlate_.levelDb() : twinFdn_.levelDb();
            if (!gesture_.frozen() && twinIn_ == 0.0f && lvl < -100.0f) twinActive_ = false;
            // Painting (holding) copies a little of the twin into the pad: what you played becomes a layer of it.
            const float pg = paint_ * paintRoom_ * voicing::kPaintLevel;
            if (pg > 0.0f)
                for (int i = 0; i < n; ++i) {
                    bufL_[i] += pg * twinL_[i];
                    bufR_[i] += pg * twinR_[i];
                }
        }

        if (activeSpace_ == 0) plate_.process(bufL_, bufR_, wetL_, wetR_, n);
        else if (!lowTank_) fdn_.process(bufL_, bufR_, wetL_, wetR_, n); // the test hook's one tank
        else processTwoTanks(n);
        for (int i = 0; i < n; ++i) revL_[i] = revR_[i] = 0.0f;
#ifndef IV_FIXED_VOICINGS
        for (int i = 0; i < n; ++i) { // the shimmer's source for the next chunk
            shimSrcL_[i] = wetL_[i];
            shimSrcR_[i] = wetR_[i];
        }
#endif
        if (reverseOn) reverseFdn_.process(revInL_, revInR_, revL_, revR_, n);
#ifndef IV_FIXED_VOICINGS
        if (altMode_ == 16) { // the short reverb's tail, played backwards in overlapping grains
            const float len = map::reverseSeconds(altTime_);
            for (int i = 0; i < n; ++i) {
                revL_[i] = revB_[0].process(revL_[i], len, 0.0f);
                revR_[i] = revB_[1].process(revR_[i], len, 0.0f);
            }
        }
#endif

        for (int i = 0; i < n; ++i) {
            // REVERSE: the swell replaces the forward wet, crossfaded over ~50 ms.
            // DECAY's level make-up (v1.0.28) on the forward wet, the twin and REVERSE, ~40 ms glides.
            decayGainNow_ += 0.0005f * (decayGain_ - decayGainNow_);
            reverseGainNow_ += 0.0005f * (reverseGain_ - reverseGainNow_);
#ifndef IV_FIXED_VOICINGS
            // The reversed tail hears the input itself, much louder than REVERSE's taps (+15.5 dB trim).
            const float rg = (altMode_ == 16 ? 8.0f : voicing::kReverseLevel) * reverseGainNow_; // 16: +18 dB, measured
#else
            const float rg = voicing::kReverseLevel * reverseGainNow_;
#endif
            const float rl = rg * revL_[i], rr = rg * revR_[i];
            reverseMix_ = approach(reverseMix_, reverseTarget_, reverseStep);
            // The twin fades with a SPACE flip too (it stops at the flip's silent point).
            float fl = decayGainNow_ * wetL_[i], fr = decayGainNow_ * wetR_[i];
#ifndef IV_FIXED_VOICINGS
            if (altMode_ == 20) {
                // GATED ROOM (Padgham's keyed gate; Yamaha SPX90 Reverb & Gate): open while there's input
                // (~10 ms follower above -50 dB), held TIME (0.1 -> 0.8 s) after it stops, then shut over a
                // 40 ms cosine; any new note opens it again within 2 ms.
                if (inputPower_ > 1e-5f) gateHold_ = int(map::expLerp(0.1f, 0.8f, altTime_) * sampleRate_);
                else if (gateHold_ > 0) --gateHold_;
                const bool open = gateHold_ > 0 || altMix_ < 1.0f;
                gateRamp_ = open ? std::fmin(1.0f, gateRamp_ + 1.0f / (0.002f * sampleRate_))
                                 : std::fmax(0.0f, gateRamp_ - 1.0f / (0.04f * sampleRate_));
                const float gg = 1.0f - altMix_ * (1.0f - (0.5f - 0.5f * std::cos(3.14159265f * gateRamp_)));
                fl *= gg;
                fr *= gg;
            }
#endif
            const float tl = decayGainNow_ * voicing::kTwinLevel * twinL_[i];
            const float tr = decayGainNow_ * voicing::kTwinLevel * twinR_[i];
            float wl = (fl + reverseMix_ * (rl - fl) + tl) * flipGain_;
            float wr = (fr + reverseMix_ * (rr - fr) + tr) * flipGain_;
#ifndef IV_FIXED_VOICINGS
            wl += echoTapL_[i] * flipGain_; // echo-verb: the first repeats heard directly
            wr += echoTapR_[i] * flipGain_;
#endif
            wl *= eraMakeup_;
            wr *= eraMakeup_;
            // ERA's tape out (v1.0.27, dsp/Tape.h), faded in and out over ~20 ms as the cassette layer
            // appears and leaves: switched in one sample, its oversampling filters' phase stepped the wet
            // (a click turning ERA through 0.35, found 25 Oct 2026).
            tapeMix_ = approach(tapeMix_, tapeDrive_ > 0.0f && tapeOn_ ? 1.0f : 0.0f, tapeStep);
            if (tapeMix_ > 0.0f) {
                const float tl2 = tape_[0].process(wl), tr2 = tape_[1].process(wr);
                wl = tapeMix_ >= 1.0f ? tl2 : wl + tapeMix_ * (tl2 - wl);
                wr = tapeMix_ >= 1.0f ? tr2 : wr + tapeMix_ * (tr2 - wr);
            }
            if (bbdW_ > 0.0f) { // ERA's bucket-brigade (ADR 0021)
                const float bl = bbd_.process(wl, 0), br = bbd_.process(wr, 1);
                wl += bbdW_ * (voicing::kEraBbdTrim * bl - wl);
                wr += bbdW_ * (voicing::kEraBbdTrim * br - wr);
            }
            if (radioW_ > 0.0f) { // ERA's AM radio (ADR 0021)
                float rl2 = wl, rr2 = wr;
                radio_.process(rl2, rr2);
                wl += radioW_ * (voicing::kEraRadioTrim * rl2 - wl);
                wr += radioW_ * (voicing::kEraRadioTrim * rr2 - wr);
            }
            tilt_.process(wl, wr);
            duckGain_ += duckGainStep_; // ducking (ADR 0014)
            wl *= duckGain_;
            wr *= duckGain_;
            limiter_.process(wl, wr);
            dryNow_ += dryStep_;
            wetNow_ += wetStep_;
            outL[start + i] = dryNow_ * inL[start + i] + wetNow_ * wl;
            outR[start + i] = dryNow_ * inR[start + i] + wetNow_ * wr;
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
    const int phase0 = lowPhase_;
    int k = 0;
    for (int i = 0; i < n; ++i) {
        float* buf[2] = {bufL_, bufR_};
        for (int c = 0; c < 2; ++c) {
            const float x = buf[c][i];
            const float lo = bq(aaL_, xoZ_[c][1], bq(xoL_, xoZ_[c][0], x));
            buf[c][i] = bq(xoH_, xoZ_[c][2], x);
            if (lowPhase_ == 0) (c ? lowInR_ : lowInL_)[k] = lo;
        }
        if (lowPhase_ == 0) ++k;
        lowPhase_ = (lowPhase_ + 1) % kLowDecimate;
    }
    fdn_.process(bufL_, bufR_, wetL_, wetR_, n);
    for (int j = 0; j < k; ++j) lowOutL_[j] = lowOutR_[j] = 0.0f;
    if (k > 0) lowFdn_.process(lowInL_, lowInR_, lowOutL_, lowOutR_, k);
    lowPhase_ = phase0;
    int j = 0;
    double power = 0.0;
    for (int i = 0; i < n; ++i) {
        if (lowPhase_ == 0) {
            lowPrev_[0] = lowCur_[0]; lowPrev_[1] = lowCur_[1];
            lowCur_[0] = lowOutL_[j]; lowCur_[1] = lowOutR_[j];
            ++j;
        }
        const float t = float(lowPhase_) / float(kLowDecimate);
        float* wet[2] = {wetL_, wetR_};
        for (int c = 0; c < 2; ++c) {
            const float up = lowPrev_[c] + t * (lowCur_[c] - lowPrev_[c]);
            const float y = bq(aaL_, xoZ_[c][4], up); // anti-image (linear interpolation's own sinc² helps)
            wet[c][i] += y;
            power += 0.5 * double(y) * y;
        }
        lowPhase_ = (lowPhase_ + 1) % kLowDecimate;
    }
    fdn_.setExternalPower(float(power / double(n)));
}

} // namespace iv
