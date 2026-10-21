#pragma once
// Numbers for the big tanks (SPEC §4.2–4.5). Starting guesses unless a
// milestone has confirmed them by ear; each says where it came from.

namespace iv::voicing {

// ---- FDN (Hall and Massive, SPEC §4.3) -------------------------------------
constexpr int kFdnLines = 8; // 16 is a stretch goal, decided at M3

// Line lengths relative to the mean, spread like Supermassive's Warp at a
// middle setting; rounded to primes at prepare() so no two lines share a
// factor (SPEC §2.3 rule 1).
constexpr float kFdnRatios[kFdnLines] = {0.60f, 0.69f, 0.79f, 0.90f, 1.01f, 1.13f, 1.27f, 1.41f};

// The longest a line may get (Massive's 2 s mean × the longest ratio, plus
// modulation room). Sets the FDN's memory: kFdnLines × this, in SDRAM.
constexpr float kFdnMaxLineSeconds = 2.05f * 1.41f + 0.01f;

// Hall's mean line delay across SIZE (SPEC §3: 40 → 180 ms, exponential).
constexpr float kHallMeanMinMs = 40.0f;
constexpr float kHallMeanMaxMs = 180.0f;

// SIZE moves glide: a line's length changes by at most this fraction of a
// second per second, which is at most a semitone of bend (2^(1/12) − 1).
// SPEC §4.3, ADR 0012.
constexpr float kSizeSlewPerSecond = 0.0595f;

// Modulation floor (SPEC §2.3 rule 1, CONTEXT "Modulation floor"): every
// line's read wanders slowly at random. Depth in seconds of delay (0.46 ms
// is ~±1.5 cents at the ~0.3 Hz rate below); a new random target every
// kFloorStepSeconds × (1 + line spread).
constexpr float kFloorDepthSeconds = 0.00046f;
constexpr float kFloorStepSeconds  = 1.7f;

// Fixed damping (SPEC §4.6): the high end's T60 as a share of the low
// end's, at TONE noon. TONE moves it (Mappings.h toneHighT60Ratio).
constexpr float kDampingHighT60Ratio = 0.45f;

// At infinite DECAY the lows hold but the highs still fade, over about this
// long at TONE noon, so an infinite tail darkens toward its floor (ADR 0008)
// instead of ringing bright forever.
constexpr float kInfiniteHighT60Seconds = 30.0f;

// Input diffusion (SPEC §4.1): Dattorro's four input allpasses, his lengths
// (142, 107, 379, 277 samples at 29.761 kHz) in seconds; the right channel's
// are kDiffuserRightScale longer so the two sides decorrelate.
constexpr float kDiffuserSeconds[4] = {0.004771f, 0.003595f, 0.012735f, 0.009307f};
constexpr float kDiffuserCoeff[4]   = {0.75f, 0.75f, 0.625f, 0.625f};
constexpr float kDiffuserRightScale = 1.071f;

// Output level of the FDN's wet, so a Hall tail sits near the dry's level
// at MIX noon (tuned by ear at M1).
constexpr float kFdnOutputGain = 0.6f;

// Predelay memory: SIZE's (Massive) + TIME's (NORMAL), SPEC §5.2.
constexpr float kPredelayMaxSeconds = 0.75f;

} // namespace iv::voicing
