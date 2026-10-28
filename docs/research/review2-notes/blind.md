# Review 2, agent "blind": blind spots and code health (28-29 Oct 2026)

Read-only on code. Code at `review/overnight-2` (`d1db5e7`, v1.0.55). Measurements with the pinned reference renderer
(`.claude/worktrees/ref-main/build/iv_render`, main at `eac761a`); scripts in `tools/review2/blind_*.py`; every render
deleted once measured. Tags: **measured** (run tonight, desktop), **code** (read in the code, file:line), **estimated**,
**literature**. Desktop results are evidence about the desktop (FMA, Apple libm, FZ off in the renderer).

## A. Blind spots, ranked by risk to the sound or the module

### A1. DC and sub-audio at the input are integrated by an endless tail (measured; desktop; plugin first)
There is no input high-pass anywhere on the wet path (`Engine::process`, `core/dsp/Engine.cpp:961-1056`: predelay →
diffusers → swell → hiss → tank; only the shimmer has an 80 Hz cut and the small room a 150 Hz one). At endless DECAY the
loop gain at DC is exactly 1, so DC (and near-DC) piles up pass after pass until the floor governor caps the *total*
level, and then the DC takes the floor's level budget from the music.
- 2 s of DC at 0.1 (−20 dBFS), then silence, MIX 1 (`blind_measure.py` §3): wet 4-20 s at DECAY 1 is **a held DC
  offset at the floor**: PLATE mean +0.090 with RMS −20.9 dBFS (i.e. almost all of the tail is DC); HALL L +0.108 /
  R +0.045; MASSIVE −0.022 / −0.048. At DECAY 0.5 the DC still sits at +0.003 4-20 s later.
- A constant +0.02 offset (−34 dBFS) under the skank at DECAY 1 (§4): the PLATE wet's DC at 30-40 s is **+0.106**, above
  the floor's whole RMS (0.089): the governor holds the level and the DC owns it.
- The skank file itself has a mean of −2.25e-4 (−73 dBFS of DC). At ×0.5 into an endless PLATE it became **−0.019 of
  DC (−34 dBFS) after 30 s**; the same music high-passed at 20 Hz first (`blind_dc.py`): DC −76 to −114 dB re the tail,
  nothing builds in 40 s in any SPACE. So the loop invents no DC; it integrates whatever comes in.
- On the module the codec probably removes DC before the Engine (libDaisy's WM8731 set-up leaves the ADC high-pass on,
  `libs/libDaisy/src/dev/codec_wm8731.cpp:55`; the AK4556 and PCM3060 have ADC DC-removal filters, **literature,
  unverified tonight**), but infrasonic content above the codec's ~1-4 Hz corner (an LFO bleeding into the audio, a
  slow envelope, a DC-coupled source's drift) is not removed. In the plugin, any DAW signal with DC goes straight in.
- Relevant to the open item "rumble at endless DECAY with TONE bright" (another agent's): the mechanism above is one
  candidate; with DC-free input I saw no sub-20 Hz growth in 40 s (shares of 0.1-20 Hz −33 to −50 dB), so a 10-minute
  rumble needs something feeding it.
- **Option:** a DC blocker (first-order high-pass, 3-5 Hz) on the tank's input, after the predelay. `core/dsp/Filters.h:39`
  already has Resilio's `DcBlocker` (dead code today). CPU ~10-15 cycles a stereo sample, 0.1-0.15 % (estimated: 3 FP
  ops a channel plus loads, DTCM state). Flash ~40-80 B (estimated; 76 B left). Sound: −0.1 to −0.3 dB at 20 Hz
  (computed for a first-order corner at 3-5 Hz), nothing audible in music; goldens move (the stimuli carry DC). Behind
  a hook first (`IV_INPUT_HP`); a plugin-only version costs the firmware nothing. Needs the owner only as a changelog
  OK, not ears.

