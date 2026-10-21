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
constexpr float kFdnMaxLineSeconds = 2.05f * 1.41f + 0.01f + 0.07f; // + DRIFT's room (EraVoicing.h kDriftMaxSeconds)

// Hall's mean line delay across SIZE (SPEC §3: 40 → 180 ms, exponential).
constexpr float kHallMeanMinMs = 20.0f;  // M1 notes: "small doesn't read as small" (was 40 → 180 ms)
constexpr float kHallMeanMaxMs = 260.0f;
// A small HALL's finite tails are shorter (× this at SIZE fully CCW, × 1 at
// CW): a small room can't ring for 16 s, and a long, sparse small-room tail
// let pitched notes build into a buzz (owner, M1 vibes). Infinite stays
// infinite. (start)
constexpr float kHallSmallT60Scale = 0.4f;

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
// at MIX noon (tuned by ear at M1). The tank's stored energy is spread over
// all its delay memory, so longer lines read out quieter (power ∝ 1/length):
// the output is scaled by sqrt(mean length / kFdnLevelRefSeconds), which
// keeps SIZE from turning the tail down (measured: −6.5 dB across HALL's
// range without it).
constexpr float kFdnOutputGain      = 0.6f;
constexpr float kFdnLevelRefSeconds = 0.1f;
constexpr float kMassiveOutputTrim  = 0.60f; // −3.3 dB: MASSIVE matches HALL's loudness (test_massive)

// Early taps (MASSIVE, M4 notes; Fdn::setEarlyTaps): kEarlyTaps reads
// partway along each line, rising from kEarlyTapLow to 1 with their place,
// at kEarlyTapLevel against the lines' ends. (start)
constexpr int   kEarlyTaps     = 3;
constexpr float kEarlyTapLow   = 0.3f;
constexpr float kEarlyTapLevel = 0.5f;

// In-loop diffusion (MASSIVE, ADR 0007): two allpasses inside each line,
// coefficient kInLoopCoeff × SIZE's diffusion, so long lines blur into a
// wash within a pass or two instead of returning as distinct echoes.
// Lengths in seconds, distinct per line.
constexpr float kInLoopSeconds[kFdnLines][2] = {
    {0.0297f, 0.0731f}, {0.0331f, 0.0659f}, {0.0373f, 0.0797f}, {0.0419f, 0.0613f},
    {0.0457f, 0.0857f}, {0.0503f, 0.0577f}, {0.0541f, 0.0911f}, {0.0239f, 0.0683f},
};
constexpr float kInLoopMaxSeconds = 0.092f;
constexpr float kInLoopCoeff = 0.62f;

// Floor governor (ADR 0008): an infinite tail's wet settles at this RMS
// level; each dB above it adds kGovernorRatePerDb of loss (1/T60, 1/s),
// measured over kGovernorSeconds.
constexpr float kFloorDbfs          = -24.0f;
constexpr float kGovernorRatePerDb  = 0.08f;
constexpr float kGovernorSeconds    = 1.5f;
constexpr float kHoldSeconds        = 8.0f; // the slow level a held tail's ceiling is judged on

// SPACE flips fade out, then in, over this long each (ADR 0013: ~100 ms
// in all). ATTACK into or out of REVERSE crossfades over kAttackFadeSeconds.
constexpr float kSpaceFlipSeconds  = 0.05f;
constexpr float kAttackFadeSeconds = 0.05f;

// ---- Plate (SPEC §4.2) -----------------------------------------------------
constexpr float kPlateMaxScale     = 1.6f;  // SIZE fully CW
constexpr float kPlateMinScale     = 0.5f;  // SIZE fully CCW
// The Plate's tails measured ~9 % longer than DECAY's mapping (low-band
// T60, test_plate_freeze), so its loss rate is scaled up by this. At the
// very short end with SIZE up, one pass round the figure 8 (~0.55 s at
// scale 1.6) is longer than the T60 asked for, so those tails stay longer
// than the mapping: a big plate can't ring for only 0.4 s.
constexpr float kPlateRateCalibration = 1.09f;
constexpr float kPlateOutputTrim   = 0.41f; // −7.8 dB: PLATE matches HALL's loudness (test_plate_freeze)

