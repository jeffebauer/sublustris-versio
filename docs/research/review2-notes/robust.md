# Overnight review 2: robustness of the Core and the module path (28 Oct 2026)

Branch `review2/robust` off `review/overnight-2` (v1.0.55). Builds on `docs/research/soak-audit.md` (341 h of chaos,
gestures and flips: nothing non-finite, nothing over the ceiling) and `docs/research/dsp-audit.md` /
`review-2026-10-26.md` (the NaN-poisoning note, the FMA note, the SDRAM item). This pass looks at what those didn't:
hot Eurorack input, DC, power-up, NaN thresholds, flush-to-zero, hour-long spectral drift, and the module-side facts
(the codec, SDRAM timing, the desktop/module maths).

Tags: **[M]** measured tonight (desktop, Apple M1 Pro, Release, heavily loaded machine: timings are noisy);
**[C]** read from code; **[L]** literature (cited); **[E]** estimate; **[U]** unverified.

Tools: `tools/review2/robust.cpp` (built by hand against `build/libiv_core.a`, see its header). Modes: `hot`, `dc`,
`dc2`, `powerup`, `fuzz SEED MIN`, `nan`, `spike`, `ftz`, `soak SPACE MODE MIN [tone era drift]`. Two desktop-only
prototypes on the Engine, both under `#ifndef IV_FIXED_VOICINGS` (release `.bin` byte-identical, 112,564 B; renderer
`same_render` SAME 60/60 against the pinned main renderer with the hooks off): `IV_DC_BLOCK_HZ` (a one-pole high-pass
on the wet's input) and `IV_INPUT_GUARD` (non-finite input sample -> 0, the rest held to +-8, wet side only).

## Findings, ranked

### 1. An endless tail slowly turns into sub-bass and DC (PLATE most; HALL some; MASSIVE barely) [M]

Hour-long soaks, MIX 1, DECAY 1, SIZE 0.5, TONE noon, WEAR 0 unless noted; 20 s of `04_skank` then silence (or fed
throughout, or freeze / paint). Level per minute in bands, dB re full scale, minute 2 -> minute 60:

| run | total | <40 Hz | 40-160 | 160-640 | 640-2.5k | 2.5-10k | wet DC |
|---|---|---|---|---|---|---|---|
| PLATE endless, silence | -22.0 -> -21.9 | -35.7 -> **-26.1** | -35.3 -> -29.8 | -27.7 -> **-51.2** | -44.3 -> **-89.5** | -83 -> -108 | -0.015 -> **-0.043** |
| PLATE frozen | -21.1 -> -21.1 | -36.6 -> -32.4 | -36.3 -> -32.1 | -27.2 -> -29.3 | -37.7 -> -53.7 | -66 -> -89 | -0.014 -> -0.022 |
| PLATE painting (fed 60 min) | -19.3 -> -19.2 | -32.5 -> **-21.3** | -34.3 -> -33.2 | -25.5 -> -32.3 | -37.0 -> -52.0 | -65 -> -69 | -0.023 -> **-0.085** |
| PLATE endless, fed 60 min | -21.2 -> -21.3 | -31.5 -> -30.0 | flat | flat | flat | flat | -0.026 -> -0.030 |
| HALL endless, silence | -24.4 -> -25.0 | -42.5 -> -39.7 | -40.1 -> -37.8 | -29.7 -> -30.5 | -40.9 -> -49.8 | -71 -> -90 | -0.006 -> -0.008 |
| HALL endless, fed 60 min | -18.7 -> -17.9 | -32.1 -> -25.3 | | | | | -0.021 -> **-0.053** |
| HALL frozen / painting | flat within 0.6 | +2.7 / +1.8 | +2.4 / +2.3 | flat | -11 / -9 | -26 / -10 | flat |
| PLATE endless, TONE 1 | -21.2 -> **-24.9** | -41.3 -> -33.3 | -39.8 -> -32.3 | -27.1 -> -35.7 | -33.7 -> -66.8 | -58 -> -95 | -0.008 -> -0.019 |
| HALL endless, TONE 1 | -23.3 -> -25.4 | +3.0 | +2.4 | -1.0 | -12.2 | -27 | small |
| MASSIVE endless, TONE 1 | -25.2 -> -26.1 | +1.0 | +0.9 | +0.6 | -2.9 | -12.8 | +0.001 |
| PLATE endless, WEAR 1 MOTION 1 | -21.6 -> -23.7 | -46.3 -> **-31.1** | -36.3 -> -30.8 | -26.8 -> **-50.4** | -41.6 -> -67.4 | -67.5 -> -70 | -0.002 (none) |
| PLATE frozen, WEAR 1 MOTION 0 | -20.5 -> -21.1 | +9.2 | +9.0 | -4.5 | -22.6 | -8.5 | none |

| MASSIVE endless, silence | -27.3 -> -27.4 | -48.6 -> -46.4 | -42.8 -> -40.8 | -33.5 -> -32.8 | -38.7 -> -45.5 | -56 -> -81 | +0.002 (flat) |
| MASSIVE endless, fed 60 min | -21.8 -> -20.9 | -41.2 -> **-29.7** | flat | flat | -3 | -5 | +0.007 -> **+0.028** |
| MASSIVE frozen / painting | flat within 0.1 | +1.2 / +0.8 | +1.2 / +0.9 | flat | -3 / -3 | -14 / -10 | flat |

**Not a desktop artefact** [M]: the same 30-minute PLATE run with the Core built `-ffp-contract=off` (module-like
arithmetic, 0 fused ops) follows the same path to within 0.1 dB in every band (mids -27.7 -> -41.2, DC -0.037 at
30 min), so the drift is the design, not the rounding.

How fast, for a performance: the endless PLATE's mids are -1 dB at 5 min, -3 dB at 10, -9 at 20; the <40 Hz band
+2.5, +5, +7 dB.

What it means, in the owner's words: **leave an endless PLATE ringing for an hour and the level stays put, but the
chord inside it drains away and a low hum and a DC shift take its place**: by 30 minutes the mids (160-640 Hz) are
13 dB down and the 40-160 Hz band 6 dB up; by an hour 23 dB down and a DC offset of -0.043 (-27 dBFS) rides the
output. HALL does it a third as much, MASSIVE hardly. Nothing blows up (no non-finite, peak <= 0.65, the limiter
never acts).

Why [C + M]: at endless DECAY the loop's loss is exactly zero at DC (`Governor.h` `absorbent()` matches the decay at
DC and Nyquist; `Era.h:72-75` deliberately has "no DC blocker, which would drain the lows of an infinite tail"), every
other frequency keeps a small residual loss (the allpass-interpolated modulated reads, the highs' fade, ADR 0008), and
the hold's make-up lifts the whole tail back to its settled level at up to 0.3 dB/s. So the band with the least loss
per pass wins over minutes, and the make-up amplifies it. Two parts:

- **The DC part comes from the input** [M]: `04_skank` has a mean of -2.25e-4 (normalised to 0.5 peak). With the
  input DC-block prototype at 3 Hz, the endless PLATE's wet DC after 30 min is -3.2e-4 instead of -0.037, and the
  painted PLATE's -3.9e-4 instead of ~-0.058 (`soak_plate_dcblock`). The fed endless HALL (-0.021 -> -0.053) and the
  painted PLATE (-0.023 -> -0.085, still climbing ~0.001 a minute at 60 min) are the input's DC being integrated.
  WEAR 1 shows no DC build-up (one of its stages takes the mean off).
- **The sub-bass part is the loop's own** [M]: with the DC block on, PLATE's <40 Hz band still climbs (-46 -> -33 dB in
  30 min) and the mids still collapse (-28 -> -40). That is a voicing question for the owner, not a bug in the sense of
  the soak: "after a long while, should an endless PLATE keep its chord, or is drifting to a low drone fine?" (The
  agent chasing the TONE-CW rumble at DECAY 1 will see the same mechanism at TONE noon; at TONE 1 the PLATE also loses
  3.7 dB of total level over the hour.)

The soak-audit's hold checks ran 3-6 minutes and measured total level; this is only visible in bands over tens of
minutes. Desktop arithmetic (FMA, item 7), so the module's drift is a cousin of this, not the same run.

Options:
- **A. Input DC block** (one-pole HP on the wet's input, the dry untouched): `IV_DC_BLOCK_HZ=3`. Sound on music: music
  band (40 Hz-10 kHz) within 0.1 dB at DECAY 0.5 and endless in every SPACE; sub-20 Hz of plain music -4 to -12 dB
  (`dc2`). Removes the DC build-up entirely; doesn't stop the sub-bass drift. CPU ~8-10 cycles a sample (0.1 % of the
  10,000) [E]; flash ~40-60 B [E] against 76 B left (a constexpr coefficient for 48 kHz, no `expf`). Risk: low.
  Sound change: tiny (none audible on music; a bit-identity break, so a golden refresh). On the module the codec's ADC
  high-pass probably does this already for steady DC (item 3), so A matters most for the Plugin, the renderer, and slow
  offsets / sub-audio under the codec's corner.
- **B. A very slow in-loop DC/sub-bass leak only at endless DECAY** (e.g. a 1-2 Hz one-pole leak on the loop's sum):
  stops both the DC and the sub-bass wins-by-default; but ADR 0008 / `Era.h` rejected an in-loop DC blocker because it
  drains an endless tail's lows. Needs a prototype, a page and the owner's ears; CPU ~16-40 cycles (0.2-0.4 %), flash
  ~60-100 B (over budget today).
- **C. Accept and document** ("an endless PLATE drifts darker over tens of minutes").

### 2. DC or a slow offset on the input wrecks an endless tail in the Plugin (and wherever DC reaches the Core) [M]

`robust dc` / `dc2`, MIX 1, NATURAL, 20 s of music at 0.5 peak plus a DC step:

| | wet DC per unit of input DC | music band of the endless tail, with DC 0.1 | with a 1 Hz 0.2 sine |
|---|---|---|---|
| PLATE | 3.5x (DECAY 0.5), 1.7x (endless); BLOOM -4.3x | **-19.3 dB** | -3.4 dB |
| HALL | 1.4-2.8x (BLOOM -2.9x) | **-16.5 dB** (and at DECAY 0.5 a +18 dB wobble in the music band) | -0.3 dB |
| MASSIVE | -2.4x (DECAY 0.5), -0.6x (endless) | **-14.0 dB** | **-15.8 dB** |

The Core's wet has no DC rejection anywhere (`Filters.h`'s DC blocker is dead code, included nowhere). Input DC is
multiplied by the tank's low-frequency gain (several times), raises the tank's power (PLATE with DC 0.5: +4.1 dB over
full scale inside the loop, so the in-loop soft clip works on a DC pedestal) and, at endless DECAY, the floor governor
regulates DC + music to the floor, so the music is pushed 14-19 dB down. With `IV_DC_BLOCK_HZ=3` all of this is gone
(wet DC <= 0.013, music band within 0.1 dB of clean). Option A of item 1 fixes this too.

### 3. Hot Eurorack input: the Core is safe; the DAC clips at MIX noon on full-scale input [M + C + L]

The module path [C]: libDaisy reads the codec as 24-bit and scales to +-1.0 (`s242f`), no gain (`postgain` 1); the
firmware hands `in[]` straight to the Engine (`firmware/main.cpp:2734`) and multiplies the output by `kOutputTrim`
-0.874 (`:2362`, undoing the analog path's inversion and +1.17 dB) before libDaisy's `f2s24`, which **clamps to +-1**
(`libs/libDaisy/src/daisy_core.h:146-151`: a hard clip, no wrap; a NaN converts to 0 on the M7). The input stage: the
Versio's audio inputs clip around **16 Vpp** [L: Noise Engineering's Desmodus Versio manual, as cited in Resilio's
`docs/m8-tuning-backlog.md:219` and SPEC §8.1], so 0 dBFS ~ +-8 V and a +-10 V source is 1.9 dB over: the analog
stage / ADC clips it flat before the Core sees it. The Core therefore never sees more than +-1.0 on the module. The
codec chip depends on the Seed revision (`daisy_seed.cpp:267-313`: AK4556 on rev4, WM8731 on 1.1, PCM3060 on 1.2 /
DFM); which one the owner's Versio carries is **[U]** (no log prints it).

The Core with full-scale and clipped input (`robust hot`: `04_skank` +12 dB clipped, pink noise +12 dB clipped, a
110 Hz full-scale square; every SPACE x ONSET x WEAR 0/1 x DECAY 0.5/1 x MIX 0.5/1, 216 runs of 20 s):
- Never non-finite. The wet never exceeds 0.891 (the limiter's ceiling) in any case.
- The limiter works hard on steady full-scale material: least gain 0.20 (-14 dB) in MASSIVE, 0.30 PLATE, 0.44 HALL
  (noise, square). On clipped music it never acted (gain 1.0): the ducking and the governor keep music below it.
- **At MIX noon the sum passes the DAC's full scale**: out = 0.707 dry + 0.707 wet reaches 1.337 (= sqrt(1 + 0.891^2),
  the equal-power worst case), x 0.874 = 1.17, so libDaisy's clamp clips it: **13 % of samples** on a full-scale
  square (PLATE SHIMMER, DECAY 0.5), up to 0.5 % on full-scale noise, **0 on clipped music**. A digital hard clip,
  only when the input is already at the jack's clip level and dry and wet peak together.
  Option: a soft clip on the summed output, identity below ~0.95 of the DAC's full scale (so MIX 0 stays a cable:
  the dry alone peaks at 0.874): ~6-12 cycles a sample (0.1 %), ~40-80 B flash (over the budget today), sound change
  only on full-scale input at mid MIX (tiny-audible: a softer clip instead of a hard one). Or accept: it's the same
  on any Versio firmware that sums a full-scale dry with a wet. Low priority.

