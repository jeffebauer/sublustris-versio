# 0042 — The radio's over-modulation lighter: a faint edge

**Status:** Accepted, 3 Nov 2026 (owner, pick C, "faint edge", on all 15 judged cards of `renders/radio_lighter`, the repeated drums cards agreeing). SPEC v1.0.67. Amends ADR 0036.

**Context:**
- **The owner, 3 Nov 2026:** "When RADIO is turned all the way up (MOTION fully CW), in addition to the phasing, I also hear a saturation-like sound even when the WEAR knob is fully CCW."
- **The cause:** the radio band's over-modulation (`band()` in `dsp/EraTexture.h`: a soft knee flattening the troughs, v1.0.60). At MOTION fully right it drove the tail about 12 dB into the knee, so 2.4-6.7 % of a HALL or PLATE tail and up to 11.7 % of CANYON's passed it. Its products sat 19-28 dB under the tail (13-19 dB at worst over 100 ms; CANYON 15-18). On WEAR, before v1.0.60, the tape and grit masked them; on MOTION alone they show.
- **The page** (100 % wet, pad, keys and drums; HALL at MOTION full and 7/8, a still PLATE at full, CANYON at full, HALL at full with WEAR 2 o'clock; all within 0.05 dB of A):
  - A, as built.
  - B, the drive into the knee 4 dB lower (about half the grit).
  - C, 8 dB lower: only the biggest peaks touch the knee.
  - D, no over-modulation: the band and the notch alone.

**Decision:**
- **C is the sound:** `tex::kOverModTrimDb` −8 dB on the over-modulation's drive (`mEff_`), across MOTION's whole right side. The band, the notch and their movement are unchanged.

**Rejected:**
- **A:** too much grit with WEAR off.
- **B:** still more than "a faint edge".
- **D:** the owner kept a trace of the over-modulation.
- **A softer knee:** real AM over-modulation is the carrier cutting off at 100 %; lighter means modulating less deeply, which a lower drive does.

**Consequences:**
- **Sound:** at most 0.1 % of the tail reaches the knee (0.7 % in CANYON); the products sit about 45 dB under on PLATE keys and drums, 34-44 dB in CANYON, and vanish on a HALL pad. Keys at fully right tick less: worst 1 ms frame PLATE +19.5 → +12.1 dB, HALL +19.6 → +11.2, CANYON +17.6 → +11.9. Loudness is unchanged (WEAR's level hold). MOTION left of the dead zone, and WEAR, are unchanged to the bit.
- **Renders** equal the page's C: `tools/same_render.py` SAME 60/60 against the prototype with `IV_RADIO_LIGHT=C` (branch `proto/radio-lighter`, not merged).
