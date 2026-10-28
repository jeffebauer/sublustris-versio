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
constexpr float kHallMeanMinMs = 35.0f;  // owner's pick on renders/small_hall (v1.0.17): 20 ms built up "slightly metallic" resonances (was 40, then 20)
constexpr float kHallMeanMaxMs = 260.0f;
// A small HALL's finite tails are shorter (× this at SIZE fully CCW, × 1 at
// CW): a small room can't ring for 16 s, and a long, sparse small-room tail
// let pitched notes build into a buzz (owner, M1 vibes). Infinite stays
// infinite. (start)
constexpr float kHallSmallT60Scale = 0.49f; // v1.0.17: the small end the owner picked (old knob 0.22) had 0.49

// HALL's in-loop diffusion (the audit's F1/F2; owner's pick on renders/small_hall, v1.0.17): two
// allpasses per line at this × kInLoopCoeff, their lengths scaled with the mean line (MASSIVE's at
// 0.2 s and up), so a small room gains modes without sounding bigger.
constexpr float kHallInLoopDiffusion = 0.5f;
constexpr float kHallInLoopUntilSize = 0.5f; // fades to 0 by SIZE noon (the owner heard it at the small end)

// SIZE moves glide: a line's length changes by at most this fraction of a
// second per second, which is at most a semitone of bend (2^(1/12) − 1).
// SPEC §4.3, ADR 0012.
constexpr float kSizeSlewPerSecond = 0.0595f;
// SIZE and TIME (NORMAL's pre-delay) move by crossfade, not by sliding (v1.0.34, ADR 0025; owner's pick
// C on renders/knob_moves): once the knob has rested kSizeRestSeconds the delays jump and the old
// ones fade out over kSizeCrossfadeSeconds (in the loop: summing to one where the two reads agree,
// keeping their power where they don't). kSizeSlewPerSecond still glides HALL's
// in-loop allpasses when they come in or go out without a SIZE move.
constexpr float kSizeCrossfadeSeconds = 0.3f;
constexpr float kSizeRestSeconds      = 0.05f;
// SIZE's hysteresis in HALL and MASSIVE (v1.0.37; Engine::hysteresisSize): SIZE moves on once the mean
// line length would move by more than 0.1 % (1.7 cents), so a knob's noise never restarts the rest timer.
constexpr float kSizeHysteresis       = 1e-3f;

