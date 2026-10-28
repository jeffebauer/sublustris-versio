# Review 2, phase 3: SHIMMER under stress (29 Oct 2026)

Branch `review2/shimmer` off `review/overnight-2` (v1.0.55). Desktop only, no module, no listening.
Tags: [M] measured here, [E] estimated, [L] literature. Probe: `tools/shimmer_probe/shimmer_probe.cpp`
(a stand-alone measuring tool, built by hand, not in CMake, not in any Host; the renderer is untouched:
`same_render` against ref-main, see the end).

## How SHIMMER is built (the facts the measurements rest on)

- `core/dsp/Engine.cpp:984-1007`: the *diffused input* (after predelay and the input diffusers) goes
  through `Shimmer::process`, and `send = shimmerMix_ * kShimmerSend (1.2) * DEPTH` of the octave is added
  back to the tank's input. **The shifter never hears the tank**: no feedback, so one octave, no cascade
  (ADR 0024). Energy runaway through the shifter is impossible by construction; what is left to test is
  whether the extra input pumps the governor or the duck, and what an infinite tail does with the octave.
- `Engine.cpp:633-634`: `shimmerTarget_ = attack == SHIMMER && !frozen`: while frozen the send fades out
  over 50 ms (it keeps recording). PAINT copies the twin into the pad (`Engine.cpp:1079-1080`), and the
  twin hears the raw input `l, r`, not the shifter. So **FREEZE and PAINT can't stack octaves either**.
- `core/dsp/Shimmer.h`: 8 heads per channel, 0.4 s sin² grains (Bhaskara), each grain at a random extra
  delay up to one grain (scatter 1.0), linear reads at 2x speed; before it an 8th-order RBJ Butterworth at
  6 kHz; after it a 9 kHz one-pole and an 80 Hz high-pass.
- DECAY >= 0.85 is infinite (`core/params/Mappings.h:31`), so 0.85, 0.90 and 1.00 rendered bit-identical
  in PLATE and HALL (MASSIVE differs slightly through its own DECAY-dependent stages).

## 1. Energy near and at endless DECAY [M]

Grid: 3 SPACEs x DECAY 0.80 / 0.85 / 0.90 / 1.00 x DEPTH 0 / 0.5 / 1 x input -30 / -18 / -6 dBFS RMS,
MIX 1 (wet only), everything else at default. Material: a sine-chord pad, notes 261-494 Hz (so every
octave, 523-988 Hz, is in its own band), two chords alternating every 4 s with 0.3 s crossfades. 90 s fed,
then 360 s of silence. Per second: power, peak, and 7 band powers. DEPTH 0 is the control (send 0).

| Question | Result |
| --- | --- |
| Fed level growth, 30-60 s vs 60-90 s | -0.7..+1.3 dB in every cell, the same as DEPTH 0 within 0.6 dB (DEPTH 1 grows *less*: PLATE 0.80 -30 dB +0.97 vs +1.28) |
| SHIMMER's added level, fed, DEPTH 1 vs 0 | finite DECAY 0.80: +0.1..+1.7 dB (MASSIVE -30 dB most); infinite: +0.2..+1.6 dB, the governor holds the floor (fed levels -17..-22 dBFS whatever the input level) |
| The octave band over the notes' band, fed | +2.5..+3.8 dB at DEPTH 1 over DEPTH 0, +0.7..+1.8 dB at DEPTH 0.5, in every SPACE and level |
| Peaks | infinite: ≤ 0.764 (PLATE -6 dB, vs 0.685 at DEPTH 0: +0.9 dB; HALL +1.1 dB); finite 0.80 at -6 dB: 0.891 = the limiter's ceiling, DEPTH 0 too |
| Infinite hold after the input stops (6 min) | slope -0.15..+0.07 dB/min, identical to DEPTH 0 to 0.01 dB/min |
| Pumping (per-second level about a 9 s running median) | hold: std 0.23-0.99 dB, max step 1.1-3.7 dB, the same as DEPTH 0 to ±0.05 dB; fed: std *lower* with SHIMMER (e.g. MASSIVE 1.00 -6 dB 0.57 vs 0.70) |
| Does the octave build up in a hold? | no: it fades |
| Does it collapse? | slowly, with the rest of the upper mids: see below |

