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
#ifdef IV_PLATE_XF_CODE
// SIZE by crossfade's small moves (control()): up to 4 % of the scale they glide at 0.0193 a second, the longest
// line (d1, 0.15 s a unit of scale) slewing 0.29 % of real time: about 5 cents while it moves, under the plate's own
// chorus (a full 4 % takes about 2 s at the largest scale). Beyond 4 % the crossfade's reads are far enough apart
// to sum as a dense comb (0.3 -> 0.4: within 0.4 dB of the held room per band).
constexpr float kXfSmallStep          = 0.04f;
constexpr float kXfSmallSlewPerSecond = 0.0193f;
#endif

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
#ifdef IV_PLATE_XF_CODE
// The taps' branch, element and sign in a byte each (processXf's loop indexes them; kLeft / kRight as data cost
// 224 B of flash): bit 0 the branch, bits 1-2 the element, bit 3 a negative sign.
struct TapCodes {
    unsigned char v[2][kTapsPerSide];
};
constexpr TapCodes tapCodes()
{
    TapCodes o{};
    for (int j = 0; j < kTapsPerSide; ++j)
        for (int c = 0; c < 2; ++c) {
            const Tap& t = (c ? kRight : kLeft)[j];
            o.v[c][j] = (unsigned char)(t.branch | (t.element << 1) | (t.sign < 0.0f ? 8 : 0));
        }
    return o;
}
constexpr TapCodes kTapCodes = tapCodes();
#endif

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
#ifdef IV_PLATE_XF_CODE
    cancelCrossfade(scaleTarget_);
#endif
}

IV_SIZE_OPT void Plate::setScale(float scale, bool snap)
{
    if (scale > maxScale_) scale = maxScale_;
    if (scale < 0.05f) scale = 0.05f;
    scaleTarget_ = scale;
    if (snap) scaleNow_ = scale;
#ifdef IV_PLATE_XF_CODE
    if (snap) cancelCrossfade(scale); // a restart (prepare, a SPACE flip): no crossfade
#endif
}

