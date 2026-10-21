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
    ready_ &= fdn_.prepare(large_, sampleRate, 0x1D5A7u);
    ready_ &= plate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x9A7Eu);
    ready_ &= companion_.prepare(fast_, sampleRate, voicing::kCompanionMaxScale, false, 0xC0A1u);
    ready_ &= reverse_.prepare(large_, sampleRate);
    ready_ &= twinFdn_.prepare(large_, sampleRate, 0x7A1Eu);
    ready_ &= twinPlate_.prepare(large_, sampleRate, voicing::kPlateMaxScale, true, 0x7B2Fu);
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) {
            const float s = voicing::kDiffuserSeconds[k] * (c ? voicing::kDiffuserRightScale : 1.0f) * 1.13f;
            ready_ &= twinDiffuser_[c].stage[k].init(fast_, uint32_t(s * sampleRate), voicing::kDiffuserCoeff[k]);
        }
    gesture_.prepare(sampleRate);
    darken_.prepare(sampleRate);
    limiter_.prepare(sampleRate);
    duckAttack_ = 1.0f - std::exp(-1.0f / (voicing::kDuckAttackSeconds * sampleRate));
    duckRelease_ = 1.0f - std::exp(-1.0f / (voicing::kDuckReleaseSeconds * sampleRate));
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
    compIn_ = paint_ = 0.0f;
    tankInStep_ = compInStep_ = 0.0f;
    companionActive_ = false;
    twinActive_ = false;
    twinIn_ = twinInStep_ = 0.0f;
    twinMean_ = twinSpread_ = -1.0f;
    twinFdn_.reset();
    twinPlate_.reset();
    for (auto& d : twinDiffuser_) d.clear();
    inputPower_ = 0.0f;
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
    plate_.reset();
    companion_.reset();
    reverse_.reset();
    muLaw_.reset();
    darken_.reset();
    limiter_.reset();
    duckEnv_ = duck_ = 0.0f;
    hissLp_[0] = hissLp_[1] = 0.0f;
    noiseSeed_ = 0xA11CEu;
    reverseMix_ = reverseTarget_;
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
    const float compStep = 1.0f / (voicing::kCompanionFadeSeconds * sampleRate_);
    const float tankStep = 1.0f / (voicing::kFreezeInputSeconds * sampleRate_);
    switch (e) {
    case GestureReader::Event::Freeze:
        // The big tank keeps the SIZE and DECAY it had; the knobs move to
        // the companion. Its input closes, its colour glides out.
        heldSize_ = smoothed(ParamId::Size);
        heldDecay_ = smoothed(ParamId::Decay);
        padFloorDb_ = 0.0f; // no cap until the pad has settled (controlTick)
        padCaptured_ = false;
        frozeAt_ = clock_;
        fdn_.resyncHoldLevel();
        plate_.resyncHoldLevel();
        // The tank's input closes, or (voicings 1, 2) drops to the feed level so playing goes into the pad.
        tankInTarget_ = freezeVoicing_ == 0 || freezeVoicing_ == 3 ? 0.0f : voicing::kFreezeFeed;
        tankInStep_ = tankStep;
        if (freezeVoicing_ == 3) {
            if (!twinActive_) {
                twinFdn_.reset();
                twinPlate_.reset();
                for (auto& d : twinDiffuser_) d.clear();
                twinMean_ = twinSpread_ = -1.0f;
                twinActive_ = true;
            }
            twinInStep_ = compStep;
        } else if (freezeVoicing_ != 1) {
            if (!companionActive_) {
                companion_.reset();
                companionActive_ = true;
            }
            compInStep_ = compStep;
        }
        break;
    case GestureReader::Event::Unfreeze:
        tankInTarget_ = 1.0f;
        tankInStep_ = tankStep;
        compInStep_ = -compStep;
        twinInStep_ = -compStep;
        break;
    case GestureReader::Event::PaintStart:
        // Room to grow while painting: the pad's level or the floor, + 3 dB.
        padFloorDb_ = (tankSlowLevelDb() > voicing::kFloorDbfs ? tankSlowLevelDb() : voicing::kFloorDbfs) + voicing::kPaintHeadroomDb;
        break;
    case GestureReader::Event::PaintStop:
        padCaptured_ = false; // the painted pad's level becomes its new ceiling
        break;
    case GestureReader::Event::None:
        break;
    }
}

// ---- Voicing -------------------------------------------------------------------

