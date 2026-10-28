# Overnight review 2, phase 2: HALL's long tails (upper mids and ringing)

*29 Oct 2026, branch `review2/halltail` (off `review2/valhalla`). Two HALL-only prototypes behind renderer hooks, off by
default, and the pick page `renders/review2_halltail`. Builds on `review2-notes/valhalla.md` findings 1-2 and
`litfdn.md` findings 6-7. Numbers: `halltail-data/*.json`; script: `tools/review2/halltail.py` (calibrate, measure,
makeup, page, verify). The Valhalla recordings were not read tonight; their figures are quoted from valhalla.md.*

## What was built

| Hook | What it does | Where |
|---|---|---|
| `IV_HALL_SHELF_HZ=6000` | HALL's loss shelf corner at 6 kHz instead of 2.5 kHz. MASSIVE keeps 2.5 kHz (switched with the SPACE, at the flip's silent restart); PLATE has its own tank. | `Engine::setHallShelfPrototype`, `Fdn::setShelfK` |
| `IV_HALL_MOD_LONG=2` | HALL's modulation floor x1 of today's (`kHallFloorScale` 3) up to a 5 s tail, rising in a straight line to x2 (= 6 x the base) from 15 s and at infinite; glided over ~1 s. The extra depth wanders round the lines' as-built mean read (`Fdn::setFloorCentre`). | `Engine::setHallModLongPrototype`, `Fdn.cpp` control |

The centring was added after the first page render: without it a DECAY move glided every line's read the same way
(the floor positions are 0..1, so a deeper floor also lengthens the mean line), which retunes a long tail like a
SIZE glide: turning a 15 s HALL down to 5 s bent the held 1 kHz by 20 cents (A: 6.4). Centred on the lines' mean
floor position, 9.4 cents. A fixed centre (0.5) was not enough: right after a start the lines' positions are all
near 0, so the shift came back.