**The octave fades out of an endless hold over 2-4 minutes** [M]. The octave band's excess over DEPTH 0
(octave band minus notes' band, DEPTH 1 minus DEPTH 0), at +10 s / +30 / +60 / +120 / +240 / +350 s after
the input stops:

| | -18 dBFS in | -6 dBFS in |
| --- | --- | --- |
| PLATE | +3.8 +3.0 +2.0 +0.7 +0.1 -0.1 | +11.3 +9.6 +6.3 +2.2 -0.3 -1.0 |
| HALL | +2.6 +1.9 +1.2 +0.4 -0.1 -0.2 | +4.9 +4.2 +2.8 +1.7 +1.5 +1.1 |
| MASSIVE (1.00) | +2.2 +1.8 +1.5 +0.6 +0.4 +0.2 | +3.3 +2.9 +2.2 +1.0 +0.3 -0.1 |

That is the infinite tail's designed darkening (ADR 0008: "the highs still fade", the loss shelf at
2.5 kHz takes some of 0.5-1 kHz too) applied to the octave: an endless SHIMMER pad ends where an endless
NATURAL pad ends. Not a bug; a musical question (below).

The bands above the octave (1-2 kHz +11..+14 dB, 2-6 kHz +11..+18 dB over DEPTH 0) sit 10-30 dB under the
octave band: the 988 Hz octave's skirt in the band filters, not new content (absolute -35..-60 dBFS).

## 2. SHIMMER with FREEZE and PAINT [M]

- **Stack** (playing throughout; 15 cycles of: freeze, 10 s, PAINT held 15 s, 5 s, unfreeze, 10 s; then
  60 s silence; DECAY 0.9 and 1.0, DEPTH 1, -18 dBFS), against NATURAL DEPTH 0:
  painted-window level cycle 1 → 15: PLATE -18.4 → -16.8 (NATURAL -18.7 → -17.1), HALL -14.2 → -15.4
  (-14.1 → -15.7), MASSIVE -14.7 → -15.1 (-14.9 → -15.3). The octave band over the notes' band, cycle 1 → 15:
  PLATE -5.4 → -6.7, HALL -3.2 → -2.5, MASSIVE -3.1 → -3.7: **no octave stacking across 10 minutes.**
- **One long paint** (freeze at 20 s, PAINT held 300 s while playing, unfreeze at 340 s): the level flat
  within ±0.5 dB from 100 to 320 s in every SPACE, within 0.6 dB of NATURAL; the octave band *falls* while
  painted (PLATE -6.3 → -9.4 dB) because the send rests while frozen.
- **Peaks**: the limiter's ceiling (0.891) is touched for 1-6 seconds in 10 minutes in HALL and MASSIVE,
  with NATURAL too (2-3 s). SHIMMER adds a few such seconds (HALL 6 vs 3), never more than the ceiling.

## 3. The shifter alone: aliasing, grain artefacts, the window [M][L]

`dsp::Shimmer` alone (shift on), a 0.25-amplitude sine, 10 s analysed after 2 s, Hann FFT at 0.1 Hz.

| Input | octave re input | folded (48k - 2f) re input | worst other spur re octave |
| --- | --- | --- | --- |
| 440 Hz - 3 kHz | -11.7..-14.6 dB (in ±1 Hz; ~3 dB more spread ±2.5 Hz) | - | ≤ -86 dB |
| 5 kHz, 6 kHz | -17.7, -16.2 | - | -79, -82 |
| 7, 9, 10.5 kHz | -28.9, -52.4, -68.8 (the 6 kHz feed filter) | - | -78, -75, -75 |
| 12.5 kHz | none (25 kHz) | **-87.6 dB**, at 23 kHz (inaudible) | |
| 14 kHz | | **-105.8 dB**, at 20 kHz | |
| 17 kHz | | **-140 dB**, at 14 kHz | |