### 4. One non-finite or huge input sample poisons the tanks for good; the threshold is ~1e20 [M]

`robust nan` / `spike`: one sample in the left input, 5 s into music (the rest normal), 60 s run.
- **NaN or Inf: every output sample is NaN from that sample on, forever**, in every SPACE x ONSET, frozen or not
  (2.64 M bad samples of 2.64 M after it). Both channels (the tank mixes them).
- **A finite 1e25 or larger does the same** (its square overflows the power followers). 1e19 and below stay finite
  but leave the tank enormous: a 1e10 spike leaves PLATE +72 dB over full scale 25 s later (the limiter then holds the
  output at 0.891 of garbage for as long as that lasts); a 1e6 spike still leaves PLATE's tank 24 dB above its normal level 25 s later.
- Frozen at the moment of a 1e30 spike: HALL and MASSIVE survive; PLATE's pad survives (tank -30 dBFS) but its output is NaN. [I] The likely path: the input follower
  `inputPower_` (`Engine.cpp`, the input loop), which drives the ducking, is a one-pole that never recovers once NaN. So the last review's proposed guard on the governors' power (`if (!(level == level))
  reset();`) would **miss** this case: the tank stays finite while the output is NaN.
- **Module: can't happen from the input** (the ADC delivers +-1.0). Only a NaN made inside the Core could; 4 x 30 min
  plus 4 x 60 min of fuzz (below) made none. **Plugin / renderer: can happen** (a broken plugin upstream, a
  denormal-flush bug, a DAW glitch).
