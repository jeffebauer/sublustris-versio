# Soak audit of the Core (23–24 Oct 2026)

An overnight robustness test of the Core: hours of audio through `iv::Engine` with every knob moving, every gesture, SPACE flips mid-tail and nasty inputs, looking for anything that breaks. It started from `main` at `02af9fa` (SPEC v1.0.20: still lows, ERA make-up retune, Wander), on branch `worktree-agent-ad12ea5895b99e5ec`.

**In one paragraph.** 341 hours of audio, 12,287 episodes. Nothing blew up: no NaN or inf, no output above the limiter's ceiling, no finite tail that grew without bound or got stuck, no denormal slowdown on this Mac. The soak found **twelve real bugs**, all in *transitions* (a gesture, a flip, a switch, a knob jump) or in *held tails* (infinite DECAY after you stop playing, freezes of dying tails). Ten are fixed without changing the steady sound: static renders are bit-identical to before and the goldens pass. **One fix changes the sound and needs the owner's ears**: infinite tails no longer plunge when a loud input stops or fades (three commits, flagged "SOUND CHANGE"). **One problem is left for the owner** because it's a voicing decision: ERA's Midiverb range eats an infinite tail's mids and highs.

Tags as in the other research notes: **[M]** measured; **[I]** inference.

## What ran

| Run | Core | Seeds × audio | Audio | Episodes (chaos / hold / decay / gesture / flip) |
|---|---|---|---|---|
| main | up to d364454 (fixes 1–10, sound change part 1) | 96 × 2 h | 194 h | 2094 / 1408 / 1019 / 1137 / 1214 |
| confirm | up to 7eb52f2 (+ part 2, pad guard) | 96 × 1 h | 98 h | 1122 / 677 / 540 / 628 / 650 |
| final | up to 4f3322e (+ part 3) | 48 × 1 h | 49 h | 552 / 349 / 267 / 312 / 318 |
| **total** | | 240 runs | **341.5 h** | **12,287** |

Each run took one sample rate (a third each at 44.1, 48 and 96 kHz: 114 h each) and one block-size pattern (every fixed size in the list below got 8–20 runs; 35 runs had a random size every block). 9 CPU-hours on 8 cores of an M1 Pro, about 40× real time per core. Calibration runs before these (≈15 h at three rates) found most of the transition bugs; each run's findings were then reproduced in small probes. Note that a seed reproduces its run only on the Core that ran it: the fixes change what happens after the first gesture, so the same seed takes a different course on a later Core.

**The harness** is `tools/soak/soak.cpp` (built as `iv_soak` with the project; not a ctest suite). Each run is one continuous life of the Engine, from a seed, at one sample rate and one block-size pattern (fixed 1, 2, 3, 7, 16, 31, 32, 33, 47, 48, 64, 100, 127, 128, 200, 255, 256, or random 1–256 every block). It strings together five kinds of episode:

- **chaos** (20–90 s): every knob automated at random, glides from 5 ms to 8 s and jumps, ends favoured; SPACE and ATTACK switched at random; button and gate presses of every length, chords of the two; the input changing every 0.3–10 s between silence, DC, full-scale noise, sines, chords, clicks and decaying bursts.
- **hold** (3–6 min): DECAY infinite or a freeze (sometimes painted), half the time with the extremes (ERA full, DRIFT at either end, SIZE at either end), then minutes of silence. The tail must hold: its 4 s level, from 25 s after it settled, mustn't fall 3 dB or grow 2 dB.
- **decay** (30 s–5.5 min): a finite tail into silence. It mustn't rise 6 dB above its running low, and if it's had time to fall 160 dB it must end below −120 dBFS (no stuck noise or buzz).
- **gesture** (30–90 s): taps, paints, double taps, rapid toggles (5–50 ms apart), button + gate chords, over silence or a steady sine, chord, DC or noise. No clicks, no level jumps at a gesture.
- **flip** (40–90 s): SPACE flips in the middle of tails: plain, two in quick succession (inside the fade), straight after an unfreeze while the twin still sounds, while frozen, and a freeze landing inside a flip's fade.