// Modulation floor (SPEC §2.3 rule 1, CONTEXT "Modulation floor"): every
// line's read wanders slowly at random. Depth in seconds of delay (0.46 ms
// is ~±1.5 cents at the ~0.3 Hz rate below); a new random target every
// kFloorStepSeconds × (1 + line spread).
constexpr float kFloorDepthSeconds = 0.00046f;
// HALL's floor is this many times deeper (v1.0.25, owner's pick on renders/valhalla_round2: a long
// HALL's tail held sharper resonances than Valhalla Room's, drums 11 dB over their surroundings
// against 8.5; ×3 gives 8.9, and a held 1 kHz wavers 6.5 → 7.3 cents).
constexpr float kHallFloorScale = 3.0f;
// "Air" (v1.0.25, valhalla_round2): HALL's and PLATE's highs lose this much more than their lows
// at every finite DECAY, in dB/s, as air does in a room (Valhalla Room measured 3-7), instead of a
// fixed share of the decay (which made short tails go dull fast: a 2 s HALL's 8 kHz rang 1.1 s
// against Room's 1.8; now 1.8). TONE scales it against noon. MASSIVE keeps the share.
constexpr float kAirHallDbPerSecond = 5.5f, kAirPlateDbPerSecond = 8.0f;
// MASSIVE's built-in swell (NORMAL) at this share of massiveNormalAttackSeconds (v1.0.25,
// valhalla_round2: "less swell… more smoothed out"; it peaks ~0.3 s sooner).
constexpr float kMassiveSwellScale = 0.5f;
// HALL's level with its early taps and air (v1.0.25): music measured 2.3 LU louder (median over SIZE
// 0 / 0.6 / 1 × DECAY 0.3 / 0.5 / 0.77), taken off so SPACE flips stay matched (short HALLs end ~1 dB up).
constexpr float kHallOutputTrim = 0.767f;
// DECAY keeps the loudness (v1.0.28, decay_level: C on every card, "levels should ideally be consistent
// across reverb types"). A tail's level falls with its T60 (music, wet LUFS): 2 → 15 s HALL −8.2 dB,
// MASSIVE −7.4, PLATE −3.2 (its early part carries more of it), so level ∝ T60^p; 15 s → the finite
// top HALL −0.4, MASSIVE −5.3 (a gentler p). It's all made up to the finite top's level, which the
// three SPACEs already share with each other and with infinite (within ~2 dB).
constexpr float kDecayLevelRefSeconds = 15.0f;
constexpr float kDecayLevelPower[3]     = {0.18f, 0.47f, 0.445f}; // PLATE, HALL, MASSIVE: below 15 s
constexpr float kDecayLevelPowerLong[3] = {0.0f, 0.09f, 0.29f};   // from 15 s to the finite top
// The finite tops the make-up above was measured to (v1.0.28): PLATE 12 s, HALL 25 s, MASSIVE 120 s. Since
// v1.0.53 (ADR 0034, the owner's pick B on renders/decay_range) PLATE's and HALL's tops are 30 / 60 s, at
// the level today's had: a tail longer than these gets no make-up, and the trim below.
constexpr float kDecayLevelTopSeconds[3] = {12.0f, 25.0f, 120.0f};
// The make-up rises at most this share of the tank's own decay rate (60 / T60 dB/s) at finite DECAYs (v1.0.55,
// the owner's pick B on renders/decay_swell): a long tail turned down fades at 0.7 of its new rate while the
// make-up catches up, never louder; played, the shorter tail grows in at the level it had.
constexpr float kDecayRiseShare = 0.3f;
// The longer tails' trim (v1.0.53, ADR 0034): PLATE / HALL ring longer than they did at every DECAY and
// sound louder while you play (the chord on renders/decay_range: up to +2.1 dB at 0.80), so the wet is taken down
// by up to this many dB, eased in across DECAY (a smoothstep from .. to) and back out across the crossover to
// infinite (x (1 - governor weight)^3: 0 dB at 0.85, so infinite is as before). Fitted to the page's level
// match (the owner heard B there at A's level while playing): within 0.45 dB of it on the chord, PLATE and
// HALL at SIZE 0.3 / 0.8 / 1.
constexpr float kDecayLongTrimDb[2]   = {1.95f, 2.10f}; // PLATE, HALL
constexpr float kDecayLongTrimFrom[2] = {0.15f, 0.35f};
constexpr float kDecayLongTrimTo[2]   = {0.85f, 0.75f};
// Still lows (v1.0.18, docs/research/still-lows.md): below this the HALL / MASSIVE tank's input goes to
// a second, unmodulated tank (Butterworth split), so the bass never wavers; above it moves as before.
constexpr float kStillLowsHz = 300.0f;
constexpr float kFloorStepSeconds  = 1.7f;

// Fixed damping (SPEC §4.6): the high end's T60 as a share of the low
// end's, at TONE noon. TONE moves it (Mappings.h toneHighT60Ratio).
constexpr float kDampingHighT60Ratio = 0.45f;
// MASSIVE's own share (v1.0.31, ADR 0022; owner's pick D on renders/massive_highs): × the Neve's
// darkening (EraVoicing kNeveHighRatio 0.7) it's 1, so at TONE noon its highs ring as long as its
// body, as Supermassive's Andromeda and Sagittarius do (they don't damp in the loop).
constexpr float kMassiveHighT60Ratio = 1.43f;

// At infinite DECAY the lows hold but the highs still fade, over about this
// long at TONE noon, so an infinite tail darkens toward its floor (ADR 0008)
// instead of ringing bright forever.
// Every tank's loss is a shelf crossing here (Governor.h absorbent): the upper
// mids fade with the highs (owner's pick on renders/tone_shelf, v1.0.15).
constexpr float kLossShelfHz = 2500.0f;

constexpr float kInfiniteHighT60Seconds = 30.0f;