- With `IV_INPUT_GUARD=1` (non-finite -> 0, held to +-8): every case finite and the tank at its normal level; one bad
  output sample remains, the dry's own (MIX passes the host's NaN through for that one sample).
- **Recommendation:** put the guard in the Plugin's `processBlock` (before `engine.process`, on the host's buffer:
  non-finite -> 0, clamp +-8), not in the Core: **0 B of firmware flash**, 2 compares a sample in the Plugin. If a
  Core-side safety net is wanted later, check the *output* once per chunk (`!(outL == outL)`) and `reset()`; ~16-24 B.

### 5. Fuzz: no non-finite, nothing over the ceiling, no subnormal output [M]

`robust fuzz` (each knob jumps with 20 % chance every block, often to an end; SPACE and ONSET switched; button and
gate toggled at random offsets; random block sizes 1-256; inputs cycling through silence, clipped hot music, full-scale
square, full-scale white noise, full-scale DC of opposite signs, full-scale Nyquist (+1, -1, ...), full-scale clicks,
a full-scale 40 Hz sine): seeds 1-4 x 30 min and 11-14 x 60 min (6 hours of audio). Every sample
finite; |out| <= sqrt(in^2 + 0.891^2) on every sample (the soak's ceiling); subnormal output samples only in one seed (8,556, harmless); the
tank's level peaked at +0.1 dBFS (seed 11); the limiter's least gain 0.25. This extends the soak-audit's 341 h with the nastiest
full-scale inputs (Nyquist, opposite-sign DC) and per-block knob jumps; it agrees with it.

### 6. Power-up, freezes, SPACE and ONSET changes: no burst, no pop [M]

- **Power-up in firmware order** (prepare with the defaults, then the first block sends the knobs, as
  `firmware/main.cpp:2700-2716` does with `sent[] = -1`; music from sample 0; 60 random knob sets over every SPACE x
  ONSET; MIX 1): the wet's peak over its first 1.5 s is at most 1.06x its peak over the next 4.5 s (music's own
  variation). One thing: **MIX starts at its default 0.5 and glides to the knob in ~5 ms**, so with BLEND fully wet
  the first 5 ms pass the dry at 0.707 (peak 0.607 of a 0.9 input). Inaudible in practice (the codec is starting too);
  a fix (read the knobs and `setParam` them before `prepare`/`reset`) costs ~20-40 B. Not worth flash now.
