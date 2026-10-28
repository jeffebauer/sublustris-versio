# Review 2, agent "hole": MASSIVE's 250 Hz hole at SIZE noon (29 Oct 2026)

Branch `review2/hole` off `review/overnight-2` (v1.0.55). Desktop only (the renderer; FMA on, FZ off). Tags: **[M]** measured
tonight, **[C]** code, **[I]** inference. Scripts in `tools/review2/hole_*.py`; every render deleted once measured.

## Short answer

- **[M] Confirmed, and it's the two tanks ringing alike.** MASSIVE's main tank (24 kHz) and its still-lows tank (12 kHz)
  have the same Hadamard, the same early-tap layout (fixed seed 0xEA51), the same in-loop allpasses and lines equal in
  seconds to within the prime rounding. At SIZE 0.50 their outputs near the 300 Hz split are 52-83 % coherent (SIZE 0.40:
  8-34 %), and a Butterworth LP + HP of one signal notches at the crossover. The interference (sum against the two
  tanks' powers, 200-400 Hz) at SIZE 0.50 is **-2.5 to -6.7 dB** on every stimulus (pink, pad, keys, a synth bass),
  every DECAY (0.3 / 0.6 / 0.9 / 1.0), both channels, while playing and in the tail.
- **[M] Noon isn't the only place.** MASSIVE's full scan (DECAY 0.6) finds a second hole at **SIZE 0.75** (-5.3 dB while
  playing, -3.4 in the tail on pink; -5.8 / -3.6 on the bass) and smaller ones at 0.70 (-3.8) and 1.00 (-3.9).
  0.46 and 0.48 sit at -1.4 to -4.2. Most other SIZEs carry -0.5 to -2.5 dB. blind's dip measure missed 0.75.
- **[M] HALL isn't affected:** its interference is within -0.5..+0.4 dB at every SIZE (one -0.9). HALL's outliers at SIZE
  0.10 / 0.15 in blind.md are something else (most likely few modes per third-octave in a small room on one stimulus).
  [I] HALL's lines are 5-20x shorter, so many more passes a second: the rounding and the motion pull the tanks apart fast.
- **[M] It also makes the endless MASSIVE quieter than designed.** The floor governor adds the two tanks' powers, not the
  summed signal, so where they cancel the output sits under the floor. Fed at the pad's level and 6 dB hotter, endless
  MASSIVE at SIZE 0.5 / 0.75 settles 1.2-3.5 dB lower than the same with the fix.
- **Fix (prototype, `IV_HOLE_FIX=1:0.94`, off by default):** MASSIVE's low tank's lines x 0.94 (6 % shorter than the main
  tank's; HALL as built). Interference within -0.7..+0.8 dB at every SIZE 0..1 and 0.45-0.55 (pink within +-0.3). No new
  hole anywhere. Firmware +24 B as a trial switch (112,588 B, 52 B under budget). It's a sound change: the owner listens.
- **The "wiggle" at DECAY 0.65 is a different mechanism:** NATURAL's built-in swell, whose attack time follows DECAY. See below.

## 1. Confirming (hole_measure.py, hole_lengths.py)

Stimuli 6 s then 4 s of silence; MASSIVE, BLEND 1; three renders each: as built, the main tank alone and the still-lows
tank alone (`IV_HOLE_SOLO`, a desktop-only diagnostic: the wet from one tank, both running and governed as built; the
two solos sum back to the full wet to -135 dB, except one bass case where the limiter acts, -19 dB at one peak).
Windows: playing 2-6 s, tail 6.3-8.3 s. Values are means of L and R.

Interference gain 200-400 Hz, dB (sum vs the tanks' summed powers; 0 = power complementary as designed):
```
stim  dec  win  .40 .45 .46 .47 .48 .49 .50 .51 .52 .53 .54 .55 .60
pink  0.6  play -1.6 -2.5 -3.3 -0.3 -3.2 -0.6 -5.5 -1.8 -0.2 -1.0 -1.8 -0.5 -1.1
pink  0.6  tail -1.0 -1.5 -2.9 -0.1 -2.3 -0.1 -5.0 -0.6 +0.3 -0.4 -1.1 +0.3 -0.5
pad   0.3  tail -1.5 -1.9 -2.8 -0.2 -1.4 -0.6 -6.4 -2.0 +0.1 -1.7 -1.3 -0.3 -0.2
pad   0.6  tail -0.6 +0.4 -1.4 -0.2 -1.9 -0.3 -2.5 -1.0 +0.4 -0.4 -0.3 +0.1 +0.0
keys  0.3  tail -2.7 -2.4 -4.2 +0.3 -3.3 -0.1 -6.7 -2.2 +0.1 -0.6 -2.5 +0.3 -0.4
keys  1.0  play -1.4 -1.2 -3.3 -0.6 -2.7 -0.3 -6.6 -3.8 +0.6 -0.8 -2.3 -2.1 +0.9
bass  0.9  tail -3.3 -3.5 -4.2 -0.2 -3.3 -0.4 -5.9 -1.9 -0.1 -1.1 -1.8 +0.0 -0.8
bass  1.0  play -2.0 -2.7 -4.0 -1.1 -4.5 -0.6 -5.6 -3.4 +0.2 -1.7 -2.2 -0.2 -1.3
```
(all 32 rows: SIZE 0.50 is the minimum in every one, -2.5 to -6.7.) Coherence 200-400 Hz at 0.50: 0.52-0.83; at 0.40:
0.08-0.34; at 0.47 / 0.49: 0.09-0.32. blind's dip (mean 160/400 - 250 Hz third-octave) on pink at 0.50: +4.6 to +6.0 dB
while playing (DECAY 0.3-0.9), +2.8 to +4.8 in the tail; on music the dip measure follows the notes and means little.

Line lengths (hole_lengths.py, replicating `Fdn::setMeanSeconds`): at SIZE 0.50 the eight lines differ by
+0.12 -0.29 -0.38 +0.21 +0.38 -0.13 +0.04 +0.04 ms (main - low); a coherence index |mean(e^(j 2 pi 280 Hz dt))| is 0.92.
It's 0.90 at 0.45 and 0.91 at 0.75 too, 0.18-0.83 elsewhere: a rough predictor (0.75 is a real hole; 0.45 a milder
one). The lengths don't depend on DECAY [C].

MASSIVE full scan, interference 200-400 Hz, DECAY 0.6 (pink while playing):
`.00 -1.1 .05 -0.6 .10 -0.7 .15 -0.9 .20 -1.0 .25 -2.1 .30 -2.5 .35 -0.1 .40 -1.6 .45 -2.5 .50 -5.5 .55 -0.5 .60 -1.1
.65 -1.6 .70 -3.8 .75 -5.3 .80 -1.6 .85 -0.3 .90 -2.4 .95 -2.5 1.00 -3.9`. HALL, same: -0.4..+0.1 everywhere.

## 2. The fix (IV_HOLE_FIX, Engine::setHoleFixPrototype)

Modes: 1 = MASSIVE's low tank's lines x amount (default 0.94; < 1 so the memory is as built); 2 = the low tank's ratios in
reverse order (the same lengths on other lines: for a Sylvester Hadamard that's the original loop with its sign flipped
each pass, so the low tank's modes fall between the main tank's); 3 = the low tank inverted (comparison only: a bump in
place of the notch). All desktop-only (`#ifndef IV_FIXED_VOICINGS`); the firmware trial is `make HOLE_FIX=0.94`
(`IV_HOLE_FIX_TRIAL`, mode 1 built in).

Scans, interference 200-400 Hz, pink and bass, DECAY 0.6, SIZE 0..1 in 0.05 and 0.45-0.55 in 0.01:
- x 0.94 (MASSIVE): -0.7..+0.8 dB (pink -0.3..+0.3). x 0.90: -0.8..+0.5. Reversed (mode 2): -0.6..+0.7 MASSIVE, HALL
  -1.7 once (bass tail, SIZE 0), else within +-0.7. All three work; x 0.94 is the smallest change and the easiest to say.
- blind's dip on pink with x 0.94: SIZE 0.50 5.2 / 4.6 -> 2.2 / 0.9 dB (play / tail); the scan's range -0.7..+2.7 while
  playing, -1.3..+2.5 in the tail (as built up to +5.2 / +4.6). The leftover ~+2 dB around 0.40-0.55 is the scan's spread.

Gates: **hook off SAME 60/60** against the pinned `ref-main` renderer. Hook on (x 0.94): DIFFER 21 of 60, exactly the 20
MASSIVE cases and `flips` (which passes through MASSIVE); every PLATE and HALL case SAME. ctest **100 % tests passed out
of 21**. Firmware all-variants: release 112,564, m0test 83,556, profile 127,808 B, all three **cmp-identical** to the base
built from `review/overnight-2`. Trial `HOLE_FIX=0.94`: release **112,588 B (+24)**.

Loop checks (hole_energy.py; bump's looped pad, MASSIVE, as built -> x 0.94; hook line seen in every fixed render):
- Endless, fed 50 s at -10 / 0 / +6 dB, SIZE 0.5 and 0.75: loudest 1 s -20.1..-19.5 -> -19.9..-18.6 dBFS; level 40-50 s
  -21.0..-23.4 -> -19.9..-20.6 (the fix settles at the floor, see above: +0.1 to +3.5 dB, most at hot input); peaks
  -8.5..-7.2 -> -8.5..-6.9 dBFS; 10-20 s after the playing within 0.7 dB of as built.
- Infinite hold (freeze tapped at 20 s, never released), DECAY 0.6 and 1, SIZE 0.5 / 0.75: drift over 60 s +0.13 / +0.07
  / -0.07 / +0.65 dB -> -0.52 / +0.37 / -0.69 / -0.04 dB; peaks -6.8..-5.2 -> -6.8..-5.5 dBFS. Bounded, far under the limiter.
- SIZE moves at DECAY 0.9 (a slow 0.4 -> 0.6; flicks 0.5 <-> 0.75 every 4 s): 1 ms click scan 9.1 / 9.0 -> 8.5 / 9.0 dB
  over the median (nothing near 12); loudest 50 ms above 3 kHz 7.1 / 6.9 -> 6.7 / 7.0 dB; peaks within 1 dB.

Draft pair (`renders/review2_hole_draft/`, hole_draft.py): MASSIVE SIZE 0.5 DECAY 0.6 wet, pad + synth bass 14 s then
6 s tail; `as_built.wav`, `fixed.wav` level-matched (the fixed one was +1.4 dB louder, K-weighted). Third-octaves while
playing as built -> fixed: 160 Hz +3.6, 250 Hz +3.1, 100 Hz -1.6, 400-1000 Hz -1.2..-1.5 (after the match). Clicks
15.8 / 15.8 dB (the bass's own plucks), above 3 kHz 6.1 -> 4.7 dB, peaks -5.2 / -4.6 dBFS. README and _verify.txt there.

## 3. The 0.65 "wiggle" (hole_wiggle.py)

bump.md's ripple (MASSIVE, pad, 50 s of playing, K-weighted level 25-49.5 s re the dry), DECAY 0.55-0.78 in 0.01:
```
size variant   .60 .61 .62 .63 .64 .65 .66 .67 .68 .69 .70
0.5  built     +2.7 +2.2 +1.9 +2.7 +3.7 +3.9 +3.3 +2.4 +2.3 +2.8 +2.4
0.5  onetank   +3.8 +3.6 +3.4 +3.3 +4.0 +4.5 +3.9 +3.2 +3.4 +3.9 +3.4
0.5  nomakeup  +0.1 -0.3 -0.4 +0.6 +1.7 +1.9 +1.5 +0.7 +0.7 +1.4 +1.1
0.5  bloom     +1.8 +1.9 +2.1 +2.2 +2.3 +2.4 +2.5 +2.7 +2.8 +2.9 +3.0
0.8  built     +1.9 +2.0 +1.7 +2.3 +3.4 +3.4 +2.7 +2.5 +2.7 +3.0 +2.8
0.8  bloom     +2.2 +2.3 +2.4 +2.5 +2.7 +2.8 +2.9 +3.0 +3.1 +3.3 +3.4
```
- **[M] Not the two tanks:** one tank (`IV_STILL_LOWS=0`) ripples the same; the tanks' interference (100-600 Hz) is flat
  across DECAY (SIZE 0.8: -0.6..-1.2 dB). Not the make-up (`IV_DECAY_MAKEUP=0` ripples the same), not the duck
  (`IV_DUCK_RATIO=1` identical: the duck doesn't act below 0.80 at this level).
- **[M] It's NATURAL's swell:** MASSIVE's built-in swell lays its 12 taps over an attack of 0.5 -> 0.2 s across DECAY (x
  SIZE's mass, `massiveNormalAttackSeconds`) [C]. On held chords the taps' sum reinforces or thins the notes as their
  spacing slides: +-0.5-1 dB, its pattern moving with SIZE (SIZE 0.2 peaks at 0.61-0.64, dips at 0.67-0.68). With ONSET
  on BLOOM (its attack from DEPTH, the same at every DECAY) the curve is smooth; on pink noise it's smooth in NATURAL too
  (+1.0..+3.3, monotonic). [I] Related to the owner's "the 12 points seemed to accentuate the mids" (swell_seed).
  No fix proposed: it's material-dependent and small; a page could ask whether it's heard at all.

## Open risks

- The fix changes MASSIVE's sound at every SIZE (it fills in 200-400 Hz by 0.5-2.5 dB at most SIZEs, 3-6 dB at the holes)
  and its endless level while playing (+1-3.5 dB at noon / 0.75 at the pad's level and hotter, now at the floor the
  governor aims for). The owner's MASSIVE level picks (level_duck, level_glue, decay_swell, bump's E1g) were heard
  with the hole; bump's numbers at SIZE 0.5 would move.
- Lows' modes: the low room is 6 % shorter, so individual bass notes land on other modes (the draft's 100 Hz -1.6 dB is
  that kind of scatter, not a trend). Needs ears on bass-heavy material.
- Goldens: MASSIVE goldens would move with the fix built in (IV_GOLDEN_UPDATE + a SPEC changelog line).

## Not checked

The module (no FMA, its own rounding; the fix's lengths are integer primes, so the module computes the same lengths
unless a float product lands on an integer boundary: unverified). CPU (none expected: the same work, shorter lines).
SHIMMER / BLOOM / WEAR / MOTION corners with the fix beyond same_render's cases. Sample rates other than 48 kHz (the
plugin at 44.1 / 96 kHz). Listening: nobody has heard the draft.