// Sets every voicing target for the SPACE in use (activeSpace_). While
// frozen, the big tank keeps the SIZE and DECAY it froze with and its loss
// glides to none (freezeMix_); the knobs play the companion. snap: jump
// straight there (reset, a SPACE flip).
void Engine::applySpace(bool snap)
{
    const bool frozen = gesture_.frozen();
    const float size = frozen ? heldSize_ : smoothed(ParamId::Size);
    const float decay = frozen ? heldDecay_ : smoothed(ParamId::Decay);
    const float time = smoothed(ParamId::Time), tone = smoothed(ParamId::Tone);
    const int attack = normalisedToSwitch(smoothed(ParamId::Attack));
    const bool massive = activeSpace_ == 2, plate = activeSpace_ == 0;

    // Freezing: loss → 0, highs kept (ratio → 1), the governor caps paint.
    // ERA's darkening scales the high end's T60 share with TONE (Era.h).
    const float toneRatio = map::toneHighT60Ratio(tone) * map::eraMix(smoothed(ParamId::Era)).highRatioScale;
    const float rate = map::decayRate(decay, activeSpace_) * (1.0f - freezeMix_);
    const float highRatio = toneRatio + freezeMix_ * (1.0f - toneRatio);
    // Frozen: full weight (only the pad's own hold acts, there's no floor). Not frozen: DECAY's own
    // weight at once, not glided with freezeMix_: gliding it pulled a fed pad (above the floor) down
    // hard on unfreeze, "a pronounced immediate drop" and a too-steep decay (owner, freeze options).
    const float governor = frozen ? 1.0f : map::governorWeight(decay);
    // The cap: a still pad's own level (or + headroom while painting); an
    // infinite tail's settled level once the input has gone quiet; else the
    // floor. Darkening always stops at the floor (ADR 0008).
    const float floorDb = frozen ? 0.0f : voicing::kFloorDbfs; // a pad isn't pulled to the floor
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
        plate_.setHoldDb(holdDb);
    } else {
        const float mean = massive ? map::massiveMeanSeconds(size) : map::hallMeanSeconds(size);
        const float spread = massive ? map::massiveSpread(size) : 1.0f;
        if (snap || std::fabs(mean - lastMean_) > 1e-6f || std::fabs(spread - lastSpread_) > 1e-6f) {
            lastMean_ = mean;
            lastSpread_ = spread;
            fdn_.setMeanSeconds(mean, spread, snap);
        }
        fdn_.setDecayRate(rate);
        fdn_.setHighT60Ratio(highRatio);
        fdn_.setGovernor(governor);
        fdn_.setFloorDb(floorDb);
        fdn_.setHoldDb(holdDb);
        // A frozen MASSIVE goes to full in-loop diffusion: its long lines otherwise replay painted
        // material as audible repeats (owner, M5 listening).
        const float diff = massive ? map::massiveDiffusion(size) : 0.0f;
        fdn_.setInLoopDiffusion(massive ? diff + freezeMix_ * (1.0f - diff) : 0.0f);
        fdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim : 1.0f);
        fdn_.setEarlyTaps(massive ? 1.0f : 0.0f);
    }

    // ERA in the loop and DRIFT (SPEC §4.6, §4.7). Freezing glides the
    // in-loop colour out (the pad holds still, ADR 0009); DRIFT stays.
    const dsp::EraMix era = map::eraMix(smoothed(ParamId::Era));
    dsp::EraMix loopEra = era;
    const float keep = 1.0f - freezeMix_;
    loopEra.midiRound = map::governorWeight(decay) >= 1.0f; // infinite: no systematic loss
    // Neve's even harmonics add energy in proportion to the level: harmless
    // while DECAY loses energy, a runaway in a lossless loop (measured: the
    // tail jumping 2-3 dB, then the governor overshooting into dips). So
    // they fade out across DECAY's infinite crossover.
    loopEra.even *= keep * (1.0f - map::governorWeight(decay));
    loopEra.drive = 1.0f + keep * (era.drive - 1.0f);
    loopEra.midiEngage *= keep;
    const dsp::DriftAmounts drift = map::driftAmounts(smoothed(ParamId::Drift), era.cassette);
    // The cassette's slow 2:1 compression: more loss above −18 dBFS.
    const float over = tankLevelDb() - voicing::kCompressThresholdDb;
    const float compress = over > 0.0f ? keep * era.cassette * voicing::kCompressRatePerDb * over : 0.0f;
    if (plate) {
        plate_.setEra(loopEra);
        plate_.setDrift(drift);
        plate_.setExtraRate(compress);
    } else {
        fdn_.setEra(loopEra);
        fdn_.setDrift(drift);
        fdn_.setExtraRate(compress);
    }
    muLaw_.set(era);
    eraMakeup_ = era.makeup;

    // Ducking (ADR 0014): how loud the input is, × how far into infinite
    // DECAY is. Not while frozen (playing over a pad mustn't darken it).
    const float envDb = 20.0f * std::log10(duckEnv_ + 1e-10f);
    const float loud = map::clamp01((envDb - voicing::kDuckFromDbfs) / (voicing::kDuckFullDbfs - voicing::kDuckFromDbfs));
    duck_ = frozen ? 0.0f : loud * map::governorWeight(smoothed(ParamId::Decay));
    darken_.set(duck_);
    // Hiss follows the input (none in silence, so an infinite tail doesn't
    // fill with noise, ADR 0005); crackle the same.
    const float inDb = 10.0f * std::log10(inputPower_ + 1e-20f);
    const float follow = map::clamp01((inDb - (voicing::kHissFollowFromDbfs - 30.0f)) / 30.0f);
    hissGain_ = era.cassette * follow * std::pow(10.0f, voicing::kHissDbfs / 20.0f);
    crackleChance_ = era.midiverb * follow * voicing::kCracklePerSecond / sampleRate_;
    crackleGain_ = std::pow(10.0f, voicing::kCrackleDbfs / 20.0f);

    const float diffusion = massive ? map::massiveDiffusion(size) : 1.0f;
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 4; ++k) diffuser_[c].stage[k].coeff = voicing::kDiffuserCoeff[k] * diffusion;

    // ATTACK (ADR 0015): what TIME means, and the swell stage's attack.
    float predelay = massive ? map::massivePredelaySeconds(size) : 0.0f;
    float attackSeconds = massive ? map::massiveNormalAttackSeconds(decay, size) : 0.0f;
    if (attack == 0) predelay += map::normalPredelaySeconds(time);
    if (attack == 1) attackSeconds = map::bloomSeconds(time) * (massive ? map::massiveMass(size) : 1.0f);
    swell_.setAttackSeconds(attackSeconds, snap);
    reverse_.setSegmentSeconds(map::reverseSeconds(time));
    reverseTarget_ = attack == 2 && !frozen ? 1.0f : 0.0f; // REVERSE rests while frozen (ADR 0015)

    const float pd = predelay * sampleRate_;
    if (snap) predelayNow_ = pd;
    predelayTarget_ = pd;

    // The twin (voicing 3): the frozen tank's space and size and the knobs' colour, never frozen,
    // decaying at the held DECAY but always within kTwinMaxT60. Full in-loop diffusion in MASSIVE,
    // like the pad (no audible repeats).
    if (twinActive_) {
        const float tSize = heldSize_;
        const float tRate = std::fmax(map::decayRate(heldDecay_, activeSpace_), 1.0f / voicing::kTwinMaxT60);
        dsp::EraMix tEra = era;
        tEra.even *= 1.0f - map::governorWeight(heldDecay_);
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
            twinFdn_.setHighT60Ratio(toneRatio);
            twinFdn_.setGovernor(0.0f);
            twinFdn_.setInLoopDiffusion(massive ? 1.0f : 0.0f);
            twinFdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim : 1.0f);
            twinFdn_.setEarlyTaps(massive ? 1.0f : 0.0f);
            twinFdn_.setEra(tEra);
            twinFdn_.setDrift(drift);
        }
    }

    // The companion: SIZE and DECAY's while frozen (ADR 0009).
    companion_.setScale(map::companionScale(smoothed(ParamId::Size)), snap);
    companion_.setDecayRate(map::companionDecayRate(smoothed(ParamId::Decay)));
    companion_.setHighT60Ratio(toneRatio);
    companion_.setGovernor(0.0f);
}

