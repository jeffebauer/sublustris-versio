# 0052 — HALL's low-note resonances: a narrower, denser low tank, still unmodulated

**Status:** Accepted, 6 Nov 2026 (owner, pick D on all 30 cards they scored of `renders/hall_lows`; the bass card at noon has no pick). SPEC v1.0.76. Amends ADR 0012's still lows and, for the low tank only, the v1.0.17 fade of HALL's in-loop diffusion. The rack check that would confirm the cause on the module is still to do.

**Context:**
- **The owner, 6 Nov 2026, on v1.0.75 on the module** (a Serge oscillator through a low-pass, every knob at noon, DECAY 1-2 o'clock, MIX full; low notes): "a resonant build up in the left channel, but not the right"; the left touched amber while the right stayed mostly green.
- **The cause, measured on the desktop:** HALL's still low tank (split at 300 Hz, no modulation, in-loop diffusion 0 at SIZE noon or above) leaves very narrow resonances, each with a chance left/right balance. 90.8 Hz leans left by 9 dB, 80 Hz right by 13 dB. v1.0.73's +3 dB made it noticeable.
- **The page** (`renders/hall_lows`, prototype `69a020c`, the Renderer's `IV_HALL_LOWS`; 100 % wet; a patch like the owner's, bass, drums, pad and piano; SIZE noon, 0.35 and 0.8, and endless). Only HALL's low tank moved; each version's trim holds the lows' level:

| Version | Output side × | In-loop diffusion | Trim |
|---|---|---|---|
| A, v1.0.75 | 1 | 0.5 fading to 0 by noon | 0 dB |
| B, narrower | 0.30 | as A | +2.3 dB |
| C, denser | 1 | 0.60 at every SIZE | +1.1 dB |
| **D, both** | 0.30 | 0.60 at every SIZE | +3.3 dB |

**Decision:**
- **D is the sound.** `voicing::kHallLowsWidth` 0.30, `kHallLowsInLoopDiffusion` 0.60, `kHallLowsOutputTrim` +3.3 dB (`params/SpaceVoicing.h`). The trim is on the low tank's own output trim, the diffusion in `Engine::applySpace`'s HALL branch, and the width in `processTwoTanks`'s upsample read, not a loop of its own (that overflowed the profile build by 324 B).
- **No modulation.** The owner keeps a bass that doesn't waver.
- **One thing beyond the page: the low tank's allpass lengths follow the SIZE its lines crossfade to,** not the glided `hallSize_`. See the SIZE flick below. Settled settings are the same either way; it changes only what happens during a SIZE move.
- **Unchanged:** the main tank's in-loop diffusion still fades out by noon. CANYON shares the low tank and has none of this. The prototype's hook and env are gone. Firmware, Plugin and Renderer share the code.

**Rejected:**
- **A:** the lean the owner reported.
- **B:** fixes the lean, not the ring (noon +8.8 dB against A's +8.3); a note that was already balanced gets about 2 dB louder (98.5 Hz at noon).
- **C:** calms the ring at noon and above, but many notes still lean (4.2 % over 6 dB at noon).
- **Modulating the low tank:** the usual cure for narrow resonances, and the one thing the owner's still bass rules out.

**Consequences:**
- **Sound,** the patch's fine scan over 40-240 Hz, 10 cents apart (`tools/pages/hall_lows_gates.py`'s `lean`). "Ring" is how far the 10 strongest notes stand over the typical note:

| Setting | Pitches over 6 dB, v1.0.75 → D | Furthest lean, D | Ring, v1.0.75 → D |
|---|---|---|---|
| SIZE noon, DECAY 0.7, WEAR 0 | 7.7 % → 0 % | +4.8 / −5.5 dB (v1.0.75 +10.0 / −12.3) | +8.3 → +6.6 |
| SIZE noon, DECAY 0.6, WEAR noon | 5.1 % → 0 % | +4.7 / −4.8 dB | +7.3 → +6.0 |
| SIZE 0.35, DECAY 0.7, WEAR 0 | 4.2 % → 0 % | +5.4 / −4.7 dB | +7.9 → **+9.1** |
| SIZE 0.8, DECAY 0.7, WEAR 0 | 4.8 % → 0 % | +5.9 / −3.8 dB | +7.6 → +6.2 |

- **The costs the owner took with the pick:** the ring at SIZE 0.35 is worse than today's (above). A low note's weight lands 50-75 ms later at noon and reaches full level 40-60 ms later (10-20 ms at SIZE 0.35 and 0.8). Around 110 Hz at SIZE 0.8 the bass reads 6-7 cents against 3.8 (the page's pooled figure). A single held sine here: 110 Hz 1.20 → 3.41 cents at SIZE 0.8, 2.41 → 4.10 at 0.35, 3.39 → 1.58 at noon; 60 Hz at most 2.28. Nothing modulates; the extra spread is the denser tank's own phase.
- **A bare sine leans further than the patch:** `test_hall_lows`' six sine hits read 5.8 / 6.7 / 5.5 dB furthest at SIZE noon / 0.35 / 0.8 (the tank at full width without the diffusion: 16.5 / 14.3 / 12.7), so its limit is 7 dB. "No pitch over 6 dB" is true of the patch, not of every sound.
- **The SIZE flick, which the page didn't test:** `test_fdn`'s flicks 0.1 ↔ 0.9 on a held note failed on the page's D, off the note −7.5 dB against −11.4 still (limit −10). `hallSize_` glides for several hundred ms, and with the low tank's allpasses now in the loop at every SIZE, their lengths slid after the crossfade had already jumped the lines, the smear `Fdn::controlInLoop` already warns of. Driving their length from the crossfade's SIZE fixes it: −11.6 dB, with no test limit moved. dB off the note, left channel, still / flicked (`flick`):

| Held note | v1.0.75 | The page's D | Port |
|---|---|---|---|
| 70 Hz | −14.1 / −7.4 | −16.2 / −3.1 | −16.2 / −11.0 |
| 110 Hz | −32.1 / −20.1 | −32.1 / −3.6 | −32.1 / −19.8 |
| 220 Hz | −37.2 / −33.0 | −34.1 / −5.7 | −34.1 / −25.3 |
| 440 Hz | −19.5 / −25.5 | −19.6 / −8.4 | −19.6 / −28.8 |

  220 Hz stays about 8 dB more off the note than v1.0.75 through a flick. The owner hasn't heard this behaviour on a page: the rack check covers it. The main tank keeps its glided length, as it has since v1.0.17 (below noon only).
- **Port gates** (`tools/pages/hall_lows_gates.py`, Renderer at 48 kHz):
  - Against the hook, patch and bass at SIZE noon, 0.35 and 0.8, on settled settings: −112.7 to −115.2 dB.
  - PLATE and CANYON to the bit against v1.0.75: 72 of 72. The golden update moved only HALL's six corners.
- **Clicks,** worst 1 ms frame over its neighbours, v1.0.75 / port. Pad above 3 kHz: SIZE slow 10.3 / 10.1, across noon 9.9 / 9.4, flick 10.0 / 10.1; DECAY into endless 9.5 / 10.4, flick 11.0 / 10.4; SPACE 9.5 / 9.5. Low notes above 600 Hz: SIZE slow 3.6 / 4.6, across noon 11.6 / 6.7, flick 8.8 / 4.4; DECAY into endless 5.7 / 3.8, flick 6.3 / 4.1; SPACE 6.3 / 5.2.
- **Energy:** `energy_check.py` at WEAR noon against v1.0.75 reports 6 trips, the same as the page's D: 5 are CANYON at endless growing +1.12 to +1.32 dB (+0.00 dB against v1.0.75: bit-identical, so today's), and one is HALL, SIZE 0.6, DECAY 0.5, MOTION full right, +0.61 dB over the check's 0.5. The ten-minute holds pass (PLATE −23.4, HALL −18.6 to −18.5, CANYON −15.1 to −14.9 dBFS).
- **At WEAR 0, off the page's settings:** the same check adds 10 HALL trips, none of them a growing tail: SIZE 0, DECAY 0.5, up to +4.05 dB in a window after the noise burst (+1.27 at MOTION full right), and SIZE 0.6, DECAY 0.5, +0.97 to +1.05 dB. The trim was matched at SIZE 0.35 to 0.8; at SIZE 0 the diffusion hardly changes, so the lows' tail likely keeps most of the +3.3 dB (not isolated). On music, port minus v1.0.75 at WEAR 0, SIZE 0 to full, DECAY 0.5 and 0.7, is within +1.9 / −2.0 LU on bass, pad and piano; drums read −3.1 at SIZE 0, DECAY 0.7. The largest low-band moves are piano: +4.8 dB at SIZE 0.35, −5.1 at noon. It isn't a level match the owner has heard; the rack check covers it.
- **Ten-minute holds of low material** (20 s of low notes, DECAY 1, SIZE 0.35 / noon / 0.8, WEAR 0 / noon; within 3 dB, never above the 60-70 s window + 1 dB, never under −60 dBFS): v1.0.75 5 of 6, the port 5 of 6. On both the one over is SIZE noon, WEAR 0: +1.12 dB on v1.0.75, +1.19 on the port, spread under 1.3 dB and finite. The prototype's 21 of 24 across its four versions was the same line on A, B and D. The held tail's left/right balance is closer, at most 2.3 dB (v1.0.75 3.2).
- **Tests:** `test_hall_lows` is new. `test_twin_level` re-pins three HALL figures (S0.9 D0.9 −8.82 → −8.28, S0 D0.5 −8.77 → −9.10, S0.9 D0.5 −10.96 → −10.46) and widens S0 D0.9's limit 5.6 → 5.7 (−10.77): the frozen pad moves with its low tank, the twin doesn't.
- **Flash:** release 112,172 B (+120; 468 B left of the 112,640 budget), profile 130,988 B (+128; 84 B left of the chip), m0test 83,556 B (unchanged).
- **Words:** SPEC §4's still lows, `CONTEXT.md`, the README, the manual and the release read-me say HALL's low notes stay near the centre.
- **Not checked:** the module's CPU, which matters first: the low tank's allpasses now run at every SIZE, where they stopped at noon before, so SIZE noon and above cost more than v1.0.75. The profile lap decides whether D stays. Also the owner's ears on the module, and the port's SIZE moves on a page.