`T60` for the floor's scale is the lows' (the DECAY mapping's, `rate` in `applySpace`). Freezing still glides the
floor out (x (1 - freezeMix_)); while unfreezing, the rate is near 0, so the scale starts toward x2 and glides back
within ~1 s on a short DECAY (not measured on its own).

## Gates

- Hooks off: `tools/same_render.py` against the pinned `ref-main/build/iv_render`: **SAME 60/60**.
- Hooks on (both): 39 SAME (all 18 PLATE and 21 MASSIVE cases), 21 differ (the 20 HALL cases and `flips`).
  `halltail-data/same_hookon.txt`.
- ctest: "100% tests passed out of 21" (before and after the centring, on the final code).
- Firmware, default: release 112,564 B, m0test 83,556, profile 127,808: all three `cmp`-identical to main's build,
  and the release to `dist/sublustris_versio-v1.0.55-3eb5e1c.bin`.
- Firmware trial (`make MODE=release HALL_LONG=n`; `-DIV_HALL_LONG_TRIAL=n`), release sizes:
  - 1, the shelf alone (two K's set in prepare, switched per SPACE): **112,716 B (+152)**
  - 2, the floor alone (scale, glide, centring): **112,980 B (+416)**
  - 3, both, prototype form: **113,084 B (+520)**
  - 4, both in a ship-like form (the corner's K per block from the SPACE, no switches, the glide without a snap,
    the centring): **112,924 B (+360)**
  Budget 112,640 B: none fits in the 76 B left. The shelf on its own could be cheaper as a constant in Fdn's own
  prepare for a HALL-only K (est. 40-80 B); the floor's centring loop is most of option 2's cost.
- CPU: ~0 (control rate: a division, a glide and an 8-term mean per block; the shelf is a coefficient the memo
  already recomputes on change). Not profiled on the module.

## Measurements (desktop renderer; HALL SIZE 0.6, ERA 0, DEPTH 0.2, TONE and MOTION noon; kit cuts as valhalla.md)

Each version matched by its own 1 kHz T60 (click EDC, T30), DECAY bisected (`halltail-data/decays.json`):
15 s: A 0.6402, B 0.6008, C 0.6365, D 0.5983; 5 s: A 0.4090, B 0.3945, C 0.4107, D 0.3949.

| 15 s tail | T60 125 / 250 / 500 / 2k / 4k / 8k re 1 kHz | drums tail colour std / ‰ over +10 dB | click std | pad / vibes spread (cents) | held 1 kHz p95 (cents) |
|---|---|---|---|---|---|
| A as built | 1.28 / 1.26 / 1.20 / 0.72 / 0.48 / 0.39 | 6.40 / 4.5 | 5.91 | 4.7 / 6.1 | 10.0 |
| B shelf 6 kHz | 1.04 / 1.04 / 1.03 / 0.93 / 0.74 / 0.49 | 6.35 / 4.4 | 5.92 | 5.2 / 6.0 | 9.9 |
| C floor x2 | 1.27 / 1.27 / 1.16 / 0.71 / 0.48 / 0.38 | 6.26 / 2.9 | 5.70 | 7.1 / 12.2 | 9.6 |
| D both | 1.03 / 1.05 / 1.01 / 0.91 / 0.73 / 0.48 | 6.23 / 2.6 | 5.70 | 7.5 / 11.2 | 10.6 |
| Room (Large / Dark), valhalla.md | 0.94-1.07 / 1.01-1.02 / 0.99-1.02 / 0.90-0.95 / 0.65-0.77 / 0.43-0.58 | 6.00 / 1.3 · 5.91 / 1.2 | 5.58-5.63 | 12.8-17.6 / 15.7-20.2 | – |

At 5 s: B 2k/4k 0.97/0.87 (A 0.87/0.73; Room 0.94-0.96/0.76-0.88); C changes nothing measurable (scale x1.04
there), as intended. A deeper floor (x3 from 15 s, 9 x the base) gives drums 1.8 ‰ and vibes 16.9 cents but a held
1 kHz wobbles 18.5 cents p95 (A 10.0): x2 is the balance, as phase 1 found.

C's uncentred first form measured drums 1.9 ‰ / pad and vibes 8.2 / 11.8 cents (as phase 1's `IV_MOD_SCALE=2`);
the centred one 2.9 ‰ / 7.1 / 12.2, with the click spectrum smoother (5.70 vs 5.85).

### The DECAY loudness make-up (kDecayLevelPower)

Wet loudness (integrated LUFS, mean of drums / pad / keys) against DECAY, A vs B (`halltail-data/makeup.json`):
B is +0.04 to +0.22 dB louder at the same DECAY (more at long DECAYs); against log10(1 kHz T60) up to 15 s the
slopes are A -0.40, B -0.32 dB per decade, so HALL's power 0.47 would become **0.474**: no refit needed for the
make-up as it was fitted (music's integrated loudness). At a matched 1 kHz T60: B - A -0.10 dB (15 s), +0.12 (5 s).

But the **tails** are not matched by that: B's first 4 s of tail sit 0.8-1.6 dB under A's (15 s; 0.3-0.7 at 5 s),
because its bass no longer rings 25 % long. The page matches the tails (the question is about them), so B and D play
0.7-1.6 dB louder than A while the phrase sounds (`_gains.txt`). If B shipped, HALL's level would want a look at long
DECAYs (a trim of ~+1 dB on the tail, or accept it: the integrated level is the make-up's own target).

## The tone_shelf pick this partly reverses

v1.0.15 (23 Oct, ADR 0012 amendment, `renders/tone_shelf`, B on all 36 cards): every tank's loss became a
first-order shelf crossing at 2.5 kHz instead of a one-pole low-pass (the DSP audit's A1, as Jot and Zita-Rev1),
so at TONE noon the upper mids (2-6 kHz: vocal edge, hats, snare crack) fade with the highs instead of ringing
almost as long as the body: "a warmer noon" (HALL at noon: 3 kHz 2.4 s instead of 3.7 s). Two days later v1.0.25
added "air" (HALL's highs lose 5.5 dB/s more at every DECAY). The two stack: on a long HALL the shelf already takes
~20 % off 1 kHz and ~30 % off 2 kHz before air. B moves HALL's corner to 6 kHz: the upper mids again ring nearer the
body, but the very top still fades (8 kHz 0.49 of 1 kHz vs 0.39) and nothing goes back to the pre-v1.0.15 one-pole.
PLATE and MASSIVE keep 2.5 kHz. ADR 0031 (TONE's resonant tilt on the wet) is untouched: it acts after the loop.
The owner should hear B as "a warmer noon partly undone, for HALL only".

## The page: `renders/review2_halltail/` (227 MB)

Question: "Should HALL's long tails stay bright in the upper mids and move a little more, like the Valhalla Room you
recorded, or stay as they are?" Versions A as built, B brighter mids, C more movement, D both; columns 1_hall_15s and
2_hall_5s; drums (make_material.py's groove, 4 bars), pad (first two chords), keys (first 8 s), each then 14 s of tail.
100 % wet with `--wet-only --dry` (MIX starts at 50 %); pick buttons (no `--showcase`); tail-matched (K-weighted, the
first 4 s after the phrase), so the page's own level match starts off. `_verify.txt`, `_hooks.txt`, `_gains.txt`.

From `_verify.txt`:
- Every file's hook line names its version (A none); every version differs from A (null -0.2 to -24 dB).
- 1 ms clicks: every version within ~1.5 dB of A's own (the material's onsets).
- Tail bursts of highs (> 3 kHz, 10 ms over a 0.5 s running median): within ~2.5 dB of A's in every card.
- Fed holds (120 s of the material at -12 / 0 / +6 dB, then 60 s): all pass, at 15 s and infinite; infinite tails
  level within 1 dB of A's; the hottest feed (+6 dB, 15 s) peaks at the limiter's -1 dBFS in A and B alike (C, D -1.5).
  A 3 min infinite chord: spread 2.4-2.7 dB, as A.
- DECAY moves over a held 1 kHz (5 -> 15 s, back, to infinite, back): pitch excursions within ~3 cents of A's after
  the centring; the 1 ms "click" ratios on the held sine (19.8-25.8 dB moving to infinite, A included) are 49-78 dB
  under the signal: inaudible.

## Open risks

- Flash: +152 B (shelf) to +360 B (both, ship-like) against 76 B left.
- B changes HALL at every DECAY (it is HALL-only, not long-only): short HALLs get brighter upper mids too (5 s: 4 kHz
  0.87 vs 0.73 of 1 kHz). TONE's darkening then starts higher up. The goldens with HALL would move (golden refresh).
- B's DECAY now means a longer 1 kHz tail (14.7 -> 18.1 s at 0.64): the knob's T60 is the lows'.
- C at infinite runs at x2 (infinite counts as long): an infinite HALL hold wobbles a little more (not judged by ear).
- Unfreeze with C: the floor briefly heads for x2 (the rate is near 0 while unfreezing) and glides back (~1 s).
- The still-lows tank keeps its 2.5 kHz shelf (it carries < 300 Hz; at 12 kHz a 6 kHz corner would be at Nyquist).
- Desktop only: the module compiles with -ffp-contract=off and no flush-to-zero in release; bit-identity proved
  the desktop only. Nothing was run on hardware.

## Not checked

- The module (CPU, sound), the plugin, ctest goldens with the hooks on.
- MOTION's Wander or WEAR together with C; SHIMMER / BLOOM with either.
- SIZE other than 0.6 (small HALLs have in-loop diffusion; B's corner there not measured).
- No listening: the page is for the owner.