// Input diffusion (SPEC §4.1): Dattorro's four input allpasses, his lengths
// (142, 107, 379, 277 samples at 29.761 kHz) in seconds; the right channel's
// are kDiffuserRightScale longer so the two sides decorrelate.
constexpr float kDiffuserSeconds[4] = {0.004771f, 0.003595f, 0.012735f, 0.009307f};
constexpr float kDiffuserCoeff[4]   = {0.75f, 0.75f, 0.625f, 0.625f};
constexpr float kDiffuserRightScale = 1.071f;
// In PLATE the first stage of each side wanders, L and R in opposite phase (v1.0.16): under a
// cent; the buffers have ~8 samples of spare room at 48 kHz.
constexpr float kDiffuserMotionSeconds = 0.00018f, kDiffuserMotionHz = 0.4f;

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
// at kEarlyTapLevel against the lines' ends. Six per line, the 8 lines' taps
// interleaved, at the same total energy as the three they replace (v1.0.21,
// owner's pick on renders/knob_feel: the swell no longer dips before its peak,
// "uneven on Hits"; it dipped 5.8 dB at large SIZE, now 3.5).
constexpr int   kEarlyTaps     = 6;
// Of them, in use (v1.0.36, ADR 0026; owner's pick B on renders/early_hits, for the CPU): HALL 2, MASSIVE 4.
constexpr int   kHallEarlyTaps    = 2;
constexpr int   kMassiveEarlyTaps = 4;
constexpr float kEarlyTapLow   = 0.3f;
constexpr float kEarlyTapLevel = 0.5f * 0.71f;
// MASSIVE's shortest DECAY rings at least this many passes of its mean line
// (v1.0.21, knob_feel: 3; at 2 s and a 0.84 s line one pass lost 25 dB, so DECAY's
// first notch jumped from "the build and one echo" to a real tail). 6 since v1.0.32
// (owner's pick E on renders/massive_drop): at 3 a short MASSIVE at a big SIZE lost ~14 dB a
// pass, held one pass after the input stopped and then dropped; at 6 (≤10 dB a pass) it fades
// smoothly, and the shortest MASSIVE is longer at big SIZEs (~7 s at SIZE 0.7, 12 s at full).
constexpr float kMassiveMinPasses = 6.0f;

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
// measured over kGovernorSeconds. -21 since v1.0.51 (ADR 0033, the owner's pick D on renders/level_duck:
// "on hall and massive with Long decays, I feel the limiter is keeping the level a bit too low"; was -24).
constexpr float kFloorDbfs          = -21.0f;
// HALL's endless tail settles this much above kFloorDbfs, so at -18 dBFS (v1.0.52, ADR 0033 amended: the owner's
// pick C on renders/hall_level, "Hall infinite examples feel slightly less level matched than massive ... reverb
// feels a bit too quiet so when the set stops, there's a less seamless transition").
constexpr float kHallTailDb         = 3.0f;
constexpr float kGovernorRatePerDb  = 0.08f;
constexpr float kGovernorSeconds    = 1.5f;
constexpr float kHoldSeconds        = 8.0f; // the slow level a held tail's ceiling is judged on
// A falling infinite tail is judged on this faster level too (soak audit: with the 1.5 s level
// alone it plunged far below the floor after a loud input stopped or faded).
constexpr float kGovernorFallSeconds = 0.05f;

// SPACE flips fade out, then in, over this long each (ADR 0013: ~100 ms
// in all). ATTACK into or out of SHIMMER fades the octave's send over kAttackFadeSeconds.
constexpr float kSpaceFlipSeconds  = 0.05f;
constexpr float kAttackFadeSeconds = 0.05f;

// The Plate's long delays d1 and d2 wander too (owner's pick on renders/plate_modulation, v1.0.12).
constexpr float kPlateLongModSeconds = 0.0006f; // depth: ~2 cents at these rates *(start)*
constexpr float kPlateLongModHz[4]   = {0.23f, 0.31f, 0.19f, 0.37f}; // L d1, L d2, R d1, R d2: none shared with ap1 *(start)*

// ---- SHIMMER (SPEC §4.11, ADR 0024; owner's picks on renders/shimmer4, attack_alts2) ----------
// The input an octave up, sent into the tank with it, no feedback (dsp/Shimmer.h). Measured (rounds
// 1-4): fold-back above 12 kHz heard as "bit crushing" (the 8th-order feed filter: -68 dB at 16 kHz);
// heads at fixed spacings combing certain notes' octaves (random onsets: within 2.6 dB across notes,
// the detuned copies -38 dB); in the reverb, no more off-pitch sound than the plain reverb.
constexpr int   kShimmerHeads         = 8;
constexpr float kShimmerWindowSeconds = 0.4f;    // each grain; its octave starts this smeared
constexpr float kShimmerScatter       = 1.0f;    // random extra delay per grain, x the window
constexpr float kShimmerFeedHz        = 6000.0f; // the feed's 8th-order low-pass: its octave reaches 12 kHz
constexpr float kShimmerAfterHz       = 9000.0f; // a one-pole after: each octave a little softer than the note
constexpr float kShimmerLowCutHz      = 80.0f;   // a one-pole high-pass after
constexpr float kShimmerSend          = 1.2f;    // x TIME: the octave's level into the tank at TIME full

