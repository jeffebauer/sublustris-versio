# 0040 — More tape: WEAR driven harder, a more worn cassette on MOTION's left

**Status:** Accepted, 3 Nov 2026 (owner, pick D, "both", on all 20 cards of `renders/more_tape`). SPEC v1.0.65. Amends ADR 0027 (WEAR's tape) and ADR 0030 (MOTION's tape transport), and moves v1.0.59's dips from about 7:30 to about 9:30. **The drive (B) is reversed by ADR 0041** (v1.0.66, too extreme on the module by 3 o'clock); the cassette (C) stands.

**Context:**
- **The owner, 2 Nov 2026:** "I'd like to hear what it sounds like with more saturation, crinkle, etc."
- **The page** (`renders/more_tape`, blind, 100 % wet; pad and stabs in ten settings: HALL at WEAR noon, 2 o'clock and full with MOTION at noon, 9 o'clock, 8 o'clock and full left; CANYON at WEAR 2 o'clock with MOTION 9 o'clock; HALL and CANYON endless; the ceilings, WEAR full and MOTION full left, alone and together):
  - A, today (v1.0.64).
  - B, more saturation: WEAR's tape drive at full 16.25 dB (was 11.25), scaled across the knob the same way (+2.5 dB at noon, +3.5 at 2 o'clock).
  - C, a more worn cassette, left of noon on MOTION only: wow and flutter ×1.25; the crinkle from about 9:35 (was about 8:50) at up to 0.9 bursts a second (was 0.6), each burst's jitter ±18 cents (was ±12) over a heavier scrape bed; the dips from about 9:30 (were from 7:30), on half the bursts at full left (were 43 %), dipping up to about 8 dB (was about 3.5) with the highs dulled toward ~3.5 kHz (was ~5 kHz).
  - D, B and C together.
- Each change stays within what a tired real cassette deck does: more drive into the tape, more wow, more creases and dropouts.
- WEAR's level hold kept every version within 0.1 dB of A's loudness; with more drive the peaks came down 0.3-2.5 dB at the same loudness.

**Decision:**
- **D is the sound.** The prototype's hook (`IV_MORE_TAPE`) is gone; its values are the Core's own:
  - `tex::kTapeDriveDb` 16.25 dB (`dsp/EraTexture.h`).
  - `voicing::kTransportWear` 1.25 on the transport's wow and flutter, applied after the variance so the curve keeps its shape; `kTransportCrinklePerSecond` 0.9 and `kTransportCrinkleFrom` 0.45 of the side (MOTION 0.26); `kTransportDipShare` 0.5 and `kTransportDipsFrom` 0.25 (`params/EraVoicing.h`, `map::transportAmounts`).
  - In a burst, ±18 cents of jitter; a dip takes the level by 0.2 + 0.6 × its roughness (about −2 to −8 dB) and the highs toward ~3.5 kHz (`dsp/Transport.h`). The scrape bed still scales with the crinkle's rate over 0.6, so it is 1.5 times v1.0.64's at full left.
- **Everything else is unchanged:** WEAR's compression, breath, grain, hiss and level hold; the transport's centre, glide and crossfade; MOTION's right side and its dead zone.

**Rejected:**
- **A, as built:** the owner asked for more.
- **B or C alone:** the owner picked D on every card.

**Consequences:**
- **Sound:** WEAR above 0 saturates harder (WEAR 0 is unchanged to the bit). MOTION left of noon warbles more and crinkles earlier and more often; at 9 o'clock (MOTION 0.2) about 12 bursts and 1 dip a minute (none before), at full left 54 bursts and 27 dips (34 and 15). A held 1.3 kHz tone's p95 at full left: PLATE 21.5, HALL 23.0, CANYON 19.7 cents (19.9 / 19.8 / 15.1). From noon rightwards, and with WEAR 0, PLATE, HALL and CANYON render bit for bit as v1.0.64.
- **Aliasing:** the harder tape folds more of a hot 7 kHz tone at WEAR 0.75: −20.2 dB below 5 kHz, 6.3 dB over the 96 kHz render (v1.0.64: −23.3, 2.6 over). The owner picked it on the page's bright stabs, where it shows; `test_era` allows 7 dB (was 6).
- **The dips no longer come in before the crinkle:** from 9:30 they ride on the bursts, which start at 9:35. v1.0.59's "closer to fully CCW" start (7:30) is undone by this pick.
- **Renders** equal the page's D to the bit: 20/20 page cards against the prototype with `IV_MORE_TAPE=3`, and `tools/same_render.py` SAME 60/60.
- **Checks:** a MOTION sweep from noon to full left (HALL, CANYON; WEAR 0.5 and 1) clicks no more than v1.0.64's (worst 1 ms frame 9.9-10.9 dB, v1.0.64 9.5-10.8). Endless at WEAR full and MOTION full left holds: HALL's windows within 0.8 dB of the first over 60 s; CANYON settles about 1 dB over its first 30 s, then holds at −17 ± 0.3 dB over 160 s, as v1.0.64 does (within 0.1 dB).
- **Flash:** release 108,252 B (−16 B), m0test 83,556 B (=), profile 128,152 B (−16 B).
- **Words:** the README, the manual and the release read-me say the crinkle and dropouts come in from about 9:30 and the tape is driven hard near the top of WEAR; CONTEXT.md's Crinkle and Dips carry the new numbers.
