# MASSIVE's loudness bump near endless (overnight review 2, "bump", 28-29 Oct 2026)

The owner's question: "While playing, MASSIVE is about 4 dB louder just before endless (DECAY ~0.75-0.80, 2:30-3 o'clock) than at endless, because the ducking and the floor governor fade out across 0.80-0.85. A slow turn down from endless passes through it. Is it a problem, and what would smooth it without changing held endless or short settings?"

Branch `review2/bump` (off `review/overnight-2`, v1.0.55). Tags: **[M]** measured here (desktop renderer), **[S]** a source / the owner's reference takes, **[I]** inference.

## Short answer

- [M] **Confirmed at the material's own level, and it depends on how hard you play.** MASSIVE SIZE 0.8, the pad / keys at their listening level (-20 dBFS RMS): DECAY 0.77-0.80 is 4.0-5.3 dB louder than endless while playing. Played 6 dB hotter it's 7-9 dB. Played 10 dB quieter it reverses: **endless is 3-8 dB louder** than 0.80. Stabs at their level: only 0.5-1 dB.
- [I] **Cause:** endless is *level-governed* (the floor governor pulls a fed tail to -21 dBFS whatever comes in; the duck takes a bit more), and every finite setting is *linear* (louder in, louder out). DECAY 0.80 is where the most linear setting (MASSIVE's 120 s top) meets the governed one. It isn't the make-up: with `IV_DECAY_MAKEUP=0` the 0.76-0.80 levels move by under 0.6 dB.
- [M] **What's actually odd is the cliff.** About 75 % of the drop happens between DECAY 0.80 and 0.81, 1 % of the travel (~3° on the pot), not across 0.80-0.85. Pad at its level: 3.7 → 1.2 dB re the dry in that one step; hot: -0.0 → -3.8. At 20 % weight the governor's added loss (0.2 × 0.08 /s per dB over the floor) is already 5-10× the tank's own loss at a 120 s T60 (1/120 /s). So it acts almost at full strength as soon as it starts.
- [M] **Split:** governor ~3-3.5 dB, duck ~0.5-1.2 dB of the drop (pad 0.6, stabs 1.2). The duck already fades in gently (linear in its weight, ~0.1 dB per 0.01). The cliff is the governor's alone.
- [I] **Is it a problem?** It's a modest one, with no click or swell artefact. On a 20 s turn from endless to 0.70, the level rises by about 3 dB over 6-8 s around 3 o'clock, overshoots the 0.70 level by 1-1.5 dB, and settles over ~10 s. Most of that rise is simply 0.70 being 2.7 dB louder than endless at that playing level. The cliff puts the whole change on one spot of the knob, and the level-dependence means the owner hears a bump when playing at a normal or hot level and a dip when playing quietly.
- **Prototype (E1g):** MASSIVE's floor governor comes in from 0.70 as t³ instead of from 0.80 linearly. It takes the steepest step from 2.5-4.6 dB per 0.01 of DECAY down to 0.3-0.9 dB per 0.01. At the material level the 0.74-0.80 settings come down 1.5-3.5 dB, so the curve from 0.60 to endless no longer rises before it falls. Held endless, every DECAY ≤ 0.70, HALL and PLATE are bit-identical. It costs +56 B of flash (20 B under the budget) and needs the owner's ears, because the finite top's tail after playing is also 1.5-5 dB lower.

## Measurements

Material: `renders/_material` pad, keys and stabs, each looped to 50 s of playing, then 10 s of silence. Also `08_held_tones`. Wet only (BLEND 1), TONE 0.5, MOTION 0.5, ONSET NATURAL, DEPTH 0, WEAR 0.1. Loudness is BS.1770 K-weighted mean square over 25-49.5 s (steady while playing), in dB re the dry over the same span. Tools: `tools/review2/bump_grid.py` (contribution switches), `bump_curve.py` (three input levels), `bump_turn.py` (DECAY turns), `bump_side.py` (tail, >4 kHz share, click scan), `bump_identity.py` (bit-identity), `bump_reference.py` (the owner's takes), `bump_summary.py`.

### Level while playing vs DECAY, as built (dB re dry)

```
DECAY            0.60  0.65  0.70  0.74  0.77  0.80  0.81  0.82  0.83  0.84  0.85  0.90  1.00
M8 pad   -10      1.9   3.4   2.8   3.3   4.6   4.1   5.3   6.0   6.9   8.2  10.2  10.6   9.8
M8 pad    +0      1.9   3.4   2.8   3.3   4.1   3.7   1.2   0.7   0.6   0.2   0.0  -0.0   0.1
M8 pad    +6      0.8   0.9   0.3   0.2   0.2  -0.0  -3.8  -5.2  -5.7  -6.5  -6.8  -6.8  -6.8
M8 keys   +0      1.7   4.2   2.4   3.3   4.5   3.3  -0.1  -0.4  -0.0  -0.3  -0.7  -0.9  -0.8
M8 stabs  +0      2.2   3.7   3.1   3.8   4.2   4.4   4.7   4.3   4.1   3.9   3.7   3.5   3.8
M8 stabs  +6      2.2   3.5   3.0   3.0   2.9   2.8  -0.7  -1.6  -2.2  -2.7  -3.1  -3.2  -2.7
M5 pad    +0      2.7   3.9   2.4   2.9   4.1   3.8   0.8   0.1  -0.3  -0.6  -0.6  -0.4  -0.4
P5 pad    +0     -0.3   0.4   1.3   2.0   2.4   2.8   1.8   1.6   1.5   1.3   1.1   1.1   1.1
P5 pad    +6     -0.8  -0.4   0.1   0.5   0.8   1.0  -1.3  -2.1  -2.8  -3.5  -4.1  -4.1  -4.1
H8 pad    +0     -1.9  -1.2  -0.3   0.6   1.3   1.9   3.0   3.5   3.8   3.8   3.7   3.7   3.7
H8 pad    +6     -2.2  -1.9  -1.5  -1.2  -0.9  -0.7  -1.6  -1.8  -2.1  -2.5  -3.0  -3.0  -3.0
```
(M8/M5 = MASSIVE SIZE 0.8/0.5, P5 = PLATE SIZE 0.5, H8 = HALL SIZE 0.8; +0 = the material's level, +6 / -10 dB re it.)

- [M] **PLATE has the same mechanism, smaller:** 1.7 dB at the material level, 5.2 hot, with a cliff of 1.0-2.3 dB per 0.01. **HALL** has none at the material level: its floor sits 3 dB higher (ADR 0033) and its v1.0.53 trim shapes its top. Played hot, HALL drops 2.3 dB, gently (≤ 0.9 dB per 0.01).
- [M] **Contributions** (`bump_grid.py`, M8 pad at its level, endless): duck off +0.6 dB; governor off (`IV_FLOOR_DB=100`) +4.3 dB; both off +4.9. Make-up off: 0.60 drops 2.6 dB, 0.76-0.80 by ≤ 0.6, the crossover not at all (MASSIVE's make-up is 1 from its 120 s top).
- [M] **Early in the playing (3-13 s) the bump is smaller** (M8 pad ~1-2 dB). The 120 s tail needs ~10-20 s of playing to build up (power time constant T60/13.8 ≈ 9 s).
- [M] Held sine tones (`08_held_tones`) swing ±3-4 dB between neighbouring DECAYs in MASSIVE (tones landing on or between tank modes), so they're useless for this. A real 1-1.5 dB wiggle at DECAY 0.65 shows in every MASSIVE row (0.60 → 0.65 → 0.70: 1.9 / 3.4 / 2.8). I didn't investigate it; it's probably the make-up's two-power curve meeting MASSIVE's SIZE-dependent shortest T60.

### A slow turn (pad, M8, playing throughout; 3 s means, LUFS-like)

DECAY held at 1.0 to 25 s, turned to 0.70 by 45 s, then held:

```
t (s)      21-24  30-33  36-39  39-42  42-45  45-48  48-51  54-57  60-63  66-69
as built   -18.9  -19.2  -18.4  -16.9  -15.5  -14.8  -15.4  -15.6  -15.8  -16.1
E1g        -18.9  -19.2  -18.7  -17.8  -16.2  -15.2  -15.7  -15.7  -15.8  -16.1
```
- [M] As built: +4 dB from endless to the peak (just after the knob reaches 0.70). The peak is 1.5-2.0 dB over 0.70's settled level, and the level settles over ~10 s. A 4 s turn gives +1.3 dB over the end level. The reverse turn (0.70 → 1.0) gives +0.5-0.9 dB, just as the governor takes hold. Stabs: +1.5-2.3 dB. HALL +0.4-1.0, PLATE +0.4-2.1.
- [M] E1g: the rise starts earlier and is gentler (39-42 s: -17.8 against -16.9), and the overshoot is 1.5 dB instead of 2.0 (M8) and 1.9 instead of 2.4 (M5). The 0.70 end level is unchanged, by design.

### The owner's reference units (numbers only; takes never copied)

Wet loudness re dry over the second half of each piece's playing, six pieces (`bump_reference.py`):

- [S/M] **Supermassive Sagittarius** at feedback 7 / 30 / 73 / 100 %: flat within ±0.5 dB up to 73 %. At 100 % (endless) it's **0.9-2.1 dB quieter** than at 73 % on every piece. That's the same sign as ours, a third to a half the size.
- [S/M] **Supermassive Andromeda** at 0 / 12 / 54 / 100 %: flat to 54 % (+1-2 dB), then **+5.3 to +8.0 dB at 100 %**, an ungoverned build-up.
- [S/M] **ValhallaPlate** (Chrome, Lithium) short → long: **+8 to +11 dB**, rising monotonically (no normalisation at all). **ValhallaRoom**: flat within ±1 dB.
- [I] So "louder with longer decay" isn't the rule. Valhalla's Room and Supermassive keep the level flat over the finite range, Plate rises, and at endless one Supermassive mode drops a little while another builds. None of the takes sits between 73 and 100 %, so they can't show whether a unit has a cliff near endless. What our design has that they don't is the endless state being level-governed, so whether endless is louder or quieter than the finite top depends on the input level.

## Prototypes (renderer hook `IV_BUMP="mode:db:from:to:pow"`, desktop only)

`Engine::setBumpPrototype` (`core/dsp/Engine.h`), applied in `Engine::applySpace` (the governor weight and the make-up) and `Engine::controlTick` (the duck). The hook is read in `host/render/main.cpp` after `prepare()` (no memory change). Off by default, and inside `#ifndef IV_FIXED_VOICINGS`.

| | What | Result |
|---|---|---|
| **T** (`1:3.5:0.6:0.8:6`) | MASSIVE's make-up trimmed by 3.5 dB × layer(0.6 → 0.8) × (1 − w)⁶, PLATE's and HALL's v1.0.53 long-tail trim given to MASSIVE | **Rejected.** Pad at its level is fixed (0.80: 3.7 → 0.2) and the cliff goes. But a static trim can't follow a level-dependent endless: stabs at their level now jump **up** 2.9 dB at 0.80 → 0.81, and quiet pad up 3.8 dB. The cliff flips direction instead of going away. |
| E2 (`2:0:0.8:0:3`) | governor and duck ramp as t³ inside 0.80-0.85 only | Spreads the cliff (pad hot: worst step 1.9 dB per 0.01) but leaves 0.74-0.80 as loud as before. Stabs at their level grow a new +1.3 dB hump at 0.81, because the crossover lengthens the tail before the governor holds it. |
| E1 (`2:0:0.7:0:3`) | governor **and** duck ramp as t³ over 0.70-0.85 | Good; same as E1g within 0.1-0.6 dB. |
| **E1g** (`3:0:0.7:0:3`) | the **floor governor only** ramps as t³ over 0.70-0.85; the duck as built | **Recommended prototype.** |

Shape sweep (M8, pad and stabs, +0 / +6): from 0.80 (t³), 0.75 (t³), 0.70 (t³, t⁴, t⁶), 0.65 (t⁴), 0.60 (t⁵). From 0.65-0.70 with t³-t⁴ is the sweet spot; 0.60/t⁵ is almost the same and touches 0.60-0.70 by ≤ 0.4 dB. t⁶ leaves a 2 dB step at 0.77 → 0.80.

### E1g, measured

- [M] **The cliff, steepest change per 0.01 of DECAY over 0.70-0.85** (as built → E1g): M8 pad 2.5 → 0.3 (at its level), 3.8 → 0.8 (hot); keys 3.5 → 0.6, 4.6 → 0.9; stabs 0.4 → 0.4, 3.5 → 0.7. M5: pad 3.0 → 0.5, 4.2 → 0.9; keys 2.9 → 0.5, 4.4 → 0.9; stabs 3.7 → 0.6 hot. Quiet input (-10 dB) is unchanged, bit for bit in level, because the governor never acts below its floor. The rise there (2-3 dB per 0.01 near 0.84-0.85) is the crossover's T60 heading to infinity, as built.
- [M] **The level** at the material's level, M8 pad (re dry): 0.70 2.8 · 0.74 2.6 · 0.77 1.7 · 0.80 0.9 · 0.81 0.8 · 0.85 0.0 · 1.0 0.1. That's monotonic from 0.65 down to endless, where today's curve goes 2.8 → 4.1 → 0.1. The finite top's excess over endless goes from 4.0-5.3 dB to 2.5-3.6 dB (the remainder is the 0.70-0.74 shoulder, which stays as built at ≤ 0.70).
- [M] **Bit-identity** (`bump_identity.py`, ref = `.claude/worktrees/ref-main/build/iv_render`, 140 cases, 60 s each). The cases: MASSIVE SIZE 0.2/0.5/0.8, HALL and PLATE, pad hot, keys and stabs, DECAY 0.2/0.45/0.6/0.7/0.85/0.9/1.0, moves 0.85 → 1 → 0.9 and 0.6 → 0.2 → 0.55, a freeze tapped at 20 s and 40 s at DECAY 1, plus every SPACE at 0.75/0.78/0.8/0.82. **SAME 128/140** with E1g on; the 12 that differ are exactly MASSIVE at 0.75/0.78/0.80/0.82 (×3 SIZEs), as intended. HALL and PLATE are SAME everywhere. With the hook off: SAME 140/140. `tools/same_render.py`: SAME 60/60 with the hook off and on (its cases never sit in MASSIVE 0.70-0.85, which is a coverage gap worth knowing about). ctest: 100 % passed out of 21.
- [M] **Clicks:** in a 1 ms frame scan (second-difference energy over its ±50 ms median), no frame is 12 dB over its neighbourhood in either renderer. The maximum is 5-7 dB in both, the same to 0.4 dB, on held settings 0.72-0.82 and on turns down20, up20, down4, and a 0.68 ↔ 0.86 wiggle every 2 s for 64 s at both input levels. Peaks: the same or lower (wiggle hot: 0.591 → 0.555).
- [M] **Brightness:** the share above 4 kHz while playing changes by ≤ 0.2 dB. The governor's `darken` term at finite DECAY makes no audible difference.
- [M] **The tail after the playing stops** (the audible side effect): at 0.75-0.80 the tail starts 1.4-4.9 dB lower and decays at about the same rate (M8 hot at 0.80: -12.9 → -17.8 dB at the stop, -18.7 → -22.9 9 s later). It now joins 0.82's tail (-18.2) instead of sitting 5 dB above it: today's tail level has the same cliff at 0.80-0.82.
- [I] **Stability:** the governor can only add loss here (`extraRate ≥ 0`). The hold's negative make-up needs `tailCaptured_` or a freeze, both still keyed by the as-built weight, so nothing can grow. The duck is unchanged.

### Cost

- **Flash** [M]: a firmware trial (`make -C firmware MODE=release BUMP_EARLY=1`, `map::floorWeight` in `core/params/Mappings.h`, t·t·t, no `pow`) builds at **112,620 B, +56 B, 20 B under the 112,640 B budget**. E1 (duck too): 112,628 B, +64 B. Release without the flag: byte-identical to main's `firmware/build/sublustris_versio.bin` (112,564 B). Note that the desktop prototype uses `std::pow(t, 3)` while the trial uses t·t·t. The endpoints are exact either way; a build would use t·t·t in both.
- **CPU** [I]: the weight is ~10-20 cycles per block (< 0.01 % of the 10,000 cycles a sample). With the governor active at 0.70-0.85, its loss changes every control chunk, so each line's `AbsorbMemo` recomputes its two `pow`s, as it already does at endless and whenever DECAY moves. That's no new worst case, but the module hasn't been checked.
- **Risk:** low. Everything outside MASSIVE 0.70-0.85 is bit-identical. ADR 0008's statement that the governor works "at infinite DECAY" would need an amendment, and the test tables for T60 at 0.70-0.80 (§8) should be re-checked: a fed tail decays faster above the floor there, though the tail's own T60 below the floor is unchanged.

## Options for the owner (a page, not decided here)

1. **E1g** (recommended to audition): the endless "settling" fades in from 2 o'clock rather than switching on at 3. Sound change: audible at 0.70-0.85 in MASSIVE when played at a normal or hot level (up to 4.6 dB quieter while playing at 0.80, the tail correspondingly lower); nothing elsewhere. +56 B. Musical question: "should the last stretch before endless already start to hold the tail at its settled level, so turning towards endless glides instead of dropping at 3 o'clock?"
2. **Leave it** and document it as the nature of endless: it's quieter than the finite top if you play hard and louder if you play softly. Supermassive's Sagittarius mode does a milder version of the same.
3. **Not prototyped here (outside the brief):** to also remove endless's 2-3 dB gap to 0.60-0.70 at a normal playing level, either endless gets louder while playing (the floor or the duck; ADR 0033 is the owner's pick) or 0.60-0.70 gets quieter (MASSIVE's make-up). Both change settings the owner asked to keep.
4. **PLATE** shows the same cliff (1.0-2.3 dB per 0.01, 1.7-5.2 dB). The same ramp would apply there (`bumpWeight` is MASSIVE-only today; a one-line change).

## Not checked

The module: its maths differs from the desktop's (no fused multiply-adds, flush-to-zero), and CPU at 0.70-0.85 wasn't profiled. Listening: nobody heard E1g. Sizes other than 0.5 and 0.8 for the level curves (SIZE 0.2 only in the identity proof). WEAR other than 0.1, and BLOOM / SHIMMER. Input levels other than -10 / 0 / +6 dB re the material. The jack-to-dBFS calibration, which decides which of those rows is the owner's real playing level. The 0.65 wiggle.
