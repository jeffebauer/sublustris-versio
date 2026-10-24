#pragma once
// The floor governor (ADR 0008), shared by every tank. It follows the wet's
// level over ~1.5 s and, at infinite DECAY (weight 1), adds loss in
// proportion to how far the tail sits above its floor, so an infinite tail
// fed for minutes settles at the floor instead of growing (every loop gain
// stays <= 1, SPEC §2.3 rule 2). darken() tells the tank how much of its
// infinite high-end fade to keep: all of it above the floor, none of it once
// the tail is down at the floor, so the tail stops darkening and holds.
#include "params/SpaceVoicing.h"

#include <cmath>

namespace iv::dsp {

struct FloorGovernor {
    float weight = 0.0f;                 // 0 finite DECAY .. 1 infinite
    float floorDb = voicing::kFloorDbfs;   // the level an infinite tail settles at (darkening stops there)
    float holdDb = 0.0f;                   // a held tail's ceiling (a pad, or infinite after the input), on the slow level
    float slowLevelDb = -200.0f;           // the level over ~kHoldSeconds (an echoey MASSIVE tail swings for seconds)
    float levelDb = -200.0f;
    float extraRate = 0.0f;              // added loss, 1/T60 in 1/s
    float darken = 1.0f;                 // share of the infinite high-end fade to keep

    void reset()
    {
        power_ = slow_ = block_ = fast_ = 0.0f;
        levelDb = slowLevelDb = -200.0f;
        extraRate = 0.0f;
        darken = 1.0f;
    }
    // The slow level starts over from the fast one (a fresh freeze: judge
    // the pad on its own level, not on the minutes before it).
    void resyncSlow()
    {
        slow_ = power_;
        slowLevelDb = levelDb;
    }
    // The level over the last ~50 ms, and the slow level starting over from it (a pad frozen from a
    // fast-dying tail: both slower levels still remember the tail, soak audit).
    float fastLevelDb() const { return 10.0f * std::log10(fast_ + 1e-20f); }
    void resyncSlowToFast()
    {
        slow_ = fast_;
        slowLevelDb = fastLevelDb();
    }
    // One output sample's power (mean of the channels' squares).
    void add(float power) { block_ += power; }

    // Once per control chunk of n samples.
    void control(int n, float sampleRate)
    {
        const float dt = float(n) / sampleRate;
        power_ += (1.0f - std::exp(-dt / voicing::kGovernorSeconds)) * (block_ / float(n) - power_);
        slow_ += (1.0f - std::exp(-dt / voicing::kHoldSeconds)) * (block_ / float(n) - slow_);
        fast_ += (1.0f - std::exp(-dt / voicing::kGovernorFallSeconds)) * (block_ / float(n) - fast_);
        block_ = 0.0f;
        levelDb = 10.0f * std::log10(power_ + 1e-20f);
        slowLevelDb = 10.0f * std::log10(slow_ + 1e-20f);
        // The floor judges a falling tail on a faster level. Judged on the 1.5 s level alone, a fed
        // tail sitting above the floor kept its full loss for seconds after the input stopped or
        // faded, while the level caught up: it plunged 10-64 dB below the floor and the hold's
        // makeup took minutes to bring it back (soak audit). A steady fed tail measures the same.
        float floorLevelDb = levelDb;
        {
            const float fastDb = fastLevelDb();
            if (fastDb < floorLevelDb) floorLevelDb = fastDb;
        }
        const float over = floorLevelDb - floorDb;
        // The floor: loss in proportion to how far a fed tail is over it.
        extraRate = over > 0.0f ? weight * voicing::kGovernorRatePerDb * over : 0.0f;
        // A held tail (a pad, or an infinite tail once the input is quiet)
        // is kept at its own settled level (SPEC v1.0.3, ADR 0008
        // amendment): above it, loss in proportion (as the floor); below
        // it, a gentle capped makeup, because fast DRIFT or the cassette's
        // wow and flutter (moving allpass reads) lose a little energy.
        if (holdDb < 0.0f) {
            const float off = slowLevelDb - holdDb;
            if (off > 0.0f) extraRate += weight * voicing::kGovernorRatePerDb * off;
            else {
                float up = -voicing::kHoldRatePerDb * off;
                if (up > voicing::kMakeupMaxRate) up = voicing::kMakeupMaxRate;
                extraRate -= weight * up;
            }
        }
        float above = (floorLevelDb - (floorDb - 3.0f)) / 6.0f;
        above = above < 0.0f ? 0.0f : (above > 1.0f ? 1.0f : above);
        darken = 1.0f - weight * (1.0f - above);
    }

private:
    float power_ = 0.0f, slow_ = 0.0f, block_ = 0.0f, fast_ = 0.0f;
};

// A tank segment's absorbent filter (Jot): a first-order shelf crossing at
// kLossShelfHz, y = gain·x + b1·x' + pole·y. Its DC gain is the low end's loss
// for `seconds` of delay at `lowRate` (1/T60), its Nyquist gain the high end's
// at lowRate / highRatio, so the upper mids fade with the highs, not the body
// (the audit's A1; owner's pick on renders/tone_shelf, v1.0.15; Jot and
// Zita-Rev1 do the same). At infinite (lowRate 0) the highs still fade over
// kInfiniteHighT60Seconds, scaled by darken. shelfK: lossShelfK(sampleRate).
struct Absorb {
    float gain, pole, b1 = 0.0f;
};

// A bilinear filter's K = tan(π·hz / sampleRate), once per prepare. A Taylor
// series rather than std::tan (~5.8 KB of flash): fine for angles under ~0.2
// rad (the loss shelf from 44.1 kHz up), where three terms are within 1e-6.
inline float bilinearK(float hz, float sampleRate)
{
    const float x = 3.14159265f * hz / sampleRate, x2 = x * x;
    return x * (1.0f + x2 * (1.0f / 3.0f + x2 * (2.0f / 15.0f)));
}
inline float lossShelfK(float sampleRate) { return bilinearK(voicing::kLossShelfHz, sampleRate); }

// air (1/T60 in 1/s; v1.0.25): the highs lose this much more than the lows at every finite DECAY, as
// air does in a room, scaled by TONE's ratio against noon. 0 = a fixed share of the decay (MASSIVE).
inline Absorb absorbent(float seconds, float lowRate, float highRatio, float darken, float shelfK, float air = 0.0f)
{
    float highRate = air > 0.0f && lowRate > 0.0f ? lowRate + air * (voicing::kDampingHighT60Ratio / highRatio)
                                                  : lowRate / highRatio;
    const float infiniteHigh = darken * (1.0f / highRatio - 1.0f) / voicing::kInfiniteHighT60Seconds;
    if (highRate < infiniteHigh) highRate = infiniteHigh;
    const float gLow  = std::pow(10.0f, -3.0f * seconds * lowRate);
    const float gHigh = std::pow(10.0f, -3.0f * seconds * highRate);
    const float K = shelfK;
    return {(gHigh + gLow * K) / (1.0f + K), (1.0f - K) / (1.0f + K), (gLow * K - gHigh) / (1.0f + K)};
}

} // namespace iv::dsp
