# 0051 — WEAR's ceiling and noon: a lower top, and a tape curve that keeps climbing

**Status:** Accepted, 6 Nov 2026 (owner, pick D on every card of `renders/wear_ceiling`, the repeated ones included). SPEC v1.0.75. Amends ADR 0044 (the tape leaves its taper) and ADR 0045 (the drive at full, 16.25 → 13.5 dB).

**Context:**
- **The owner, 6 Nov 2026, on v1.0.74 on the module:** "Now that the levels are higher, I think we need to revisit the wear amount and mapping across the knob to reduce the overall ceiling of the effect, and have it slightly less present at noon."
- **What the wear is made of** (`renders/wear_ceiling/_measure.txt`): nearly all of it is the tape being driven. Above 9 o'clock the tape is 5 to 10 dB louder in the wear than anything else. The breath comes next and hardly grows past noon. The grit is about 27 dB under the tape at full, the hiss about 39 and the crackle about 63. The in-loop compression doesn't act at the page's levels; it wakes up played hot. Between 3 o'clock and full the wear moved by less than 0.5 dB, so v1.0.74's knob had nearly finished by 3 o'clock.
- **What the louder wet did:** v1.0.73's lift comes after WEAR, so it doesn't drive the tape harder. v1.0.74 is v1.0.72 times one gain (+3.87 dB on HALL, +3.00 on CANYON at DECAY 0.6), to within about 120 dB, at every WEAR. The wear is the same share of the wet, but all of it is that much louder, including against the dry. Separately, v1.0.71's fuller tail feeds the tape 0.2 to 0.9 dB hotter than when 16.25 dB was picked.
- **The page** (`renders/wear_ceiling`, prototype `c1ef754`, the Renderer's `IV_WEAR_CEILING`; 30 cards, 100 % wet; pad, piano, drums and bass; HALL, CANYON and PLATE across WEAR at DECAY 0.6, CANYON endless at WEAR full). Only the tape's drive and taper moved, and no card needed a level match (every version within 0.2 dB of A):

| Tape's drive, dB | 9 o'clock | Noon | 3 o'clock | Full | Taper |
|---|---|---|---|---|---|
| A, v1.0.74 | 6.3 | 12.7 | 15.8 | 16.25 | 2.2 |
| B | 5.6 | 11.3 | 14.1 | 14.5 | 2.2 |
| C | 4.5 | 9.0 | 11.2 | 11.5 | 2.2 |
| **D** | 3.4 | 8.0 | 11.8 | 13.5 | 1.3 |

- **The clip's products on the pad,** dB against the tape's output (ADR 0041's measure; higher is denser), A / B / C / D:

| Card | A | B | C | D |
|---|---|---|---|---|
| HALL 9 o'clock | −31.3 | −32.5 | −34.6 | −37.5 |
| HALL noon | −17.8 | −20.3 | −24.4 | −27.1 |
| HALL 3 o'clock | −11.5 | −14.6 | −19.8 | −19.0 |
| HALL full | −10.6 | −13.8 | −19.1 | −15.6 |
| CANYON noon | −9.2 | −11.6 | −15.9 | −18.4 |
| CANYON 3 o'clock | −4.1 | −6.7 | −11.5 | −10.5 |
| CANYON full | −3.4 | −6.0 | −10.9 | −7.6 |
| CANYON endless full | −4.6 | −7.4 | −12.6 | −9.1 |

- **The whole wear,** dB against the same render at WEAR 0, mean over each card's materials, A → D: HALL noon −17.2 → −20.1, HALL full −13.3 → −15.8, CANYON noon −12.8 → −17.3, CANYON full −9.9 → −11.9, PLATE noon −16.9 → −21.1, PLATE full −11.9 → −14.4.

**Decision:**
- **D is the sound.** `tex::kTapeDriveDb` 13.5 dB (`dsp/EraTexture.h`), and the tape gets its own taper, `voicing::kWearTapeTaperPower` 1.3 (`params/EraVoicing.h`), in `map::eraMix`. The tape's drive, emphasis and bias all follow it.
- **Unchanged:** the breath, the in-loop compression, the hiss and the crackle stay on `kWearTaperPower` 2.2; the grit starts at noon and reaches the same amount at full; WEAR 0 is the same bits. The prototype's hook and per-stage scales are gone. Firmware, Plugin and Renderer share the code.

**Rejected:**
- **A:** the ceiling and the noon the owner asked to lower.
- **B:** the same curve, its top 1.75 dB down: about what was picked before the levels rose, and the knob still finishes by 3 o'clock.
- **C:** the lowest top, on the same curve, so it finishes early too. C and D are close at 3 o'clock; they differ most at noon, where D is lower, and at full, where D keeps going further.

**Consequences:**
- **Sound:** past WEAR 0, the tape is driven less at every position: 2.9 dB less at 9 o'clock, 4.7 at noon, 3.9 at 3 o'clock, 2.75 at full. The knob now keeps moving between 3 o'clock and full. Less drive keeps a little more of the peaks: on the page's cards, up 0.3 to 1.6 dB at the same loudness.
- **9 o'clock, to check on the module:** its tape is the lowest of the four versions. The breath still arrives early, so it stays audible, but it's the setting to check against lfad's "very subtle until you get past noon" (ADR 0044).
- **Port gates** (`tools/pages/wear_ceiling_gates.py`):
  - The page's D, all 30 cards: to the bit, against both the page's recorded sha1s and the prototype's renderer.
  - WEAR 0 against v1.0.74: to the bit on 24 of 24 (every SPACE, four materials, DECAY 0.6 and endless).
  - `tools/same_render.py`: 40 of 60 cases the same, all at WEAR 0; the 20 that differ all have WEAR above 0, down to the lowest it tries, 0.3.
- **Clicks,** worst 1 ms frame above 3 kHz, v1.0.74 / v1.0.75. WEAR 0 → 1 → 0 over 20 s: HALL 12.2 / 10.7, PLATE 11.2 / 10.9, CANYON 11.3 / 10.8. WEAR flicked 0.2 → 1 → 0.2 in 20 ms: HALL 10.3 / 10.8, PLATE 10.6 / 10.4, CANYON 11.0 / 11.0.
- **Energy:** infinite holds don't grow: 12 of 12 over 60 s, and the 10-minute holds pass at WEAR noon and full (PLATE −23.4, HALL −18.5 to −18.4, CANYON −15.1 to −14.9 dBFS). `energy_check.py` against v1.0.74 reports 60 trips, the same count as the page's D:
  - 50 are tails over the check's 0.5 dB against v1.0.74, by up to 1.26 dB (worst: PLATE, SIZE full, WEAR full, MOTION full right), after a hot noise burst. The likely reason is that the tape squashes the burst less; that wasn't isolated.
  - 15 are CANYON at endless growing by more than 1 dB, up to +1.94 (5 of them also among the 50). v1.0.74 trips the same way on 16 settings, by more (+2.27 at the setting where the port reads +1.94).
- **Off the page, on music:** MOTION full right wasn't on the page. There, against v1.0.74, CANYON's drums and bass come up by up to 0.86 and 0.99 LU at WEAR full, and 0.51 and 0.76 at noon; PLATE's drums 0.51 at full. Everything else measured, MOTION full right or full left, is within 0.32 LU. It isn't a level match the owner has heard; the rack check covers it.
- **Aliasing:** `test_era`'s hot 7 kHz tone at WEAR 0.75 reads −18.7 dB (v1.0.70: −16.7), inside its limit. No test limit moved. `test_era` gains a pin on the drive, the taper and the unchanged stages.
- **Flash:** a second `std::pow` call in `eraMix`: release 112,052 B (+16; 588 B left of the 112,640 budget), profile 130,860 B (+40; 212 B left of the chip), m0test 83,556 B (unchanged).
- **Words:** SPEC §3 and §4.7, `CONTEXT.md`, the README, the manual and the release read-me no longer say the tape is driven hard in the knob's upper half, or that most of the wear has arrived by noon: the tape climbs to the end of the knob.
- **Not checked:** the module's CPU and the owner's ears on the module.
