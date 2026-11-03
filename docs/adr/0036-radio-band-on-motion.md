# 0036 — The radio band moves from WEAR to MOTION's right side; Wander goes

**Status:** Accepted, 1 Nov 2026 (owner, pick F, "band alone", on all 18 judged cards of `renders/dip_motion`: drums, keys and pad at WEAR 0.5 and 1, MOTION ¾, ⅞ and full; MIX 78 %, level-matched; the six repeated pad cards agreeing both times). SPEC v1.0.60. Amends ADR 0027 (WEAR loses its band) and ADR 0030 (MOTION's right side is no longer Wander).

**Context:**
- **The owner, 1 Nov 2026:** "the AM radio notch filtering that reads as phasing. I really like this sound, but I'm wondering if it would be better suited replacing the right side of the motion knob in place of the wander. The wander is still pretty subtle, and I like the idea of having independent control over that phasing-like effect, leaving the wear knob more focused on saturation-like effects." Then, of the radio's band-pass (~300 Hz-3 kHz): "what if that narrowing moved to the right side of the motion knob as well?" And, after the first page: "I want dip+band, dip+band+cuts examples to hear alongside those with wander".
- **The research** (`docs/research/dip-motion.md`):
  - Reverbs put their phase-like movement inside the reverb, not on its wet; a phaser has two or more notches.
  - Inside the radio's narrow band, one moving notch reads as phasing because the band's edges act as the other notches. On the full range it can read as a wandering EQ.
- **The page:** seven versions.
  - A, as built.
  - B, the dip alone on the full range.
  - C, Wander and the dip.
  - D, Wander and the whole band.
  - E, Wander and three moving cuts.
  - F, the whole band alone.
  - G, the band with the cuts.
  - The owner picked F everywhere.

**Decision:**
- **MOTION's right side is the AM radio's band**, on the wet as before (`dsp/EraTexture.h`'s last stage): the gentle two-pole band-pass, its over-modulation and its wandering dip (−4 → −18 dB, 0.1 → 0.5 Hz, Q 1.5 → 3, centre 300 Hz-3 kHz). Every number of the band is as it was on WEAR.
- **Its layout on MOTION** (`map::radioBand`) is as it came in on WEAR, across the whole side:
  - `side` runs 0 at the dead zone's edge (0.53) to 1 fully right;
  - its blend is a smoothstep over the side's first twentieth × side^1.5;
  - its intensity is `side`.
- **At infinite DECAY** it keeps half its blend (`tex::kBandAtInfinite`), as on WEAR. *(Amended v1.0.61, below: whole at endless too.)*
- **Its loudness:** the wear chain's level hold, as before, plus its ~0.7 dB on music, now following its blend on MOTION.
- **No Wander.** The tank's lines move no more on the right than at noon. The modulation floor, the small room's and PLATE's own wanders, the dead zone (0.47-0.53) and the left side (the tape transport) are unchanged.
- **WEAR is tape, breath, clock and grain** over the Neve, with its in-loop compression, hiss and crackle. It no longer narrows the tone.
- **The over-modulation gets a soft knee:**
  - Before, a hard clamp at −1 of the carrier.
  - Now it is linear down to −0.5 and curves towards −1.25 (`tex::kOverModKnee`, `kOverModFloor`; slope 1 at the knee, as the breath's in v1.0.52).
  - Why: the clamp's corner ticked on a clean, still PLATE once the band no longer came with WEAR's tape and grit around it: +13.9 dB on the 1 ms measure, today's worst there +10.4.
- **The plugin** shows the right side as "Radio n %" (was "Drift n %").

**Consequences:**
- **Sound:**
  - At MOTION right of noon, the tail is narrowed and phases instead of wavering in pitch.
  - At noon and on the left, WEAR's sound loses the band (fuller lows and highs at high WEAR).
  - Equal to the page's F to the bit before the knee (`tools/same_render.py` against the prototype at `ae2e601` with `IV_DIP_MOTION=F`: SAME 60/60). The knee moves the sound at most 19.6 dB under F (keys in MASSIVE at full right); a still PLATE ticks at +11.0 dB (F +13.9).
- **The band is still a narrowing:** MOTION thins the tone on its right side, which used to be TONE's and WEAR's. The owner heard that on the page and picked it.
- **Loudness** against v1.0.59, LUFS over five materials × three SPACEs:
  - Medians within ±0.6 LU at every WEAR and MOTION.
  - Drums move most: −2.2 to −3.6 LU at MOTION full, where the band now is, and +1.9 to +3.1 LU at WEAR 1, where it no longer is. The band takes the kick's lows, which its level hold (above 200 Hz) doesn't see; on WEAR it cost drums the same 2.0-3.3 LU.
- **Residual:** keys in MASSIVE at full right with WEAR 0.5 still reach +15.7 dB on the 1 ms measure (F +21.1; v1.0.59 at the same settings +9.1). It is the over-modulation on a hot wet, not the corner: no knee shape tried took it under +13.5.
- **Flash:** release 108,748 B (−24 B: Wander's mapping and rate spread out, the knee in).
- **Tests:**
  - `test_motion`: the right side's stillness (a held tone within 2.5 cents of noon) and its band mapping; MOTION's right end judged for growth, since the band's hold catches up after a burst as it did on WEAR.
  - `test_era`: WEAR has no band.
  - Goldens re-recorded.
- **Removed:** Wander (`kWander*`, the right side of `map::driftAmounts`, `DriftAmounts::rateSpread`), WEAR's band blend in `map::eraMix` and its makeup field (`EraWear::makeup`), and the prototype hook `IV_DIP_MOTION` (never on main).

**Not picked (heard on the page):**
- A (as built).
- B (the dip alone on the full range, no Wander).
- C (Wander with the dip over the side's last half).
- D (Wander with the band over the side's last half).
- E (C with three moving cuts, a phaser's notches).
- G (F with the three cuts on top).

**Amendment (2 Nov 2026, the owner at the rack on v1.0.61; SPEC v1.0.61; ADR 0008 amended):** **the band is whole at infinite DECAY too.** With MOTION's right side near full the owner heard "an audible difference between endless and non-endless tails, as if the tone shaping of the radio is only applying to one or the other": the band kept half its blend at endless (`tex::kBandAtInfinite`, carried over from WEAR), so endless had 11-12 dB more lows and highs than DECAY 0.80 at MOTION full, and the level stepped up to 12.3 dB across 3 o'clock. `kBandAtInfinite` is gone. Its reason (a held tail darkening to the floor, the band taking 4-6 dB off a 10-minute hold) no longer holds since endless follows the playing: 10-minute holds at MOTION 0.9 and full stay within 0.3 dB on noise in every SPACE.

**Amendment (3 Nov 2026, owner's pick C on every card of `renders/radio_lighter`; SPEC v1.0.67; ADR 0042):** the over-modulation's drive is 8 dB lower across MOTION's right side, so only the biggest peaks touch its knee: "a faint edge". The band and its wandering dip are unchanged.
