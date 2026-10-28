# Overnight review 2: knob ranges, tapers, CV and the knob path (28 Oct 2026)

Branch `review2/knobs` (worktree `.claude/worktrees/r2-knobs`, off `review/overnight-2` at eac761a, v1.0.55).
Tags: **[M]** measured here (desktop renderer), **[C]** read from code (ours or libDaisy), **[S]** a source says it,
**[E]** estimated, **[U]** unverified. Nothing here changes the shipped sound: the one code change is a renderer
hook, `IV_KNOBPATH`, off by default; with it unset `tools/same_render.py` against the pinned ref-main renderer is
**SAME 60/60** (run twice, after the last hook edit too), and ctest is **100 % tests passed out of 21**. No Core or
firmware file changed, so the firmware sizes are main's (not rebuilt).

## What was done

1. **Part A.** Re-ran `tools/sweetspot.py` (21 points, `sweetspot-21.json`, and the usual 11, which regenerated
   `docs/sweetspot.md`; the 11-point run equals every other point of the 21). The old table was from 24 Oct (v1.0.21,
   149 Core commits ago). `tools/review2/sweetspot_compare.py` traces each big step to the measure behind it.
2. **Part B.** Ported the release firmware's knob path into the renderer behind `IV_KNOBPATH=1`
   (`host/render/main.cpp`, `struct KnobPath`), then drove SIZE, TIME (DEPTH), DECAY, TONE, WEAR and MOTION with CV
   shapes (`tools/review2/cv_tests.py`), and the gate and button with fast and short gestures
   (`tools/review2/gate_tests.py`). SIZE crossfades were counted with a second renderer built with
   `-DIV_PROFILE_HOOKS` (`build-prof/`, `IV_BLOCK_PROFILE`).

### The port: what is the firmware and what is modelled