- **Silence in, everything moving for 20 min** (knobs jumping, SPACE / ONSET / button / gate at random): max |out|
  1.0e-6 (-120 dBFS; half the samples are non-zero at that level: the hiss / crackle floors). No pop.
- **A steady music bed, wet only, SPACE flipped / FREEZE pressed / ONSET switched** at 4 s, every from/to pair: the
  largest sample-to-sample step within 1 s before to 2 s after, against the same run without the event: 1.17x (flip),
  1.23x (freeze), 1.10x (ONSET). The music's own steps dominate; nothing pops.
- Not checked: the codec's and the analog stage's own power-up (hardware).

### 7. The desktop's maths and the module's: what every bit-identity, soak and denormal claim is worth [M + C]

**Correction to the brief:** the release firmware does **not** flush to zero. Only the profile build sets FZ
(`SetFlushToZero`, profile only); ADR 0026 records "Not adopted: flush-to-zero (subnormals cost nothing on this FPU)"
(round 11: `subX` = `norm`); the release disassembly has no `vmsr fpscr` at all [M]. So:

| | fused multiply-add | flush-to-zero | libm |
|---|---|---|---|
| Release firmware | none in the Core (`-ffp-contract=off`; the 62 `vfma` in the release image are all inside newlib's prebuilt `sinf`/`cosf`/`powf`/`expf`/`logf`/`exp2f`/`log10f` and one in libDaisy's `AnalogControl::Process`) [M] | **off** (IEEE subnormals) | newlib |
| Profile firmware | none | on (`PROFILE_FZ=1`) | newlib |
| Desktop renderer / tests / soak | **901** `fmadd`/`fmsub`-family in `libiv_core.a` (Apple clang's default `-ffp-contract=on`; `CMakeLists.txt` sets no FP flags) [M] | off | Apple |
| Plugin | the same 901 | **on** (`juce::ScopedNoDenormals`, `plugin/PluginProcessor.cpp:74`) | Apple |

Measured: the desktop Core built with `-ffp-contract=off` (0 fused ops) against the normal desktop build,
`tools/same_render.py`: **DIFFER 60 of 60**, from the first ms, largest difference per 12 s render 1e-5 to 2e-3
(-100 to -54 dBFS; the PLATE knob-hunt sweep worst). FZ on against off on a 4-minute quiet tail (`robust ftz`): the
outputs differ only below 1e-37 (the subnormal range); no timing difference on this Mac (Apple silicon handles
subnormals at full speed; the machine was loaded, so +-10 % noise both ways).

What that means:
- **`same_render` bit-identity** ("this change is sound-neutral") is a desktop-against-desktop proof. It is still the
  right proof for a refactor, because both sides use the same compiler and flags, but a change that only moves which
  multiply-adds clang fuses can read DIFFER on the desktop and be identical on the module (a false alarm), and the
  reverse is possible in principle. It says nothing about desktop-equals-module.
- **Soak and hold claims** (hour-long levels, holds within +-1 dB, the drift in item 1): a desktop run diverges from
  the module's in the last bits within milliseconds, and in gain-1 loops that grows to a different waveform over
  minutes. Statistics (levels, band trends, the absence of NaN) carry over; individual waveforms and exact dB do not.
- **Denormal claims**: since release doesn't flush, the renderer (no FZ) and the module (no FZ) agree; the Plugin
  (FZ) differs from both, but only below 1e-37: inaudible. "No denormal slowdown" is measured on the module (round 11)
  and on this Mac independently; the desktop says nothing about the M7's timing.
- **Making the desktop match cheaply:** add `-ffp-contract=off` to `iv_core` (one line in `CMakeLists.txt`, e.g.
  `target_compile_options(iv_core PRIVATE -ffp-contract=off)`, and the same for the Plugin's copy of the Core). Then
  +, -, x, / and sqrt are IEEE single on both and the only remaining differences are libm (Apple vs newlib: the
  control-rate `expf`/`powf`/`sinf`/`cosf` calls, last-bit) and the Plugin's FZ. Cost: **no golden refresh even** (the
  `-ffp-contract=off` build passes ctest as it stands: "100% tests passed out of 21", the golden tolerances absorb it) [M], every desktop render changes in the last bits (inaudible: -54 dBFS worst sample
  difference over 12 s, mostly -80 to -100), CPU on the desktop a few percent [E]. Firmware unaffected. A full match
  (newlib's libm on the desktop, or QEMU `mps2-an500`) is not cheap.

### 8. The release SDRAM timing (3 / 3) and its single check [C + L + M]

What release does: `firmware/Makefile:30` `RELEASE_SDRAM_TIGHT ?= 1`; `firmware/main.cpp:2797-2799` rewrites
`FMC_SDTR1`'s TRP and TRCD from libDaisy's 16 / 10 to 3 / 3 SDCLK at boot (PALL before and after, interrupts off).
The SDRAM clock: PLL2R 200 MHz / `SDClockPeriod 2` = **100 MHz** (`libs/libDaisy/src/sys/system.cpp:478-497`,
`src/dev/sdram.cpp:79`; BENCH prints `sdclk 100 MHz`), CAS 3.

The chip: Alliance AS4C16M32MSA-6BIN, 512 Mb mobile SDRAM, 1.8 V, -6 = 166 MHz grade, **industrial -40 to 85 C**
[L: distributor listings, e.g. https://www.futureelectronics.com/p/7102054 ; the datasheet
https://www.alliancememory.com/wp-content/uploads/20171206_AllianceMemory_512M_LPSDRAM_AS4C16M32MSA-6BINTR_rev1.0_Dec2017.pdf,
AC table p. 17 per the last review: tRP = tRCD = 18 ns]. At 100 MHz, 3 clocks = 30 ns: **67 % over the datasheet
minimum**, which holds over the whole operating range (JEDEC-style AC tables are specified over the operating
temperature and voltage range, with no separate derating; [U] for this exact PDF, which I could not text-extract
tonight). The other timings stay libDaisy's (TRC 8 = 80 ns vs tRC 60 ns; TWR 3; TRAS-field 4), and the firmware's
comment checks RM0433's rules (TWR >= TRAS - TRCD = 1; TWR >= TRC - TRCD - TRP = 2) [C].

What libDaisy's 16 / 10 are for: nothing measured; the source says `RCDDelay = 10; // started at 2` and RPDelay 16
next to a commented block of all-16 values (`sdram.cpp:84-97`), and libDaisy issue #408 (cited by the last review;
the URL 404s tonight) calls 16 a placeholder nobody retuned. They are generous safe values, not a characterised margin.

The evidence for 3 / 3: `SDRAMCHK` (8 MB, address-pattern and complement, scattered so every read opens a row)
**0 / 0 / 0 in eight profile runs** (m3-round11/12/12-itcm/15 logs) [M on the module], all on **USB power, one unit,
at whatever temperature the desk was**. Never on rack power, never warm, never on a second unit.

Weighing it [E]: tRP / tRCD are command spacings inside the FMC's own clock domain; they don't depend on the supply
(the Seed regulates its own 3.3 V; the SDRAM's 1.8 V comes from the Seed either way) or on the PCB's signal timing
(setup / hold are set by the clock, not by these fields). A 67 % margin over a datasheet minimum that is already
guaranteed to 85 C makes a temperature or unit-to-unit failure unlikely. **The larger reliability item is libDaisy's
refresh**, untouched by us: `HAL_SDRAM_ProgramRefreshRate(0x81A - 20)` = one row every ~20.7 us, a full pass every
~168 ms, where the chip asks 8192 rows in 64 ms (7.8 us; COUNT = 781 - 20 = 761). That is 2.6x too slow; at room
temperature cells hold far longer than 64 ms, but the 64 ms is what's guaranteed at 85 C, and a frozen MASSIVE pad's
rows are only touched once a loop (seconds). A weak cell would sound like a rare click in a long hold. Every Daisy
product runs libDaisy's value; no failures are reported that I know of [U]. Fix if wanted: write `FMC_SDRTR` with
COUNT 761 after `hw.Init` (~8-16 B); costs ~0.5 % more of the SDRAM's time in refresh (each refresh ~70 ns every
7.8 us instead of every 20.7 us) [E], so it needs a module CPU check.

Cheap ways to close the 3 / 3 gap, no flash in release: run the profile build's `SDRAMCHK` lap **on rack power**
(USB unplugged: but then serial is off limits, `firmware/README.md` / memory "USB power: knobs unpowered"); so the
profile would need to show `SDRAMCHK`'s result on an LED (a few bytes in the profile, which has 3 KB of headroom
under 128 KB, not in release). Let it run warm in a closed case for an hour. If a second Versio is ever at hand, the
same. And `RELEASE_SDRAM_TIGHT=0` already exists as the fallback build.

