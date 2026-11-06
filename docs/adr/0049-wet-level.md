# 0049 — The wet's level: +3 dB, and +1.5 dB more at short tails

**Status:** Accepted, 6 Nov 2026 (owner, picks on `renders/wet_loudness`; offered a rebuilt page first, chose "Port it without a page"). SPEC v1.0.73.

**Context:**
- **The note:** lfad on Modwiggler, 4 Nov 2026, fully wet: "it does remain quieter on the output than my other reverbs".
- **Measured** (`docs/research/wet-loudness-2026-11-05.md`): at MIX full on v1.0.70 the wet came out 2 to 3 LU under its dry at 9 o'clock and noon, about level at long tails. Valhalla Room, on the same input, comes out 2 to 3 LU over at every decay.
- **The picks** (MIX 50 %, moving the fader): +4.5 dB on PLATE 9 o'clock and HALL noon; +3 dB on HALL 0.75, PLATE endless (pad +4.5), CANYON noon (stabs +4.5) and the hot endless cards. T60 sorts them: +4.5 at or under 11.9 s, +3 at or over 25.8 s. Nothing was picked in between.

**Decision:**
- **+3 dB in TONE's make-up table** (`Output.h` `kMakeupDb`): 0 B in any build, at every TONE, after WEAR and before the limiter.
- **+1.5 dB more at T60 up to 12 s, a smoothstep to none at 26 s, shared by the SPACEs** (`map::decayWetLift`, in the Engine after WEAR). Keyed to the plain T60 of DECAY's scale after the knob's curve, so it moves with `map::decayScale` and SIZE doesn't move it. On the knob the bend runs PLATE 0.66-0.88, HALL 0.50-0.68, CANYON 0.33-0.50. Knob noon is 5.9 s, 11.9 s and 25.8 s, as fitted. Endless takes the finite top's T60, so no extra lift.
- **Rises under the make-up's cap** (`kDecayRiseShare`, v1.0.55), falls on a ~40 ms glide, so turning DECAY down doesn't lift a tail still ringing.
- **The profile build has no lift** (`IV_MODE_PROFILE`). It has 4 B to spare and fixed settings, and the lift's ongoing cost is 2 multiplies and a glide a sample, an estimated 0.1 % of a sample's cycles, under the measurement's own spread. Trimming elsewhere to fit it would change the code being measured. The flat +3 dB is in it.

**Rejected:**
- **Flat +3 alone:** 10 of the picks' 28 cards more than 0.5 dB off (8 of them 1.5 dB under), on v1.0.70's numbers.
- **`claude/rack-levels`' per-SPACE trim:** HALL has both picks, so it can't do this, and it overshoots the HALL-to-CANYON gap as it now stands.
- **The lift inside `decayLevelMakeup`, before WEAR:** 120 B cheaper, but drives WEAR's tape 1.5 dB harder at short tails (drums' worst click frame 20.9 against 15.8 at WEAR 0.75), and the owner heard the lift after WEAR.
- **A CANYON trim for the fuller tail:** not added. See below.

**Consequences:**
- **Against what the owner picked**, re-measured at the T60s they heard (LU, now minus v1.0.70 + the pick, mean of four sources): PLATE 9 o'clock 0.0, HALL noon +0.5, **CANYON noon +1.9** (pad +2.5), HALL 0.75 0.0, PLATE endless −0.4 (only the pad's −1.5), **CANYON hot endless +1.3** (stabs +1.9), HALL hot endless −0.1. The excess is v1.0.71's fuller tail, which the picks were made without. Open: whether CANYON wants less.
- **Played hot, against v1.0.72** (each phrase to a −1 dBFS peak; DECAY's scale 0.3, 0.5, 0.75, endless; four sources): cells with the limiter over 1 dB, PLATE 0 → 4 of 16 (worst 2.0 % of samples), HALL 0 → 8 (18.9 %, endless pad), CANYON 1 → 6 (36.9 %, the pad at scale 0.3, a 10 s tail; 17.7 % at noon). Click frames per 10 s on HALL's endless pad 1.0 → 2.0, under the dry's 2.3; its worst frame 10.9 → 14.7 dB, within music's own 15-20. No card heard the hot CANYON at short tails. If the owner's "a little crackle/distortion" was the limiter, the module will have more of it there.
- **At the material's own level** the limiter never reaches 1 dB, 48 of 48 cells.
- **The module's output** (×0.874) peaks at −0.20 dBFS at worst, played hot, MIX 40-80 %; −2.29 at own level.
- **The steady level** over the knob, MIX full, pad and drums, LU over the dry: PLATE +1.8 to +4.3 (a hump at 0.75-0.80, endless +2.6), HALL +1.8 to +4.8 (a valley at 0.65, +3.9 at 0.20), CANYON +3.1 to +5.2. The biggest row step is −1.1 (CANYON 0.30 to 0.40, −0.8 on v1.0.72).
- **Turning DECAY through the bend:** worst 1 ms click frames, v1.0.72 → v1.0.73: PLATE 10.5 → 10.5, HALL 12.2 → 12.2, CANYON 11.5 → 11.5 at MIX full; 10.7 → 10.6, 12.2 → 12.2, 11.6 → 11.5 at noon. The worst 10 ms step is unchanged at MIX full. A pad's tail turned from long to short: its 100 ms level climbs at most 0.5 dB more than on v1.0.72 on a 50 ms flick, 0.1 over 3 s; those climbs are the tail's own beating (up to +2.8 on v1.0.72).
- **Energy:** `energy_check.py` against v1.0.72, 135 settings: never more than +4.50 dB over A in any window. 10-minute holds: PLATE −23.6 to −23.5, HALL −18.5 to −18.4, CANYON −15.1 to −14.9 dBFS.
- **MIX 0** is the same bits as v1.0.72. MIX noon rises by 1.9 to 2.9 LU over the dry's own level; on v1.0.57 the owner found noon "a little too loud", so this is one to hear.
- **Tests:** goldens moved once. Chord loudness by the gain. `c80_db` and `smooth_after_s` on `plate_small` and `hall_small` move because the metric's onset rule reads level: the render is +4.5 dB at every moment and a fixed window reads the same C80 (−7.83). Re-pinned, each with a comment: `test_tone` (the bell's tests take the flat gain off), `test_room_motion` (+3.00), `test_soak_regressions` (a flip's ratio reads x13.5 as x15.4 with the same click, its limit 15 to 17).
- **Flash:** release 112,284 B (+216, of which the cap 64; 356 B left of the 112,640 budget), m0test 83,556 B (unchanged), profile 131,068 B (unchanged, 4 B left).
- **Not checked:** the module's CPU, the module's volts, and the owner's ears on any of this. The plugin's test isn't built in the worktree (`libs/JUCE` is empty).
