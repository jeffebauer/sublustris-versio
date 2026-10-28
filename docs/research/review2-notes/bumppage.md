# The loudness bump before endless: PLATE, re-checks, the page (overnight review 2, "bumppage", 29 Oct 2026)

Builds on `review2-notes/bump.md` (branch `review2/bump`, phase 1). Branch `review2/bumppage` (off `review2/bump`). Tags: **[M]** measured here (desktop renderer), **[I]** inference.

## Short answer

- **PLATE gets the ramp too, plus one extra piece.** With the floor governor alone on the ramp (phase 1's E1g given to PLATE) the hot cliff goes (2.3 → ~0.5 dB per 0.01 of DECAY) but at PLATE's own playing level a new dip appears at 0.80 with a 0.7 dB step back **up** at 0.80 → 0.81. The cause is PLATE's v1.0.53 long-tail make-up trim (1.95 dB × layer × (1 − governor weight)³): it lets go at 0.80 → 0.81 by +0.9 dB, which the governor's big step down used to hide. Fading that trim on the same ramp (prototype **mode 4**) makes PLATE's curve flat at its own level and smooth hot. Shape sweeps (from 0.65-0.75, t³-t⁶) with the governor alone all kept the dip; with the trim on the ramp, t³ from 0.70 is the best and is the same ramp as MASSIVE.
- **Final version (B on the page): `IV_BUMP=4:0:0.7:0:3:5`.** MASSIVE: exactly phase 1's E1g (bit-identical renders). PLATE: the governor's weight and the long-tail trim's fade both follow t³ over DECAY 0.70-0.85. HALL: untouched.
- **Firmware trial `BUMP_EARLY=3`: 112,628 B, +64 B, 12 B under the 112,640 B budget.** (`BUMP_EARLY=1`, MASSIVE only: 112,620 B; `=2`, PLATE's governor without its trim: 112,620 B.) Release, m0test and profile without the flag are byte-identical to main's.
- **On a 20 s turn, B's difference is real but moderate.** It's biggest held at 2:30-3 o'clock (MASSIVE at 0.78: 2.5 dB quieter at the pad's level, 3.3 dB hot) and on the way back up through 0.79-0.82 (1.4-2.9 dB quieter just before the drop, so the drop is smaller). The level still rises a lot from endless to 0.70 when played hot, in both (about 7 dB in MASSIVE): that's 0.70 being louder than a held endless, not the cliff.

## The hook (desktop only, off by default)

`IV_BUMP="mode:db:from:to:pow[:spaces]"`. New here: an optional 6th field, the SPACE bits that get the early ramp (1 PLATE, 2 HALL, 4 MASSIVE; default 4, so phase 1's strings render as before), and **mode 4** = mode 3 (the floor governor on the ramp, the duck as built) + in PLATE/HALL the long-tail trim's (1 − weight)³ taken from the ramp's weight instead of the as-built one (`Engine::applySpace`, a correction factor on `decayGain_` that is exactly 1 where the weights agree). Firmware trial: `make -C firmware MODE=release BUMP_EARLY=3` (`map::floorWeight` for every SPACE but HALL, and `decayLevelMakeup`'s trim on it).

## PLATE (SIZE 0.5), level while playing vs DECAY, dB re dry (steady 25-49.5 s, K-weighted)

```
DECAY              0.60  0.70  0.74  0.77  0.79  0.80  0.81  0.82  0.83  0.84  0.85  1.00
as built  pad  -10 -0.3   1.3   2.1   2.7   3.1   3.3   5.2   6.9   8.7  10.2  10.4  10.4
as built  pad   +0 -0.3   1.3   2.0   2.4   2.6   2.8   1.8   1.6   1.5   1.3   1.1   1.1
as built  pad   +6 -0.8   0.1   0.5   0.8   1.0   1.0  -1.3  -2.1  -2.8  -3.5  -4.1  -4.1
as built  keys  +6 -1.6  -0.7  -0.4  -0.1   0.1   0.2  -1.9  -2.8  -3.5  -4.2  -4.9  -4.9
as built  stabs +6 -1.2   0.2   0.7   1.1   1.3   1.4   0.1  -0.4  -0.8  -1.3  -1.8  -1.8
mode 4    pad  -10 -0.3   1.3   2.2   3.2   4.1   4.6   5.7   7.1   8.7  10.2  10.4  10.4
mode 4    pad   +0 -0.3   1.3   1.8   1.7   1.6   1.6   1.6   1.6   1.5   1.3   1.1   1.1
mode 4    pad   +6 -0.8   0.1   0.2  -0.6  -1.1  -1.3  -1.8  -2.4  -2.9  -3.5  -4.1  -4.1
mode 4    keys  +6 -1.6  -0.7  -0.7  -1.3  -1.7  -1.9  -2.4  -3.0  -3.6  -4.2  -4.9  -4.9
mode 4    stabs +6 -1.2   0.2   0.5   0.3   0.2   0.2  -0.2  -0.5  -0.9  -1.3  -1.8  -1.8
mode 3 (governor only) pad +0: 1.3 1.7 1.2 · 0.3 1.0 1.4 1.4 1.3 1.1 1.1 (0.70 0.74 0.77 · 0.80 ...; the dip and the step up)
```
- [M] Steepest change per 0.01 of DECAY over 0.70-0.85, as built → mode 4: pad +0 1.0 → 0.2; pad +6 2.3 → 0.6; keys +6 2.1 → 0.7; stabs +6 1.3 → 0.4. Keys and stabs at their own level: ≤ 0.2 dB change at 0.81-0.85.
- [M] **Quiet playing (−10 dB) changes in PLATE, unlike MASSIVE:** the trim is a make-up gain, so it acts at every input level. 0.77-0.81 come up 0.5-1.3 dB, towards endless (which is the louder one when played quietly). Its steepest step stays 1.6-1.8 dB per 0.01 (0.82-0.84), as built: that's the crossover's T60 heading to infinity.

## Bit-identity (desktop)

- [M] `tools/same_render.py` against the pinned `.claude/worktrees/ref-main/build/iv_render`: **SAME 60/60** hook off, **SAME 60/60** with `IV_BUMP=4:0:0.7:0:3:5` (and with `3:0:0.7:0:3:5`).
- [M] `tools/review2/bump_identity.py` (140 cases, every SPACE, DECAY 0.2-1.0, moves, freezes) with the final hook: **SAME 124/140**; the 16 that differ are exactly MASSIVE ×3 SIZEs and PLATE at DECAY 0.75/0.78/0.80/0.82. HALL SAME everywhere; every DECAY ≤ 0.70 and ≥ 0.85, the moves 0.85 → 1 → 0.9 and 0.6 → 0.2 → 0.55, and the freezes at endless SAME.
- [M] On the page's own material, the held grid (`bumppage.py holds`, in the page's `_verify.txt`): see below.
- [M] MASSIVE renders with mode 4 / spaces 5 are bit-identical to phase 1's mode 3 / spaces 4 (E1g).
- [M] ctest: **100% tests passed out of 21**.

