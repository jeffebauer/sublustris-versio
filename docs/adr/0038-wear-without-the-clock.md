# 0038 — WEAR without the BBD's clock

**Status:** Accepted, 2 Nov 2026 (owner, pick C, "no clock", on all 12 cards of `renders/wear_no_bbd`). SPEC v1.0.64. Amends ADR 0027.

**Context:**
- **The owner, 2 Nov 2026:** "The aliasing can feel a bit static in a way that it didn't when paired with the radio. And I think the tape effect is cleaner to describe and thematically cohesive when tape and bbd aren't combined as they're really from multiple eras." Then: "I'm not saying move the bbd to motion; rather just remove it from wear."
- **What WEAR's BBD was:** two stages on the wet (`dsp/EraTexture.h`).
  - The breath: the NE570 compander's 2:1 / 1:2 with its noise, so level and noise breathe with the playing. It comes in from the start of the knob, with the tape.
  - The clock: a sample-and-hold at 32 → 28 kHz with no anti-alias or reconstruction filter, so the reverb's top folds back (aliasing) and the steps add a fizz. It came in from WEAR 0.65 (v1.0.49) with the grain.
- **Until v1.0.60** the radio's band sat after the clock on WEAR and narrowed the top the clock folded; on its own (the band went to MOTION, ADR 0036) the folded top stays still under the tail.
- **The page** (`renders/wear_no_bbd`, blind, 100 % wet): pad and stabs in six settings: HALL at WEAR noon and 2 o'clock, each with MOTION at noon and at the cassette (about 9 o'clock); CANYON at WEAR full; HALL endless at WEAR 2 o'clock.
  - A, today.
  - B, no BBD: neither the clock nor the breathing.
  - C, no clock: the breathing stays.
  - At WEAR noon C was A to the bit (the clock came in only from 0.65), so those four cards didn't tell A from C.

**Decision:**
- **WEAR loses the clock stage for good:** its code, its constants (`tex::kClockHzHi` / `Lo`) and its mapping (`TexMix::clockW` / `clockT` in `map::eraMix`) are removed, not weighted to zero.
- **WEAR is tape, breath and grain** over the Neve, with the tape's compression, hiss and crackle. Every one of them, and the level hold, is exactly as in v1.0.62.
- **Nothing moves to MOTION.**

**Rejected:**
- **B, the whole BBD out:** the breathing also takes 3-7 dB off the top at WEAR noon (its own noise and harmonics); the owner kept it on every card.
- **Moving the BBD to MOTION:** the owner ruled it out.

**Consequences:**
- **Sound:** on the page's cards the energy above 8 kHz is 3-5 dB lower at WEAR 2 o'clock and 12-13 dB lower at WEAR full (most of what was up there was the folding); loudness within 0.05 dB (the level hold). Below WEAR 0.65 nothing changes. A 1 kHz tone at WEAR 1 leaves −54.9 dB re the tone at 19/21 kHz, where the clock's images were (−27.6 with it).
- **The grit above 0.65 is the grain alone**; `voicing::kWearGritStart` stays where it is.
- **Renders** equal the page's C to the bit (`tools/same_render.py` SAME 60/60 against the prototype, `IV_WEAR_NO_BBD=1`).
- **Flash:** release 108,268 B (−392 B), profile 128,168 B (−552 B); m0test unchanged.
- **CPU:** a little less above WEAR 0.65 (one stage fewer per sample); not profiled on the module.
- **Words:** the README, the manual, the release read-me and CONTEXT.md no longer describe the clock's folding; the breathing's description stays.