Ported from `firmware/main.cpp` (IV_MODE_RELEASE `AudioCallback`, ~2676-2717):
- each of the seven knobs read once per 48-sample block (the module's 1 ms), at offset 0;
- the 10 ms one-pole `kKnobSmooth = 1 / (1 + 10 * 48 / block)`, seeded with the first reading;
- `ivpot::endStops` (the firmware's own `PotEndStops.h`, included as is);
- the deadband from the last value sent, 0.001, or 0.003 for SIZE and TIME, with the end-stop exception;
- `setParam` only when it passes; SPACE and ATTACK sent every block;
- the gate polled once a block, every change sent at offset 0 (libDaisy `GateIn`: no debounce, no latch);
- the button through libDaisy `Switch::Debounce`'s 8-bit shift register: `Pressed()` = eight 1 ms highs in a row,
  one low releases.

Modelled (not in the firmware's source):
- A knob's value (automation track, or its `--set` value) is the parameter the panel shows, turned back into the raw
  pot + CV reading the end stops expect: raw = 0.025 + 0.95 v, clamped to the ADC's 0..1 (CV past an end clips).
- `IV_KNOBPATH_NOISE=<lsb>`: Gaussian noise (std, 16-bit LSBs) on every knob's reading, then quantised to libDaisy's
  16 bits (value = 1 − code / 65536, the knobs are flipped).
- The ADC's 32× oversampling window (tens of µs [E]) is treated as a point sample at the block's start.
- libDaisy's own 2 ms slew is left out because it does nothing at this rate (finding 7).
- `IV_KNOBPATH_PLATE_SIZE_BAND`: an option tried here (SIZE's deadband in PLATE), not the firmware.
- Not modelled: audio coupling into the readings (the rack's real noise source, per the firmware comment), any
  analog filtering on the CV inputs, callback jitter.

### The CV tests

Stimuli: `tones` (10 s of `08_held_tones` then 4 s of skank; the most sensitive to zipper) and `pink` (10 s of pink
noise, then its tail; the fair one for level). 18 s renders; the CV moves until 14 s, then holds its centre.
Base: MIX 1, DECAY 0.6, TONE 0.5, WEAR 0, SIZE 0.5, MOTION 0.5, DEPTH 0.2, NATURAL. CV centres: SIZE 0.5, TIME 0.3,
DECAY 0.55 (and 0.8, across the infinite crossover), TONE 0.5, WEAR 0.5, MOTION 0.5 (crossing noon).
Shapes:
- slow LFO 0.1 Hz ±0.25;
- 5 and 20 Hz ±0.1;
- 100, 440 and 1000 Hz ±0.1;
- a random step every 125 ms over ±0.25;
- triangles at 2-16 s per sweep;
- a still knob with 4-132 LSB of noise.

Every measure is against the statics across the CV's range, each also through the knob path, taking their
per-window maximum (the ceiling), so visiting a louder setting doesn't count:
- clicks: 1 ms frames of the second difference, a frame ≥ 20 dB over its ±250 ms median and ≥ 10 dB over the
  ceiling's frame;
- HF: energy above 3 kHz, whole run and the worst 250 ms window;
- level: the worst 0.5 s window over the ceiling, and the last 2 s.

## Findings, ranked

### 1. SIZE (HALL, MASSIVE) and DEPTH ignore any CV faster than ~6 % of the travel per second [M][C]

The knob path sends SIZE/TIME only in 0.003 steps, and a SIZE or TIME move starts its crossfade only after 50 ms
with no new step (`kSizeRestSeconds`, ADR 0025). So the tank follows CV only while the CV moves less than 0.003 per
50 ms, **0.06 of the travel a second** (0.3 V/s if 0-5 V spans the knob, SPEC §3; [U] scaling). Measured (SIZE
crossfade starts while the CV moved, 14 s; HALL / MASSIVE):

| CV | slope | HALL | MASSIVE |
|---|---|---|---|
| 5 Hz, 20 Hz, 100 Hz, 440 Hz LFO | fast | 0 | 0 |
| 0.1 Hz ±0.25 | up to 0.157/s | 24 | 24 |
| triangle, 2 s a sweep | 0.25/s | 0 | 0 |
| triangle, 4 s | 0.125/s | 4 | 6 |
| triangle, 7 s | 0.071/s | 2 | 4 |
| triangle, 10 s | 0.05/s | 92 | 92 |
| triangle, 16 s | 0.031/s | 90 | 92 |
| random step every 125 ms | steps | 54 | 76 |

- While a fast CV moves, the tank stays at the SIZE it had before the modulation started, then jumps when the CV
  stops.
- A 0.1 Hz LFO is sampled near its turning points.
- A sequencer works, since each step rests longer than 50 ms.
- TIME behaves the same: fast CV in NATURAL and BLOOM gives output identical to the still knob (HF 0.00 dB, level
  0.00), and a 125 ms step sequence moves it.
- The renderer's own automation hides none of this (it follows the same rest rule) but reads every 16 samples
  without the deadband.

ADR 0025's amendment calls the slow-LFO hold "by design; the owner may revisit". It isn't written anywhere a
CV user would see it.

**Audio-rate CV** is read once per 1 ms [C]: anything above 500 Hz aliases (a 1 kHz CV reads as a constant offset;
SIZE moved once, 8 starts, with the offset). The module can't do audio-rate modulation of any knob, by
construction.

### 2. No clicks, no broadband bursts, no runaway anywhere [M]

Across 6 knobs × 3 SPACEs × 7 shapes + noise, TIME in BLOOM and SHIMMER, and DECAY swept across the infinite
crossover (0.8 ± 0.25 / 0.1), no case has a single click frame, through the knob path or the plain renderer.

- **Level (pink):** worst 0.5 s window over the ceiling +3.0 dB (MASSIVE SIZE, slow LFO); otherwise ≤ +2.2 dB.
- **HF (pink):** whole-run above 3 kHz ≤ +1.8 dB (MASSIVE TIME at 440 Hz, a constant alias offset). The worst
  250 ms window is ≤ +9.8 dB, and the one traced (HALL SIZE steps) sits in the decaying tail at −74 dBFS: a
  different tail, not a burst.
- **Held tones read up to +16 dB "over":** that is modal statistics, not runaway. A held tone into a 20 s MASSIVE
  sits wherever its frequency falls between the modes, and statics a quarter-turn of SIZE apart differ 6-10 dB on
  this material. Any movement (SIZE or even pre-delay, which can't change a steady level) averages that out. Pink
  noise, through the same moves, stays within ~1-3 dB.

### 3. PLATE SIZE under CV or a slow turn: stepped glides on held tones [M][C], ears needed

On held tones, energy above 3 kHz is +14.8 dB over the still knob through the knob path, against +7.8 dB for the
renderer's smooth automation: −35.6 dB re the total, against −42.6 (renderer) and −49.9 (static). Pink: 0 dB.

Mechanism:
- The PLATE glides its scale at a fixed slew (0.4 scale units/s, `Plate::control`).
- Its line lengths and output-tap delays move once per control block (`tapDelays`, held across the block). At
  48-sample blocks that's ~3-sample steps of the taps a millisecond at full slew.
- The knob path turns a slow move into 0.003 steps every ~20 ms, each run at full slew.

Split (`tools/review2/block_probe.py`, slow LFO):
- the 48-sample block against 16: +4.5 dB (deadband ~0: +12.3 at 48 against +7.8 at 16);
- the 0.003 deadband: +2.5 dB;
- a 0.001 PLATE deadband buys only 0.4 dB, so it is not worth the flash.

**The renderer's automation under-reports this:** it runs 16-sample micro-blocks.

### 4. The gate misses triggers shorter than a block [M][C]

`hw.Gate()` is read once per callback with no latch. A trigger shorter than 1 ms is seen only if a poll falls
inside it (chance ≈ width / 1 ms).
- **Measured:** a 0.5 ms trigger starting 10 samples into a block gives output bit-identical to no gate at all
  (PLATE, HALL, MASSIVE). The renderer's sample-exact gate froze on it.
- **1 ms triggers** are always caught in the model; callback jitter on the module could drop some [E].
- **The button** needs 8 ms held to register (libDaisy `Switch::Pressed()` = `state_ == 0xff`), and a release
  registers at once.
- Some modules send 1 ms or shorter triggers [U].

### 5. Fast gates and taps behave as designed [M]

Gates at 2/10/20/50 Hz and button taps at 10 Hz toggle the freeze at half the rate (a press freezes; the next press
released within 0.3 s unfreezes, ADR 0009/0011).
- No click frames, no runaway (pink: worst +3.7 dB over the unfrozen control's maximum, a frozen pad).
- The final state depends on the edge count's parity (`frozen=1` after 100 pulses at 10 Hz).
- **A gate held through SPACE changes:** the flips wait for the unfreeze (by design, SPEC v1.0.2) and land at the
  tap, clean.
- **Painting while SIZE moves:** no level difference from painting with SIZE still (pink).
- **The polled and debounced reading** changes nothing audible against sample-exact edges, for gestures ≥ 1 ms.
- **CPU of 5-25 Hz freeze toggling** on the module (the small room starting at each unfreeze): not measurable here.

### 6. A noisy pot is no problem for the deadband [M]

Gaussian reading noise of 4, 16, 33 and 66 LSB (std; 66 LSB = 0.001, twice the rack's ±0.0005):
- **SIZE and TIME:** never sent after the first reading, no crossfades. The 10 ms one-pole cuts the noise about
  4.6× ([E] √(2·11−1)) before the 0.003 = 197 LSB deadband.
- **Other knobs at 66 LSB:** 32-54 sends in 18 s, with no measurable effect (level ≤ 0.2 dB; HF window ≤ +5.8 dB
  in PLATE's quiet tail).
- **Not modelled:** audio coupling, which moved SIZE's reading on the rack (the reason for the 0.003 band).

### 7. libDaisy's 2 ms knob slew is a pass-through [C]

`AnalogControl::Init`/`SetSampleRate` set the coefficient to 1 / (0.002 · 1000 · 0.5) = **1.0** at the 1 kHz
callback rate (`libs/libDaisy/src/hid/ctrl.cpp`; `DaisyVersio::SetAudioBlockSize` → `SetHidUpdateRates`). The
firmware's comment "on top of libDaisy's 2 ms" (firmware/main.cpp ~2697) is therefore wrong: the 10 ms one-pole is
the only smoothing. Comment-only fix, 0 B. Also, the task brief's "0.003 deadband" is SIZE and TIME only; the
other five knobs use 0.001.

## Part A: the sweet-spot table now (docs/sweetspot.md, regenerated)

**Changed since 24 Oct (v1.0.21 → v1.0.55):**
- **Gone:** WEAR's 80 → 90 % cliff (5.2 / 16.6 / 15.7). It's now ≤ 3.3 a notch, with no cliff (v1.0.42's single
  wear amount).
- **Gone:** MASSIVE DECAY's first-notch cliff (13.6 → 3.3; T60 5.4 s at DECAY 0, ADR 0034 / knob-feel B).
- **Gone:** MASSIVE TIME (NORMAL)'s dead 0-20 % (now an even 0.4-0.8 a notch).
- **Gone:** MASSIVE WEAR's dead 0-50 % (0.4 a notch over 0-30 %, small but not dead).
- REVERSE has become SHIMMER.

**Still there:**
- **DECAY 85-100 % is one plateau (infinite)** in every SPACE, 15 % of the travel, about 3:20 to 5 o'clock on
  the module.
  - [C] `kDecayInfiniteAt` 0.85, the owner's "held tails at 3 o'clock" (decay-range.md).
  - Supermassive puts its endless at the very end of travel [S, via decay-range.md]. Ours is a wide, easy-to-hit
    target, and CV that reaches it holds.
  - By design; listed so it isn't mistaken for a dead zone.
- **MOTION across noon:** HALL 4.3×, PLATE 3.8× at 45 → 50 %. It's the wet's arrival, 43 → 24 ms: the tape
  transport's 20.8 ms centre leaves on the right side (SPEC P6). By design; the measure counts arrival time.
- **MASSIVE BLOOM 90 → 100 %:** 6.5×. A ~5 s swell (4 s × mass 1.25): the click's T60-from-peak falls 7.8 → 4.2 s
  and the skank's 6 s window loses 3 dB. Mostly the measure; also true that a short phrase barely reaches level
  at full BLOOM.
- **TONE's CCW half:** the tail's centroid hardly moves over 0-30 % (PLATE 100.2 → 100.3 st, HALL 100.5 → 100.8),
  while the CW half moves 1.4-2.5 st a notch.
  - The centroid bottoms out near 320 Hz, the lows' own, so the measure can't see further darkening. T60 and
    loudness do move (steps 0.5-1.3).
  - **For the owner's ears:** does TONE darken evenly from 9 o'clock down?
- **SHIMMER DEPTH** reads 0.1-0.7 a notch, "dead" 0-20/30 %. These measures can't see an added octave. A proper
  measure would be the energy an octave above the input notes.
- **PLATE TIME (NORMAL)** 0.1 over the first 10 % (x²: 0-2.5 ms, fused with the attack; by design, knob-feel audit).

**Measure limits:**
- **T60 is read within an 11 s window:** it saturates near 25 s. DECAY above ~70 % (PLATE/HALL) and MASSIVE's
  upper half are under-measured; their mapping is exactly exponential by construction (`map::decayRate`).
- **The arrival-peak time is bimodal** (0.136 ↔ 0.207 s), which makes isolated 3.0 steps that aren't real: PLATE
  TONE 70 → 75, WEAR 15 → 20, SIZE 85 → 90; HALL TONE 15 → 20. They are the "cliffs" in the 21-point file.

**Ranges against other units** (DECAY's are in decay-range.md and not redone):
- DECAY: PLATE 0.4-30 s, HALL 0.8-60 s, MASSIVE 2-120 s, then infinite. Valhalla Future/VintageVerb 0.2-70 s;
  BigSky Hall 0.5-20 s, Cloud 1-50 s [S, as cited in decay-range.md and knob-feel-audit.md].
- DEPTH's pre-delay: 0-250 ms (ADR 0015). Plugin and pedal pre-delays commonly reach 500 ms or more [U: no
  manual could be fetched tonight; BigSky's PDF link in knob-feel-audit.md is now 404, Valhalla's pages 403].
  250 ms is an eighth note at 120 BPM. Enough for separation, short of rhythmic pre-delay at slow tempos.
  Musical question for the owner if it ever comes up.
- BLOOM 0.12-4 s, exponential; SIZE and MOTION have no common unit to compare.

## Options

| # | Option | Sound | CPU (of 10,000 cycles/sample) | Flash | Risk | Needs |
|---|---|---|---|---|---|---|
| 1a | Say in the manual / SPEC that SIZE and DEPTH follow only slow CV (< 6 %/s), and fast CV moves them once it rests | none | 0 | 0 | none | owner's wording |
| 1b | A max wait: while SIZE/TIME keeps moving, crossfade to the current value every N ms anyway (e.g. 300-600), so an LFO steps the room | audible (only under CV / fast turns) | sustained at the SIZE-move peak (≤ 89.6 % on the module, ADR 0025 v1.0.49) instead of brief [E] | ~40-80 B [E], with 76 B headroom | CPU on the module; how it sounds | ears + module |
| 3 | PLATE: ramp tap delays and line lengths per sample across the block | tiny (only while PLATE SIZE moves; held settings unchanged) | ~0.3-0.6 % [E] (22 increments a sample) | ~100-200 B [E]: over budget unless found elsewhere | low | ears (held tones), module |
| 4 | Latch the gate's rising edges between blocks (poll in the main loop, or EXTI), so a < 1 ms trigger still taps | none for ≥ 1 ms gates | ~0 | ~30-60 B [E] (EXTI more) | low | module (trigger sources) |
| 7 | Fix the comment about libDaisy's 2 ms | none | 0 | 0 | none | – |

## What only the module can answer

- **The CV range in volts and how CV and pot sum.** SPEC says 0-5 V plus the pot (and M0 checked CV at 0 and 5 V);
  whether negative CV subtracts, and where the sum clips, are hardware facts. libDaisy's Versio file only gives
  `AdcChannelConfig::InitSingle` per pot, 16-bit, `OVS_32`, value = 1 − raw / 65536. No Noise Engineering Versio
  spec turned up in a web search tonight.
- Any RC filtering on the CV inputs (it would soften the audio-rate aliasing).
- Callback jitter against 1 ms triggers.
- The CPU of continuous SIZE crossfades (option 1b) and of 5-25 Hz freeze toggling.
- Audio coupling into the knob readings under CV.

## Files

- `host/render/main.cpp`: `IV_KNOBPATH`, `IV_KNOBPATH_NOISE`, `IV_KNOBPATH_PLATE_SIZE_BAND`. Prints sends and the
  end state (frozen / painting) to stderr.
- `tools/review2/cv_tests.py`, `cv_table.py`, `gate_tests.py`, `envelope_probe.py`, `hf_probe.py`,
  `block_probe.py`, `sweetspot_compare.py`.
- `docs/sweetspot.md` (regenerated), `docs/research/review2-notes/sweetspot-21.{md,json}`, `sweetspot-10-from-21.md`.