Every sample, always: **NaN / inf**; **the ceiling** (|out| ≤ √(in² + 0.891²), which is exact for an equal-power MIX of the dry and a wet limited to −1 dBFS); **clicks** (the output's sample-to-sample second difference more than 10× its own level over the last 10 ms, where the input is smooth enough to tell: not during noise, clicks or bursts or for 8 s after them, not within 0.5 s of a knob jump, and not where clicks are the design: ERA's Midiverb crackle and the limiter's instant attack on a pad loud enough to reach it); **subnormal** outputs. Every block: time taken. Every 10 s: subnormal floats in the delay memory. Every finding is logged as a line of JSON with its seed, sample, settings and a reproduction command:

```
build/iv_soak --seed 12 --rate 44100 --minutes 15 --dump-at 29371542 --dump out.wav   # 4 s of in L/R, out L/R around it
build/iv_soak --seed 12 --rate 44100 --minutes 15 --trace-episode 6                     # the episode, second by second
SOAK_BIN=iv_soak tools/soak/run_soak.sh build-wt build-wt/soak/main 101 196 120 8      # the main run (seed s: 44.1 / 48 / 96 kHz by s % 3)
python3 tools/soak/analyse.py 'build-wt/soak/main/*.jsonl'
```

Runs are deterministic from the seed (only the timing numbers vary), so every finding reproduces. The bugs below were also each pinned down with a small probe and then a regression test in `host/tests/test_soak_regressions.cpp` (49 checks, ~10 s), which fails on the Core before the fix and passes after (one exception, noted at 6).

## Findings

### Fixed, steady sound unchanged

**1. A tap that landed part-way into a control chunk never unfroze (high, Plugin and Renderer).** [M] The Engine does its control work every 32 samples, and the gesture reader asked "has this press been held 0.3 s yet?" at the *start* of the chunk, before a press made inside it. The subtraction wrapped round to a huge number, so every such press became a paint at once, and the tap that should unfreeze just painted and let go. The firmware sends its button and gate at offset 0, so the module was fine; but every MIDI-note gate in the Plugin (sample-accurate offsets) and any Renderer press off the 32-sample grid hit it (31 times in 32). Fix: `GestureReader::tick` checks the press isn't in the future. *Commit 2fafb2e.*

**2. A SPACE flip soon after an unfreeze clicked (medium).** [M] When you unfreeze, the twin (what you played over the pad) keeps fading for up to 8 s, and a SPACE change made while frozen happens 1 s after the unfreeze. The flip faded the big tank but not the twin, then cut the twin off at the flip's silent point: a click of 30–123× the local level on a held chord. And the other space's twin, never cleared, could start replaying old sound. Fix: the twin fades with the flip and both twins restart clean at its silent point. *f0f79f3.*

**3. A freeze inside a flip's fade froze nothing (low–medium).** [M] Pressing freeze within the 50 ms a flip fades out (you hear the tail going and grab it) froze an empty tank: the flip carried on and restarted the tank under the freeze. Fix: the flip gives way, the old space fades back in as the pad, and the flip waits for the unfreeze like any flip does (ADR 0013). *f0f79f3.* (A freeze after the flip's silent point still freezes the new, empty space: that is the flip's design.)

**4. The small HALL clicked when SIZE jumped (medium).** [M] Since v1.0.17 a small HALL has allpasses inside its loop whose lengths follow SIZE. They followed the raw knob, so a SIZE jump (a CV, a fast twist, an unfreeze after SIZE moved) jumped their lengths: clicks of 50–200× the local level. Crossing noon was worse: there the allpasses were switched in or out of the loop at full length, and while out their buffers weren't written, so switching them in replayed stale sound (an 80 dB dropout for 30–90 ms after an unfreeze). Also HALL's T60 scale, output trim and diffusion amount stepped with the raw knob. Fix: the allpass lengths glide at SIZE's own semitone rate, read between samples while they move; they join and leave the loop from zero length; HALL's SIZE-dependent gains follow an 80 ms glide; the FDN's output trim glides (50 ms). Now 2× on the same jumps, like MASSIVE's. *f0f79f3, f7a4006 (flash).*

**5. REVERSE coming in while playing clicked (medium).** [M] Switching ATTACK to REVERSE, or the unfreeze that ends REVERSE's rest, fed REVERSE's freshly restarted short reverb the whole swell at once; 25–80 ms later, when that first came round the reverb's lines, it clicked (18–46×) through the crossfade. Fix: the taps fade into the reverb with REVERSE's own 50 ms crossfade. *8f68332.*

**6. A SPACE flip while playing in REVERSE clicked a TIME later (low–medium).** [M] The flip cleared REVERSE's recording line, which then held a hard edge where the input was cut off; each tap that reached it clicked, up to a TIME after the flip (1300× the local level from silence in one soak case). REVERSE doesn't depend on SPACE, so it now keeps running through flips, its recording faded with the flip as the tank's input is. *7efa2a5.* Residue: on a pure sine a faint roughness remains about a TIME after a flip into or out of MASSIVE (the predelay's jump at the flip's silent point, replayed by the taps; 7–9× a pure sine's tiny step size, 2× on a chord). Gliding the predelay instead removes it but puts a brief pitch drop into the new tank; not worth it. A follow-up from the confirmation soak: the flip still *snapped* REVERSE's TIME and SIZE glides to their targets, a jump now that REVERSE runs on (a click 20–35 ms into the fade back in, seeds 226, 245 and 273; all three clean after). They now glide on. *76e34ed.* (Its regression test can't tell old from new, because the glide's own chirp outweighs the snap; the three soak seeds are the before and after.)

