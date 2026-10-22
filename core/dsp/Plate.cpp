#include "dsp/Plate.h"

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
constexpr Tap kLeft[7]  = {{1, 0, 266, 1},  {1, 0, 2974, 1}, {1, 1, 1913, -1}, {1, 2, 1996, 1},
                           {0, 0, 1990, -1}, {0, 1, 187, -1}, {0, 2, 1066, -1}};
constexpr Tap kRight[7] = {{0, 0, 353, 1},  {0, 0, 3627, 1}, {0, 1, 1228, -1}, {0, 2, 2673, 1},
                           {1, 0, 2111, -1}, {1, 1, 335, -1}, {1, 2, 121, -1}};
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

float lerpRead(const ExactLine& l, float d)
{
    const int32_t di = int32_t(d);
    const float t = d - float(di);
    const int32_t i = int32_t(l.pos) - di;
    return l.at(i) + t * (l.at(i - 1) - l.at(i));
}

} // namespace

IV_SIZE_OPT bool Plate::prepare(Arena& arena, float sampleRate, float maxScale, bool colour, uint32_t seed)
{
    sampleRate_ = sampleRate;
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

void Plate::control(int n)
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
        br.loss1 = absorbent((br.ap1Len + br.d1Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfHz_, sampleRate_);
        br.loss2 = absorbent((br.ap2Len + br.d2Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfHz_, sampleRate_);

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
        float target = excursion * (1.0f + modDepth_ * 0.7f * std::sin(6.2831853f * br.lfoPhase) + 0.3f * br.floorB);
        if (colour_) {
            // DRIFT swings the modulated allpass's length (the buffer has
            // room for the longest swing).
            const float room = voicing::kDriftMaxSeconds * sampleRate_;
            float cap = voicing::kDriftMaxShareOfLine * br.ap1Len;
            if (cap > room) cap = room;
            target += br.drift.control(dt, drift_, cap);
        }
        br.modStep = (target - br.modNow) / float(n);

        // PROTOTYPE: d1 and d2 wander too, each at its own rate (a sine plus the floor's random
        // walk), about 2 cents; eased with modDepth_ while frozen like ap1.
        const float longDepth = longMod_ * modDepth_ * voicing::kPlateLongModSeconds * sampleRate_;
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
        control(m);
        const float k = scaleNow_ * sampleRate_ / kPaperRate;
        const float outGain = kOutputGain * levelGain_;
        for (int s = start; s < start + m; ++s) {
            const float in[2] = {inL[s], inR[s]};
            float next[2];
            for (int b = 0; b < 2; ++b) {
                Branch& br = b_[b];
                // Input plus the other branch's output (the figure 8).
                float x = in[b] + b_[1 - b].out;
                // Modulated allpass, decay diffusion 1 (inverted signs).
                br.modNow += br.modStep;
                const float d1 = br.ap1.readAllpass(br.ap1Len + br.modNow, br.i1);
                const float v1 = x - kDecayDiffusion1 * d1;
                br.ap1.write(v1);
                x = d1 + kDecayDiffusion1 * v1;
                // Delay, then loss (DECAY + TONE, Jot).
                br.d1.write(x);
                br.longNow[0] += br.longStep[0];
                x = br.d1.readAllpass(br.d1Len + br.longNow[0], br.i2);
                br.y1 = br.loss1.gain * x + br.loss1.b1 * br.x1 + br.loss1.pole * br.y1;
                br.x1 = x;
                x = br.y1;
                // Allpass, decay diffusion 2.
                const float d2 = br.ap2.readAllpass(br.ap2Len, br.i3);
                const float v2 = x + kDecayDiffusion2 * d2;
                br.ap2.write(v2);
                x = d2 - kDecayDiffusion2 * v2;
                // Delay, then loss.
                br.d2.write(x);
                br.longNow[1] += br.longStep[1];
                x = br.d2.readAllpass(br.d2Len + br.longNow[1], br.i4);
                br.y2 = br.loss2.gain * x + br.loss2.b1 * br.x2 + br.loss2.pole * br.y2;
                br.x2 = x;
                next[b] = colour_ ? br.era.process(br.y2) : br.y2;
            }
            b_[0].out = next[0];
            b_[1].out = next[1];

            float l = 0.0f, r = 0.0f;
            for (const Tap& t : kLeft) {
                const Branch& br = b_[t.branch];
                const ExactLine& line = t.element == 0 ? br.d1 : (t.element == 1 ? br.ap2 : br.d2);
                l += t.sign * lerpRead(line, 1.0f + t.offset * k);
            }
            for (const Tap& t : kRight) {
                const Branch& br = b_[t.branch];
                const ExactLine& line = t.element == 0 ? br.d1 : (t.element == 1 ? br.ap2 : br.d2);
                r += t.sign * lerpRead(line, 1.0f + t.offset * k);
            }
            outL[s] += outGain * l;
            outR[s] += outGain * r;
            governor_.add(0.5f * outGain * outGain * (l * l + r * r));
        }
    }
}

} // namespace iv::dsp
