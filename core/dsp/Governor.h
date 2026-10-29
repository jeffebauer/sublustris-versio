#pragma once
// The floor governor (ADR 0008), shared by every tank. It follows the wet's
// level over ~1.5 s and, at infinite DECAY (weight 1), adds loss in
// proportion to how far the tail sits above its floor, so an infinite tail
// fed for minutes settles at the floor instead of growing (every loop gain
// stays <= 1, SPEC §2.3 rule 2). darken() tells the tank how much of its
// infinite high-end fade to keep: all of it above the floor, none of it once
// the tail is down at the floor, so the tail stops darkening and holds.
#include "dsp/ProfileHook.h"
#include "dsp/SizeOpt.h"
#include "params/SpaceVoicing.h"

#include <cmath>

namespace iv::dsp {

struct FloorGovernor {
    float weight IV_ZI;                 // 0 finite DECAY .. 1 infinite
    float floorDb = voicing::kFloorDbfs;   // the level an infinite tail settles at (darkening stops there)
    float holdDb IV_ZI;                   // a held tail's ceiling (a pad, or infinite after the input), on the slow level
    float slowLevelDb = -200.0f;           // the level over ~kHoldSeconds (an echoey MASSIVE tail swings for seconds)
    float levelDb = -200.0f;
    float extraRate IV_ZI;              // added loss, 1/T60 in 1/s
    float holdRate IV_ZI;               // the hold's share of extraRate (the still lows leave it out, Fdn::control)
    float darken = 1.0f;                 // share of the infinite high-end fade to keep