**7. ATTACK switches and MASSIVE SIZE jumps swept the predelay (low).** [M] Switching ATTACK between NORMAL and BLOOM with TIME up moves the predelay between 0 and 250 ms, and SIZE moves MASSIVE's (0–100 ms); both jumped within one 32-sample chunk, a burst of sped-up sound into the tank. Now the predelay moves at most half a sample per sample (the input at 0.5–1.5× speed; TIME's own knob glide is unaffected unless faster), and below 2 samples it blends instead of jumping. The input diffusers' amount glides too (50 ms). *7efa2a5.*

**8. Letting go after painting burst the twin into the pad (low).** [M] At the release the pad's ceiling is lifted, and the paint's "room left" jumped to full while the paint was still fading out: a short burst (6–7×). The room now only moves while painting. *8f68332.*

**9. A pad frozen from a fast-dying tail swelled for minutes (medium).** [M] Freeze a short tail just after the input stops (a short DECAY), and the pad grew 20–55 dB over the next minutes (PLATE −61 → −40 dBFS, HALL −64 → −47; even a finished tail crept up from −165 dBFS). The pad's ceiling is taken 20 s after the freeze from an 8 s level, which still remembered the tail from before the freeze (a DECAY-0 PLATE falls ~130 dB in the second after a freeze, while the colour glides out), and the makeup lifted the pad towards it at 0.3 dB/s. Now, when the 8 s level is still more than 3 dB above both the pad's own level (50 ms) and its 1.5 s level, it starts over from the 50 ms level before the ceiling is taken. (The 1.5 s condition came from the main soak: with the 50 ms level alone, a pad whose level swings, DC fed into a PLATE, was caught in a dip and crushed by 30 dB.) Pads frozen from a held or steady tail are unchanged. *d364454, acbe686.*

