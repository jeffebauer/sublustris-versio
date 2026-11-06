# 0050 — PLATE's hit: held taps, an early tap, slower movement

**Status:** Accepted, 6 Nov 2026 (owner, D on every card and source of `renders/plate_hit`, the hard-left cards and SIZE sweeps too; confirmed in that session: "Yes, port D"). SPEC v1.0.74. Supersedes the v1.0.12 pick of the long-delay wander at full depth, for PLATE.

**Context:**
- **The notes:** launch players called PLATE and HALL "pretty similar" and the sound "thinner, wispier". Next to the owner's Valhalla Plate takes, ours built slowly and turned grainy at big SIZE.
- **The research:** `docs/research/plateau-vs-plate-2026-11-06.md`, how another Dattorro plate is voiced. Written from Dattorro's paper and that note; no Plateau or Valley code (ADR 0006).

**Decision:**
- **The output taps stay put.** They read at SIZE's floor (`kPlateMinScale`, 0.5 × Dattorro's offsets) at every SIZE, so a big plate starts as a plate instead of a spatter of separate echoes.
- **An early tap, each side off the first allpass of the other side's branch, at full weight.** The wet speaks at once, and a hard-left source still answers on the right first (v1.0.16's "more cohesive" pick).
- **Slower movement.** The first allpass's sine at a fifth of its rate (0.142 / 0.186 Hz), its floor at half depth (0.15), the long delays' wander at half depth (`kPlateLongModSeconds` 0.0003 s).
- **The smaller form:** with held taps the old and new reads in the SIZE crossfade are the same read, so the taps' fade, their agreement gain and `xfKOld_` are gone.
- **A trim:** `kPlateOutputTrim` 0.43 → 0.40 (−0.63 dB). The page was level-matched and D runs louder than A, and the wet-level pick (ADR 0049) was heard on A, so PLATE keeps v1.0.73's loudness.

**Rejected:**
- **A table of trims over SIZE or DECAY:** not added. See the spread below; most of it is between sources, which a table couldn't touch either.
- **Keeping the taps' fade:** it does nothing with held taps and costs flash.

**Consequences:**
- **Against the prototype's D:** 58 of 60 `same_render` cases are the same bits once the port is written in the hook's arithmetic; the other two are the crossfade cards the smaller form changes, at 1.8e-7 and 6.7e-7. As written (cleaner constants) most PLATE cases differ by about 2e-7, and by up to 2.9e-3 at WEAR 0.7-0.9, where a one-bit difference in a rate grows in the loop; the same control shows that's all it is.
- **HALL and CANYON** are the same bits as v1.0.73, 40 of 40 cases.
- **Loudness against v1.0.73**, integrated LU, 100 % wet, four sources, 7 DECAYs × 5 SIZEs: mean +0.01. By DECAY within ±0.05. By SIZE 0 / ¼ / ½ / ¾ / full: +0.25, −0.34, +0.28, −0.13, +0.01. Single cells (source mean) −0.78 to +0.63. By source: piano +0.75, pad +0.14, stabs −0.02, drums −0.81; single renders −3.00 to +1.46. Steady over DECAY's knob, pad and drums: −0.18 to +0.28.
- **Against the wet_loudness picks** (now minus v1.0.70 + the pick, mean of four sources): PLATE 9 o'clock +0.3 (pad +0.3, piano +1.0, drums −0.6, stabs +0.4), endless 0.0 (pad −1.3, piano +0.6, drums +0.3, stabs +0.2).
- **The hit** (one click, DECAY noon, SIZE 0 / noon / full): C80 −7.2 / −8.8 / −7.8 → −5.6 / −3.9 / −2.8; kurtosis 1.4 / 6.9 / 14.2 → 1.3 / 3.2 / 6.0.
- **Hard left at full SIZE leans right for longer, accepted:** D won those cards too. L over R, SIZE 0 / noon / full: 0-20 ms −130 / −132 / −134 (the left is silent, the pick kept); 20-50 ms −8.6 / −8.3 / −7.2; 100-300 ms +0.5 / −1.3 / −3.8 (A −0.6 / −0.9 / −0.2).
- **Metal:** the calibrated ringing peaks at 8.4 (flagged from 15). Lines at 1.6-6.4 kHz per mille, mean: 8.0 → 10.7, HALL 3.0, Valhalla's plates 12.4-12.6. The largest raw peak rises 14.4 → 17.1 at DECAY 9 o'clock, SIZE 0, at 5.2 kHz. A held 1 kHz's pitch spread: 6.27 → 2.27 cents.
- **Clicks,** worst 1 ms frame above 3 kHz: SIZE moves (crossfade, glide, frozen, Paint, endless, full shrink) at most +1.0 dB over v1.0.73, worst 10.8. Freezes 8.9-13.2; the 13.2 (stabs, SIZE full, DECAY 9 o'clock) is there without the freeze, a stab's own attack through the early tap. DECAY through the bend: 10.5 → 10.5 at MIX full, 10.6 → 10.4 at MIX 50 %.
- **A crossfaded SIZE move** (0.3 → 0.85, a held pad) reads −1.71 dB, −0.70 on v1.0.73: the steady level differs between those SIZEs. No jump in it.
- **Energy:** `energy_check.py` against v1.0.73: PLATE at most +0.69 dB in any window above −70 dBFS (the script's +6.63 is at −174 dBFS). MASSIVE's 10 trips are the same bits as v1.0.73. 10-minute holds pass in every SPACE: PLATE −23.4, HALL −18.5 to −18.4, CANYON −15.1 to −14.9.
- **Played hot** (each phrase to −1 dBFS; 16 cells): the limiter over 1 dB in 4 of 16 either way, worst 2.0 → 6.5 % of samples (the pad at DECAY's 0.75); those pad cells' worst click frame 11.5 → 13.7. Module's worst peak −0.48 → −0.79. At the material's own level the limiter never reaches 1 dB. The early tap adds peaks the loudness doesn't show.
- **Tests re-pinned,** each with why, none a change in what's being guarded except where noted:
  - `test_motion`: PLATE's left side 14.7 / 16.5 / 21.5 → 6.0 / 11.7 / 18.7 cents. Noon falls 11.7 → 3.1, and what MOTION adds holds: 3/4 left 11.8 → 11.3, full left 18.0 → 18.4; 9 o'clock adds 5.1 where it added 8.9. **The page was at MOTION noon, so the owner hasn't heard D at MOTION left of noon.**
  - `test_soak_regressions`' cassette plunge: 3.7 → 5.7 dB, limit 4 → 6.5. Not a plunge: the tank's slow beating on one lone 1 kHz sine at 44.1 kHz, with less modulation to smear it. 2.8 at 48 kHz (v1.0.73 3.2), chords 1.0-1.4, a pad 1.1.
  - `test_era`, PLATE, WEAR quickly through its start: +9.4 over a still +4.9 (was +7.9 over +5.5), limit +3 → +5 for that one case. Those frames are about −90 dBFS under a −14 dBFS chord.
  - `test_twin_level`: PLATE's twin 0.46 dB closer to its pad (−8.57 → −8.11), its v1.0.41 limit 1.5 → 2 dB.
  - `test_plate_freeze`'s paint growth, a fail on the hook, passes with the trim.
- **Flash:** release 112,036 B (−248; 604 B left of the 112,640 budget), profile 130,820 B (−248, 252 B left), m0test 83,556 B (unchanged). Memory: the long delays' room shrinks with the depth.
- **Not checked:** the module's CPU and volts, the owner's ears on the trimmed build, MOTION left of noon by ear. The plugin's test isn't built in the worktree (`libs/JUCE` is empty).
