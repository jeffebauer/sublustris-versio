# 0048 — DECAY's curve: more knob for the long tails, endless from half past 4

**Status:** Accepted, 6 Nov 2026 (owner, pick C on `renders/decay_curve`). SPEC v1.0.72. Amends where ADR 0008's endless and ADR 0034's T60 range sit on the knob; neither changes in sound.

**Context:**
- **The note:** the owner on the module, 5 Nov 2026: "the decay seems to bias towards shorter decays until noon, then 12-3 controls longer decays, which is a small zone to dial in longer decays with precision ... is there any granularity in effect past 3? ... Should we shift endless to fully CW or last 'hour' of the knob?"
- **Measured** (`docs/research/decay-curve-2026-11-06.md`): from about 3:30 to the stop, 45° of the knob, nothing new happened. HALL's 15-60 s had 77°.
- **The page:** A today; B the curve stretched, endless from 4:30 (noon falls to 8 s); C B bent so noon keeps its 12 s; D C with endless only from about 4:50.

**Decision:**
- **The knob stays the knob, and it maps onto DECAY's scale** (`map::decayScale`). The scale's 0-0.80 (finite) takes the knob's 0-0.92, its 0.80-0.85 (the crossover) takes 0.92-0.95, its 0.85-1 (endless) takes 0.95-1. Over the finite part, with x = knob / 0.92, the scale's share is x + 0.328 x (1 − x): T60 stays exponential in that share, and the bend keeps noon where it was.
- **HALL at full SIZE:** 0.8 s fully left, 2.6 at 9, 7.4 at 11, 12 at noon, 18 at 1, 28 at 2, 40 at 3, 48 at 3:30, 56 at 4, endless from 4:30. HALL's 15-60 s has 110° of the knob, endless 15°.
- **Where it applies:** where DECAY's smoother reads its target (the Engine's control block, and `reset`). `setParam` keeps the knob and `Engine::param(Decay)` returns it. Everything after the smoother reads the scale, so nothing keyed to DECAY needs its own change (§10 of the research note goes through them, with v1.0.71's HALL ease).
- **The endless zone runs the scale's 0.85-1 across the knob's 0.95-1**, so CANYON's built-in attack, the one thing that moves inside endless, still ends at fully CW's.
- **The profile build has none of it:** it reads no knobs and its fixed settings are on the scale, so its measurements mean what they did.

**Rejected:**
- **A:** the note stands.
- **B:** a shorter noon than the owner knows, for no gain over C.
- **D:** a smaller endless zone than picked.
- **The remap in `setParam`** (the prototype's shape): `Engine::param(Decay)` would stop being the knob.

**Consequences:**
- **Sound:** none, at the same scale value. The Core at a knob position is bit-identical to v1.0.71 at that position's scale value: 126/126 renders (14 knob positions × 3 SPACEs × MOTION noon, 0 and 1), and DECAY 0 and fully CW 18/18. Against the prototype's C with DECAY moving, 6/6 (`tools/pages/decay_port_checks.py`).
- **Saved settings:** parameter keys don't change and every saved set opens, but a stored DECAY now means the new position. A set saved at 0.85-0.95 was endless and opens as a long tail (0.9 is 56 s in HALL); other finite values move in T60, least near noon. The site's starting points and demos are on the old curve until re-rendered.
- **Clicks at 1 ms frames** (sweeps and flicks across the crossover), port against v1.0.71, in dB: PLATE 9.2/9.2, HALL 11.1/11.1, HALL MOTION fully left 10.9/11.5, fully right 11.9/12.1, CANYON worst 11.2/10.9 on a 20 ms flick. The music's own frames read about 15-20.
- **Endless follows the playing** at the new point exactly as v1.0.71 (playing / held dB): PLATE −0.01 / +0.26, HALL −0.05 / +0.13, CANYON −0.78 / −1.60.
- **Energy:** `energy_check.py` against v1.0.71 at the same scale value, 180 settings: B never more than +0.00 dB over A. 10-minute holds at knob 0.95: PLATE −26.6 to −26.5, HALL −21.5 to −21.4, CANYON −18.1 to −18.0 dBFS. Against v1.0.71 at the same *knob* value CANYON at SIZE 0 reads up to +1.21 dB and 15 of 180 settings go over the check's 0.5 dB line, which only says those knob positions are now different tails.
- **Sweetspot** (21 steps): the dead run past 0.85 is gone. HALL's zero steps were 85-100 %, now 95-100 %; PLATE's the same. The step into endless is still the T60 reading's jump (HALL 9.1, was 9.2), now at 90-95 %. CANYON's flagged cliff at 80-85 % has gone.
- **Tests:** `host/tests/DecayKnobFor.h` (the remap's inverse) keeps the tests that pin a tail measuring that same tail. `test_fdn` `decayKnobCurve` tests the remap. Goldens stay on the knob, so they moved once for this version.
- **Flash:** release 112,068 B (+184, 572 B left of the 112,640 budget), m0test 83,556 B (unchanged), profile 131,068 B (unchanged, 4 B left).
- **Not checked:** the module's feel and pot reach at half past 4 (the firmware's end stop snaps from 0.975, well past it); its CPU, though it's one short call per control block.