**10. A phantom level for 2 s after a flip into HALL or MASSIVE (low).** [M] The big tank's governor kept the still-lows tank's level from before the flip and read it once into the fresh tank: a phantom level falling from about −48 dBFS for a couple of seconds (it nudges ERA's cassette compression and the paint room). `Fdn::reset` now clears it. *7efa2a5.*

### Fixed, sound changes: needs the owner

**11. Infinite tails plunged when a loud input stopped (medium–high).** [M] Fed loud enough to sit above the floor (−24 dBFS; easy with Eurorack levels), an infinite tail collapsed the moment you stopped playing, then crept back up over a minute or two:

| After a loud input stops (noise 0.7, SIZE 0.18) | dip below where it then holds, before | after the fix |
|---|---|---|
| PLATE, NORMAL | 27 dB (to −55 dBFS within 2 s; back after ~100 s) | 2.3 dB |
| HALL, NORMAL | 10.5 dB | 1.7 dB |
| PLATE, BLOOM (TIME full) | 11.3 dB | 2.0 dB |
| HALL, BLOOM | 3.5 dB | 1.4 dB |
| MASSIVE, NORMAL / BLOOM | 0.6 / 5.1 dB | 0.6 / 0.5 dB |

Why: the floor governor's loss follows how far the tail is over the floor, judged on a 1.5 s level. While you play, that loss balances the input (a fed tail decays at up to ~30 dB/s). When you stop, the 1.5 s level takes seconds to come down, so the full loss runs on long after the real tail has passed the floor. Separately, in BLOOM the swell keeps feeding the tank for up to 7.5 s after the input stops, and the hold took its level while the tail was still rising, then pulled it down through the 8 s level's lag.

Fix: the floor judges a *falling* tail on a 50 ms level as well (it takes the lower of its 1.5 s and 50 ms levels; `kGovernorFallSeconds`), and the infinite tail's hold waits for the big tank's own input (after the swell) to go quiet. ERA's cassette compression (2:1 above −18 dBFS from ERA 0.35) judged the tail on the same lagging level and did the same thing, so it now does the same (the confirmation soak's seed 225, an infinite PLATE fed a loud sine: the output fell from −23 to −48 dBFS before, and settles near −25 after; part 3). The first version did the fast judgement only once the input was quiet; the main soak then found that an input fading out over seconds (a decaying hit) never counts as quiet in time, and the tail still plunged (HALL −10 → −74 dBFS, then three minutes to come back), so now it always applies. A steady fed tail measures the same as before (`test_massive`: HALL −22.3, MASSIVE −23.6 dBFS, unchanged); finite tails and frozen pads are untouched (the goldens are finite and pass; static finite renders are bit-identical). What the owner will hear: an infinite tail now settles to the floor within a couple of dB when you stop (or fade out) and stays there, instead of ducking away and swelling back. *Commits 2127d78, d607816 and 4f3322e, "SOUND CHANGE (needs the owner)", parts 1–3.* Suggested listen: the same material through infinite PLATE, HALL and MASSIVE, NORMAL and BLOOM, stopping abruptly and fading out, before and after.

