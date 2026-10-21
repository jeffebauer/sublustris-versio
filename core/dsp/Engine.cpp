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
    ready_ &= fdn_.prepare(large_, sampleRate, 0x1D5A7u);
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
    lastMean_ = map::hallMeanSeconds(smoothed(ParamId::Size));
    fdn_.setMeanSeconds(lastMean_, true);
    fdn_.setDecayRate(map::decayRate(smoothed(ParamId::Decay), 1));
    fdn_.setHighT60Ratio(map::toneHighT60Ratio(smoothed(ParamId::Tone)));
    fdn_.reset();
    predelayNow_ = map::normalPredelaySeconds(smoothed(ParamId::Time)) * sampleRate_;
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

    // SIZE: the FDN glides its lines itself; only retarget on a real change.
    const float mean = map::hallMeanSeconds(smoothed(ParamId::Size));
    if (std::fabs(mean - lastMean_) > 1e-6f) {
        lastMean_ = mean;
        fdn_.setMeanSeconds(mean);
    }
    fdn_.setDecayRate(map::decayRate(smoothed(ParamId::Decay), 1));
    fdn_.setHighT60Ratio(map::toneHighT60Ratio(smoothed(ParamId::Tone)));

    // TIME (NORMAL): predelay, gliding per sample across the chunk.
    const float pd = map::normalPredelaySeconds(smoothed(ParamId::Time)) * sampleRate_;
    predelayStep_ = (pd - predelayNow_) / float(n);

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
    for (int start = 0; start < numSamples; start += kChunk) {
        const int n = (numSamples - start) < kChunk ? (numSamples - start) : kChunk;
        controlTick(n);
        for (int i = 0; i < n; ++i) {
            const float l = inL[start + i], r = inR[start + i];
            predelay_[0].write(l);
            predelay_[1].write(r);
            predelayNow_ += predelayStep_;
            float pl = l, pr = r;
            if (predelayNow_ >= 2.0f) {
                pl = predelay_[0].readCubic(predelayNow_);
                pr = predelay_[1].readCubic(predelayNow_);
            }
            bufL_[i] = diffuser_[0].process(pl);
            bufR_[i] = diffuser_[1].process(pr);
            wetL_[i] = wetR_[i] = 0.0f;
        }
        fdn_.process(bufL_, bufR_, wetL_, wetR_, n);
        for (int i = 0; i < n; ++i) {
            dryNow_ += dryStep_;
            wetNow_ += wetStep_;
            outL[start + i] = dryNow_ * inL[start + i] + wetNow_ * wetL_[i];
            outR[start + i] = dryNow_ * inR[start + i] + wetNow_ * wetR_[i];
        }
    }
}

} // namespace iv