A 40 s log sweep 20 Hz-20 kHz (excluding the octaves of the last 0.9 s of input, the grains' lag): every
other component ≤ -79 dB re the octave from 1 to 12 kHz; above 12 kHz in the worst fold -82 dB re input at
22.4 kHz, and ≤ -118 dB re input landing below 20 kHz. **The fold-back is solved**: the 8th-order filter's
-48 dB/octave plus its eight zeros at 24 kHz. litnl.md:94 said the same from the design.

**The grain artefact that remains is random-phase beating, by design.** The octave of a held sine spreads
over ±2.5 Hz (87-91 % of its power; centroid within 0.3 cents: in tune), and its 10 ms level wanders
p5/p95 -9..-15 / +5..+6 dB about its median: the statistics of a sum of random phasors (Rayleigh: -11 /
+5 dB). That is the random grain onsets (`kShimmerScatter` 1.0) doing their job (no fixed comb on the
octave). **In the wet it's masked**: on a held 440 Hz sine through the whole Engine (renderer, MIX 1), the
880 Hz octave's 10 ms level wanders p5/p95 -10.5..-12 / +5.4..+8 dB, and the 440 Hz fundamental itself
-7.4..-12.7 / +4.1..+8.8 dB, the same with NATURAL: the tank's own modulation treats a pure tone the same.

**The window** [M]: Bhaskara's sin² is within 2.1e-3 of sin²; eight evenly spaced windows sum to a
constant within **0.0005 dB** peak to peak (amplitude) and 0.0001 dB (power); the mean gain is 0.99876
(-0.011 dB). The polynomial on review2/cpu (`IV_SHIMMER_POLY`, w = q²(9.91796 + q(18.84379 + 21.92742 q))):
within 4.0e-5, ripple 0.0001 dB, gain 1.00001. Both are far below anything audible: the poly is a CPU
choice only, its sound change is the +0.011 dB level (as cpu.md measured).

**Established designs** [L]:
- Valhalla Shimmer (Costello's notes, https://www.valhalladsp.com/shimmer/ValhallaShimmerNotes.pdf,
  section "ValhallaShimmer: a bit of history"): its shifter is in the *feedback* loop, and was built to make the artefacts of a
  "deglitched" shifter "without performing the costly autocorrelation analysis"; its randomisation "added
  noise and texture", "like a huge orchestra warming up". Ours is the same family (random grains), fed
  forward.
- DAFx26 "Shimmer Reverberation with Nonlinear Feedback Delay Networks" (Dal Santo, Pi, Prawda, Schlecht,
  Välimäki; https://www.dafx.de/paper-archive/2026/papers/DAFx26_paper_05.pdf): §3.4-3.5 a two-pointer ring
  buffer (and a granular variant with random read points), sin/cos *power*-complementary crossfades, cubic
  interpolation; §3.4 notes the crossfade "cancellation artifacts" and that a periodicity detector would fix
  them; §5.1: "does not include a synchronization mechanism, and produces sidebands" around the octaves, and
  no significant aliasing. **The energy compensation** (§4.1, eq. 7-8): each shifter in the loop is followed
  by a dc blocker (R = 0.995, 76 Hz) whose gain g_env tracks the input/output energy ratio x²/y² through a
  τ = 50 ms follower, so each pass through the shifter keeps its energy; §4.2: the time-compression shifters'
  energy ratio has the widest spread of all their nonlinearities, and "using lower amplitudes helps ensure
  the stability". The shifters sit on 4 of 8 lines (the longest). This is the reference for the TASKS item
  "Shimmer inside the reverb".
- Eventide / Strymon [L, memory, unverified]: Eventide's H-series "deglitched" shifters choose splice points
  by autocorrelation; the BigSky / Strymon shimmer is a shifter in the reverb's feedback. Neither publishes
  its algorithm.

## 4. Clicks and >3 kHz energy while knobs move and on gestures [M]

A held chord (-18 dBFS), DECAY 0.9 and 1.0, DEPTH 0.6 unless moved, MIX 1; per event, the worst 1 ms frame
above 3 kHz over the median of the 2 s before, the mean above 3 kHz in the second after over the 2 s before,
and a click ratio (max |second difference| over the local 10 ms RMS; baseline 0.007-0.05). SHIMMER against
NATURAL (where DEPTH is the predelay).

- **DEPTH** jumps (0.6 → 1, 1 → 0), ramps (0 → 1 over 1 s, 1 → 0.6 over 0.2 s) in SHIMMER: worst 1 ms
  +8..+16 dB (no-event spread of this metric: +5..+12 dB), 1 s mean -2..+4 dB, click ratio ≈ baseline
  (0.011-0.035 vs 0.012-0.030). Nothing.
- **DECAY** jumps and ramps: the same as NATURAL.
- **FREEZE tap, PAINT hold, unfreeze**: SHIMMER's click ratios 0.016-0.053 against NATURAL's 0.007-0.061:
  the same or lower. The largest, MASSIVE's unfreeze (+22..+27 dB worst ms, 0.058-0.061), is NATURAL's too:
  not SHIMMER's.
- **PLATE SIZE** (jump 0.5 → 0.8, ramp 0.8 → 0.2 over 2 s, jump 0.2 → 0.5): +26..+28 dB above 3 kHz for
  the second after and click ratios 0.18-0.27 (25-35x baseline) **in NATURAL**, +16..+18 dB and 0.20-0.27 in
  SHIMMER. A pure-tone chord shows it louder than knobs.md's +14.8 dB (its finding 3, "PLATE's SIZE steps");
  the same issue, not SHIMMER's.

## Options

| # | What | Sound | CPU (of 10,000 cycles a sample) | Flash | Risk | Needs |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Nothing: SHIMMER is bounded, unstackable, alias-free and click-free as built | none | 0 | 0 | none | - |
| 2 | Ask the owner: "an endless SHIMMER pad lets its octave mellow away over 2-4 minutes, like the rest of the tail's brightness. Keep it, or should the sparkle hold?" Keeping the octave would mean a gentler infinite loss in 0.5-1 kHz for every ONSET, or a slowly re-fed octave: a real design change | audible (if changed) | ~0 / +shifter | tens of B to 200+ B | medium | ears, a page (DEPTH 1, held 5 min, before/after) |
| 3 | `IV_SHIMMER_POLY` (review2/cpu): sound-neutral here (ripple 0.0001 dB, -0.011 dB gain step) | none (+0.011 dB) | -1.9..-2.7 % while SHIMMER plays [E, cpu.md] | +16 B | low | module lap |
| 4 | Feed filter 6th order instead of 8th: audible-range folds ≤ -81 dB re input, 16-22 kHz folds ≤ -61 dB [E] | tiny | -0.1..-0.2 % [E: one biquad x 2 channels] | ~0 | low-medium (the owner once heard fold-back as "bit crushing") | not recommended: too little for the risk |
| 5 | A steadier octave (autocorrelation splice, Eventide-style) | tiny-audible | +several % [E] | hundreds of B | medium | not recommended: masked by the tank (§3) |
| 6 | Shimmer in the loop (TASKS "Shimmer inside the reverb"): DAFx26 §4.1's per-pass energy compensation. At our infinite DECAY each pass would climb an octave until the 6 kHz filter, and the governor caps only the level, not the climb | audible, a new effect | +10-19 % per extra shifter [E, review-2026-10-26 M] | far over the 76 B spare | high | after v1.0 |

## Not checked

The module (CPU, its float maths without fused multiply-adds), listening, SHIMMER with WEAR or MOTION above
0.5 (all at default), input above -6 dBFS RMS or clipped input, the ONSET switch into and out of SHIMMER
(the onset-dip test covers it), SPACE flips in SHIMMER (covered by `onsetAndSpaceChangesDontBurst`), the
gate as against the button (they share one reader, ADR 0011), `IV_SHIMMER_POLY` through the whole Engine
(only the window's maths here).

Reproduce: build the probe (its header), then `shimmer_probe energy <space> <decay> <time> <dB> 90 360`,
`gesture <space> <decay> <time> -18 <0|1> <attack>`, `clicks <space> <attack> <decay>`,
`shifter <hz|-1> <seconds> <out.f32>`.

Renderer unchanged: `python3 tools/same_render.py <ref-main iv_render> build/iv_render` → **SAME 60/60**.
