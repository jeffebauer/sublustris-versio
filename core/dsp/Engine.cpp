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
    ready_ &= reverse_.prepare(large_, sampleRate);
    reset();
}

IV_SIZE_OPT void Engine::reset()
{
    buttonDown_ = false;
    gateHigh_   = false;
    frozen_     = false;
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
    reverse_.reset();
    reverseTarget_ = reverseMix_ = normalisedToSwitch(smoothed(ParamId::Attack)) == 2 ? 1.0f : 0.0f;
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

// Sets every voicing target from the smoothed knobs for the SPACE in use
// (activeSpace_). snap: jump straight there (reset, a SPACE flip).
void Engine::applySpace(bool snap)
{
    const float size = smoothed(ParamId::Size), decay = smoothed(ParamId::Decay), time = smoothed(ParamId::Time);
    const int attack = normalisedToSwitch(smoothed(ParamId::Attack));
    const bool massive = activeSpace_ == 2;
    const int t60Space = massive ? 2 : 1; // PLATE plays the Hall until M5

    const float mean = massive ? map::massiveMeanSeconds(size) : map::hallMeanSeconds(size);
    const float spread = massive ? map::massiveSpread(size) : 1.0f;
    if (snap || std::fabs(mean - lastMean_) > 1e-6f || std::fabs(spread - lastSpread_) > 1e-6f) {
        lastMean_ = mean;
        lastSpread_ = spread;
        fdn_.setMeanSeconds(mean, spread, snap);
    }
    fdn_.setDecayRate(map::decayRate(decay, t60Space));
    fdn_.setHighT60Ratio(map::toneHighT60Ratio(smoothed(ParamId::Tone)));
    fdn_.setGovernor(map::governorWeight(decay));
    fdn_.setInLoopDiffusion(massive ? map::massiveDiffusion(size) : 0.0f);
    fdn_.setOutputTrim(massive ? voicing::kMassiveOutputTrim : 1.0f);

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
    reverseTarget_ = attack == 2 ? 1.0f : 0.0f;

    const float pd = predelay * sampleRate_;
    if (snap) predelayNow_ = pd;
    predelayTarget_ = pd;
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

    // SPACE flip (ADR 0013): fade out over kFlipSeconds, restart the tank in
    // the new voicing, fade back in.
    const int space = normalisedToSwitch(smoothed(ParamId::Space));
    const float flipChunk = float(n) / (voicing::kSpaceFlipSeconds * sampleRate_);
    if (flipPhase_ == 0 && space != activeSpace_) flipPhase_ = 1;
    if (flipPhase_ == 1) {
        flipStep_ = -flipChunk / float(n);
        if (flipGain_ <= 0.0f) {
            activeSpace_ = space;
            fdn_.reset();
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
        return;
    }
    const float reverseStep = 1.0f / (voicing::kAttackFadeSeconds * sampleRate_);
    for (int start = 0; start < numSamples; start += kChunk) {
        const int n = (numSamples - start) < kChunk ? (numSamples - start) : kChunk;
        controlTick(n);
        for (int i = 0; i < n; ++i) {
            flipGain_ += flipStep_;
            flipGain_ = flipGain_ < 0.0f ? 0.0f : (flipGain_ > 1.0f ? 1.0f : flipGain_);
            const float l = inL[start + i] * flipGain_, r = inR[start + i] * flipGain_;
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
            swell_.process(pl, pr);
            bufL_[i] = pl;
            bufR_[i] = pr;
            wetL_[i] = wetR_[i] = 0.0f;
        }
        fdn_.process(bufL_, bufR_, wetL_, wetR_, n);
        for (int i = 0; i < n; ++i) {
            // REVERSE: always recording, crossfaded in and out over ~50 ms.
            float rl = wetL_[i], rr = wetR_[i];
            reverse_.process(rl, rr);
            reverseMix_ += reverseMix_ < reverseTarget_ ? reverseStep : (reverseMix_ > reverseTarget_ ? -reverseStep : 0.0f);
            reverseMix_ = reverseMix_ < 0.0f ? 0.0f : (reverseMix_ > 1.0f ? 1.0f : reverseMix_);
            const float wl = (wetL_[i] + reverseMix_ * (rl - wetL_[i])) * flipGain_;
            const float wr = (wetR_[i] + reverseMix_ * (rr - wetR_[i])) * flipGain_;
            dryNow_ += dryStep_;
            wetNow_ += wetStep_;
            outL[start + i] = dryNow_ * inL[start + i] + wetNow_ * wl;
            outR[start + i] = dryNow_ * inR[start + i] + wetNow_ * wr;
        }
    }
}

} // namespace iv
