# 0034 — DECAY reaches longer tails before the hold: PLATE 30 s, HALL 60 s

**Status:** Accepted, 28 Oct 2026 (owner, pick B on every card of `renders/decay_range`, the repeated cards agreeing). SPEC v1.0.53. Amends ADR 0020 (DECAY keeps the loudness) for PLATE and HALL; ADR 0008's crossover and endless are unchanged.

**Context:**
- **The owner's note:** "i often find myself wanting longer lush decays, but the knob biases towards smaller decays until you hit the 3 oclock mark and it turns into perpetual tails. I'd like more usable range so i can access longer decays without hitting the perpetual holds. I'd still like the held tails to kick in at the 3 oclock position. This applies to plate and hall".
- **Where 3 o'clock is:** on a 300° pot with the firmware's end stops, 3 o'clock is DECAY 0.816. The finite curve ended at 0.80 and endless starts at 0.85 (about 3:10). PLATE topped out at 12 s and HALL at 25 s, so the long tails lived in a sliver just before the hold.
- **Research** (`docs/research/decay-range.md`): big reverbs that put endless on the knob either have a long finite top (Valhalla and Lexicon 70 s, Strymon Cloud 50 s, BigSky 20 s) or approach endless gradually (Valley Plateau's eased loop gain, Make Noise Erbe-Verb). All keep an exponential (log T60) taper.
- **The page** offered A (today), B (longer tops on today's curve: PLATE 30 s, HALL 60 s), C (today's tops at 2 o'clock, then a gentle run-in to endless) and D (both). It was level-matched while playing. The owner picked B.

**Decision:**
- **PLATE's and HALL's finite tops are 30 s and 60 s** (`map::kT60Plate`, `kT60Hall`; were 12 / 25 s), on the same exponential over DECAY 0-0.80, the same 0.80-0.85 crossover (1/T60 falls in a straight line from the new top) and endless from 0.85. MASSIVE is unchanged.
- **Tail length on the module** (HALL at full SIZE): PLATE 1.1 / 1.9 / 3.4 / 5.9 / 10 / 19 / 25 / 44 s and HALL 2.2 / 3.8 / 6.7 / 12 / 21 / 37 / 49 / 88 s at 9 / 10 / 11 / noon / 1 / 2 / 2:30 / 3 o'clock (was PLATE 0.9 / 1.4 / 2.1 / 3.4 / 5.2 / 8.2 / 10 / 18 s, HALL 1.8 / 2.8 / 4.4 / 6.9 / 11 / 17 / 21 / 37 s).
- **The level stays where the same DECAY had it while playing** (the owner heard B at A's level). ADR 0020's make-up is still measured to the old tops (`voicing::kDecayLevelTopSeconds` 12 / 25 / 120 s: a tail longer than that gets none). On top, PLATE and HALL take `kDecayLongTrimDb` (1.95 / 2.1 dB) off the wet, eased in across DECAY by a smoothstep (PLATE 0.15-0.85, HALL 0.35-0.75) and out across the crossover by (1 − the governor's weight)³. There is no level step anywhere on the knob, and 0 dB from 0.85, so endless is bit-identical to v1.0.52. It was fitted to the page's level match on the chord (HALL at SIZE 0.3 / 0.8 / 1): the build plays within 0.44 dB of v1.0.52 at every card.

**Consequences:**
- Pad and keys (sustained, denser) read −0.7 to +1.2 dB against v1.0.52 at the same DECAY, a little louder at 0.7-0.8. A small HALL (SIZE 0.3) playing a pad reads up to +2 dB inside the crossover (0.81-0.83), where the trim has already eased out. Sparse white-noise bursts read 0.5-1.2 dB quieter at noon (`test_plate_freeze`'s SPACE loudness check carries a 0.8 dB offset for this).
- Checked on the build (`tools/pages/decay_range.py --built`): fed 3-minute holds at 0.80 and 0.835 (three input levels) stay bounded (0 of 24 fail; peaks ≤ −3.7 dBFS). DECAY flicked across the range clicks no more than v1.0.52 (worst 1 ms frame above 5 kHz) and leaves no burst of highs beyond the measure's spread. Renders null −26 to −37 dB against the page's B as heard.
- Flash +184 B (release 112,244 B).
- Tests that pin a page's measure at a tail length set DECAY for v1.0.52's tail (`host/tests/DecayV1052.h`): `test_motion` (the pitch spread at the page's DECAY 0.7) and `test_twin_level` (the room against the pad).

**Not picked (heard on the page):** C, today's tops reached at 2 o'clock and then a gentle run-in (1/T60 falling from 0.70); D, PLATE 20 s / HALL 40 s with C's run-in.