### A2. MASSIVE at SIZE noon (the default) has a 250 Hz hole in its tail, from the still-lows split (measured; desktop)
Pink noise 6 s, DECAY 0.6, MIX 1, tail 6.5-9 s, third-octave at 250 Hz against the mean of 160 and 400 Hz
(`blind_250.py`): MASSIVE SIZE 0.00-1.00 in 0.05 steps reads within −1.4..+2.2 dB
everywhere **except SIZE 0.50: 5.6 dB down** (and 4.5-7.4 dB under its neighbours at 125-500 Hz). With
`IV_STILL_LOWS=0` (one tank) the same setting reads 0.7-1.7 dB: most of the hole is the split. HALL shows smaller
outliers at SIZE 0.10 (3.9) and 0.15 (4.7).
- Mechanism (hypothesis, not proven): the low tank (12 kHz) and the main tank (24 kHz) use the same ratios and means,
  so their lines are equal in seconds to within the prime rounding. At some SIZEs the rounding lines up and the two
  tanks' outputs near 300 Hz stop being uncorrelated. `Engine::prepare`'s split is a Butterworth pair
  (`Engine.cpp:186-198`), power complementary for uncorrelated outputs (`docs/research/still-lows.md:56`), and a
  2nd-order Butterworth LP + HP of the *same* signal sums to (1 + s²)/(s² + √2 s + 1): a notch at the crossover.
- Why it matters: knob noon is where a new user starts and where the owner's pages often sit; the hysteresis keeps
  the module within ±0.003 of wherever the knob rests.