// ---- Freeze and paint (ADR 0009, 0010; SPEC §3 "Button + Gate") -----------
constexpr float kFreezeInputSeconds  = 0.02f; // the big tank's input closes
constexpr float kFreezeColourSeconds = 0.2f;  // in-loop colour glides out
constexpr float kUnfreezeSeconds     = 1.0f;  // the big tank glides back to the knobs
constexpr float kPaintSeconds        = 3.0f;  // painting builds into the pad over ~this
constexpr float kPaintReleaseSeconds = 0.2f;
constexpr float kPaintLevel          = 0.5f;  // the twin's share sent into the pad (× the room left)
// A frozen PLATE's chorus depth (× Dattorro's): a lossless loop piles its
// few cents a pass into a seasick wobble (owner, M5 listening).
constexpr float kFrozenPlateModDepth = 0.15f;
// The freeze twin (ADR 0009, owner's picks on freeze_options and
// freeze_twin): what you play over a pad goes into a twin of the frozen tank
// (the same space, SIZE and DECAY playing it), heard as a quieter layer over
// the untouched pad, fading back out within kTwinMaxT60 so the pad returns
// to what it was. No predelay or swell on its input ("too much
// predelay... disjointed"). (start)
constexpr float kTwinLevel     = 0.3f;  // −10.5 dB against the pad: "a more subtle layer... not the main focus"
constexpr float kTwinMaxT60    = 8.0f;  // it always fades, even when DECAY is infinite
constexpr float kTwinFadeSeconds = 0.02f; // its input opens / closes at freeze / unfreeze
constexpr float kPaintHeadroomDb     = 1.5f;  // a painted pad may grow this far above its frozen level (or the floor)
constexpr float kPaintFadeDb         = 1.0f;  // painting eases off over the last this-many dB below that
// A still pad's ceiling over the level it settled at: its slowly wandering
// reads (the modulation floor) nudge its energy up or down a little; the
// governor stops it creeping up (measured +1.5 dB over 5 min without it).
constexpr float kPadCeilingDb        = 0.25f;
// A fresh pad settles for a while (MASSIVE's 1–3 s lines take several passes
// to spread their energy evenly: measured ±3 dB over its first ~15 s), so
// its ceiling is taken after this long.
constexpr float kPadSettleSeconds    = 20.0f;
// The same for an infinite tail once the input has been quiet (below
// kQuietDbfs) for kQuietSeconds: it holds where it settled instead of
// drifting up toward the floor (measured +4 dB over 10 min without it).
constexpr float kQuietDbfs   = -70.0f;
constexpr float kQuietSeconds = 2.0f;
// The infinite tail's ceiling ratchets down with its slow level for this
// long after the input goes quiet (settling), then stays put.
constexpr float kTailSettleSeconds = 10.0f;
// A held tail below its settled level is raised back by kHoldRatePerDb
// (1/T60 in 1/s per dB), capped at kMakeupMaxRate (0.005 ≈ 0.3 dB/s);
// above it, it loses kGovernorRatePerDb per dB, as at the floor.
constexpr float kHoldRatePerDb = 0.004f;
constexpr float kMakeupMaxRate = 0.005f;

// ---- TONE's tilt on the wet (Output.h Tilt; M1 notes) (start) ----------
constexpr float kToneTiltHz       = 600.0f;
constexpr float kToneDarkLowDb    = 4.0f;  // TONE fully CCW
constexpr float kToneDarkHighDb   = -9.0f;
constexpr float kToneBrightLowDb  = -4.0f; // TONE fully CW
constexpr float kToneBrightHighDb = 6.0f;

// ---- Ducking: always DARKEN (ADR 0014, SPEC §4.9) -------------------------
// An envelope follower on the input; depth 0 below DECAY 0.85, full at 1.
// DARKEN is a low-pass on the wet (not in the loop: a loop filter would
// take the highs for good, and the tail must brighten back when you stop,
// SPEC v1.0.4), closing to kDuckDarkHz at full depth, plus a kDuckDipDb dip.
constexpr float kDuckAttackSeconds  = 0.005f;
constexpr float kDuckReleaseSeconds = 0.15f; // 0.4 s dwelt between skanks (owner, M5 listening)
constexpr float kDuckFromDbfs = -42.0f, kDuckFullDbfs = -24.0f;
constexpr float kDuckDarkHz = 1500.0f;
constexpr float kDuckDipDb  = -3.0f;

// ---- Output soft limiter (SPEC §4.10): the wet only, before MIX ------------
constexpr float kLimitCeiling        = 0.891f; // −1 dBFS
constexpr float kLimitReleaseSeconds = 0.2f;

// Predelay memory: SIZE's (Massive) + TIME's (NORMAL), SPEC §5.2.
constexpr float kPredelayMaxSeconds = 0.75f;

} // namespace iv::voicing