IV_SIZE_OPT void Plate::control(int n) // once per block: built for size (flash; it shares Fdn::control's helpers)
{
    const float dt = float(n) / sampleRate_;
    float slewPerSecond = kScaleSlewPerSecond;
#ifdef IV_PLATE_XF_CODE
    bool starting = false;
    if (xfOn_ && xs_ && !xfGlideNow_) {
        PlateXf& X = *xs_;
        // SIZE by crossfade: rests kSizeRestSeconds (the longest element's target still within half a sample a
        // block, as Fdn::control), then everything jumps at once (startCrossfade, after the lengths below).
        const float perScale = kD1[0] * sampleRate_ / kPaperRate;
        X.xfRest_ = std::fabs(scaleTarget_ - X.xfLastTarget_) * perScale > 0.5f ? 0.0f : X.xfRest_ + dt;
        X.xfLastTarget_ = scaleTarget_;
        const float diff = std::fabs(scaleTarget_ - scaleNow_);
        const bool pending = diff * perScale > 0.5f;
        // A small move (within kXfSmallStep of the scale) glides, slowly enough not to bend (kXfSmallSlewPerSecond):
        // crossfaded, the old and new reads a few samples apart summed as a comb in the loop, and the tail kept it
        // (0.3 -> 0.303 lost 5 dB at 1.2-4 kHz for seconds; the owner's "drop" at 5.5 s on renders/plate_size_xfade's
        // slow turn, and at the unfreeze, 3 Nov 2026). Further, it waits for the rest and crossfades.
        const bool small = diff <= kXfSmallStep * scaleNow_;
        if (xfSince_ >= 0 || !small) slewPerSecond = 0.0f;
        else slewPerSecond = kXfSmallSlewPerSecond;
        if (pending && !small && xfSince_ < 0 && X.xfRest_ >= voicing::kSizeRestSeconds) {
            starting = true;
            X.xfKOld_ = scaleNow_ * sampleRate_ / kPaperRate;
            // The level that follows SIZE (levelGain_, on the output) steps with the jump; what is in the tank
            // is scaled by the old gain over the new as it is read (xfR_), so the tail carries on at its level
            // and only new sound comes in at the new one (ADR 0025's "level that follows SIZE", here on the output).
            X.xfR_ = std::sqrt(scaleNow_ / scaleTarget_);
            scaleNow_ = scaleTarget_;
        }
    }
#endif
    {
    const float slew = slewPerSecond * dt;
    const float diff = scaleTarget_ - scaleNow_;
    scaleNow_ += diff > slew ? slew : (diff < -slew ? -slew : diff);
    }
    const float k = scaleNow_ * sampleRate_ / kPaperRate;

    governor_.control(n, sampleRate_);
    // DECAY's own loss; the floor governor's correction rides on it flat (absorbShared(), v1.0.58).
    const float lowRate = decayRate_ * voicing::kPlateRateCalibration + extraRate_;
    levelGain_ = voicing::kPlateOutputTrim * std::sqrt(scaleNow_ / kLevelRefScale);

    const float excursion = kExcursion * sampleRate_ / kPaperRate; // not scaled: the chorus depth stays put
    const float floorCoeff = 1.0f - std::exp(-2.0f * 3.14159265f * 0.3f * dt);
#ifdef IV_PLATE_XF_CODE
    if (starting) startCrossfade(lowRate + governor_.extraRate, k); // the lengths and losses below are still the old ones
#endif
    for (int b = 0; b < 2; ++b) {
        Branch& br = b_[b];
        br.ap1Len = kAp1[b] * k;
        br.d1Len = kD1[b] * k;
        br.ap2Len = kAp2[b] * k;
        br.d2Len = kD2[b] * k;
        br.loss1 = br.memo1.get((br.ap1Len + br.d1Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfK_, air_,
                                governor_.extraRate, endlessTop_);
        br.loss2 = br.memo2.get((br.ap2Len + br.d2Len) / sampleRate_, lowRate, highRatio_, governor_.darken, shelfK_, air_,
                                governor_.extraRate, endlessTop_);

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
#ifdef IV_PLATE_XF_CODE
    if (xfSince_ >= 0) {
        PlateXf& X = *xs_;
        const float a = X.xfLeft_ > 0 ? X.xfOld_ : 0.0f, w = 1.0f - a;
        const float t = float(xfSince_);
        for (int b = 0; b < 2; ++b) {
            Branch& br = b_[b];
            PlateXf::Branch& o = X.xf_[b];
            for (int j = 0; j < 2; ++j) {
                // The loss filters fade from the old lengths' to the new (ADR 0025: switched at once, the old read
                // stepped to the new trip's loss, a click).
                if (a > 0.0f) {
                    Absorb& l = j ? br.loss2 : br.loss1;
                    l.gain = a * o.loss[j].gain + w * l.gain;
                    l.pole = a * o.loss[j].pole + w * l.pole;
                    l.b1 = a * o.loss[j].b1 + w * l.b1;
                }
                // The new d1 / d2 reads' trip correction (Fdn::control's corr): held for one trip, eased out over the fade.
                const float since = t - o.hold[j];
                o.corr[j] = std::exp(o.lnGain[j] * (since < 0.0f ? 1.0f : since < float(X.xfLen_) ? 1.0f - since / float(X.xfLen_) : 0.0f));
            }
        }
        // Each read pair's gain for its agreement (Fdn::control): the powers' own fade over the mix's, capped at 1.42.
        if (a > 0.0f)
            for (PlateXf::Agree& g : X.agree_) {
                const float on = std::fmax(0.0f, g.on);
                const float want = a * g.oo + w * g.nn, mix = a * a * g.oo + w * w * g.nn + 2.0f * a * w * on;
                const float K = mix > 1e-24f ? std::fmin(1.42f, std::sqrt(want / mix)) : 1.0f;
                g.dK = (K - g.K) * (1.0f / float(kControlBlock)); // reached over the block, sample by sample (stepped once a
            }                                                     // block, it ticked in the HF on music, 3 Nov 2026)
    }
#endif
}

#ifdef IV_PLATE_XF_CODE
// ---- SIZE by crossfade (PROTOTYPE, renders/plate_size_xfade) ----------------------------------------------------
// ADR 0025's crossfade, the PLATE's phase 2. A glide in a loop retunes everything circulating by the length ratio
// for the rest of its tail (PASP, "Delay-line interpolation": crossfade between the two delay settings rather than
// glide). Lexicon's units mute the reverb while Size changes (PCM 80 manual, Size; the MPX 550 reloads the program);
// Valhalla smooths every control for live tweaking (ValhallaShimmer's notes), and a moving Size there bends the pitch
// like an analogue delay (a user's review): what the PLATE's glide did. Here, as HALL's and
// CANYON's lines: once SIZE has rested every one of the eight lengths (each branch's ap1, d1, ap2, d2) and the output
// taps jump to the new scale inside the same lines (they are sized for the largest), and the old reads fade out
// sin^2 / cos^2 with each pair's agreement gain (Bristow-Johnson). Inside the two allpasses the delayed read is
// crossfaded the same way (as HALL's in-loop allpasses present on both sides). The loss filters fade from old to new;
// the new d1 / d2 reads carry the one-trip decay correction. No extra memory: about twice the reads for the fade.
// The PLATE's level that follows SIZE is on its output (levelGain_), so the tank's content is rescaled by the
// old gain over the new (xfR_) sample by sample as it is read: each sample written before the jump, wherever it is
// read (the loop, the old or new read, an output tap), counts once, so the tail neither swells nor dips.
namespace {
// The sample written a ticks ago (ExactLine::at), scaled by r if that was before the jump (more than w writes ago).
IV_NOINLINE float agoSc(const ExactLine& l, int32_t a, int32_t w, float r)
{
    const float x = l.at(int32_t(l.pos) - a);
    return a > w ? r * x : x;
}
// Allpass-interpolated read (ExactLine::readAllpassFull's sums), any ring, the samples from before the jump scaled.
IV_NOINLINE float readSc(const ExactLine& l, float d, float& state, int32_t w, float r)
{
    const int32_t di  = int32_t(d - 0.5f);
    const float   f   = d - float(di);
    const float   eta = (1.0f - f) / (1.0f + f);
    const float   ai  = agoSc(l, di, w, r);
    state = agoSc(l, di + 1, w, r) + eta * (ai - state);
    return state;
}
constexpr float kXfAgree = 0.004f; // the reads' agreement followers, ~5 ms (Fdn's kXfAgreeA)
template <class G> inline float xfMix(G& g, float nw, float od, float a, float w)
{
    g.oo += kXfAgree * (od * od - g.oo);
    g.nn += kXfAgree * (nw * nw - g.nn);
    g.on += kXfAgree * (od * nw - g.on);
    g.K += g.dK;
    return g.K * (w * nw + a * od);
}
// Where a fade stands this sample: writes since the jump, the content scale, the old and new gains.
struct XfNow {
    int32_t t;
    float   r, a, w;
    bool    fading;
};
// One of the loop's reads while SIZE crossfades: the new length (its correction applied), and while fading the old
// beside it, mixed (out of line: eight calls a sample, one copy, flash).
template <class G>
IV_NOINLINE float xfRead(const ExactLine& l, float dNew, float dOld, float& sNew, float& sOld, int32_t w, float corr, G& g,
                         const XfNow& f)
{
    const float n = readSc(l, dNew, sNew, w, f.r) * corr;
    return f.fading ? xfMix(g, n, readSc(l, dOld, sOld, w, f.r), f.a, f.w) : n;
}
} // namespace

IV_NOINLINE IV_SIZE_OPT void Plate::setCrossfadeState(PlateXf* s)
{
    xs_ = s;
    s->xfSeconds_ = voicing::kSizeCrossfadeSeconds;
}

// No crossfade (a reset, a snap): the old reads are dropped.
IV_NOINLINE IV_SIZE_OPT void Plate::cancelCrossfade(float target)
{
    xfSince_ = -1;
    if (PlateXf* x = xs_) {
        x->xfLeft_ = 0;
        x->xfRest_ = 0.0f;
        x->xfLastTarget_ = target;
        for (auto& o : x->xf_) o.corr[0] = o.corr[1] = 1.0f;
    }
}

IV_SIZE_OPT void Plate::startCrossfade(float rate, float k)
{
    PlateXf& X = *xs_;
    X.xfLen_ = X.xfLeft_ = int32_t(X.xfSeconds_ * sampleRate_);
    if (X.xfLen_ < 1) X.xfLen_ = X.xfLeft_ = 1;
    xfSince_ = 0;
    const float step = 3.1415927f / float(X.xfLen_);
    X.xfRotC_ = std::cos(step);
    X.xfRotS_ = std::sin(step);
    X.xfCos_ = 1.0f;
    X.xfSin_ = 0.0f;
    X.xfOld_ = 1.0f;
    X.xfNew_ = 0.0f;
    for (PlateXf::Agree& g : X.agree_) g = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    const float r = X.xfR_;
    int32_t end = 0;
    for (int b = 0; b < 2; ++b) {
        Branch& br = b_[b];
        PlateXf::Branch& o = X.xf_[b];
        // Everything the tank remembers from before the jump is scaled by r: the filters', the branch's own, the reads'
        // allpass-interpolation states. The old reads carry on with theirs (as Fdn's); the new ones start from the same
        // (Fdn starts them on a cubic read: here they come in under a fade still at zero for their first milliseconds).
        float* mem[9] = {&br.i1, &br.i2, &br.i3, &br.i4, &br.y1, &br.x1, &br.y2, &br.x2, &br.out};
        for (int q = 0; q < 9; ++q) {
            *mem[q] *= r;
            if (q < 4) o.st[q] = *mem[q];
        }
        // The old lengths and losses (control() works out the new ones after this), and the new lengths as it will.
        const float lenNew[4] = {kAp1[b] * k, kD1[b] * k, kAp2[b] * k, kD2[b] * k};
        const float lenOld[4] = {br.ap1Len, br.d1Len, br.ap2Len, br.d2Len};
        for (int e = 0; e < 4; ++e) o.len[e] = lenOld[e];
        o.loss[0] = br.loss1;
        o.loss[1] = br.loss2;
        for (int j = 0; j < 2; ++j) {
            const float seg = (o.len[2 * j] + o.len[2 * j + 1]) - (lenNew[2 * j] + lenNew[2 * j + 1]);
            // Old minus new: Fdn's xfLnGain_. A shrink takes 0.6 of it (measured, 3 Nov 2026: in full, a full shrink
            // at DECAY 0.2-0.4 swelled 1.4-1.6 dB past both rooms; 0.6: within 0.8 dB at every DECAY).
            o.lnGain[j] = (seg > 0.0f ? 0.6f : 1.0f) * 6.9077553f * seg / sampleRate_ * rate;
            o.hold[j] = lenNew[2 * j + 1];
            o.corr[j] = std::exp(o.lnGain[j]);
        }
        const ExactLine* lines[4] = {&br.ap1, &br.d1, &br.ap2, &br.d2};
        for (const ExactLine* ln : lines) // every sample from before the jump read; the corrections eased out
            if (int32_t(ln->size) + X.xfLen_ > end) end = int32_t(ln->size) + X.xfLen_;
    }
    X.xfEnd_ = (end > X.xfLen_ ? end : X.xfLen_) + kControlBlock;
}

// One control block while SIZE crossfades (and until every sample written before the jump has been read): the
// sample loop of process() with the old reads beside the new. Rare (about 0.3 s after a move), so out of line.
IV_SIZE_OPT void Plate::processXf(const float* inL, const float* inR, float* outL, float* outR, int start, int m)
{
    PlateXf& X = *xs_;
    const float k = scaleNow_ * sampleRate_ / kPaperRate;
    const float outGain = kOutputGain * levelGain_;
    const float r = X.xfR_;
    int32_t tapDi[2][kTapsPerSide], oldDi[2][kTapsPerSide];
    float   tapT[2][kTapsPerSide], oldT[2][kTapsPerSide];
    tapDelays(k, tapDi, tapT);
    tapDelays(X.xfKOld_, oldDi, oldT);
    for (int s = start; s < start + m; ++s) {
        XfNow f{xfSince_, r, 0.0f, 1.0f, X.xfLeft_ > 0}; // t: writes since the jump, before this sample's
        const int32_t t = f.t;
        const bool fading = f.fading;
        if (fading) {
            f.a = X.xfOld_;
            f.w = X.xfNew_;
            const float c = X.xfCos_;
            X.xfCos_ = c * X.xfRotC_ - X.xfSin_ * X.xfRotS_;
            X.xfSin_ = X.xfSin_ * X.xfRotC_ + c * X.xfRotS_;
            X.xfNew_ = 0.5f - 0.5f * X.xfCos_;
            X.xfOld_ = 1.0f - X.xfNew_;
            if (--X.xfLeft_ == 0) {
                X.xfOld_ = 0.0f;
                X.xfNew_ = 1.0f;
            }
        }
        const float a = f.a, w = f.w;
        // What's playing in comes up (or down) to the new size's level over the fade, from the old: stepped with the
        // output's level, it clicked, and ap1 rang the click on every old length (a full grow under a pad, 3 Nov 2026).
        const float inGain = r + (1.0f - r) * w;
        const float in[2] = {inL[s] * inGain, inR[s] * inGain};
        float next[2];
        for (int b = 0; b < 2; ++b) {
            Branch& br = b_[b];
            PlateXf::Branch& o = X.xf_[b];
            PlateXf::Agree* g = X.agree_ + 4 * b;
            float x = in[b] + b_[1 - b].out;
            br.modNow += br.modStep;
            float d = xfRead(br.ap1, br.ap1Len + br.modNow, o.len[0] + br.modNow, br.i1, o.st[0], t, 1.0f, g[0], f);
            const float v1 = x - kDecayDiffusion1 * d;
            br.ap1.write(v1);
            x = d + kDecayDiffusion1 * v1;
            br.d1.write(x);
            br.longNow[0] += br.longStep[0];
            x = xfRead(br.d1, br.d1Len + br.longNow[0], o.len[1] + br.longNow[0], br.i2, o.st[1], t + 1, o.corr[0], g[1], f);
            br.y1 = br.loss1.gain * x + br.loss1.b1 * br.x1 + br.loss1.pole * br.y1;
            br.x1 = x;
            x = br.y1;
            d = xfRead(br.ap2, br.ap2Len, o.len[2], br.i3, o.st[2], t, 1.0f, g[2], f);
            const float v2 = x + kDecayDiffusion2 * d;
            br.ap2.write(v2);
            x = d - kDecayDiffusion2 * v2;
            br.d2.write(x);
            br.longNow[1] += br.longStep[1];
            x = xfRead(br.d2, br.d2Len + br.longNow[1], o.len[3] + br.longNow[1], br.i4, o.st[3], t + 1, o.corr[1], g[3], f);
            br.y2 = br.loss2.gain * x + br.loss2.b1 * br.x2 + br.loss2.pole * br.y2;
            br.x2 = x;
            next[b] = colour_ ? br.era.process(br.y2) : br.y2;
        }
        b_[0].out = next[0];
        b_[1].out = next[1];

        // The output taps (lerpFull's sums), new and, while fading, old, the samples from before the jump scaled.
        float sum[2][2] = {}; // [new, old][left, right]
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < kTapsPerSide; ++j) {
                const unsigned   code = kTapCodes.v[c][j];
                const Branch&    br = b_[code & 1u];
                const unsigned   el = (code >> 1) & 3u;
                const ExactLine& line = el == 0 ? br.d1 : (el == 1 ? br.ap2 : br.d2);
                const float      sign = code & 8u ? -1.0f : 1.0f;
                for (int v = 0; v < (fading ? 2 : 1); ++v) {
                    const int32_t di = v ? oldDi[c][j] : tapDi[c][j];
                    const float   ta = agoSc(line, di, t + 1, r);
                    sum[v][c] += sign * (ta + (v ? oldT[c][j] : tapT[c][j]) * (agoSc(line, di + 1, t + 1, r) - ta));
                }
            }
        const float nl = sum[0][0], nr = sum[0][1], ol = sum[1][0], orr = sum[1][1];
        float l = nl, rr = nr;
        if (fading) {
            PlateXf::Agree& go = X.agree_[8];
            go.oo += kXfAgree * (ol * ol + orr * orr - go.oo);
            go.nn += kXfAgree * (nl * nl + nr * nr - go.nn);
            go.on += kXfAgree * (ol * nl + orr * nr - go.on);
            go.K += go.dK;
            l = go.K * (w * nl + a * ol);
            rr = go.K * (w * nr + a * orr);
        }
        outL[s] += outGain * l;
        outR[s] += outGain * rr;
        governor_.add(0.5f * outGain * outGain * (l * l + rr * rr));
        ++xfSince_;
    }
    if (X.xfLeft_ <= 0 && xfSince_ >= X.xfEnd_) { // every sample from before the jump read, the corrections done
        xfSince_ = -1;
        for (auto& o : X.xf_) o.corr[0] = o.corr[1] = 1.0f;
    }
}
#endif

void Plate::process(const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (!ready_) return;
    for (int start = 0; start < n; start += kControlBlock) {
        const int m = (n - start) < kControlBlock ? (n - start) : kControlBlock;
        prof::mark(prof::kTank);
        control(m);
        prof::mark(prof::kTankCtl);
#ifdef IV_PLATE_XF_CODE
        if (xfSince_ >= 0) {
            processXf(inL, inR, outL, outR, start, m);
            continue;
        }
#endif
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
