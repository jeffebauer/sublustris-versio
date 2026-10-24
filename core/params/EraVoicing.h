#pragma once
// Numbers for the loop's character (SPEC §4.6, §4.7; ADR 0003, 0004, 0005,
// 0012): DRIFT, the always-on soft saturation, and ERA's five stacked
// stages. Starting guesses (SPEC §4.7's table) until the owner tunes them.

namespace iv::voicing {

// ---- DRIFT (P6), bipolar (ADR 0012) ----------------------------------------
constexpr float kDriftDeadLo = 0.47f, kDriftDeadHi = 0.53f; // noon: only the modulation floor
// Left: tape wow (random, 0.2 → 1 Hz) and flutter (5 → 9 Hz), cents at full.
constexpr float kWowCents = 15.0f, kWowHzMin = 0.2f, kWowHzMax = 1.0f;
constexpr float kFlutterCents = 3.0f, kFlutterHzMin = 5.0f, kFlutterHzMax = 9.0f;
// Right: Wander (v1.0.20, owner's pick on renders/drift_cw; docs/research/drift-cathedra-side.md):
// quick, small random movement, as Lexicon's Random Hall and Valhalla's Random Space do, the
// lines' rates fanned out ±50 % so they never sway together; 1 → 2.25 Hz, cents at full.
// (Until v1.0.20: a slow sine per line, 0.05 → 0.3 Hz, ±35 cents: "too seasick".)
constexpr float kWanderCents = 20.0f, kWanderHzMin = 1.0f, kWanderHzMax = 2.25f, kWanderRateSpread = 1.0f;
// A line's drift excursion never exceeds this share of its own length (a
// 24 ms HALL line can't swing by 65 ms), nor kDriftMaxSeconds.
constexpr float kDriftMaxShareOfLine = 0.35f;
constexpr float kDriftMaxSeconds = 0.07f;

// ---- Soft saturation, always on in the loop (SPEC §4.6) -------------------
// Linear below kSatKnee × kSatCeiling, then a smooth curve to ±kSatCeiling
// (internal full scale 1). Era.h softClip.
constexpr float kSatCeiling = 1.2f;
constexpr float kSatKnee    = 0.7f; // a held tail's peaks (crest ~4-5) stay under it; only a hot tank saturates

// ---- ERA (P4): where each stacked layer comes in (ADR 0003) ----------------
constexpr float kEraNeveFrom = 0.10f, kEraCassetteFrom = 0.35f, kEraMuFrom = 0.65f, kEraMidiverbFrom = 0.85f;
// Neve's share grows in straight steps from kEraCleanTop to kEraCassetteFrom (v1.0.21, owner's pick on
// renders/knob_feel: ERA's first 20 % changed nothing measurable; Clean was 0 → 0.10, then a smoothstep).
constexpr float kEraCleanTop = 0.03f;

// Neve (SPEC §4.7): a little darker (16 kHz in the table, here a shorter
// high-end T60); gentle asymmetric, even harmonics.
constexpr float kNeveHighRatio = 0.7f;   // × the high end's T60 (the era's darkening, Era.h)
constexpr float kNeveEven = 0.12f;  // x² share into the soft clip (DC removed in the loop)

// Cassette: much darker (9 kHz in the table); tape drive into the soft clip; a slow
// 2:1 compression above −18 dBFS (as extra loop loss); hiss into the tank's
// input, up to −66 dBFS, following the input level (ADR 0005); wow and
// flutter added to DRIFT's left side.
constexpr float kCassetteHighRatio = 0.4f;
constexpr float kCassetteDrive = 1.4f;           // × into the soft clip at full cassette (1.8 pushed held tails' peaks into the knee)
constexpr float kCompressThresholdDb = -18.0f;
constexpr float kCompressRatePerDb = 0.05f;      // loss (1/T60, 1/s) per dB over the threshold
constexpr float kHissDbfs = -66.0f;
constexpr float kHissFollowFromDbfs = -36.0f;    // input at or above this: full hiss; silence: none
constexpr float kCassetteWowCents = 4.0f, kCassetteWowHz = 0.55f;
constexpr float kCassetteFlutterCents = 1.0f, kCassetteFlutterHz = 7.0f;

// μ-law (ADR 0004): the wet output at 24 kHz, 12 → 10 bits across the stage.
constexpr float kMuLaw = 255.0f;
constexpr float kMuBitsStart = 12.0f, kMuBitsEnd = 10.0f;
constexpr float kMuFullScale = 1.0f;
constexpr float kMuEngageSpan = 0.03f; // its crossfade in, on the ERA knob

// Midiverb (ADR 0004, amended v1.0.16): in the loop, a 48 → 24 → 48 kHz box
// at the real unit's 13 bits, truncated toward zero (a fading tail can't hold
// a buzz), with its ½ one-pole low-pass every pass (emeb/MIDIVerb_RE; it was
// 12 → 8 bits, a bitcrusher: owner's pick on renders/midiverb); sparse crackle
// into the tank's input, following the input level.
constexpr float kMidiBitsStart = 13.0f, kMidiBitsEnd = 13.0f;
// The box engages across the Midiverb's whole stretch as (share)² (v1.0.21, knob_feel: it switched on
// within 2 % at 0.85 and a tail got ~5× shorter in one notch, then the last 13 % did nothing).
constexpr float kMidiEngagePower = 2.0f;
// At infinite DECAY the box keeps only this share of its dark loop (v1.0.23, owner's pick on
// renders/midiverb_hold round 2, "brighter fading" on all 16 cards): the whole loop turned a held tail
// into a low drone (a held 1 kHz tone gone within a minute); none let ERA's per-pass grain pile up in the
// highs (+18 dB of hiss above 5 kHz in a minute). A quarter: brighter, a held tone fades ~0.25 dB/s.
constexpr float kMidiDarkAtInfinite = 0.25f;
constexpr float kCrackleDbfs = -48.0f;
constexpr float kCracklePerSecond = 4.0f;

// Vintage reads (v1.0.22, ADR 0019): from the μ-law stretch on (Fdn::setVintage). The 32 steps follow
// Costello on the 224; the jitter's size is ours, tuned on the measured noise halo (docs/research/dsp-audit.md E4).
constexpr float kVintageSteps = 32.0f;          // fraction steps per sample (Costello: "32 or 64 chunks")
constexpr float kVintageJitterSamples = 0.15f;   // the noisy modulation's random offset per control block

// Tape out (v1.0.22, ADR 0019). Its asymmetry is a third of the first one's (v1.0.26, owner's pick on
// renders/tape_bias: the full bias bent even quiet held notes, a 2nd harmonic 14-22 dB under them; at a
// third it's 24-32 dB under, the tape's even warmth kept on peaks).
constexpr float kTapeBias = 0.05f;          // × the cassette layer (was 0.15)
// v1.0.27 (owner's pick on renders/tape_out, E on all 24 cards): pre-emphasis, drive and light hysteresis
// (dsp/Tape.h), each at full with the cassette layer.
constexpr float kTapeEmphHz = 2000.0f, kTapeEmphDb = 6.0f, kTapeDriveDb = 12.0f;
constexpr float kTapePlayWidth = 0.15f;     // the play operator's width × the 50 ms level × the drive
// (A tape that hears a steady level, driving quiet wets harder into the curve, was tried and turned down:
// A on 12 of 15 cards of renders/tape_level, "jittery saturation" in long tails.)

// Loudness makeup per layer (dB on the wet), so ERA's loudness on music stays
// within ±1 dB across the knob (SPEC §7 M6, v1.0.19), summed as the layers stack.
// Measured on music (LUFS over the twelve page materials, HALL and MASSIVE): the
// stages themselves barely lose level (Neve −0.2, cassette −0.2, μ-law 0, Midiverb
// −0.7 dB in HALL, less in MASSIVE). Until v1.0.19 these were set on noise-burst
// tails (0.6 / 1.4 / 2.4 / 1.3 dB), which made music 4–6 LU louder at the end.
constexpr float kMakeupNeveDb = 0.2f, kMakeupCassetteDb = 0.2f, kMakeupMuDb = 0.0f, kMakeupMidiverbDb = 0.4f;

// era_v2 prototype: the new eras' level against the Neve base on music (set from LUFS, era_v2 page).
constexpr float kEraV2BbdTrim = 1.0f, kEraV2SamplerTrim = 1.0f, kEraV2RadioTrim = 0.89f; // radio: its AGC lifts music ~1 LU

} // namespace iv::voicing