    void reset()
    {
        power_ = slow_ = block_ = fast_ = 0.0f;
        levelDb = slowLevelDb = -200.0f;
        extraRate = holdRate = 0.0f;
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
    // A chunk not judged (a low tank's: it follows its leader's, Fdn::control): its power dropped.
    void skip() { block_ = 0.0f; }

    // Once per control chunk of n samples (built for size: its three log10f are its cost).
    IV_SIZE_OPT void control(int n, float sampleRate)
    {
        const float dt = float(n) / sampleRate;
        // Worked out once for each of the two chunk lengths (std::exp ~110 cycles on the M7): 32 and 16 on the module.
        int k = dt == dt_[0] ? 0 : 1;
        if (dt != dt_[k]) {
            dt_[1] = dt_[0];
            for (int j = 0; j < 3; ++j) c_[1][j] = c_[0][j];
            k = 0;
            dt_[0] = dt;
            c_[0][0] = 1.0f - std::exp(-dt / voicing::kGovernorSeconds);
            c_[0][1] = 1.0f - std::exp(-dt / voicing::kHoldSeconds);
            c_[0][2] = 1.0f - std::exp(-dt / voicing::kGovernorFallSeconds);
        }
        power_ += c_[k][0] * (block_ / float(n) - power_);
        slow_ += c_[k][1] * (block_ / float(n) - slow_);
        fast_ += c_[k][2] * (block_ / float(n) - fast_);
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
        const float floorRate = extraRate;
        if (holdDb < 0.0f) {
            const float off = slowLevelDb - holdDb;
            if (off > 0.0f) extraRate += weight * voicing::kGovernorRatePerDb * off;
            else {
                float up = -voicing::kHoldRatePerDb * off;
                if (up > voicing::kMakeupMaxRate) up = voicing::kMakeupMaxRate;
                extraRate -= weight * up;
            }
        }
        holdRate = extraRate - floorRate;
        float above = (floorLevelDb - (floorDb - 3.0f)) / 6.0f;
        above = above < 0.0f ? 0.0f : (above > 1.0f ? 1.0f : above);
        darken = 1.0f - weight * (1.0f - above);
    }

private:
    float power_ IV_ZI, slow_ IV_ZI, block_ IV_ZI, fast_ IV_ZI;
    float dt_[2] IV_ZI, c_[2][3] IV_ZI; // the smoothing coefficients (power, slow, fast) for dt_[k] (dt is never 0)
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
// flat (1/T60 in 1/s; v1.0.58): the floor governor's correction, a flat gain, the same at the highs as at the
// lows; top: the least the highs lose, an endless tail's settling mellow (Engine). See absorbShared().
inline Absorb absorbent(float seconds, float lowRate, float highRatio, float darken, float shelfK, float air = 0.0f,
                        float flat = 0.0f, float top = 0.0f);

// absorbent() for one line, worked out again only when one of its inputs has changed: its two powf
// calls (~210 cycles each on the M7, M3 round 6) ran for every line every block, where most blocks
// ask the same. The same inputs give the same coefficients, to the bit.
struct AbsorbMemo {
    float  in[8] = {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    Absorb out{};
    IV_NOINLINE IV_SIZE_OPT const Absorb& get(float seconds, float lowRate, float highRatio, float darken, float shelfK, float air,
                                              float flat, float top)
    {
        const float x[8] = {seconds, lowRate, highRatio, darken, shelfK, air, flat, top};
        bool same = true;
        for (int k = 0; k < 8; ++k) same = same && x[k] == in[k];
        if (!same) {
            for (int k = 0; k < 8; ++k) in[k] = x[k];
            out = absorbent(seconds, lowRate, highRatio, darken, shelfK, air, flat, top);
        }
        return out;
    }
};

// One-argument maths (std::exp and the like) worked out again only when its argument changes. Two
// arguments kept (CPU hunt, 28 Oct 2026): the module's 48-sample blocks run as control chunks of 32 and
// 16, so a time step's argument alternated and a one-place memo missed every time.
struct Memo1 {
    float x IV_ZI, y IV_ZI, x2 IV_ZI, y2 IV_ZI; // (never asked for 0: a time step)
    // One shared copy (flash): f a plain function (a lambda without captures).
    IV_NOINLINE IV_SIZE_OPT float get(float arg, float (*f)(float))
    {
        if (arg == x) return y;
        if (arg == x2) return y2;
        x2 = x;
        y2 = y;
        x = arg;
        y = f(arg);
        return y;
    }
};

// absorbent() in two halves (CPU save 2, 28 Oct 2026): what every line of a tank shares (the rates, the shelf's
// constants: four divides), then each line's own (its length's two std::pow). The same operations: the same bits.
struct AbsorbShared {
    float lowRate, highRate, k, onePlusK, pole;
};
// lowRate is DECAY's own loss; TONE's ratio and the air shape it alone. The floor governor's correction (flat: its
// loss above the floor or a held tail's ceiling, the hold's make-up below it) comes on top as a flat gain, the same
// at the highs as at the lows (v1.0.58, ADR 0008 amended). Until then it rode inside lowRate: any loss at all from it
// switched the air on (air > 0 && lowRate > 0), several dB a second of highs, while its make-up (lowRate < 0) lifted
// the lows alone (the highs clamped at infiniteHigh), so an endless tail held round its level lost its highs and
// gained its lows for minutes: the top end drained and a few low resonances were left ringing.
// top (1/T60 in 1/s): the highs lose at least this much (an endless tail's settling mellow, Engine::applySpace);
// where TONE's or the infinite fade's own high loss is larger (TONE noon and below), that one stands.
IV_INLINE AbsorbShared absorbShared(float lowRate, float highRatio, float darken, float shelfK, float air, float flat = 0.0f,
                                    float top = 0.0f)
{
    float highRate = air > 0.0f && lowRate > 0.0f ? lowRate + air * (voicing::kDampingHighT60Ratio / highRatio)
                                                  : lowRate / highRatio;
    const float infiniteHigh = darken * (1.0f / highRatio - 1.0f) / voicing::kInfiniteHighT60Seconds;
    if (highRate < infiniteHigh) highRate = infiniteHigh;
    if (highRate < top) highRate = top;
    const float K = shelfK;
    return {lowRate + flat, highRate + flat, K, 1.0f + K, (1.0f - K) / (1.0f + K)};
}
IV_INLINE Absorb absorbLine(float seconds, const AbsorbShared& a)
{
#if defined(IV_ABSORB_EXP) || defined(IV_PROFILE_HOOKS) || !defined(IV_FIXED_VOICINGS)
#if !defined(IV_ABSORB_EXP)
    if (!prof::absExp) { // the profile's A / B (and the Renderer's IV_ABSORB_EXP): as shipped
        const float gLow = std::pow(10.0f, -3.0f * seconds * a.lowRate), gHigh = std::pow(10.0f, -3.0f * seconds * a.highRate);
        return {(gHigh + gLow * a.k) / a.onePlusK, a.pole, (gLow * a.k - gHigh) / a.onePlusK};
    }
#endif
    const float gLow  = std::exp(2.30258509f * (-3.0f * seconds * a.lowRate));
    const float gHigh = std::exp(2.30258509f * (-3.0f * seconds * a.highRate));
#else
    const float gLow  = std::pow(10.0f, -3.0f * seconds * a.lowRate);
    const float gHigh = std::pow(10.0f, -3.0f * seconds * a.highRate);
#endif
    const float K = a.k;
    return {(gHigh + gLow * K) / a.onePlusK, a.pole, (gLow * K - gHigh) / a.onePlusK};
}
inline Absorb absorbent(float seconds, float lowRate, float highRatio, float darken, float shelfK, float air, float flat, float top)
{
    return absorbLine(seconds, absorbShared(lowRate, highRatio, darken, shelfK, air, flat, top));
}

} // namespace iv::dsp