## The page: `renders/review2_bump/` (main checkout)

Question: "When you turn DECAY slowly down from endless (or up into it), should the level glide, or is the drop at about 3 o'clock fine?" A = as built (pinned reference renderer), B = the gentle ramp. Columns MASSIVE SIZE 0.8, PLATE SIZE 0.5. Rows: slow turns (endless 8 s, down to 0.70 over 20 s, 6 s there, back up to endless over 20 s, 6 s, stop) with the pad at its level, the pad 6 dB hotter, the keys at their level; and DECAY held at 0.78 with the pad at its level and hot. Keys +6 dB is left out: the keys peak at −4.3 dBFS, so their dry would clip. Not level-matched (the README says so), 100 % wet with the live MIX fader (50 %), pick buttons and the hidden twin; per-row chapters. The page files are 16-bit with TPDF dither (24-bit put the page at ~620 MB; it's 347 MB); every check uses the 32-bit float renders. Script: `tools/review2/bumppage.py` (`page`, then `holds`).

From `_verify.txt` [M]:
- Every A render (pinned reference) is bit-identical to the hook build with the hook off; every B render's stderr carries the `IV_BUMP: mode 4 … spaces 5` line.
- Slow turns: A and B are **bit-identical until 18.00 s**, when the knob passes 0.85 on the way down, in every card.
- Level (3 s windows, re dry), the slow turn's largest change between 2 s steps, A → B: MASSIVE pad 1.4 → 1.0 dB; pad hot 3.3 → 2.1; keys 1.9 → 1.9 (B's is where the knob stops at 0.70). PLATE pad 2.2 → 2.1, pad hot 2.8 → 2.6, keys 1.8 → 1.8 (PLATE's largest steps come from the material, at the same moments in A and B). B − A on the way up at DECAY 0.79-0.82: MASSIVE −1.4 (pad), −2.9 (hot), −1.5 (keys); PLATE −0.4 to −1.8.
- Held at 0.78 (15-29 s): MASSIVE A 3.9 / B 1.4 dB re dry (pad), 0.2 / −3.1 (hot); PLATE 2.5 / 1.7, 0.9 / −0.7.
- Clicks (1 ms second-difference scan): B's maximum equals A's within 0.7 dB in every card and has the same number of frames over 12 dB (those few are the keys' and PLATE's own attacks, in A too). >3 kHz in 50 ms frames, B against A: at most +1.0 dB (mostly B is darker by up to 2-8 dB where it's quieter). Peaks: B ≤ A in every card (largest 0.624, ceiling 0.891).

## Loop-change safety and side effects (final version) [M]

- **Fed holds** (pad looped 60 s at −10 / 0 / +6 / +12 dB re its level; MASSIVE SIZE 0.8 and PLATE SIZE 0.5; DECAY 0.72 / 0.76 / 0.80 / 0.84): B's level at 48-58 s never above 35-45 s by more than +0.6 dB, and that one (MASSIVE, −10 dB, 0.84) is identical in A (B − A 0.0: the as-built tank still filling at a ~10 min T60). Peaks ≤ 0.732 (ceiling 0.891), B ≤ A everywhere except PLATE played quietly (0.204 vs 0.177 at 0.80, the trim's +1.3 dB). B − A while playing: MASSIVE 0 to −4.8 dB (deepest at 0.80, hot), PLATE −2.4 to +1.3 dB.
- **Infinite hold** (pad +6 dB for 20 s, then 70 s of silence) at endless: bit-identical in both SPACEs, and with a freeze at endless. **Freeze at 0.78** (tapped at 15 s, released at 60 s): B freezes what it has, so the frozen pad is 1.8 dB quieter in MASSIVE and 0.4 dB in PLATE; level steady while frozen (−12.2 → −12.4 dBFS), clicks within 0.7 dB of A's.
- **Tail after the playing stops** (`bump_side.py`, now with PLATE): MASSIVE at 0.75-0.80 starts 1.4-4.9 dB lower (as phase 1); PLATE 0.5-3.3 dB lower; 0.72 and 0.82 within 0.2 dB of A.
- **Brightness:** MASSIVE's share above 4 kHz while playing moves ≤ 0.2 dB. **PLATE's rises 0.5-2.4 dB at 0.75-0.80** (pad hot at 0.80: −47.2 → −44.8 dB). Absolute energy above 3 kHz is never more than +0.7 dB over A (page cards), so it's the lows and mids coming down more than the highs: at those settings B's PLATE is a touch brighter-balanced while played. Worth an ear.
- **Clicks** (1 ms scan) on held 0.72-0.82, turns down20 / up20 / down4 and the 0.68 ↔ 0.86 wiggle, both SPACEs, pad +0 / +6: B's maximum within 0.4 dB of A's, the same frame counts over 12 dB (PLATE's 1-2 are the pad's own attacks, in A too). Peaks equal or lower (wiggle hot: MASSIVE 0.591 → 0.555, PLATE 0.609 → 0.591).

## Not checked

The module (no fused multiply-adds, release without flush-to-zero; desktop identity proves the desktop only), CPU at 0.70-0.85 on the module, listening. SIZEs other than 0.5 (PLATE) and 0.2/0.5/0.8 (MASSIVE). WEAR other than 0.1, BLOOM/SHIMMER. The jack-to-dBFS calibration (which input row is the owner's real level).
