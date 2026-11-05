# 0044 — WEAR's taper: the wear arrives earlier on the knob, the grit from noon

**Status:** Accepted, 5 Nov 2026 (owner, pick D on all 10 cards of `renders/wear_taper`). SPEC v1.0.69. Amends ADR 0041 (the path to full WEAR, not full WEAR itself) and the v1.0.49 grit start (`renders/wear_grit` D: 0.65 → 0.5).

**Context:**
- **lfad on Modwiggler, 4 Nov 2026:** WEAR is "very subtle until you get past noon, and then still relatively mild until you max it out, at which point it becomes what I would normally want to hear around the noon mark". They want it mild but noticeable early, clearly there at noon, and strong toward the end.
- **Full WEAR was settled** in v1.0.66 (ADR 0041: "the saturation is too extreme now" before it), so it had to stay exactly as it is. Only the path there could change.
- **Before:** after a smoothstep over the first twentieth, the intensities (the tape's drive and emphasis, the BBD's breathing, the in-loop compression, the hiss, the crackle) grew in proportion to the knob: the tape's drive 5.6 dB at noon, 11.25 at full. The grit started at 0.65 (about half past one).
- **The page** (`renders/wear_taper`, prototype branch commit 1cdc109, the Renderer's `IV_WEAR_TAPER`; 100 % wet; pad, piano and drums; HALL and PLATE at DECAY 0.6 with WEAR 9 o'clock, noon, 3 o'clock and full, and both at endless with WEAR noon). Every version equals A at WEAR 0 and 1 (checked on `eraMix`'s outputs and on the full cards, to the bit). The level hold kept every version within 0.18 dB of A, so no card was level-matched. Wear is measured as the difference against the same setting at WEAR 0, dB re that render:
  - A, as built.
  - B, 1 − (1 − v)^1.6: +1.0 dB of wear at 9 o'clock, +1.5 at noon, +1.3 at 3 o'clock.
  - C, 1 − (1 − v)^2.2: +2.0, +2.5 (+2.9 at endless) and +1.8. 3 o'clock lands within 0.3 dB of full.
  - D, C with the grit from 0.5. It is the same as C at noon and below, so it differs only on the 3 o'clock cards.

**Decision:**
- **D is the sound** (`map::eraMix`): the intensities follow w = 1 − (1 − WEAR)^`voicing::kWearTaperPower` (2.2), and the grit starts at `voicing::kWearGritStart` 0.5 on the knob itself. Firmware, Plugin and Renderer share the code; the prototype hook is gone.
- **Full WEAR is unchanged** to the bit, and so is WEAR 0.

**Rejected:**
- **A:** too subtle until past noon (lfad).
- **B:** gentler; the owner picked D on every card.
- **C:** D's taper with the grit at 0.65; the owner preferred the grit from noon on the 3 o'clock cards.

**Consequences:**
- **Sound:** every mid-WEAR setting is more worn. Noon is about where the old 0.78 was (the tape driven 8.8 dB, was 5.6), and the grit is in from noon. WEAR 0 and 1 are as before; `tools/same_render.py` against v1.0.68 differs only on mid-WEAR cases.
- **Flash:** +16 B release (111,844 of the 112,640 budget), +72 B profile (131,028, 44 B under the chip). `powf` was already linked (`dsp/EraTexture.h`).
- **Words:** SPEC §3 and §4.7, `CONTEXT.md` and the manual's WEAR section say the wear comes in early and the grit from noon.