- **To check:** a finer SIZE sweep around 0.5 at several DECAYs and inputs; the correlation of the two tanks' outputs
  in the 200-400 Hz band against SIZE (desktop, an hour). **Options** (sound changes, need the owner's ears): give the
  low tank its own length set (another ratio order or a fixed prime offset; 0 CPU, ~0-16 B), or a decorrelating
  allpass on the low tank's output (~1 % CPU estimated, ~100 B).

### A3. SIZE and DEPTH under continuous CV (an LFO) in HALL and MASSIVE (code; not studied)
ADR 0025 and the M3 laps cover hand moves ("a hand move never overlaps or restarts a crossfade", ADR 0025 amendment
v1.0.45). A modular user will patch an LFO into SIZE. Through the release knob path (10 ms smoothing, 0.003 deadband,
`firmware/main.cpp:2700-2716`) a slow LFO arrives as steps; the crossfade starts only after 50 ms of rest
(`kSizeRestSeconds`) and runs four groups × 0.3 s. So: a fast-ish LFO (steps under 50 ms apart) holds SIZE still until
it turns round; a slow one starts a crossfade at every step, back to back for as long as it runs. ADR 0025 itself
records back-to-back crossfades draining an infinite MASSIVE 45 dB in a minute, and the crossfade is MASSIVE's CPU peak
(93-95 % with SHIMMER + WEAR + MOTION's tape side, HANDOFF). Nobody has rendered, timed or listened to it; the last
review flagged it as "a behaviour to decide on" (section 7) and no ADR decides it.
- **Check:** desktop, a renderer `--auto` LFO on SIZE quantised as the firmware does (0.003 steps), at 0.02-0.5 Hz,
  MASSIVE DECAY 1 and 0.7: level drift, crossfade count, clicks; then a profile lap with an LFO-shaped SIZEHAND
  (module). The owner decides what CV into SIZE should do (a musical question: "should an LFO on SIZE sweep the room,
  or step it?").

### A4. Absolute levels: where the module's volts sit against the Engine's dBFS thresholds (not measured)
The Engine's behaviour hangs on absolute digital levels tuned on desktop stimuli peaking near −6 dBFS: the floor
(−21 / HALL −18 dBFS), the duck threshold (−24 dBFS, `SpaceVoicing.h`), the quiet detector that lets an endless tail
hold where it settled (−70 dBFS for 2 s, `kQuietDbfs`), the hiss follower. The M0 check measured gain relative to a
cable (Resilio's, carried over), not volts per full scale, and never the input noise floor (HANDOFF "Will bite": "the
module's jack calibration is unknown"). If a patched source idles above −70 dBFS (VCA bleed, noisy oscillator), the
quiet detector never fires and the endless tail drifts up (+4 dB in 10 min per `SpaceVoicing.h:258-261`); if
modular levels run hot, the 2:1 duck works much harder than on the pages.
- **Check (module):** the input RMS with nothing patched and with the owner's usual sources idling, and the dBFS of a
  known voltage (m0test already prints levels; a few lines of serial). Desktop: the same renders at −30 / −18 / −6 dBFS
  to see which thresholds a level change crosses.

### A5. The gate: sampled once per 1 ms block, no debounce or minimum width (code; module)
`firmware/main.cpp:2682-2685` reads `hw.Gate()` (a raw GPIO read, `libDaisy/src/hid/gatein.h:42`) once per callback and
sends edges at offset 0. A trigger shorter than ~1 ms can fall between reads and be missed; a slow or noisy edge
through the input comparator can chatter, and the gesture reader turns rise-fall-rise-fall inside 0.3 s into freeze
then unfreeze (`core/params/GestureReader.h:33-60`). SPEC §7 M5 promises tests for "pulses from a sequencer, bounces";
`test_plate_freeze` has none (grep: no bounce/chatter case). The button is fine (libDaisy's 8-read shift register).
- **Check:** desktop, a chattering-edge test through `Engine::gate` (cheap); module, a 0.5 ms trigger and a slow
  LFO into the gate, watching the freeze tint. **Option:** a 3-5 ms minimum between gate edges in the firmware
  (~24-40 B estimated, 0 CPU).

### A6. A SPACE switch nudge that returns still restarts the tank (code; not tested)
`Engine::controlTick` (`Engine.cpp:856-899`): once a flip starts fading out, it always resets the tanks at the bottom
of the fade, with `activeSpace_ = space` even when `space` is back to the old position. A brushed or bouncing SPACE
switch (libDaisy's `Switch3` is a raw read each block) loses the whole tail, endless pads included (not while frozen:
flips wait). No test covers a flip that returns inside 50 ms. **Option:** if the switch is back on `activeSpace_` when
the fade-out bottoms, fade back in without the reset (~20-40 B, 0 CPU; a behaviour change in a corner only).

### A7. The plugin at 44.1 / 88.2 / 96 kHz is not the module (measured; plugin)
`blind_rates.py`, pink noise, MIX 1, DECAY 0.6, SIZE 0.5, third-octaves against 48 kHz: at **96 kHz HALL's and
MASSIVE's wet is +34 dB at 14 kHz and +85 dB at 16 kHz** (the half-rate tank runs at 48 kHz there, so the tail no
longer stops at 11-12 kHz); PLATE +1.6 to +2.7 dB at 11-16 kHz. At 44.1 kHz HALL/MASSIVE lose 2.4-3.8 dB at 11 kHz.
Below 8 kHz within ±1.5 dB apart from mode scatter. Other rate-dependent constants (code): the input and tank-input
power followers (`Engine.cpp:966, 1054`, 0.002 a sample), the hiss's 3 kHz low-pass (`:1040`, 0.33 a sample: ~6 kHz at
96 kHz), DECAY's make-up glide (`:1100`), the crossfade agreement follower (`Fdn.cpp:89`), the transport's fixed 1000-
sample centre (20.8 ms at 48 kHz, 10.4 at 96; `Transport.h:40`; at 176.4/192 kHz the ±20 c wow at 0.3 Hz needs ±1152
samples and clamps at both ends: estimated), the PLATE's input-allpass wander clamped at 8 samples (`Engine.cpp:778`:
half the depth at 96 kHz). The only rate test (`test_fdn.cpp:341-357`) checks HALL's low-band T60. Whether the owner's
Ableton runs at 48 kHz is unknown (the friend's package too). **Options:** a fixed ~11.5 kHz low-pass on the wet when the
host rate is above 50 kHz (plugin-only code path, 0 firmware bytes if kept out of `IV_FIXED_VOICINGS`); per-sample
constants from time constants in `prepare()`. Plugin only; no ears needed beyond confirming 48 kHz matches.

### A8. The plugin's host block size (measured; plugin)
`blind_block.py`, skank, MIX 1, DECAY 0.6, against `--block 48` (the module's 32 + 16 chunks):
| | block 64 | 37 | 441 | 1 |
|---|---|---|---|---|
| HALL as built | −49.3 dB | −23.0 | −22.9 | −21.5 |
| HALL `IV_HALFRATE_PRIME=1` | −49.3 | −47.0 | −47.7 | −28.3 |
| MASSIVE as built | −47.5 | −19.5 | −19.4 | −19.4 |
| MASSIVE primed | −47.5 | −49.7 | −47.5 | −39.6 |
| PLATE (no half-rate) | −56.7 | −57.0 | −58.1 | −25.5 |

So the last review's "block 48 vs 32/64/96 at −22 to −31 dB" is now −47 to −57 dB (the CPU-save work's per-block control
maths), but any odd host block still leaves the one-sample hole and a wet one sample late (−19 to −23 dB against the
module), after every reset, SPACE flip (`resetHalf(tankHalf_)`, `Engine.cpp:875`) and, for the twin, every freeze
(`:351`). Hosts that split blocks at automation points or bounce with remainders hit it. The plugin host test compares the
plugin with the Engine **at the same block schedule** (`plugin/tests/PluginHostTest.cpp:327-341`), so it cannot see this.

### A9. Things measured or decided that are fine (checked, low risk)
- **Hot input through BLEND** (`blind_measure.py` §2, skank normalised to 1.0 peak, every SPACE, MIX 0.3/0.5/0.7, DECAY
  0.6/0.95): output peak 0.52-0.95, no sample over 1.0, none over 1.0 after the firmware's ×0.874 trim. The bound is
  |out| ≤ √(in² + 0.891²) = 1.34 for a full-scale in (soak audit), ×0.874 = 1.17 at the DAC, which clips hard
  (`libDaisy/src/daisy_core.h:146-151`); only correlated dry and wet peaks at MIX ~0.45 could reach it. Low risk.
- **Subnormals on the module**: round 11 measured `subX` = `norm` = 9.0 cycles (`docs/research/m3-round11.log`): no FZ
  needed. The plugin flushes (`ScopedNoDenormals`), the module doesn't: last-bit divergence only.
- **The button**: libDaisy's `Switch` needs 8 consecutive 1 ms reads to press (`switch.h:74`); a glitch while held
  drops a press for 8 ms (a release then a press): painting could end early. Negligible.
- **Power-up**: the Engine starts at ParamSpec defaults (HALL, DECAY 0.5) and glides to the knobs over the first
  ~100 ms; a SPACE flip runs at boot if the switch isn't on HALL. Silent at boot, harmless. (The plugin calls
  `reset()` after its first `pushParams()`; the firmware doesn't. A one-line difference if anyone ever cares.)
- **SPACE switched while frozen**: the flip waits for the unfreeze and its 1 s glide (`Engine.cpp:858`), then cuts the
  old pad over 50 ms. Click-tested (`test_soak_regressions.cpp:144-170`) but never heard by the owner as a behaviour:
  the pad doesn't decay naturally after an unfreeze if SPACE moved meanwhile. One listen.
- **True stereo / mono input**: `test_fdn` checks L-only input reaches both sides decorrelated; the module's hardware
  normal covers mono. The plugin's mono bus has the aliasing bug (B1.3).
- **LED meters**: maths tested (`test_led_meter`), PWM by DMA checked by the owner at the rack.

### A10. Process blind spots
- **The soak has not run since v1.0.33** (`git log -- tools/soak`: last change 25 Oct; the audit is 23-24 Oct). Since
  then: the 24 kHz half-rate tank, the 12-point swell, WEAR, MOTION's transport, the small room, TONE's bell, the
  DECAY range and its make-up cap, the duck changes, SIZE groups. `iv_soak` still builds (CMakeLists.txt:58-59). A few
  hours on the desktop, no owner; add a DC-content check (A1) and an odd-block schedule.
- **The shipped configuration is never built on the desktop.** The firmware compiles the Core with
  `-DIV_FIXED_VOICINGS -ffp-contract=off` (`firmware/Makefile:104, 112`), where `IV_ZI` drops member initialisers
  (`SizeOpt.h:48-61`) and `#else` branches take over (`Engine.cpp:449, 761-763, 1106, 1111`). Every desktop test and
  the renderer build the other configuration with fused multiply-adds (`CMakeLists.txt` has no FP flag). A misplaced
  `IV_ZI` on a class built as a local would read garbage only on the module.
- **The firmware never checks `engine.ready()`** (grep: only a comment at `firmware/main.cpp:232`). `Arena`'s cache-set
  stagger pads by the absolute address (`Memory.h:42-44`), so the desktop's vectors prove nothing about the firmware's
  arrays' padding; a too-small bound after a layout change would ship as a silent dry passthrough with normal LEDs.

## B. Code health

### B1. Test gaps: regressions no test in `host/tests` or `plugin/tests` would notice
1. DC / infrasonic build-up in an endless tail (A1): no test measures the wet's DC.
2. The still-lows split's coherence (A2): no test of the 200-400 Hz balance across SIZE.
3. The plugin's mono input bus (B1.3 bug below): `renderPlugin` copies the stimulus into two channels
   (`PluginHostTest.cpp:197-198`), so `inR == inL` aliasing is never exercised.
4. Plugin at 44.1 / 96 kHz: only "loads" (`PluginHostTest.cpp:247-272`); a tail 85 dB brighter at 16 kHz passes.
5. Odd host blocks: `test_halfrate_prime` asserts the prototype only and prints the as-built hole; the plugin test
   compares like with like.
6. The firmware's knob path (10 ms smoothing, per-knob deadbands, end stops, `firmware/main.cpp:2700-2716`) is
   inline in `AudioCallback`: only `PotEndStops.h` is tested. Moving it into a header next to `PotEndStops.h` and
   testing it would cost nothing in flash.
7. Gate chatter and short pulses (A5); SPACE switch bounce (A6).
8. The firmware's configuration (A10): no desktop build with `IV_FIXED_VOICINGS` + `-ffp-contract=off`; a
   `same_render.py` run between the two configurations would bound the desktop/module gap.
9. Arena padding at adversarial base addresses (A10): `attachMemory` with the arrays offset by k × 32 B, k = 0..127,
   asserting `ready()` (cheap, desktop).
10. NaN / Inf input recovery (still no guard, B2); two Engines preparing on two threads (sieve race, B2).
11. `Swell::process` (`Swell.h:317-330`), the per-sample reference its comment promises to check `processBlock`
    against, has no caller and no test.
12. SIZE / DEPTH under a CV LFO (A3).

### B2. The last review (`docs/research/review-2026-10-26.md`), finding by finding
| Finding | Now | Evidence |
|---|---|---|
| §0.1 / F1 SDRAM tRP/tRCD 3/3 | **done** | `firmware/main.cpp:2798` (release, at boot); round 11 SDRAMCHK 0/0/0, `sdr32` 34 → 17 (`m3-round11.log`) |
| §0.2 / A tank line by line | **done** | `Fdn::runBlock` / `lineFast` / `lineSlow` (`Fdn.cpp:612, 803`) |
| §0.3 SIZE never takes effect | **done**, two layers | `kSizeHysteresis` 1e-3 on by default (`SpaceVoicing.h:47`, `Engine.h:288`, v1.0.37 `3212714`); firmware 10 ms smoothing + 0.003 deadband for SIZE/TIME (`main.cpp:2700-2716`); rack jitter stayed inside ±0.0005 (TASKS, 26 Oct) |
| §0.3 twin swell 24 taps forever | **done** | `Engine.cpp:1029` (`twinRuns && activeSpace_ == 0`) |
| §0.4 / G, H memory placement | **done**, extended | Engine in DTCM (`main.cpp:225`); allpasses in AXI; since 28 Oct the shimmer rings in D2/AXI and the room in D2 (`Engine.h:70-91`) |
| §0.5-6 neutral savings, ~90 % | **done**; margin still thin | round 11 everyday MASSIVE 61-80 %; worst lap 93-95 % (HANDOFF) |
| §0.7 swell 24 points at 24 kHz (K) | **not done** | only `IV_SWELL_TAPS` (the old 48 kHz swell) |
| §0.7 six warped early taps | **not done** | `kHallEarlyTaps` 2 / `kMassiveEarlyTaps` 4 |
| §0.8 / I tape at 2× (`IV_TAPE_OS`) | **done, then moot** | adopted v1.0.38; the tape stage left with v1.0.42 (WEAR); the renderer prints "ignored" (`host/render/main.cpp:256-258`) |
| §0.8 / J still lows at 6 kHz (`IV_LOW_DECIMATE`) | **still an option** | hook live (`Engine.cpp:71-75`), page trashed 27 Oct ("the CPU fits"); −7..−10 % in MASSIVE (last review's estimate) is the biggest unspent lever for A3's peaks |
| §0.8 / L shimmer lazy (`IV_SHIMMER_LAZY`) | **still an option** | hook live (`Engine.cpp:993-1000`); the rings moved on chip on 28 Oct, so its −2..−3 % (which counted two SDRAM writes) is now smaller (estimated) |
| §0.8 / N smaller twin | **done differently** | the small room, v1.0.43, ADR 0028 |
| §0.8 / O 4-line velvet FDN | not done | — |
| §0.9 Engine split | **not done, grown** | `Engine.cpp` 1,233 lines (961 then) |
| §0.9 control twice per block | **partly** | `kChunk` still 32 (`Engine.h:256`); the per-chunk maths now memoised for the two chunk lengths (`Engine.cpp:85-96, 785-792`); plugin vs module at even blocks −47..−57 dB (was −22..−31), odd blocks still −19..−23 (A8) |
| §0.9 / §5.5 FMA on the desktop | **still true** | `CMakeLists.txt` sets no FP flag; `Mappings.h:193` works around contraction by hand ("no fused multiply-add (renders as picked)") |
| §5.1.1 SIZE bug | done | above |
| §5.1.2 HalfRate hole (`IV_HALFRATE_PRIME`) | **still true** | `halfPrime_` off (`Engine.h:333`); measured tonight (A8) |
| §5.1.3 plugin mono aliasing | **still true** | `PluginProcessor.cpp:91` (`inR = inL`), `Engine.cpp:1144-1145` writes `outL` before reading `inR[i]` |
| §5.1.4 NaN poisoning | **still true** | no finiteness test in `Engine::process`; `Limiter::process` passes NaN (`Output.h:198-206`) |
| §5.1.5 sieve race | **still true** | `gSieved` plain bool, lazily set (`Fdn.cpp:55-65, 103`) |
| §5.1.6 twin outlives its sound (`IV_TWIN_STOP_FAST`) | **done** | `twinStopFast_ = true` (`Engine.h:334`); since v1.0.43 HALL/MASSIVE's room stops on its own 50 ms power (`Engine.cpp:1068-1077`), so the flag now matters for PLATE's twin only |
| §5.1.7 twin input side idle | done | `Engine.cpp:957, 1011` |
| §5.2 dead headers `Filters.h`, `Select.h`, `Seed.h` | **still true** | included nowhere (grep) |
| §5.2 `IV_NO_UNSWITCH`, `IV_INLINE` unused | **wrong now** | used at `Fdn.cpp:612, 803`, `Governor.h:175, 184` |
| §5.2 `Swell::process` unused | still true | no caller |
| §5.2 `Oversampler` 4× branch, `latencySamples`, `groupDelay` | still true | only `Halfband` and the coefficients are used (by `HalfRate.h`) |
| §5.2 `kLimitThreshold` duplicate | still true | `Engine.h:50`, no user |
| §5.2 stale Resilio comments | still true | `Oversampler.h:1-27` (Spring, Loop, "SPEC §4.9, §5 mitigation 4"), `Seed.h:2` (Splash, KickVoice, Wobble), `SizeOpt.h:3` ("ADR 0011: 128 KB": 0011 is the gate here; the flash ADR is 0002), `SizeOpt.h:24-29` (Resilio's Tank, "ADR 0038") |
| §5.4 firmware never checks `ready()` | still true | A10 |
| §5.4 denormals (FZ) | **resolved** | `subX` = `norm`; release runs without FZ on purpose (`main.cpp:1955`) |
| §5.5 stimulus as a CTest fixture | still true | `CMakeLists.txt:40-47` has no fixture |
| §5.6 `queueEdge`, constructors | **done** | `queueEdge` is `IV_SIZE_OPT` (`Engine.cpp:294`); `IV_ZI` −1,288 B (`0893ced`) |
| §6.4 / R `-fno-math-errno`, `-fmove-loop-invariants` | not done | `firmware/Makefile` has only `-ffp-contract=off` among FP flags |

### B3. SPEC and ADRs against the code
Numbers that match (SPEC ↔ code): BLEND cos/sin (`Mappings.h:24`); DECAY 0.4→30 / 0.8→60 / 2→120 s, 0.80 / 0.85
(`Mappings.h:30-37`); TONE 0.08 / 0.45 / 1.5, shelf 2.5 kHz, tilt 600 Hz +4/−9, −4/+6 (`Mappings.h:59`,
`SpaceVoicing.h`); SIZE HALL 35→260 ms, ×0.49; MASSIVE 0.35→2 s, spread 0.4→1, diffusion 0.45→1, predelay 0.1·x²; DEPTH
0.25·x² (MASSIVE linear), BLOOM 0.12→4 s, shimmer send 1.2; MOTION dead zone; in-loop allpasses 24-91 ms × 0.62; duck
−24 dBFS, 2:1, 30 ms / 0.4 s, 3 dB / 0.5 s / 0.3 s / 40 % / 0.08 s; floor −21, HALL +3; limiter 0.891; twin −6 dB, 8 s;
paint 1.5 dB; freeze ramps 20 ms / 0.2 s / 1 s; SPACE flip 50 + 50 ms; smoothing tiers (`ParamSpec.h:56-66`).

Drift (SPEC says → code does):
1. **§4.5 MASSIVE's built-in attack** "A = A₀(Decay)·m(Size), A₀ 0.5 → 0.2 s" → it is ×0.5 since v1.0.25
   (`kMassiveSwellScale`, `Engine.cpp:611`); only the changelog says so (SPEC:59).
2. **§5 target "≤ 75 % peak, 80 % ceiling"** (also `firmware/README.md:108`, SPEC §7 M7) vs §5.1's own "under the 95 %
   target" and the ~90 % the project works to (review §1).
3. **§5.1 table**: "Swell stage (4 stages × 2 ch)" (12 taps a side since v1.0.35); "`Select.h` VSEL" (dead header);
   "SHIMMER ... SDRAM" (D2/AXI since 28 Oct).
4. **§5.2 table**: the small room "AXI SRAM" → D2 SRAM (`Engine.h:87-91`, `Engine.cpp:179`); SHIMMER's line "SDRAM" →
   D2 + AXI (`Engine.cpp:177`); allpass lines "256 + 128 KB" → 192 + 96 KB (`Engine.h:64`); FDN lines "~3.2 MB" (the 24 kHz
   tank: 8 × 128 K floats = 4 MB, last review App. B); swell "4 × 2 ch × 1 s, ~1.5 MB" → ~5.6 MB each; predelay "~0.3 MB"
   → 2 × 64 K floats = 512 KB; Plate "~150 KB" → ~270 KB each; stack "AXI SRAM" → top of DTCM (last review §G); "As built
   (v1.0.47)".
5. **§5.3** "release 110,732 B at v1.0.47, 1,908 B under" → 112,564 B, 76 B under (v1.0.55).
6. **§6.2** lists `Oversampler.h, Filters.h, Select.h, Seed.h` as live files (three are dead).
7. **§4.1 diagram**: an "input gain" stage that doesn't exist; hiss and crackle drawn before the freeze router, but they
   join after the swell (`Engine.cpp:1035-1050`).
8. **§3 Switches** "ONSET crossfades ~50 ms on the wet" → the swell's input and output dip over 50 ms while the tail
   rings on (`Engine.cpp:618-631`).
9. **§3 LEDs** "(ADR 0031)" means Resilio's ADR 0031; this repo's 0031 is TONE's resonant tilt (also
   `test_led_meter.cpp:1`).
10. **§7 M5** promises gesture tests for "pulses from a sequencer, bounces": none (A5). **§7 M2** "Works at 44.1/48/96 kHz
    (T60 within 5 %)" is checked for HALL's low band only (A7).
11. **`firmware/README.md`**: the size table stops at v1.0.37 (112,412 B); "## Memory: Nothing yet (M0 passthrough) ... the
    Plate engine in AXI SRAM ... the stack in AXI SRAM" (`:230-232`) is M0-era; "re-checked in Immensio's" (`:234`) while
    `docs/m0-hardware-check.md` step 10 was a quick listen, not a re-measure of the −0.874 trim.
12. **ADR 0025** has no decision on SIZE under CV (A3); **ADR 0013** none on a flip that returns (A6).

### B4. Dead code and stale comments (new since the last review, or not listed there)
- `Engine.h:285-287`: an empty `#ifndef IV_FIXED_VOICINGS` / `#endif` pair.
- `Engine.h:18-20`: "attachMemory() with its SDRAM and DTCM arrays" (now also AXI and D2).
- `Engine.h:363-367` and `Engine.cpp:178`: the small room "its rings in AXI SRAM" → D2 (`Engine.cpp:179` takes `d2_`).
- `Engine.cpp:1009, 1015-1016`: the twin "in MASSIVE, its own swell" / "MASSIVE's twin swells as its tank does": the room
  has had no swell since v1.0.43 (`:1029`).
- `Engine.cpp:1064`: `twinPlate_.fastLevelDb()` read every chunk in HALL/MASSIVE too, then overwritten (`:1068-1077`).
- `Engine.cpp:396-397`: SIZE's hysteresis repeats `massiveMeanSeconds`' 0.35 / 2 as a literal (`log(2.0f / 0.35f)`); a change
  to the mapping would silently change the band. A `constexpr` pair in `SpaceVoicing.h` fixes it.
- `ParamSpec.h:36` "Slewed: SIZE slew-limited so a move bends like tape" (HALL/MASSIVE crossfade since v1.0.34; the Engine
  implements Slewed as no smoothing, `Engine.cpp:795`); `smoothingMs` 80 / 100 / 50 for SIZE / SPACE / ONSET unused.
- `SpaceVoicing.h:19` "SPEC §3: 40 → 180 ms" (35 → 260 since v1.0.17).
- `Mappings.h:55-57` "M1 placeholder for the tilt ... lands with the loop chain (M6)"; `Mappings.h:88-89` "BLOOM: swell 0.1 → 4 s"
  (0.12).
- Test headers: `test_size_noise.cpp:5` and `test_twin_stop.cpp:5-6` "a prototype behind a hook ... off as built" (both on
  since v1.0.37); `test_output.cpp:3` "4:1 compression" (2:1 since v1.0.51); `test_led_meter.cpp:7-8` "returns once the
  limiter lands" (it has).
- `PluginProcessor.cpp:49` "reported to the host once the Core adds latency": the Core adds wet-only latency (half-rate,
  MOTION's 20.8 ms), correctly not reported; the comment reads as a to-do.
- `Filters.h:39` `DcBlocker` (dead today) is the ready-made piece for A1.

### B5. Proposed additions to TASKS' "review's housekeeping list" (not edited on this branch, to avoid conflicts)
- A DC blocker on the tank input, behind a hook first (A1); a DC-content check in the soak.
- `engine.ready()` checked at boot with a red boot pattern (~16-24 B); an Arena padding test at all base alignments.
- A desktop build of the shipped configuration (`IV_FIXED_VOICINGS`, `-ffp-contract=off`) and a `same_render` between it and
  the usual one.
- The firmware's knob path moved into a tested header; a gate minimum-width/hold (A5); a SPACE flip that returns keeps the
  tail (A6).
- Plugin: the half-rate prime (+32 B, goldens' bits), a band-limit above 50 kHz host rates (A7), a mono-bus host test.
- Re-run the soak on v1.0.55.
- The SPEC §4.5 / §5 / §5.1 / §5.2 / §5.3 / §6.2 / §4.1 / §3 corrections in B3, `firmware/README.md`'s table and Memory
  section, and the comments in B4.

## Could not check
- Anything on the module (A3-A6 module halves, A4 entirely, codec DC filters).
- A2's mechanism (correlation of the two tanks) and its extent over DECAY and other inputs: one stimulus, one DECAY.
- The plugin itself (not built: `IV_BUILD_PLUGIN=OFF` worktrees); its mono and rate findings rest on code and on the Engine
  at those rates through the renderer.
- Ears: nothing here was listened to.