// ---- Plate (SPEC §4.2) -----------------------------------------------------
constexpr float kPlateMaxScale     = 1.59f; // SIZE fully CW (1.6 lined a 9.8 kHz mode up, v1.0.11)
constexpr float kPlateMinScale     = 0.5f;  // SIZE fully CCW
// The Plate's tails measured ~9 % longer than DECAY's mapping (low-band
// T60, test_plate_freeze), so its loss rate is scaled up by this. At the
// very short end with SIZE up, one pass round the figure 8 (~0.55 s at
// full SIZE) is longer than the T60 asked for, so those tails stay longer
// than the mapping: a big plate can't ring for only 0.4 s.
constexpr float kPlateRateCalibration = 1.09f;
constexpr float kPlateOutputTrim   = 0.43f; // −7.3 dB: PLATE matches HALL's loudness (test_plate_freeze; +0.4 dB at v1.0.28, DECAY's make-up)

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
// freeze_twin): what you play over a pad goes into a twin (PLATE: a second Plate; HALL and MASSIVE: a small
// room since v1.0.43; SIZE and DECAY playing it), heard as a quieter layer over
// the untouched pad, fading back out within kTwinMaxT60 so the pad returns
// to what it was. No predelay or swell on its input ("too much
// predelay... disjointed"). (start)
constexpr float kTwinLevel     = 0.5f;  // −6 dB against the pad (was 0.3, −10.5 dB): "the sweet spot is between plus3 and plus6" (owner, renders/twin_level, v1.0.14)
constexpr float kTwinMaxT60    = 8.0f;  // it always fades, even when DECAY is infinite
// HALL's and MASSIVE's twin is a small room (v1.0.43, the owner's pick E on renders/small_room; dsp/TwinRoom.h), at
// these gains on its output (dB, then kTwinLevel): matched on the page to v1.0.41's twin, which sat about 8 dB under
// a frozen pad (owner, renders/twin_louder: "Let's shoot for 8db under the pad"). PLATE's twin is a second Plate.
constexpr float kTwinRoomHallDb    = -8.63f;
constexpr float kTwinRoomMassiveDb = -7.65f;
// Elsewhere it follows the pad (v1.0.43, the owner's pick B on renders/room_level; Engine::applySpace): v1.0.41's lift
// for a twin cut short, up to this (+5.4 dB), is where the gains above hold; a room under a shorter tank tail is that
// much under them.
constexpr float kTwinRoomLift = 1.862f;
// v1.0.47: the room's wander scaled with its lines (owner's pick B, renders/room_motion) made it louder on short lines;
// the page level-matched B to the room as before, and the owner chose it at that level: these trims on the room's
// output at SIZE 0 / 0.5 / 0.9 (dB, straight between them, held above 0.9).
constexpr float kTwinRoomTrimDb[3] = {-0.93f, -0.88f, -0.53f};
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

// ---- Ducking: 2:1 compression of the wet (ADR 0014 amended, SPEC v1.0.17, v1.0.51) --
// The wet is compressed above kDuckThresholdDbfs of input (its ~10 ms
// power), with this attack and release, scaled by how far into infinite
// DECAY is (0 below 0.85, full at 1) and off while frozen. No darkening: the
// owner picked it over DARKEN on renders/ducking ("closer to 4:1 compression
// than extreme ducking, so it feels cohesive"; D on all 18 cards). 2:1 since
// v1.0.51 (renders/level_duck D: "ducking on large washy sounds still feels a
// bit heavy handed"; was 4:1).
constexpr float kDuckThresholdDbfs  = -24.0f;
constexpr float kDuckRatio          = 2.0f;
constexpr float kDuckAttackSeconds  = 0.03f;
constexpr float kDuckReleaseSeconds = 0.4f;
constexpr float kDuckFullDb         = 6.0f; // reduction the LEDs show as full ducking
// The duck follows the playing (v1.0.51, the owner's pick E on renders/level_glue: "glue the dry and wet
// at points of transition so they sound more integrated rather than two separate layers"). Held playing
// (a chord, a pad: the input's ~10 ms power within kDuckSustainCrest, 3 dB, of its kDuckSlowSeconds power,
// judged while over the threshold and smoothed over kDuckSustainSeconds) is ducked less (x (1 -
// kDuckSustainCut)) and let go faster (kDuckSustainReleaseSeconds) when it stops; short sounds (stabs)
// keep the full reduction and kDuckReleaseSeconds, so they don't pump. A program-dependent release
// (Giannoulis, Massberg and Reiss, JAES 2012).
constexpr float kDuckSlowSeconds           = 0.5f;
constexpr float kDuckSustainSeconds        = 0.3f;
constexpr float kDuckSustainCrest          = 1.99526231f; // 3 dB, as a power ratio
constexpr float kDuckSustainCut            = 0.6f;  // held playing gets 40 % of the reduction
constexpr float kDuckSustainReleaseSeconds = 0.08f;

// ---- Output soft limiter (SPEC §4.10): the wet only, before MIX ------------
constexpr float kLimitCeiling        = 0.891f; // −1 dBFS
constexpr float kLimitReleaseSeconds = 0.2f;

// Predelay memory: SIZE's (Massive) + TIME's (NORMAL), SPEC §5.2.
constexpr float kPredelayMaxSeconds = 0.75f;

} // namespace iv::voicing
