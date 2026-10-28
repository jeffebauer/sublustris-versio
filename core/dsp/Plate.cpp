#include "dsp/Plate.h"
#include "dsp/ProfileHook.h"

#include "dsp/SizeOpt.h"

#include <cmath>

namespace iv::dsp {

namespace {

using namespace plate;

constexpr float kDecayDiffusion1 = 0.70f; // the modulated allpasses (drawn with inverted signs)
constexpr float kDecayDiffusion2 = 0.50f;
constexpr float kLfoHz[2] = {0.71f, 0.93f};
// Output taps (his Table 2): which branch, which element, offset (samples at
// 29.761 kHz), sign. Element 0 = d1, 1 = ap2's delay, 2 = d2. Each output
// starts from the *other* branch: the left from branch 1 (his 4217 / 2656 /
// 3163 lines), the right from branch 0 (4453 / 1800 / 3720). Every offset
// must fit inside its line (checked below): a tap past the end read off the
// buffer at large SIZE and clicked once a lap (owner, M5 listening).
struct Tap {
    int   branch, element;
    float offset, sign;
};
// The cross-branch taps (the last three of each side) are added, not subtracted as in his table
// (v1.0.25, owner's pick on renders/valhalla_round2). With true-stereo input the branches are
// independent, so his signs made L ≈ 2·b1 − 3·b0 and R ≈ 2·b0 − 3·b1 where the taps cohere (the lows):
// left and right lows nearly mirror images (vibes −0.86 correlation below 250 Hz; Valhalla's plates
// +0.1 to +0.45). Added, the lows centre (vibes +0.14) and the highs stay wide.
constexpr int kTapsPerSide = 7;
constexpr Tap kLeft[kTapsPerSide]  = {{1, 0, 266, 1},  {1, 0, 2974, 1}, {1, 1, 1913, -1}, {1, 2, 1996, 1},
                           {0, 0, 1990, 1}, {0, 1, 187, 1}, {0, 2, 1066, 1}};
constexpr Tap kRight[kTapsPerSide] = {{0, 0, 353, 1},  {0, 0, 3627, 1}, {0, 1, 1228, -1}, {0, 2, 2673, 1},
                           {1, 0, 2111, 1}, {1, 1, 335, 1}, {1, 2, 121, 1}};
constexpr float elementLength(const Tap& t)
{
    return t.element == 0 ? kD1[t.branch] : (t.element == 1 ? kAp2[t.branch] : kD2[t.branch]);
}
constexpr bool tapsFit(const Tap* taps)
{
    for (int i = 0; i < 7; ++i)
        if (taps[i].offset + 2.0f >= elementLength(taps[i])) return false;
    return true;
}
static_assert(tapsFit(kLeft) && tapsFit(kRight), "every Plate output tap must lie inside its line");
constexpr float kOutputGain = 0.6f;     // Dattorro's output scaling
constexpr float kLevelRefScale = 1.0f;  // level compensation reference (√(scale / ref))
// SIZE glides: the longest element (4453) changes by at most a semitone of
// bend, 0.0595 s of delay per second (as the FDN, ADR 0012).
constexpr float kScaleSlewPerSecond = 0.0595f / (4453.0f / kPaperRate);

float uniform(uint32_t& s)
{
    s = s * 1664525u + 1013904223u;
    return float(int32_t(s)) * (1.0f / 2147483648.0f);
}

// The output taps' offsets as one table (left, right), for tapDelays.
struct TapOffsets {
    float v[2][kTapsPerSide];
};
constexpr TapOffsets tapOffsets()
{
    TapOffsets o{};
    for (int j = 0; j < kTapsPerSide; ++j) {
        o.v[0][j] = kLeft[j].offset;
        o.v[1][j] = kRight[j].offset;
    }
    return o;
}
constexpr TapOffsets kTapOffsets = tapOffsets();

// The output taps' delays for a block (k, the scale, only moves in control()): whole samples and the
// fraction, as the per-sample read worked them out (the same bits; overnight review, 26 Oct 2026).
// Once a block, built for size (flash).
IV_NOINLINE IV_SIZE_OPT void tapDelays(float k, int32_t (&di)[2][kTapsPerSide], float (&t)[2][kTapsPerSide])
{
    for (int c = 0; c < 2; ++c)
        for (int j = 0; j < kTapsPerSide; ++j) {
            const float d = 1.0f + kTapOffsets.v[c][j] * k;
            di[c][j] = int32_t(d);
            t[c][j] = d - float(di[c][j]);
        }
}

// The reads while a ring is still filling (after a reset, until it has gone round once) go through
// at(), out of line: one small copy, so the per-sample loop carries only the full ring's reads (flash).
IV_NOINLINE float atFilling(const ExactLine& l, int32_t i) { return l.at(i); }

// An output tap: one shared copy (14 calls a sample), the full ring's read first.
IV_NOINLINE float tapRead(const ExactLine& l, int32_t di, float t)
{
    if (l.full()) return l.lerpFull(di, t);
    const int32_t i = int32_t(l.pos) - di;
    const float   a = atFilling(l, i);
    return a + t * (atFilling(l, i - 1) - a);
}

// As ExactLine::readAllpass, while the ring is filling.
IV_NOINLINE float allpassFilling(const ExactLine& l, float d, float& state)
{
    const int32_t di  = int32_t(d - 0.5f);
    const float   f   = d - float(di);
    const float   eta = (1.0f - f) / (1.0f + f);
    const int32_t i   = int32_t(l.pos) - di;
    state = atFilling(l, i - 1) + eta * (atFilling(l, i) - state);
    return state;
}

} // namespace

IV_SIZE_OPT bool Plate::prepare(Arena& arena, float sampleRate, float maxScale, bool colour, uint32_t seed)
{
    sampleRate_ = sampleRate;
    shelfK_ = lossShelfK(sampleRate);
    maxScale_ = maxScale;
    colour_ = colour;
    const float room = colour ? voicing::kDriftMaxSeconds * sampleRate : 0.0f;
    const float k = maxScale * sampleRate / kPaperRate;
    ready_ = true;
    for (int b = 0; b < 2; ++b) {
        Branch& br = b_[b];
        // The excursion isn't scaled (the chorus depth stays put), so it's
        // added on top of the scaled length.
        ready_ &= br.ap1.init(arena, size_t(kAp1[b] * k + 2.0f * kExcursion * sampleRate / kPaperRate + 2.0f * room) + 4);
        br.era.prepare(sampleRate);
        br.drift.prepare(sampleRate, (seed + 1u) * 2246822519u + uint32_t(b) * 3266489917u, float(b));
        const float longRoom = 2.0f * voicing::kPlateLongModSeconds * sampleRate;
        ready_ &= br.d1.init(arena, size_t(kD1[b] * k + longRoom) + 4);
        ready_ &= br.ap2.init(arena, size_t(kAp2[b] * k) + 4);
        ready_ &= br.d2.init(arena, size_t(kD2[b] * k + longRoom) + 4);
        br.lfoRate = kLfoHz[b];
    }
    rng_ = seed ? seed : 1u;
    setScale(1.0f > maxScale ? maxScale : 1.0f, true);
    reset();
    return ready_;
}

IV_SIZE_OPT void Plate::reset()
{
    for (int b = 0; b < 2; ++b) {
        Branch& br = b_[b];
        br.ap1.clear();
        br.d1.clear();
        br.ap2.clear();
        br.d2.clear();
        br.y1 = br.y2 = br.x1 = br.x2 = br.out = 0.0f;
        br.i1 = br.i2 = br.i3 = br.i4 = 0.0f;
        br.era.reset();
        br.drift.reset();
        br.lfoPhase = 0.5f * float(b);
        br.floorA = br.floorB = br.floorTarget = br.floorCountdown = 0.0f;
        br.modNow = br.modStep = 0.0f;
        for (int j = 0; j < 2; ++j) {
            br.longPhase[j] = 0.25f * float(2 * b + j);
            br.longFloorA[j] = br.longFloorB[j] = br.longTarget[j] = br.longCountdown[j] = 0.0f;
            br.longNow[j] = br.longStep[j] = 0.0f;
        }
    }
    scaleNow_ = scaleTarget_;
    governor_.reset();
}

IV_SIZE_OPT void Plate::setScale(float scale, bool snap)
{
    if (scale > maxScale_) scale = maxScale_;
    if (scale < 0.05f) scale = 0.05f;
    scaleTarget_ = scale;
    if (snap) scaleNow_ = scale;
}

IV_SIZE_OPT void Plate::control(int n) // once per block: built for size (flash; it shares Fdn::control's helpers)
{
    const float dt = float(n) / sampleRate_;
    const float slew = kScaleSlewPerSecond * dt;
    const float diff = scaleTarget_ - scaleNow_;
    scaleNow_ += diff > slew ? slew : (diff < -slew ? -slew : diff);
    const float k = scaleNow_ * sampleRate_ / kPaperRate;

    governor_.control(n, sampleRate_);
    const float lowRate = decayRate_ * voicing::kPlateRateCalibration + governor_.extraRate + extraRate_;
    levelGain_ = voicing::kPlateOutputTrim * std::sqrt(scaleNow_ / kLevelRefScale);

    const float excursion = kExcursion * sampleRate_ / kPaperRate; // not scaled: the chorus depth stays put
    const float floorCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 0.3f * dt);
    for (int b = 0; b < 2; ++b) {
        Branch& br = b_[b];
        br.ap1Len = kAp1[b] * k;
        br.d1Len = kD1[b] * k;
        br.ap2Len = kAp2[b] * k;
        br.d2Len = kD2[b] * k;
        br.loss1 = br.memo1.get((br.ap1Len + br.d1Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfK_, air_);
        br.loss2 = br.memo2.get((br.ap2Len + br.d2Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfK_, air_);

        // The modulated allpass: Dattorro's slow sine, plus the modulation
        // floor's slow random wander (as every tank's, SPEC §2.3 rule 1).
        br.lfoPhase += br.lfoRate * dt;
        if (br.lfoPhase >= 1.0f) br.lfoPhase -= 1.0f;
        br.floorCountdown -= dt;
        if (br.floorCountdown <= 0.0f) {
            br.floorTarget = uniform(rng_);
            br.floorCountdown = 1.9f * (0.8f + 0.2f * uniform(rng_)) * (1.0f + 0.3f * float(b));
        }
        br.floorA += floorCoeff * (br.floorTarget - br.floorA);
        br.floorB += floorCoeff * (br.floorA - br.floorB);
        float target = excursion * (1.0f + modScale_ * (modDepth_ * 0.7f * std::sin(6.2831853f * br.lfoPhase) + 0.3f * br.floorB));
        if (colour_) {
            // DRIFT swings the modulated allpass's length (the buffer has
            // room for the longest swing).
            const float room = voicing::kDriftMaxSeconds * sampleRate_;
            float cap = voicing::kDriftMaxShareOfLine * br.ap1Len;
            if (cap > room) cap = room;
            target += br.drift.control(dt, drift_, cap);
        }
        br.modStep = (target - br.modNow) / float(n);

        // d1 and d2 wander too, each at its own rate (a sine plus the floor's random walk),
        // about 2 cents; still on a frozen pad (longMod_ 0).
        const float longDepth = longMod_ * modDepth_ * modScale_ * voicing::kPlateLongModSeconds * sampleRate_;
        for (int j = 0; j < 2; ++j) {
            br.longPhase[j] += voicing::kPlateLongModHz[2 * b + j] * dt;
            if (br.longPhase[j] >= 1.0f) br.longPhase[j] -= 1.0f;
            br.longCountdown[j] -= dt;
            if (br.longCountdown[j] <= 0.0f) {
                br.longTarget[j] = uniform(rng_);
                br.longCountdown[j] = 2.3f * (0.8f + 0.2f * uniform(rng_)) * (1.0f + 0.2f * float(2 * b + j));
            }
            br.longFloorA[j] += floorCoeff * (br.longTarget[j] - br.longFloorA[j]);
            br.longFloorB[j] += floorCoeff * (br.longFloorA[j] - br.longFloorB[j]);
            const float t = longDepth * (1.0f + 0.6f * std::sin(6.2831853f * br.longPhase[j]) + 0.4f * br.longFloorB[j]);
            br.longStep[j] = (t - br.longNow[j]) / float(n);
        }
    }
}

void Plate::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (!ready_) return;
    for (int start = 0; start < n; start += kControlBlock) {
        const int m = (n - start) < kControlBlock ? (n - start) : kControlBlock;
        prof::mark(prof::kTank);
        control(m);
        prof::mark(prof::kTankCtl);
        const float k = scaleNow_ * sampleRate_ / kPaperRate;
        const float outGain = kOutputGain * levelGain_;
        // What stays put through the block is worked out once, with the same float operations as per
        // sample, so the same bits (overnight review, 26 Oct 2026): the output taps' delays, and ap2's
        // read (its length moves only in control(): its coefficient's divide). And once every line has
        // gone round since the last reset, the reads skip the ring's fill tests.
        int32_t tapDi[2][kTapsPerSide], ap2Di[2];
        float   tapT[2][kTapsPerSide], ap2Eta[2];
        tapDelays(k, tapDi, tapT);
        bool steady = true;
        for (int b = 0; b < 2; ++b) {
            const Branch& br = b_[b];
            ap2Di[b] = int32_t(br.ap2Len - 0.5f);
            const float f = br.ap2Len - float(ap2Di[b]);
            ap2Eta[b] = (1.0f - f) / (1.0f + f);
            steady = steady && br.ap1.full() && br.d1.full() && br.ap2.full() && br.d2.full();
        }
        auto readAp = [steady](const ExactLine& l, float d, float& state) {
            return steady ? l.readAllpassFull(d, state) : allpassFilling(l, d, state);
        };
        for (int s = start; s < start + m; ++s) {
            const float in[2] = {inL[s], inR[s]};
            float next[2];
            for (int b = 0; b < 2; ++b) {
                Branch& br = b_[b];
                // Input plus the other branch's output (the figure 8).
                float x = in[b] + b_[1 - b].out;
                // Modulated allpass, decay diffusion 1 (inverted signs).
                br.modNow += br.modStep;
                const float d1 = readAp(br.ap1, br.ap1Len + br.modNow, br.i1);
                const float v1 = x - kDecayDiffusion1 * d1;
                br.ap1.write(v1);
                x = d1 + kDecayDiffusion1 * v1;
                // Delay, then loss (DECAY + TONE, Jot).
                br.d1.write(x);
                br.longNow[0] += br.longStep[0];
                x = readAp(br.d1, br.d1Len + br.longNow[0], br.i2);
                br.y1 = br.loss1.gain * x + br.loss1.b1 * br.x1 + br.loss1.pole * br.y1;
                br.x1 = x;
                x = br.y1;
                // Allpass, decay diffusion 2.
                const float d2 = steady ? br.ap2.allpassFull(ap2Di[b], ap2Eta[b], br.i3) : allpassFilling(br.ap2, br.ap2Len, br.i3);
                const float v2 = x + kDecayDiffusion2 * d2;
                br.ap2.write(v2);
                x = d2 - kDecayDiffusion2 * v2;
                // Delay, then loss.
                br.d2.write(x);
                br.longNow[1] += br.longStep[1];
                x = readAp(br.d2, br.d2Len + br.longNow[1], br.i4);
                br.y2 = br.loss2.gain * x + br.loss2.b1 * br.x2 + br.loss2.pole * br.y2;
                br.x2 = x;
                next[b] = colour_ ? br.era.process(br.y2) : br.y2;
            }
            b_[0].out = next[0];
            b_[1].out = next[1];

            float l = 0.0f, r = 0.0f;
            for (int j = 0; j < kTapsPerSide; ++j) {
                const Tap& t = kLeft[j];
                const Branch& br = b_[t.branch];
                const ExactLine& line = t.element == 0 ? br.d1 : (t.element == 1 ? br.ap2 : br.d2);
                l += t.sign * tapRead(line, tapDi[0][j], tapT[0][j]);
            }
            for (int j = 0; j < kTapsPerSide; ++j) {
                const Tap& t = kRight[j];
                const Branch& br = b_[t.branch];
                const ExactLine& line = t.element == 0 ? br.d1 : (t.element == 1 ? br.ap2 : br.d2);
                r += t.sign * tapRead(line, tapDi[1][j], tapT[1][j]);
            }
            outL[s] += outGain * l;
            outR[s] += outGain * r;
            governor_.add(0.5f * outGain * outGain * (l * l + r * r));
        }
    }
}

} // namespace iv::dsp
