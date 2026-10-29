# Where MOTION's tape dips come in (29 Oct 2026)

Owner at v1.0.57 at the rack: "Left side of the modulation knob: the tape-style sounds is good; I like the slight pitch warbles and whatnot, but it'd be good if the dropouts start entering closer to fully CCW instead of ~9 o'clock."

Prototype on `claude/motion-dips`; decision page `renders/motion_dips/` (main checkout), built by `tools/pages/motion_dips.py page`; the table below by `tools/pages/motion_dips.py measure`.

## What the "dropouts" are [M]

Since v1.0.46 (ADR 0030; `drift_rethink` round 3's E with `left_tape_final`'s B) the transport's dips ride on the crinkle: 43 % of the crinkle bursts, counted, bring a dip (the level rough by 1-3.5 dB, the highs toward ~5 kHz, 50-300 ms; `kTransportDipShare`). The crinkle comes in over the left side's last 40 % (`map::transportAmounts`: `t > 0.6`, i.e. MOTION < 0.188), so the dips come in with it. There is no other dropout left (round 2's were removed in v1.0.46).

## Where each tape behaviour enters [M]

Clock positions are the module's: the pots' end stops (`firmware/PotEndStops.h`) map raw 0.025-0.975 to 0-1, so value v sits at 7:00 + (0.025 + 0.95 v) x 10 h. Fully CCW 7:00, noon 12:00.

| MOTION | clock | wow c | flutter c | variance | crinkle /min | dips /min today | dip depth (median / deepest) |
|---|---|---|---|---|---|---|---|
| 0.000 | 7:00 | 20.4 | 4.5 | 1.00 | 36 | 15 | -1.8 / -2.3 dB |
| 0.026 | 7:30 | 19.0 | 4.2 | 0.97 | 31 | 14 | -1.8 / -2.2 |
| 0.050 | 7:43 | 17.4 | 3.9 | 0.88 | 26 | 11 | -1.7 / -2.0 |
| 0.079 | 8:00 | 15.1 | 3.4 | 0.74 | 21 | 9 | -1.7 / -1.9 |
| 0.100 | 8:12 | 13.5 | 3.1 | 0.61 | 17 | 7 | -1.7 / -2.0 |
| 0.130 | 8:29 | 11.2 | 2.6 | 0.42 | 11 | 4 | -1.9 / -2.1 |
| 0.150 | 8:41 | 9.9 | 2.3 | 0.30 | 7 | 3 | -1.9 / -2.1 |
| 0.170 | 8:52 | 8.7 | 2.1 | 0.19 | 3 | 2 | -1.6 / -1.8 |
| 0.188 | 9:02 | 7.7 | 1.9 | 0.10 | 0 | 0 | - |
| 0.250 | 9:38 | 5.6 | 1.4 | 0 | 0 | 0 | - |
| 0.350 | 10:34 | 3.1 | 0.8 | 0 | 0 | 0 | - |
| 0.470 | 11:43 | 0 | 0 | 0 | 0 | 0 | - |

- Wow and flutter (the mapping's depths) grow straight from the dead zone (11:43) to fully CCW; the variance (wow speed changes, bigger swings) eases in from 9:28; crinkle and dips both start at 9:02.
- Counts are the transport's own over 60 s of a held pad (HALL SIZE 0.6, DECAY 0.7). Depths are paired against a no-dip control with the same random path (10 ms frames under -0.5 dB), the method of `drift-rethink.md` round 4. A dip's depth does not change along the knob, only how many there are.
- A pitch track of a held tone through the tail is dominated by the tank's own beating (~19 c p95 at noon), so the warbles' depths are the mapping's; `test_motion` pins their p95.
- **The owner's ~9 o'clock is exactly where the dips start (9:02).** They are rare there (2-4 a minute down to 8:30), and the counter puts the first dip on the second burst, so near 9 one comes only every 20-30 s.

## Reading of the note

The left side runs from noon (no tape) to fully CCW (most tape). "Start entering closer to fully CCW" means the dips' zone should be narrower, starting nearer the left end; fully CCW stays as it is. The warbles stay as they are everywhere. The pick it touches: `left_tape_final`'s B fixed how many dips (a third of C's, 15 a minute at full left) but they came in with E's crinkle zone, which was never asked about separately; fully CCW keeps B's count.

## Prototype

`IV_MOTION_DIPS=<from>` (`Engine::setMotionDipsPrototype`, `map::transportDipShare`): the share of bursts with a dip is 0 above MOTION `from` and rises straight to 0.43 at fully CCW (the same there to the bit). -1 as built (with the hook set, the renderer prints the transport's counts), 0 no dips (the control). Only `TransportAmounts::crinkleDip` changes; the dips draw their own random numbers, so the wow, flutter, crinkle bursts and scrape are untouched.

| version | from | 7:43 | 7:30 | 7:21 | 7:00 |
|---|---|---|---|---|---|
| A now | (0.188) | 11 | 14 | 15 | 15 |
| B from 8 o'clock | 0.079 | 4 | 10 | 13 | 15 |
| C from 7:30 | 0.026 | 0 | 0 | 9 | 15 |
| D the last ~10 degrees | 0.010 | 0 | 0 | 0 | 15 |

Dips a minute (60 s held pad). On the slow turn card (noon to 9 in 6 s, then to fully CCW over 24 s) the first dip comes at 8:10 / 7:32 / 7:16 / 7:00 for A / B / C / D.

## Gates

- Hook off: `tools/same_render.py` against main (`d33b938`) SAME 60/60; with the hook at 0.079, 0.026 and 0.010 also SAME 60/60 (its cases sit at MOTION 0 and 1, where every version equals today); the control (0) differs in exactly the 8 MOTION-0 cases.
- With the hook on, every render without a dip equals the no-dip control to the bit (the measure table's `=`, and the page's fixed cards); against today the renders differ only inside either render's dip windows (+-50 ms), at most -127 dBFS outside them (the smoothed level and highs settling back after a dip, float rounding).
- Clicks (1 ms frames of the highs over 3 kHz against their neighbours) the same as today on every card; energy above 3 kHz within +0.13 dB (fewer dulled moments).
- ctest 100 % of 21. Firmware: the default release (110,924 B), m0test and profile byte-identical to main (`cmp`); the trial `MOTION_DIPS=0.079` (`sublustris_versio-dips0079.bin`) 110,996 B (+72 B).

## Outcome (29 Oct 2026)

The owner picked **C, from 7:30, on every card**. Shipped in v1.0.59 as `voicing::kTransportDipsFrom` (0.026) inside `map::transportAmounts` (no hook; ADR 0030 amendment). The renderer's `IV_MOTION_DIPS` hook, `Engine::setMotionDipsPrototype`, the firmware's `MOTION_DIPS` trial and `tools/pages/motion_dips.py` stayed on `claude/motion-dips`. Proof: renders equal the prototype at 0.026 to the bit (13 of 13 cards, and `same_render` SAME 60/60); against v1.0.57 `same_render` SAME 60/60, held pads from noon to 7:30 equal the no-dip control and differ from v1.0.57 only inside its dip windows (outside at most −120 dBFS); fully CCW bit-identical. Release 110,988 B (+64 B).
