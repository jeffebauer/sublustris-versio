#pragma once
// Numbers for the loop's character (SPEC §4.6, §4.7; ADR 0005, 0012): DRIFT,
// the always-on soft saturation, and ERA's Neve base and wear.

namespace iv::voicing {

// ---- DRIFT (P6), bipolar (ADR 0012) ----------------------------------------
constexpr float kDriftDeadLo = 0.47f, kDriftDeadHi = 0.53f; // noon: only the modulation floor
// Left: tape wow (random, 0.2 → 1 Hz) and flutter (5 → 9 Hz), cents at full.
constexpr float kWowCents = 15.0f, kWowHzMin = 0.2f, kWowHzMax = 1.0f;
constexpr float kFlutterCents = 3.0f, kFlutterHzMin = 5.0f, kFlutterHzMax = 9.0f;
// Right (v1.0.60, ADR 0036, the owner's pick F on renders/dip_motion): the radio band on the wet (Mappings.h
// radioBand, dsp/EraTexture.h), no pitch movement. Wander (v1.0.20-v1.0.59) is gone.
// v1.0.46: the left side's tape transport on the wet (dsp/Transport.h; owner's picks round 3's E with the confirm
// page's B, renders/drift_rethink), at full left before its variance: one shared wow and flutter, crinkle bursts
// a second over the side's last 40 %, and the share of them bringing a dip (about a third of round 2's crinkle dips).
constexpr float kTransportWowCents = 12.0f, kTransportWowHzMin = 0.5f, kTransportWowHzMax = 1.2f;
constexpr float kTransportFlutterCents = 3.0f, kTransportFlutterHzMin = 6.0f, kTransportFlutterHzMax = 10.0f;
constexpr float kTransportCrinklePerSecond = 0.6f, kTransportDipShare = 0.43f;
// v1.0.59 (owner's pick C, "from 7:30", on renders/motion_dips; their note: "it'd be good if the dropouts start
// entering closer to fully CCW instead of ~9 o'clock"): the dips no longer come in with the crinkle (MOTION 0.188,
// ~9 o'clock) but from here, about 7:30 on the module (the pots' end stops map raw 0.025-0.975 to 0-1,
// firmware/PotEndStops.h), their share rising straight to kTransportDipShare at fully left (the same there).
constexpr float kTransportDipsFrom = 0.026f;
// The tank's own per-line wow and flutter keep this share on the left side: at full, its incoherent ~20 cents
// buried the transport's coherent movement (docs/research/drift-rethink.md).
constexpr float kTransportLineShare = 0.33f;
// A line's drift excursion never exceeds this share of its own length (a
// 24 ms HALL line can't swing by 65 ms), nor kDriftMaxSeconds.
constexpr float kDriftMaxShareOfLine = 0.35f;
constexpr float kDriftMaxSeconds = 0.07f;

// ---- Soft saturation, always on in the loop (SPEC §4.6) -------------------
// Linear below kSatKnee × kSatCeiling, then a smooth curve to ±kSatCeiling
// (internal full scale 1). Era.h softClip.
constexpr float kSatCeiling = 1.2f;
constexpr float kSatKnee    = 0.7f; // a held tail's peaks (crest ~4-5) stay under it; only a hot tank saturates

// ---- ERA (P4): one wear amount (v1.0.42, the owner's pick C on renders/era_rethink) --------------
// The Neve is the base, always on in the loop; the wear is on the wet (dsp/EraTexture.h, its own numbers
// in tex::), coming in at the start of the knob and deepening with it (Mappings.h eraMix).

// Neve (SPEC §4.7): a little darker (16 kHz in the table, here a shorter
// high-end T60); gentle asymmetric, even harmonics.
constexpr float kNeveHighRatio = 0.7f;   // × the high end's T60 (the Neve's darkening, Era.h)
constexpr float kNeveEven = 0.12f;  // x² share into the soft clip (DC removed in the loop)

// The tape's slow 2:1 compression above −18 dBFS (as extra loop loss) and its hiss into the tank's
// input, up to −66 dBFS, following the input level (ADR 0005), each × ERA's wear.
constexpr float kCompressThresholdDb = -18.0f;
constexpr float kCompressRatePerDb = 0.05f;      // loss (1/T60, 1/s) per dB over the threshold
constexpr float kHissDbfs = -66.0f;
constexpr float kHissFollowFromDbfs = -36.0f;    // input at or above this: full hiss; silence: none
// The Midiverb's sparse crackle into the tank's input, following the input level, × ERA's wear.
constexpr float kCrackleDbfs = -48.0f;
constexpr float kCracklePerSecond = 4.0f;

// The Neve base's loudness makeup (dB on the wet), measured on music (LUFS over the twelve page
// materials, HALL and MASSIVE; v1.0.19). The wear's own is in Mappings.h eraMix.
constexpr float kMakeupNeveDb = 0.2f;

// Where WEAR's grit (the grain; the BBD's clock too until v1.0.64) starts on the knob (v1.0.49, renders/wear_grit D).
constexpr float kWearGritStart = 0.65f;

} // namespace iv::voicing
