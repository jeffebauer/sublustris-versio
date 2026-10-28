# The endless tail keeps its colour (overnight review 2, phase 2, 29 Oct 2026)

Branch `review2/endless` off `review2/rumble` (read `rumble.md` first). Core v1.0.55. Desktop renderer unless said;
the module rounds differently (no FMA, `-ffp-contract=off`, no flush-to-zero in release), so desktop numbers are
evidence about the desktop. Tags: **[M]** measured tonight, **[E]** estimated, **[L]** literature.

## In one paragraph

The candidate is **`IV_RUMBLE_FIX=19`**: the floor governor's correction as a flat gain (bit 1), the still-lows tank
keeping its own hold (bit 16), and a **5 Hz** one-pole DC blocker on the wet's input (bit 2). Over 10-minute holds at
TONE fully clockwise it keeps every band within ~±1 dB where as built loses 17-19 dB above 2 kHz (PLATE, HALL) and
lifts the lows; frozen pads keep their highs (±0.2 dB where as built loses 14-23 dB above 2 kHz in 10 minutes). At
TONE noon and below, ADR 0008's darkening still darkens the held tail (both versions), and **neither version stops it**:
the hold's make-up keeps the total level, so the lows still gain 2-8 dB in 10 minutes at TONE 0 (the fix sometimes a
little more than as built, MASSIVE). The DC blocker is what removes input DC and sub-audio: without it, 17 is as helpless
as as built against an offset (PLATE's music bands 40 dB down in 2 minutes). Hook off the renderer is SAME 60/60; with
the hook on, every finite-DECAY case is bit-identical for 17, and 19 differs only by the blocker (music bands within
0.03 dB above 40 Hz). Firmware trial switch `ENDLESS_FIX=1`: **113,132 B release (+568 B, 492 B over the 112,640 B
budget)**; without the blocker (`ENDLESS_FIX=17`) 113,036 B (+472). Default build byte-identical. A listening page
asks the owner: `renders/review2_endless/`.

## 1. Settling the candidate

### 17 vs 19, and the blocker's corner [M]

`tools/review2/endless_jobs_dc.py` (2 minutes, endless DECAY, TONE 1, every SPACE; `endless_sum.py scratch/dc '*' 20 110`).
Band levels at 110-120 s, dBFS:

| PLATE | wet DC | 1-20 | 40-200 | 200-2k | >2k |
|---|---|---|---|---|---|
| skank (its own mean −2.25e-4), as built | −0.0083 | −52.4 | −33.9 | −21.4 | −54.3 |
| skank, 17 | −0.0073 | −53.8 | −35.3 | −20.3 | −32.3 |
| skank, 19 (3 Hz / 5 Hz) | −0.00006 / −0.00005 | −54.3 / −54.7 | −35.2 | −20.2 | −32.2 |
| skank + 0.01 DC, as built | +0.055 | −37.4 | −67.2 | −67.9 | −86.3 |
| skank + 0.01 DC, **17** | +0.055 | −37.4 | −67.1 | **−60.9** | **−70.2** |
| skank + 0.01 DC, 19 (3 Hz / 5 Hz) | −0.0004 / −0.0002 | −54.0 / −55.1 | −35.3 | −20.3 | −32.3 |
| chord + 1 Hz sine at −30 dBFS, 19 (3 Hz / 5 Hz): 1-20 Hz | | −59.4 / **−62.3** | | | |

(HALL and MASSIVE the same pattern: with an offset, 17 keeps the DC (+0.072 / −0.040) and loses 3-30 dB of music;
19 at either corner is within 0.7 dB of the clean skank above 20 Hz. MASSIVE's 1 Hz case: −45.9 vs −50.0 dB at 1-20 Hz.)

- **17 alone does not cover input DC**: the flat governor fixes the colour leak, not the integration of an offset.
  With a DC-coupled source (or a plugin host's offset), an endless tail still turns into a DC pedestal at the floor.
- **19 it is.** The blocker is the one part that's insurance rather than voicing; it's also the cheapest (96 B).
- **Corner: 5 Hz.** Both corners remove steady DC completely (wet DC ≤ 4e-4); 5 Hz removes 3-4 dB more of a 1 Hz LFO
  bleed. The cost on finite DECAY (`same_render` pairs, `endless_null.py`; the worst of 60 cases): 5 Hz 20-40 Hz
  −0.20 dB, 40-200 Hz −0.03, above −0.03 dB; 3 Hz −0.10 / −0.02 / −0.03. Both inaudible; robust's "music band within
  0.1 dB" holds at 5 Hz too. Matches EraTexture's own 5 Hz subtractor and litfdn's 5-10 Hz suggestion.
- On the module the codec's ADC high-pass probably takes steady DC already (rumble.md §6, robust item 3; unverified);
  the blocker then matters for slow sub-audio above the codec's corner, and for the Plugin.

### 10-minute holds [M]

`tools/review2/endless_jobs_soak.py`: the pad material's first 8 s (a soft chord), then silence to 10 minutes;
endless DECAY; every SPACE × TONE 0 / 0.5 / 1 × WEAR 0 / 0.5; as built, 17, 19. `endless_table.py scratch/soak`.
Band change (dB) from 25-35 s to 590-600 s, then the largest swing of the per-minute total from minute 1, and the peak:

| | as built 1-20 / 40-200 / 200-2k / >2k / total | 19 | swing (asb / 19) | peak (asb / 19) |
|---|---|---|---|---|
| PLATE T1 W0 | +1.9 +2.0 −4.7 **−19.0** −2.2 | +0.8 +0.3 −0.6 +0.5 −0.4 | −1.6 / −0.2 | −4.0 / −4.0 |
| PLATE T1 W.5 | +2.6 +2.2 −4.6 **−17.4** −2.4 | +1.0 +0.3 −0.6 +0.6 −0.5 | −1.7 / −0.2 | −5.9 / −5.9 |
| PLATE T.5 W0 | +1.1 +2.8 −10.7 −1.5 0.0 | +0.1 +2.7 −8.4 −2.0 −0.1 | −0.1 / −0.1 | −5.7 / −5.7 |
| PLATE T0 W0 | +9.0 +2.3 −25.6 +1.0 +0.2 | +5.9 +2.4 −24.5 −0.5 +0.1 | −0.1 / −0.1 | −4.0 / −4.1 |
| HALL T1 W0 | −4.0 +1.0 −2.8 **−18.3** −1.8 | −5.2 −0.2 +0.1 +0.3 +0.1 | −1.2 / +0.1 | −3.9 / −3.1 |
| HALL T1 W.5 | −1.6 +1.2 −2.8 **−17.4** −1.9 | −2.8 −0.3 0.0 +0.5 0.0 | −1.3 / +0.1 | −4.4 / −4.4 |
| HALL T.5 W0 | −3.1 +2.1 −4.0 +4.2 −0.3 | −4.9 +1.4 −1.2 −9.1 −0.3 | −0.1 / +0.2 | −4.6 / −4.6 |
| HALL T0 W0 | +4.1 +2.4 −19.0 −5.9 −0.3 | −0.6 +3.4 −6.3 +2.4 −0.4 | −0.2 / −0.3 | −3.2 / −3.2 |
| MASSIVE T1 W0 | −1.9 +0.3 −0.2 −4.5 −0.2 | −1.9 0.0 +0.1 0.0 +0.1 | −0.4 / −0.2 | −6.2 / −5.5 |
| MASSIVE T.5 W0 | +3.3 +2.1 −3.1 −12.0 −0.2 | +3.5 +1.5 −1.2 −9.5 −0.1 | −0.4 / −0.2 | −7.9 / −7.4 |
| MASSIVE T0 W0 | +4.4 +2.6 −9.1 −2.4 −0.6 | **+7.6 +4.0** −4.8 −0.5 0.0 | −0.7 / −0.8 | −6.2 / −5.4 |

(WEAR 0.5 rows follow their WEAR 0 rows within ~1 dB; the full table is reproducible from the scripts.)
Frozen pads (the chord frozen at 4 s, DECAY 0.5), same columns:

| | as built | 19 |
|---|---|---|
| PLATE T1 | −2.7 +1.4 −2.5 **−22.6** −1.4 | +0.4 +0.2 −0.1 −0.2 −0.1 |
| PLATE T.5 | −0.9 +1.4 −1.0 **−14.6** 0.0 | +0.1 +0.1 −0.1 −0.1 0.0 |
| HALL T1 | −1.1 +0.9 −1.9 **−22.2** −1.6 | +0.2 0.0 +0.1 +0.2 +0.1 |
| HALL T.5 | +0.7 +0.8 −0.8 **−13.6** −0.4 | −0.4 0.0 0.0 +0.2 0.0 |
| MASSIVE T1 | +0.2 +0.2 −0.9 −4.9 −0.9 | +0.8 +0.4 −0.4 −0.1 −0.3 |

- **TONE clockwise and every freeze: the fix holds the colour** (all bands within ~1 dB, HALL's sub band −5 dB: the
  chord's little sub-20 Hz content simply decays). 17 and 19 agree to 0.2 dB on this clean material.
- **Energy bounded**: per-minute totals swing at most 1.7 dB as built and 0.8 dB with the fix; peaks −3.1 dBFS or lower,
  under the limiter's ceiling (−1.0 dBFS); wet DC ≤ 6e-4 with 19.
- **TONE noon and below (ADR 0008's darkening) still darkens with the fix** (PLATE T.5: 200-2k −8.4 dB vs −10.7;
  T0: −24.5 vs −25.6), as ADR 0008 intends. But it never stops: the hold keeps the tail's total at its settled level,
  so it never falls to the floor, where the darkening is meant to stop, and the hold's make-up keeps lifting
  whatever the darkening doesn't take. Hence the lows' slow rise at TONE 0 in both versions (PLATE 1-20 Hz +9.0 as
  built, +5.9 fixed; **MASSIVE +4.4 as built, +7.6 fixed**). That is robust.md's "sub-bass part is the loop's own"
  finding; the fix doesn't make it, and doesn't cure it. Open question below.

### Fed holds [M]

`endless_jobs_fed.py`: the pad looped for 3 minutes at −12 / 0 / +6 dB (peaks ~−19 / −7 / −1 dBFS), endless DECAY,
TONE 1, every SPACE, WEAR 0 (+ WEAR 1 at +6 dB). Every case: the total's largest 10 s level after 30 s within
+3.0 dB of its 30 s level (the same in both versions: the tank still filling at −12 dB), from 90 s to 170 s within
0.5 dB, peaks −2.4 dBFS or lower, the two versions within 0.5 dB in total; above 2 kHz the fix is 0.4-6.3 dB brighter while fed
(the air's leak acts even when fed at the floor; PLATE −12 dB: −40.4 vs −44.5 dBFS at 170 s).

### Clicks and >3 kHz [M]

`endless_jobs_clicks.py` / `endless_clicks.py` over the fed pad, every SPACE: DECAY 0.7 → 1 → 0.75 → 1, TONE 1 → 0 →
1 (fast) → 0.5 (a jump) at endless DECAY, FREEZE in and out. Largest rise between 1 ms frames, as built / 19: PLATE
29.1/29.0, 61.8/61.8, 61.9/61.9; HALL 26.3/26.8, 29.4/28.0, 31.6/29.3; MASSIVE 26.9/25.9, 27.3/29.7, 23.3/23.8 dB (the
pad loop's own onsets; no new step). Largest 50 ms rise of the >3 kHz level, as built / 19: 5.3/4.5, 5.3/4.5,
3.8/3.8; 3.5/3.5, 7.7/8.3, 7.0/6.9; 3.7/3.7, 4.4/4.3, 3.0/2.9 dB: no burst. The fix's >3 kHz sits +0 to +3 dB above
as built (median), up to +11 dB in the freezes' late minutes (the highs as built loses), a slow difference, not a step.
Peaks within 0.7 dB.

## 2. The gates [M]

- Hook off: `tools/same_render.py` against the pinned ref-main renderer **SAME 60/60** (before and after the firmware
  refactor below). ctest **100% tests passed out of 21**.
- Hook on, finite DECAY: `IV_RUMBLE_FIX=17` against ref-main: DIFFER 11 of 60, all of them endless or painting cases
  (`*_infinite`, `*_paint`, `*_hunt_*`); **all 49 finite cases bit-identical**. `IV_RUMBLE_FIX=2` (the blocker alone)
  and 19: every case differs (the blocker is on the wet's input): residual −29 to −32 dB re the signal (the one-pole's
  phase shift below 40 Hz, not level), band levels 1-20 Hz −0.3 to −2.5 dB, 20-40 −0.10 to −0.20, 40-200 ≤ 0.03,
  above ≤ 0.03 dB (5 Hz; `endless_null.py`).
- The refactor (below) left the hook's sound unchanged: `IV_RUMBLE_FIX=19` and `=17` SAME 60/60 against the
  renderer built before it.

## 3. The firmware cost [M]

`firmware/Makefile`: `ENDLESS_FIX=1` (tag `-endless`, its own obj dir, `-DIV_ENDLESS_FIX=1`) builds 19 in;
`ENDLESS_FIX=17` (`-endless17`) without the blocker. The prototype's code is now under `IV_RUMBLE_CODE` (desktop, or
`IV_ENDLESS_FIX`): in the trial build the switches are `static constexpr` (`Plate::flatGovern_`, `Fdn::flatGovern_`,
`Engine::rumbleFix_`), the blocker is `DcBlocker::process` with constant 5 Hz / 48 kHz coefficients
(`Engine::endlessHp`), and the plate keeps its loss-filter memo (`AbsorbMemo::getFlat`, sharing `absorbent()` in the
trial build) instead of the prototype's memo-less path.

| build | bytes | vs default |
|---|---|---|
| release (default) | 112,564 | `cmp` identical to main's v1.0.55 `.bin`; m0test 83,556 and profile 127,808 identical too |
| release `ENDLESS_FIX=1` (19) | **113,132** | +568 B: **492 B over** the 112,640 B budget |
| release `ENDLESS_FIX=17` | 113,036 | +472 B |

Where (nm): `Fdn::control` +236 (the flat rate, the low tank's own hold, the rate's change test), `Engine::process`
+158 (the blocker; making it a NOINLINE call cost more), `AbsorbMemo::getFlat` +32 over `get`, `absorbent` +16,
`applySpace` / `controlTick` / `reset` +124 together (later members' offsets: `holdRate` in each governor and three
floats in each Fdn). Moving the new members to the ends of their classes made it bigger (113,252 B), so they stay
where they are. CPU [E]: the blocker ~15-20 cycles a sample (~0.2 %); the rest ~0 (the memos' hit rates unchanged:
at finite DECAY the governor's rate is 0).

## 4. The page

`renders/review2_endless/` (script `tools/review2/endless_page.py`, then `make_review.py --wet-only --dry ... 
--no-level-match`). Three cards, each one 10-minute render of the chord then silence per version, heard at three
moments (first 30 s, 25 s from 5:00, the last 25 s): PLATE endless TONE CW (the owner's case), HALL endless TONE CW,
HALL FREEZE at DECAY noon TONE CW. A = the pinned v1.0.55 renderer; B = `IV_RUMBLE_FIX=19`. Not level-matched (the
level and colour over minutes is the question). `_verify.txt` in the folder (`tools/review2/endless_verify.sh`).
139 MB; 18 excerpts + 3 dry, 16-bit.

What the page's renders measure [M] (25 s → 590 s, dBFS; A / B):

| card | total | >2 kHz | 1-20 Hz | 200-2k |
|---|---|---|---|---|
| PLATE endless | −20.9 → −23.1 / −20.7 → −21.1 | **−47.5 → −85.1** / −43.7 → −43.2 | −73.5 → −69.0 / −74.2 → −71.9 | −21.9 → −26.6 / −21.6 → −22.3 |
| HALL endless | −18.5 → −20.3 / −17.9 → −17.9 | **−43.9 → −68.6** / −36.3 → −36.0 | −68.6 → −67.9 / −68.8 → −68.5 | −19.4 → −22.2 / −18.7 → −18.6 |
| HALL freeze | −20.9 → −22.5 / −21.0 → −20.9 | **−42.0 → −77.4** / −41.8 → −41.6 | flat / flat | −21.4 → −23.4 / −21.5 → −21.4 |

- Which version: A from the reference renderer; the candidate renderer with `IV_RUMBLE_FIX=0` gave a byte-identical
  10-minute file for every card (cmp); B's stderr line reads `IV_RUMBLE_FIX: 19 (flat governor on, still lows' own
  hold, input DC blocker on at 5 Hz, ...)`.
- Nulls: A and B are identical for the first ~65 ms (the blocker's settling), then differ; the freeze's first 30 s
  null to −33.8 dB (the same until the leak starts); the endless cards differ already in the first 30 s (as built's
  highs start to go within seconds: >3 kHz B−A +4.3 dB PLATE, +9.8 dB HALL).
- Clicks: B's largest 1 ms step is at or below A's in every excerpt (the chord's own onset 61.8 dB in both); B's largest
  50 ms rise above 3 kHz ≤ 4.1 dB (A ≤ 10.4); the excerpt edges have 20 ms fades.
- Peaks ≤ −3.1 dBFS; wet DC ≤ 1e-4.
- On this chord the as-built change is mostly the highs going (−37.6 dB in PLATE) and the mids sagging, with the sub
  band +4.5 dB; it's a "going dull and boomy" more than an outright rumble. Noisier material (the rumble agent's burst)
  shows the sub rise more (+9 dB).

## Open risks and questions

1. **The darkening never stops at TONE noon and below** (both versions): over 10 minutes the mids drain 8-25 dB and
   the lows gain 2-8 dB while the total holds. ADR 0008 meant the darkening to stop once the tail is down at the floor;
   since v1.0.3's hold the tail never gets there. A follow-up worth a prototype: let the hold make up only what the
   modulation loses, not what the darkening takes (e.g. hold the level of the band under the loss shelf), or let
   `darken` fade out a few minutes after the hold is taken. Owner question (robust's): "after a long while, should an
   endless tail at TONE noon keep its chord, or is drifting to a low drone fine?"
2. MASSIVE at TONE 0 gains more lows with the fix (+7.6 dB 1-20 Hz, +4.0 dB 40-200 in 10 min, against +4.4 / +2.6):
   the flat make-up now replaces the mids' darkening loss with level at every frequency, and the lows lose least.
3. The tape compression's loss (WEAR ≥ 0.35, loud tails) still goes through the air's on/off gate (rumble.md risk);
   not reached in these renders (fed at +6 dB with WEAR 1: the fix and as built within 0.2 dB).
4. Flash: +568 B against 76 B spare. Needs flash found first (or a cheaper partial: `ENDLESS_FIX=17` is +472 B).
5. Desktop only. The governor's dithering on the module (no FMA) is a cousin of this run; the module's ears decide.

## What I couldn't check

The module (no hardware tonight), the plugin in a host, the codec's high-pass on this Versio, hour-long runs with the
fix (10 minutes here; robust.md's hour-long soaks are as built), and burst / noise material on the page (the page uses
the chord the task named; the rumble agent's noise burst shows a stronger as-built sub-bass rise, +9 dB in PLATE).