**The contradiction in `firmware/README.md`:** line 141 (the profile's BENCH field notes) says "**The release build
never changes the timing.**" Since v1.0.37 it does (`Makefile:30`, `main.cpp:2797`, and the same README's own
lines 172 and 178 "Release runs the SDRAM at 3 / 3"). Also stale in the same file: line 44 / line 232 (memory "Nothing
yet (M0 passthrough)", the PLATE "in AXI SRAM").

### 9. Pot ADC noise against the deadband (reasoning only; the knob port is another agent's) [C + L]

The chain (`firmware/main.cpp:2691-2716`): libDaisy's 16-bit ADC reading with its 2 ms one-pole (at 1 kHz block rate
that's a coefficient of 0.5: ~1 block), then our 10 ms one-pole (coefficient 1/11 = 0.091 per 1 ms block), then a
deadband of 0.001 (0.003 for SIZE and TIME) against the last value sent, with exact 0 / 1 at the stops. The rack
measurement (TASKS.md, 26 Oct, knobs at noon): every reading wobbles, none rests 50 ms, but none left +-0.0005 (after
libDaisy's smoothing, before ours): peak-to-peak under 0.001 (~65 LSB of 16 bits). Playing audio pushed SIZE's
reading past 0.0005 (crosstalk, sampled once per block, so it aliases down to low frequencies).
- On uncorrelated noise the 10 ms one-pole scales the deviation by sqrt(a / (2 - a)) = 0.22 (-13 dB), so the quiet
  wobble becomes ~0.0002 peak-to-peak: the 0.001 deadband is ~5x it, 0.003 ~15x. Comfortable for still knobs.
- The audio crosstalk is the unknown: it is correlated with the playing and partly below the one-pole's 16 Hz corner
  (kicks, bass, the block-rate aliasing), so it is attenuated less. It was measured only as "> 0.0005 raw". If it ever
  reaches ~0.003 smoothed, SIZE / TIME sends again; the late-light / blue-green flash on a send is the right rack test.
- What the deadband costs: a value that can sit up to 0.001 (0.003) from the knob, and a slow turn or a CV moving in
  steps of 0.001 / 0.003 (for SIZE in HALL / MASSIVE each step starts a crossfade, by design; 0.003 = ~9 cents of
  MASSIVE's room). A slow LFO into SIZE's CV therefore crossfades every few ms of travel: worth a listen on the rack.

## What I could not check
- Anything on the module: the codec chip on the owner's Seed, its ADC high-pass (WM8731's is on by default and
  libDaisy's `CODEC_REG_DIGITAL_ROUTING` write leaves ADCHPD at 0 [C]; AK4556 and PCM3060 also list an ADC high-pass
  [L, from memory: **U**]); so whether steady DC ever reaches the Core on the module is likely-no but unverified.
- The datasheet's AC table text (the PDF didn't extract); the libDaisy issue #408 page (404).
- Analog power-up of the codec / output stage.
- Desktop timing is noisy tonight (load average 100-200 from other agents).

## Files
- `tools/review2/robust.cpp` (the harness; build: `clang++ -O3 -DNDEBUG -std=c++17 -Icore -Ihost/common
  tools/review2/robust.cpp build/libiv_host_common.a build/libiv_core.a -o build/review2/robust`, run from the
  worktree root after `python3 tools/make_stimulus.py`).
- `core/dsp/Engine.{h,cpp}`: `setInputDcBlockPrototype`, `setInputGuardPrototype` (desktop only).
- `host/render/main.cpp`: `IV_DC_BLOCK_HZ`, `IV_INPUT_GUARD`.

## Gates on this branch
- ctest (the branch's Release build): "100% tests passed out of 21". The `-ffp-contract=off` build: the same.
- `tools/same_render.py` pinned main renderer vs this branch, hooks off: SAME 60/60. Release firmware rebuilt with
  the prototypes in the source: byte-identical, 112,564 B (76 B under the budget); m0test 83,556 B, profile 127,808 B.
- Raw results (small CSVs) in `tools/review2/data/`.