void Engine::controlTick(int n)
{
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
        padFloorDb_ = tankSlowLevelDb() + voicing::kPadCeilingDb;
        padCaptured_ = true;
    }
    // An infinite tail holds where it settled once the input goes quiet.
    if (10.0f * std::log10(inputPower_ + 1e-20f) > voicing::kQuietDbfs) {
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

    // SPACE flip (ADR 0013): fade out, restart the tank in the new voicing,
    // fade back in. Not while frozen: the pad waits for the unfreeze.
    const int space = normalisedToSwitch(smoothed(ParamId::Space));
    const float flipChunk = float(n) / (voicing::kSpaceFlipSeconds * sampleRate_);
    if (flipPhase_ == 0 && space != activeSpace_ && !gesture_.frozen() && freezeMix_ == 0.0f) flipPhase_ = 1;
    if (flipPhase_ == 1) {
        flipStep_ = -flipChunk / float(n);
        if (flipGain_ <= 0.0f) {
            activeSpace_ = space;
            fdn_.reset();
            plate_.reset();
            swell_.reset();
            reverse_.reset();
            applySpace(true);
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

    applySpace(false);
    swell_.control(n);
    predelayStep_ = (predelayTarget_ - predelayNow_) / float(n);

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
    for (int start = 0; start < numSamples; start += kChunk) {
        const int n = (numSamples - start) < kChunk ? (numSamples - start) : kChunk;
        handleEdges(start, start + n);
        controlTick(n);

        // Input side: predelay, diffusion; the swell feeds the big tank, the
        // diffused input feeds the companion.
        for (int i = 0; i < n; ++i) {
            flipGain_ = clamp01(flipGain_ + flipStep_);
            {   // toward the target: closed, the feed level, or (painting, voicings 1-2) all the way open
                const float target = tankInTarget_ + (freezeVoicing_ != 0 && gesture_.frozen() ? paint_ * (1.0f - tankInTarget_) : 0.0f);
                tankIn_ = tankIn_ < target ? std::fmin(target, tankIn_ + tankInStep_) : std::fmax(target, tankIn_ - tankInStep_);
            }
            compIn_ = clamp01(compIn_ + compInStep_);
            const float l = inL[start + i], r = inR[start + i];
            inputPower_ += 0.002f * (0.5f * (l * l + r * r) - inputPower_); // ~10 ms follower
            const float peak = l > 0.0f ? l : -l;
            duckEnv_ += (peak > duckEnv_ ? duckAttack_ : duckRelease_) * (peak - duckEnv_);
            predelay_[0].write(l);
            predelay_[1].write(r);
            predelayNow_ += predelayStep_;
            float pl = l, pr = r;
            if (predelayNow_ >= 2.0f) {
                pl = predelay_[0].readCubic(predelayNow_);
                pr = predelay_[1].readCubic(predelayNow_);
            }
            pl = diffuser_[0].process(pl);
            pr = diffuser_[1].process(pr);
            compInL_[i] = compIn_ * pl;
            compInR_[i] = compIn_ * pr;
            compL_[i] = compR_[i] = 0.0f;
            // The twin hears the input itself, through its own diffusion: no predelay, no swell.
            twinIn_ = clamp01(twinIn_ + twinInStep_);
            twinInL_[i] = twinDiffuser_[0].process(twinIn_ * l);
            twinInR_[i] = twinDiffuser_[1].process(twinIn_ * r);
            twinL_[i] = twinR_[i] = 0.0f;
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
            wetL_[i] = wetR_[i] = 0.0f;
        }

        // The companion first, so painting can send it into the pad.
        if (companionActive_) {
            companion_.process(compInL_, compInR_, compL_, compR_, n);
            if (!gesture_.frozen() && compIn_ == 0.0f && companion_.levelDb() < -100.0f) companionActive_ = false;
        }
        if (twinActive_) {
            if (activeSpace_ == 0) twinPlate_.process(twinInL_, twinInR_, twinL_, twinR_, n);
            else twinFdn_.process(twinInL_, twinInR_, twinL_, twinR_, n);
            const float lvl = activeSpace_ == 0 ? twinPlate_.levelDb() : twinFdn_.levelDb();
            if (!gesture_.frozen() && twinIn_ == 0.0f && lvl < -100.0f) twinActive_ = false;
            // Painting (holding) copies the twin into the pad: what you played becomes part of it.
            if (paint_ > 0.0f)
                for (int i = 0; i < n; ++i) {
                    bufL_[i] += paint_ * voicing::kPaintLevel * twinL_[i];
                    bufR_[i] += paint_ * voicing::kPaintLevel * twinR_[i];
                }
        }
        if (paint_ > 0.0f && freezeVoicing_ != 1 && freezeVoicing_ != 3)
            for (int i = 0; i < n; ++i) {
                bufL_[i] += paint_ * voicing::kPaintLevel * compL_[i];
                bufR_[i] += paint_ * voicing::kPaintLevel * compR_[i];
            }

        if (activeSpace_ == 0) plate_.process(bufL_, bufR_, wetL_, wetR_, n);
        else fdn_.process(bufL_, bufR_, wetL_, wetR_, n);

        for (int i = 0; i < n; ++i) {
            // REVERSE: always recording, crossfaded in and out over ~50 ms.
            float rl = wetL_[i], rr = wetR_[i];
            reverse_.process(rl, rr);
            reverseMix_ = approach(reverseMix_, reverseTarget_, reverseStep);
            const float cg = freezeVoicing_ == 2 ? voicing::kBothCompanionLevel : 1.0f;
            float wl = (wetL_[i] + reverseMix_ * (rl - wetL_[i])) * flipGain_ + cg * compL_[i] + voicing::kTwinLevel * twinL_[i];
            float wr = (wetR_[i] + reverseMix_ * (rr - wetR_[i])) * flipGain_ + cg * compR_[i] + voicing::kTwinLevel * twinR_[i];
            wl *= eraMakeup_;
            wr *= eraMakeup_;
            muLaw_.process(wl, wr); // ERA's μ-law output layer (the wet only)
            darken_.process(wl, wr);
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

} // namespace iv
