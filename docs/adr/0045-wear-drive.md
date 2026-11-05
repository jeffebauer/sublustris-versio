# 0045 — The tape's drive back to 16.25 dB, on the new taper

**Status:** Accepted, 5 Nov 2026 (owner, pick C on all 28 cards of `renders/wear_drive`, the repeated piano cards agreeing). SPEC v1.0.70. Reverses ADR 0041's drive (on ADR 0044's taper) and amends ADR 0044.

**Context:**
- **The owner, 5 Nov 2026, after picking ADR 0044's taper:** they wanted "to increase the saturation ceiling as well, returning it to the strength it was before we reduced it". That means `tex::kTapeDriveDb` back to v1.0.65's 16.25 dB (ADR 0040; ADR 0041 had taken it to 11.25).
- **On ADR 0044's taper** the drive in dB is `kTapeDriveDb` × w, with w = 1 − (1 − WEAR)^2.2. At 16.25 dB, noon is 12.7 dB and 3 o'clock is 15.8. On v1.0.65 (no taper) 3 o'clock was 13 dB, which the owner found "folding back too much" (ADR 0041).
- **The page** (`renders/wear_drive`, prototype commit 88a5148, the Renderer's `IV_WEAR_DRIVE`; 100 % wet; pad, stabs, piano and drums; HALL and CANYON at WEAR noon, 3 o'clock and full, DECAY 0.6, and CANYON endless at WEAR full). The level hold kept every version within 0.19 dB of A, so no card was level-matched.
  - A, v1.0.69: 11.25 dB at full.
  - B, 13.75 dB.
  - C, 16.25 dB.
  - D, as A up to WEAR 0.75, then a smoothstep up to 16.25 at full.
- **Measured on the pad** with ADR 0041's method (the tape alone in numpy on the card's WEAR-0 wet, 8x oversampled). The figures are the clip's products against the output in dB, then the tail's crest in dB:

| Card | A | B | C | D |
|---|---|---|---|---|
| HALL noon | −25.1 / 13.3 | −21.7 / 13.2 | −18.2 / 12.9 | = A |
| HALL 3 o'clock | −20.5 / 13.3 | −16.3 / 13.0 | −11.9 / 12.5 | −19.6 / 13.2 |
| HALL full | −19.8 / 13.3 | −15.5 / 13.0 | −11.0 / 12.4 | = C |
| CANYON noon | −16.6 / 13.0 | −12.9 / 12.3 | −9.5 / 11.4 | = A |
| CANYON 3 o'clock | −12.2 / 12.5 | −8.1 / 11.4 | −4.3 / 10.4 | −11.3 / 12.2 |
| CANYON full | −11.5 / 12.4 | −7.4 / 11.3 | −3.6 / 10.3 | = C |
| CANYON endless full | −13.5 / 11.3 | −9.0 / 10.4 | −4.9 / 9.4 | = C |

  For reference, v1.0.65's HALL pad at 3 o'clock, "folding back too much", read −16.1 dB (ADR 0041; v1.0.62's −23.4). B is about there at 3 o'clock; C is well past it, and so is its noon on CANYON.
  On the CANYON drum cards, C's sharpest 1 ms frames read 17–19 dB over their neighbours (A 9–11): that is the tape biting on the hits, not a click (the WEAR sweeps read 10–11 dB in every version).

**Decision:**
- **C is the sound:** `tex::kTapeDriveDb` 16.25 dB (`dsp/EraTexture.h`), on ADR 0044's taper. The prototype hook is gone. Firmware, Plugin and Renderer share the code.
- **Chosen knowingly:** 3 o'clock is now hotter than the v1.0.65 setting the owner called "folding back too much". They heard that on the page and picked C on every card. The rack check confirms it on the module.

**Rejected:**
- **A:** the ceiling the owner asked to raise.
- **B:** halfway.
- **D:** only at the far end; the owner picked C on the 3 o'clock cards too.

**Consequences:**
- **Sound:** WEAR's tape is driven harder everywhere past the start of the knob: up to 4 dB more at noon and 5 dB more at 3 o'clock and full. WEAR 0, the grit, the breath and MOTION are unchanged.
- **Aliasing:** `test_era`'s hot 7 kHz tone at WEAR 0.75; its limit is raised again (see the test's comment for the reading).
- **Flash:** no change (one constant).
- **Words:** the README, the manual and the release read-me say again that the tape is driven hard, now "in the knob's upper half".