What remains, by design: with TONE fully dark an infinite tail loses its highs within seconds (ADR 0008's infinite high fade), so it can still sink up to ~10 dB under the floor after a bright input and be lifted back by the makeup over about a minute.

### Not fixed: for the owner

**12. ERA's Midiverb range eats an infinite tail's mids and highs (medium).** [M] From ERA 0.85 the loop gets the Midiverb box, whose ½ one-pole low-pass (v1.0.16, the owner's pick) acts on every pass. On a finite tail that's the intended darkening. At infinite DECAY it's a loss the floor governor doesn't control (ADR 0008 says darkening stops at the floor): a chord held in an infinite tail collapses, then the hold's makeup lifts what's left (the lows and the box's rounding residue) back up at 0.3 dB/s:

| Infinite, ERA 1, a chord played 5 s then silence | fall within 30 s | then |
|---|---|---|
| PLATE, chord on 700 Hz | 41 dB | back up ~24 dB over 2 min |
| PLATE, chord on 2 kHz | 46 dB | back up ~27 dB over 2 min |
| HALL, chord on 700 Hz | 25 dB | back up ~14 dB over 1 min |
| MASSIVE, chord on 700 Hz | swings ±3 dB | |
| ERA 0.8 (no Midiverb), any space | holds within 1 dB | |

Noise-like material mostly holds (its lows carry the level). Options, as musical questions: should an infinite tail at ERA's Midiverb end stay bright and hold (fade the box's low-pass out at infinite, as its rounding already replaces truncation there), or darken to a low drone and hold it there (let the floor's hold judge the low band only, and drop the makeup's lift of the residue)? Repro: infinite DECAY, ERA 1, SIZE 0.18, TONE 1, MIX 1; a chord on 700 Hz (root, major third, fifth, octave) at −12 dBFS for 5 s, then silence; watch the level for 4 minutes. The soak first met it as seed 12 at 44.1 kHz (episode 6) on the Core before these fixes.

### Observations (no action proposed)

- **An infinite PLATE settles ~2 dB under and is lifted back by the makeup over about a minute** [M] (the infinite highs fading, ADR 0008); the approved makeup's design.
- **A freeze in REVERSE reveals the forward tank** [M] (ADR 0015: REVERSE rests while frozen). At infinite DECAY the hidden tank sits at the floor, so a freeze in REVERSE gives a full pad even after a long silence. By design, possibly surprising.
- **The limiter's instant attack kinks a pad loud enough to reach it** [M] (~10× the local step size, at −10 dBFS RMS pads). The SPEC's design.
- **Some numbers are per sample, so they shift with the sample rate** [I]: the input follower (`0.002` a sample: 10 ms at 48 kHz, 5 ms at 96 kHz; ducking, ERA's hiss and the quiet detector use it), and the PLATE's wandering input allpass, capped at 8 samples of depth, so at 96 kHz it wanders half as far (0.08 ms instead of 0.18 ms). Small; worth fixing if 96 kHz ever ships.
- **Subnormals.** [M] Long silences after finite tails leave up to 10.2 million subnormal floats in the delay memory (about a third of it), and the output itself is subnormal for long stretches (3 × 10¹⁰ samples over the soak). Harmless on this Mac (Apple silicon handles them at full speed) and in the Plugin (JUCE's `ScopedNoDenormals`). The firmware doesn't set flush-to-zero (libDaisy leaves the FPU's FZ bit clear); the Cortex-M7's FPU handles subnormals in hardware [I], so the M3 profile build should confirm by timing a long silent tail.
- **SPACE flips fade linearly** [M]: on a pure tone the fade's start shows a 6–8× step, as before. A raised-cosine fade would remove it; nobody has heard it.
- **Tonal material beats** [M]: a held sine or chord in a decaying MASSIVE swings up to ±7 dB from second to second (the soak's "finite tail rose" findings, 19 in all, every one examined tonal), and a frozen MASSIVE pad of a chord ±1–3 dB with a period of ~20–30 s. Few modes, nearly equal line lengths at small SIZE, the energy moving between them; noise holds within 0.5 dB, and the SPEC's ±0.5 dB pad criterion (measured on noise) passes at ±0.1 dB.
- **Level jumps at gestures are the design** [M]: the soak's 10 ms before/after check flagged ~3 % of gestures, but every case examined was the pad plus the twin's layer over a playing input (up to ~4.5 dB, the owner's twin level), the ducking letting go in 50 ms at a freeze at infinite DECAY (v1.0.17), REVERSE resting at a freeze (above), or simply a 10 ms window too short for a tonal or sparse pad.
- **What's left of the click findings** [M]: in the confirmation runs 145 of 5,415 episodes had a flagged click. 21 are onsets from near-silence (the local level ~0, so any start of sound counts), 72 small (a step under 0.02), 52 larger; the larger ones examined were fast knob moves in chaos (a glide of a few ms counted as a glide, not a jump; the harness now treats glides under 100 ms as jumps), the REVERSE snap above (fixed), or a DECAY/TIME change replayed through BLOOM. No other systematic source turned up.
- **DC into the PLATE** [M] makes a pad whose output swings 15 dB from second to second while its energy holds. Not musical input; noted because it fooled a guard (fixed, item 9).

## What the soak didn't find

Across all 341.5 hours, at every rate and block size (including 1-sample blocks and random sizes every block), with full-scale noise, DC, clicks, sines and bursts:

- **No NaN or inf**, ever.
- **Nothing above the limiter's ceiling** (|out| ≤ √(in² + 0.891²) held on every sample).
- **No stuck tail**: of 1,771 finite tails that had time to fall 160 dB, the loudest ended at −138 dBFS; most at the floor of float precision. No buzz in silence (the Midiverb's truncation and the hiss/crackle following the input both work).
- **No finite tail that grew** other than tonal beating (above).
- **No denormal slowdown** on this Mac: 479 seconds in 341 hours ran 3× slower than their space's baseline, all of them explained by 8 parallel runs sharing 10 cores (two of them efficiency cores); the CPU table, run alone, shows a HALL or MASSIVE tail after 5 minutes of silence costing exactly what it does while playing.
- **No problem from block size**: no finding was specific to a block size once the mid-chunk tap (item 1) was fixed.

**Held tails on the final Core** (the final run, Midiverb range aside): an infinite tail stayed within ±1 dB of where it settled in 28 of 37 HALL, 20 of 32 MASSIVE and 24 of 39 PLATE holds, and a freeze in 28 of 36 HALL, 23 of 35 MASSIVE and 21 of 27 PLATE holds; the worst were +8.8 dB (an infinite PLATE at TONE fully dark: the highs fading and the makeup lifting, above) and −6.5 dB (a frozen MASSIVE chord beating). On the main run, before parts 2–3 and the pad guard, the worst were +50 dB (HALL, an input fading out) and +29 dB (PLATE). With noise, as the SPEC measures, pads hold within ±0.2 dB.

## Desktop CPU per block

Desktop time per 48-sample block (1 ms of audio at 48 kHz) on an M1 Pro, one thread, Release build, `iv_soak --cpu`: the median of three 20 s runs, and the 99th percentile block. **Relative numbers only**: the STM32H750's Cortex-M7 at 480 MHz is far slower and its memory (SDRAM for the long lines) behaves differently, so the hardware check (M3) is still needed. The ratio column says what costs what.

| Situation | µs per block | p99 µs | × HALL |
|---|---|---|---|
| PLATE, defaults | 4.3 | 7.0 | 0.68 |
| HALL, defaults (still lows on) | 6.2 | 8.9 | **1.00** |
| HALL, still lows off (one tank) | 4.9 | 7.4 | 0.78 |
| MASSIVE, defaults (still lows on) | 15.1 | 20.2 | 2.42 |
| MASSIVE, still lows off (one tank) | 13.0 | 17.4 | 2.08 |
| PLATE, frozen + twin playing | 6.1 | 8.7 | 0.97 |
| HALL, frozen + twin playing | 8.4 | 11.8 | 1.35 |
| MASSIVE, frozen + twin playing | 25.8 | 39.5 | 4.14 |
| MASSIVE, ERA full + DRIFT full Wander | 17.9 | 26.1 | 2.87 |
| MASSIVE, BLOOM, TIME full | 15.7 | 21.5 | 2.52 |
| MASSIVE, REVERSE, TIME full | 20.9 | 28.9 | 3.35 |
| **MASSIVE worst: frozen + twin, ERA full, DRIFT CW, BLOOM** | **28.2** | **45.5** | **4.52** |
| HALL, 5 min of silence after a tail | 6.2 | 9.0 | 1.00 |
| MASSIVE, 5 min of silence after a tail | 15.1 | 19.6 | 2.42 |

What it says: still lows cost +28 % in HALL and +16 % in MASSIVE (the low tank runs at a quarter rate). A frozen MASSIVE with its twin is the heaviest single feature, 1.7× MASSIVE playing (two full FDNs, the twin's swell); with ERA's Midiverb box and fast DRIFT on top it is 1.9×, the worst case measured. REVERSE adds 38 % to MASSIVE (the 32 taps per side and the short reverb). Silence costs the same as playing. For M3, the corner to profile first is the last bold row; if it fits, everything does.

## Flash

Release firmware with all of this: **101,668 B** (main at `02af9fa`: 99,180 B; budget 110 KB = 112,640 B). The fixes cost about 2.5 KB (11 KB left), most of it the allpass glide, already moved into a size-optimised control-rate function. m0test 82,356 B, profile 102,656 B, all within 128 KB.

## Files

- `tools/soak/soak.cpp` (the harness), `tools/soak/run_soak.sh`, `tools/soak/analyse.py`, a target `iv_soak` in `CMakeLists.txt`.
- `host/tests/test_soak_regressions.cpp`: one test per fixed bug, each failing on the Core before its fix (except the REVERSE snap, see 6). Part of ctest: 14 suites, all passing.
- Core changes: `core/params/GestureReader.h`, `core/dsp/Engine.{h,cpp}`, `core/dsp/Fdn.{h,cpp}`, `core/dsp/Governor.h`, `core/dsp/Plate.h`, `core/params/SpaceVoicing.h`.
