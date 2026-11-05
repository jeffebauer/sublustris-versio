# Firmware — build guide

The Versio firmware: a standard libDaisy app in the Seed's internal flash, installed with Noise Engineering's Firmware Swap (SPEC §5.3, §8). One `main.cpp`, three variants. The `.bin` files were `immensio_versio*.bin` until v1.0.48, when the product became Sublustris Versio (ADR 0032); the code inside is the same. Seeded from Resilio Versio's firmware (its variants, flash measures and LED PWM); everything Resilio-specific is gone.

| Variant | Command | Output | What it is |
|---|---|---|---|
| **release** | `make -C firmware` (MODE defaults to `release`) | `build/sublustris_versio.bin` | The instrument: P1–P7 MIX, DECAY, TONE, ERA, SIZE, DRIFT, TIME (+CV); SW0 SPACE, SW1 ATTACK; button and gate edges to the Engine (it reads tap / hold itself). LEDs: In L / In R / Out L / Out R level meters, green → amber with level, red on input clip or when the output limiter pulls down; while frozen the output pair shows cool blue under the meter, pulsing slowly while painting. LED dimming is DMA-driven (`LedPwm` in `main.cpp`, TIM5 + DMA, 512 steps at ~1 kHz), not libDaisy's flickery software PWM. No serial printing (flash). **At M0 the Engine is a passthrough**, so the release build is a calibrated cable with meters. |
| **m0test** | `make -C firmware MODE=m0test` | `build/sublustris_versio_m0test.bin` | The M0 hardware check (`docs/m0-hardware-check.md`): dry passthrough (no trim), every control → LEDs and USB serial, in panel order. Doesn't link the Core. |
| **profile** | `make -C firmware MODE=profile` | `build/sublustris_versio_profile.bin` | M3 CPU profiling: ignores the controls (so its fixed DECAYs are on DECAY's scale, without the knob's curve, v1.0.72, or the wet's lift at short tails, v1.0.73), cycles 24 setting corners (SPACE × DECAY × SIZE × ERA) on a synthetic click + noise signal, prints CPU load per corner and per section over USB serial. Until M1 it measures the callback's own overhead only. |

## How to build

```bash
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
make -C libs/libDaisy -j8            # once (and after a libDaisy update)
make -C firmware all-variants -j8    # all three, with a size report each
```

One at a time: `make -C firmware MODE=<variant> -j8 && make -C firmware MODE=<variant> size`.

`all-variants` reports each `.bin` against the 128 KB internal-flash limit, fails if any is over and warns at 95 %. **Budget: release ≤ 110 KB at v1.0** (SPEC §5.3; Resilio ended at 97.7 %).

Sizes at M0 (21 Oct 2026, passthrough Engine):

| Variant | Bytes | Of 131,072 |
|---|---|---|
| release | 53,852 | 41 % |
| m0test | 82,356 | 62 % |
| profile | 52,224 | 39 % |

The M0 release figure is the platform's floor before any DSP: libDaisy, the HAL, the audio and LED PWM set-up. Resilio's release with its full Tank was 112,628 B after its flash study, so its DSP and controls cost ~59 KB.

| Milestone | release | profile | m0test |
|---|---|---|---|
| M0 (passthrough) | 53,852 | 52,224 | 82,356 |
| M1 (Hall: FDN, diffusers, predelay) | 61,484 (46 %) | 62,312 | 82,356 |
| M4 (MASSIVE, swell, REVERSE, floor governor, in-loop diffusion) | 66,628 (50 %) | 68,248 | 82,356 |
| M5 (PLATE, companion, gesture reader, freeze, paint) | 75,044 (57 %) | 76,112 | 82,356 |
| M6 (ERA, DRIFT, soft saturation, μ-law, Midiverb box) | 84,100 (64 %) | 85,088 | 82,356 |
| M7 (DARKEN, limiter) | 85,076 (64 %) | 86,008 | 82,356 |
| Knob-jitter readout (26 Oct 2026) | 112,484 (86 %) | 123,684 | 83,420 |
| v1.0.37: ITCM, SDRAM 3/3, twin stop, SIZE hysteresis (26 Oct 2026) | 112,412 (86 %) | 123,756 | 83,420 |

Memory at M5: `gLarge` 16 MB in SDRAM (FDN lines and in-loop allpasses, the PLATE, swell, REVERSE segments, predelay), `gFast` 100.6 KB of DTCM's 128 KB (the companion plate and the input diffusers), the Engine object in AXI SRAM. M3 decides whether the PLATE moves from SDRAM to AXI SRAM (SPEC §5.2).

Memory since the overnight review (26 Oct 2026, release map; the numbers of that day, today's are under **Memory** below): the Engine object (24,232 B) in DTCM with `gFast` (36,864 B): 61,096 B of 128 KB from 0x20000000, the stack grows down from 0x20020000 (~68 KB for it). `gAxi` (433,984 B, `Engine::axiFloats`) in AXI SRAM's `.bss`: the in-loop allpass lines of the tank and the low tank (16 × 16 KB + 16 × 8 KB, staggered across the cache's sets); AXI SRAM 452,024 B used of 512 KB. The twin's allpasses stay in SDRAM (all three tanks' don't fit). `gLarge` 24.9 MB in SDRAM.

### Flash-budget techniques in use (from Resilio's flash study, SPEC §5.3)

All in `firmware/` (no changes inside `libs/libDaisy`):

- **`no_uart_spi.cpp`** stubs libDaisy's UART and SPI DMA bookkeeping (and IRQ handlers) so `uart.o`, `spi.o` and the HAL behind them never link (~15–20 KB). The UART half is skipped in `m0test`, which still logs through libDaisy's Logger. The file's header comment has the reasoning.
- **No QSPI flash chip set-up** (release and profile): `QSPIHandle::Init` is replaced by one that only marks the chip's 8 MB address window no-access in the MPU. Nothing in this firmware uses the QSPI chip; the long delay lines go to SDRAM, not QSPI.
- **No USB device stack in release**, and in profile no ADC/controls set-up and no Seed 1.1 codec set-up (it drives only the SAI).
- **`printf` / `putchar` / `exit` stubs** (release and profile): libDaisy's USB core logs through `printf`, and crt0 calls `exit`; together they pulled in newlib's stdio and `malloc`. No variant retargets stdout.
- **`-Os` on `main.cpp`**, while `core/dsp/*.cpp` stays `-O3` in every variant, so release and profile run identical per-sample code and profile's CPU numbers hold for release.
- **`IV_SIZE_OPT` / `IV_NO_UNSWITCH` (`core/dsp/SizeOpt.h`)**: mark control-rate Core code for `-Os` in the firmware only (`IV_FIXED_VOICINGS` is defined here).
- **Profile's serial line** is built with small integer helpers and sent straight to USB CDC, never through `printf` (~2.1 KB).
- **`-ffp-contract=off`**: no fused multiply-add. On the M7, GCC's `vfma` serialises every multiply-add (~9 cycles each, Resilio's M3 bench); separate `vmul` + `vadd` pipeline.
- Rejected in Resilio and not used here: `-flto` (a few hundred bytes larger on this few translation units).

## Which `.bin` to flash

- **M0 hardware check:** `build/sublustris_versio_m0test.bin`.
- **CPU profiling (M3):** `build/sublustris_versio_profile.bin`.
- **Playing it:** `build/sublustris_versio.bin`.

Flashing: noiseengineering.us/portal/firmware → **Select Custom File** → the `.bin` → **CONNECT** → **CHANGE FIRMWARE**. The Firmware Swap app also restores the stock firmware.

**Never have the Eurorack power cable connected at the same time as the USB cable.** The module runs on USB power alone for flashing and for serial monitoring; unplug the rack power first (SPEC §8).

## Reading serial output

```bash
screen /dev/tty.usbmodem* 115200
```

(To quit: `Ctrl-A`, then `K`, then `Y`.)

**m0test**, 10 lines a second:

```
P  500  250 1000    0  500  500  200 | SW 1 0 | BTN 0 3 | GATE 0 7
```

`P` = pots P1–P7 in panel order (× 1000), `SW` = SPACE and ATTACK positions (0 left, 1 centre, 2 right), `BTN` = held now + presses counted, `GATE` = high now + rising edges counted.

Plus, once a second, the **knob jitter** (overnight review section 5.1: a reading that never rests keeps restarting SIZE's 50 ms rest timer, and defeats every "skip it while the knob is still" saving). Each knob is read once per audio block, exactly as the release build reads it, over 1,000 blocks (1 s); columns are P1–P7 in panel order:

```
JPP     12    40     9    15   180    11     8
JCHG    40   310    22    35   990    30    18
JRUN   210    35   400   180     2   250   600
JHYS     0     0     0     0    14     0     0
```

`JPP` = peak-to-peak of the reading × 100,000 (one 16-bit ADC step ≈ 1.5; the SIZE fix's band ≈ 50). `JCHG` = blocks (of 1,000) whose reading differed at all from the block before. `JRUN` = the longest run of identical readings, in blocks (1 block = 1 ms); SIZE's rest timer needs 50. `JHYS` = times the reading left a ±0.0005 band around its last accepted value: the SIZE fix's band in HALL, its tightest (0.1 % of the mean line length; MASSIVE's is ±0.00057), so 0 = the fix would hold SIZE still. (Until 26 Oct 2026's rack check the band was ±0.001, twice the fix's.) Hands off the knobs while reading it, and read it at a few positions (the ends read exact 0 / 1 in the release build, so mid-travel is what matters). **On USB power alone the knob circuits aren't powered** (they run from the rack's ±12 V): every knob reads full scale (`P 1000 …`) wherever it is, so the serial `J` lines then measure the converter's noise at full scale, not the knobs (found 26 Oct 2026). For the knobs themselves use **jitter mode** on rack power (USB unplugged): SPACE (SW0) left, ATTACK (SW1) right. Each knob's LED channel lights if its reading never stayed still for 50 ms in the last second (LED 1 R / G / B = P1 / P2 / P3, LED 2 R / G / B = P4 / P5 / P6, LED 3 R = P7); LED 4 is **green** if no knob left the ±0.0005 band (the SIZE fix's, in HALL) in the last second (the SIZE fix would hold SIZE still), **red** if one did, dim blue for the first second. Other switch positions show the M0 check's LEDs as before.

**profile**, every ~3 s, one corner:

```
CORNER MASSIVE D1 S1 E1 (worst?)   avg  42.3% max  58.1% min  39.0% | mem 0 B | block 48 | fs 48000 Hz | ~4230 cyc/sample
  SPLIT ctl  0.4 in  1.2 swell  3.1 tank 30.0 comp  0.0 era  4.4 out  2.0  (% of budget)
  PEAK  ctl  2.0 in  1.4 ...
```

- `CORNER` — SPACE, DECAY / SIZE / ERA at 0 or 1. ATTACK is BLOOM and MIX, TONE, DRIFT and TIME are at their top on every corner. `(worst?)` marks SPEC §5.1's worst case (MASSIVE with Era full).
- `avg / max / min` — CPU load over the corner's ~3 s, as a share of the block's time budget. **Watch `max`: SPEC §5 targets ≤ 75 % peak, 80 % ceiling.**
- `mem` — bytes of delay memory the Engine uses (0 at M0).
- `block` — audio block size (default 48). `make -C firmware MODE=profile PROFILE_BLOCK=96` compares a bigger block; the output filename doesn't change, so check this field.
- `~N cyc/sample` — the average as cycles per sample (480 MHz ÷ 48 kHz ≈ 10,000).
- `SPLIT` / `PEAK` — each section's share (`dsp/ProfileHook.h`: ctl, in, shim, swell, twin, tank, era, out), averaged and in the worst single block. The marks cost ~1 % (a few per sample).
- Timing (since 25 Oct 2026): the Engine runs in the main loop, block after block off the audio clock, each block timed by the cycle counter; the audio plays silence. A corner over 100 % still reports (timed in the callback, it starved the serial, M3 round 1).
- `BENCH` (once per lap) — clock speed and whether the caches are on; see below for the rest of the line.

On the module: **LED_0** brightens through the corners; **LED_3 turns red** for good once any corner's `max` passes 75 %.

### Profile additions from the overnight review (26 Oct 2026)

**Boot takes ~15–20 s before the first line** (the micro-tests below run once, before the Engine starts). A lap is the 30 corners of round 10, then three new ones, then the headroom test: about 3 minutes.

New corners (MASSIVE, Neve, DECAY full, BLOOM unless named):

| Corner | What it measures |
|---|---|
| `MASSIVE S1 neve FROZEN+twin` | 6 s. The button is pressed at its start (freeze at once), released 10 ms later; the input keeps coming, so the twin plays over the frozen pad all the way. A tap at the end unfreezes it for the next corner. The suspected worst case (cost-tank, cost-input: ~1.5–2× MASSIVE). |
| `MASSIVE S1 neve SHIMMER` | ATTACK = SHIMMER, TIME 0.7. Never profiled before. |
| `MASSIVE neve SIZE .3<>.8` | 6 s, SIZE flicked between 0.3 and 0.8 every 1.5 s: each move starts the 0.3 s SIZE crossfade once SIZE rests, so one runs ~20 % of the time. **Read `max`** (the crossfade blocks) as well as `avg`. |

The headroom test still uses `MASSIVE S1 radio`. No round-10 corner was dropped (the lap stays under 5 min).

Two more lines follow `BENCH` once a lap:

```
BENCH clock 480 MHz icache on dcache on | FZ on (irq on) | sdram TRP 3 TRCD 3 (tight) sdclk 100 MHz | engine 0x20009000 axi 0x240006e0
SDRAMCHK libdaisy 0 tight 0 laps 0 in 2 (wrong words, of 4194304 a check)
MICRO (cycles per op) sdr1 10.0 sdr32 33.0 ... fdnMassiveApAxit 4321.0
```

- `FZ on/off (irq on/off)` — flush-to-zero, read from the FPU now: this thread's FPSCR (the corners run here) and FPDSCR (what the audio interrupt starts with: the headroom test). On by default in the profile build only; `make -C firmware MODE=profile PROFILE_FZ=0` builds `sublustris_versio_profile-nofz.bin` to compare.
- `sdram TRP n TRCD n (libDaisy|tight)` — the SDRAM's precharge and row-to-column waits in SDRAM clock cycles, read from the FMC now. libDaisy sets 16 / 10; the profile build tries **3 / 3** (the AS4C16M32MSA-6 datasheet needs 18 ns each: 2 cycles at 100 MHz) and **keeps it for the corner lap** by default. `PROFILE_SDRAM_TIGHT=0` builds `sublustris_versio_profile-sdlibdaisy.bin`, whose corners run at libDaisy's timing (its micro-tests still try 3 / 3 and then put 16 / 10 back). `sdclk` is the SDRAM clock worked out from PLL2 and the FMC divider. Release sets 3 / 3 too, at boot, since v1.0.37 (`RELEASE_SDRAM_TIGHT ?= 1` in the Makefile, `SdramSetTiming` in `main.cpp`'s release `main`); `RELEASE_SDRAM_TIGHT=0` builds `sublustris_versio-sdlibdaisy.bin` at libDaisy's 16 / 10. (Until 29 Oct 2026 this line said the release build never changes the timing: true only before v1.0.37.)
- `engine` / `axi` — where the Engine object (DTCM: 0x2000xxxx) and the allpass lines' array (AXI SRAM: 0x240xxxxx) landed.
- `SDRAMCHK` — **read this before trusting the tight timing.** 8 MB of SDRAM nobody else uses (24–32 MB) written with a pattern, read back scattered (every read opens another SDRAM row, which is what TRP / TRCD time), then the opposite pattern written scattered and read back in order: 4,194,304 words checked each time. `libdaisy` = at boot at 16 / 10 (the control), `tight` = at boot at 3 / 3, `laps` = wrong words summed over every lap so far, at the lap's timing (the module warming up: leave it running a while). **Every number should be 0.** Anything else: the tight timing is not safe on this module; don't use it.
- `MICRO` — what each building block costs, in cycles per operation (one decimal), measured once at boot. Round 6–10's entries keep their names and meaning, except **`axi32`**, which until now timed nothing (the compiler had removed its array; 3.0 was the adder's latency) and now really reads AXI SRAM. New entries:

| Name | What | Unit |
|---|---|---|
| `sub` / `norm` | a one-pole decay (3 dependent FP operations a step) on subnormal numbers (1e-38 and down) / on normal ones, with FZ as built | cycles a step |
| `subX` | `sub` with FZ flipped (so a default build shows the cost without FZ here) | cycles a step |
| `vdivDep` | 1000 divides, each needing the last (VDIV latency) | cycles a divide |
| `vdivThr` | 1000 independent divides (VDIV throughput) | cycles a divide |
| `vdivIndep` | `vdivThr` plus 4 independent multiplies each: ≈ `vdivThr` means other work hides behind a divide; ≈ `vdivThr` + 5 means a divide blocks the FPU | cycles a divide |
| `pld32` | `sdr32` with a prefetch (PLD) of each stream's next cache line | cycles a float |
| `swell12/blk` | the swell (BLOOM at MASSIVE S1, 7.5 s span, 12 taps a side, smear on), one 32-sample block | cycles a block |
| `swellNS/blk` | the same without the smear | cycles a block |
| `swellW/blk` | the same with no taps (writes only). The taps' part = `swellNS` − `swellW`; 24 taps ≈ `swell12` + that again | cycles a block |
| `fdnMassive24k` | a real-size MASSIVE tank alone (24 kHz, 1.4 s mean, in-loop diffusion, 4 early taps a line, DECAY infinite, Neve): everything in SDRAM, as release was | cycles a tank sample (= 2 output samples) |
| `fdnMassiveApAxi` | the same tank with its in-loop allpass lines in AXI SRAM, as release is now | cycles a tank sample |
| `sdr1t` `sdr32t` `sdw32t` `sdline8t` `pld32t` `fdnSDRAMt` `fdnMassive24kt` `fdnMassiveApAxit` | the same tests again at SDRAM 3 / 3 | as above |

**Profile link order (fixed in the overnight review).** Since round 10 the profile's micro-tests call the ERA stages (and now the swell) from `main.cpp`, which is built `-Os`. Inline functions from the Core's headers get one copy per object and the linker keeps the first: with `main.o` first, the Engine itself ran `main.o`'s `-Os` copies of `Swell::processBlock` (596 B against release's 1,658), `TapeOut::process` (248 against 1,766) and `RadioStage::process` / `set`. The profile now links the Core first, so it runs release's code; the `.bin` grew ~3.8 KB to 124.6 KB (95 %: the `all-variants` warning is expected for profile). **Round 10's cassette and radio numbers (`tape`, `radio` in SPLIT, `tape/smp`, `radio/smp`) were measured on the slower `-Os` code, so expect them lower now for that reason alone.** (`bbd/smp`, `loopNeve/smp` and `loopMidi/smp` still time out-of-line `-Os` copies that only the micro-tests use; the Engine inlines its own.)

**ITCM (release and profile since v1.0.37; `PROFILE_ITCM=0` builds `sublustris_versio_profile-noitcm.bin` with the code in flash).** `Engine.o`, `Fdn.o` and `Plate.o` (~52 KB of code, with the swell, tape and radio stages they use) run from ITCM (`firmware/STM32H750IB_flash_itcm.lds`, ITCM from 0x400) instead of flash through the 16 KB instruction cache, copied there first thing in `main()` (`CopyItcm`). BENCH says `itcm 52152 B` (else `itcm off`). Round 12 booted it and measured 2.5-3.5 points off every corner (lit-m7 expected 3-6).

To halve a tank-sample figure into the share of the 10,000-cycle budget: `fdnMassive24k` / 2 / 100 = % of the budget for one tank (the Engine also runs the low tank, the swell, etc.).

### Round 11 results and round 12 (26 Oct 2026)

Round 11's serial output is in `docs/research/m3-round11.log`. In short: `SDRAMCHK` 0 / 0 / 0; `sdr32` 34.0 → `sdr32t` 17.0; `fdnMassive24k` 4,640 → `fdnMassiveApAxi` 3,721 → `fdnMassiveApAxit` 3,180; `subX` = `norm` (subnormals cost nothing: FZ not needed in release); `pld32` slower than `sdr32` (no prefetching); `vdivIndep` > `vdivThr` (a divide blocks the FPU). The plain corners: PLATE 44–60 %, HALL 57–75 %, MASSIVE 61–80 % average (round 9b: 115–129 %), the cassette dearest everywhere (`tape` 16.3). The headroom test's padded PLATE ran without a late block up to 99 %. But the frozen corner came before SHIMMER, the SIZE move and the headroom test, and its twin rang on through all three (`twin` ~14 % plus a second tank's loop): SHIMMER 113 %, SIZE .3<>.8 117 % (max 174 %) and the headroom's MASSIVE 117 % are not their own.

Round 12 (the same `make -C firmware all-variants` / `PROFILE_ITCM=1` commands):
- **Release runs the SDRAM at 3 / 3** (`SdramSetTiming` at boot after `SdramWriteAllocate`, without the cache clean the profile needs: +120 B, release 112,604 B, 36 B left of the 110 KB budget).
- **The frozen corner is last**, and after it the profile runs the Engine on, untimed, until the twin stops (capped at 30 s), so the headroom test starts clean. Its CORNER line ends `| twin on N ms after`.
- **The profile's twin stopped on its 50 ms level** (the review's twin-stop fix, then a profile-only flag; since v1.0.37 the Core's default for every build).
- **Every CORNER line ends `| max at block N`**: which block, counted from the corner's start, took longest. Block 0-2 means the spike is the settings change between corners (a SIZE 0 → 1 jump starts a SIZE crossfade); in the SIZE move, the moves land at blocks 1500, 3000 and 4500 (every 1.5 s).
- ITCM build: 124,260 B, 52,632 B of code in ITCM.

**v1.0.37 (26 Oct 2026): adopted in release.** ITCM (+~350 B), the twin-stop fix and SIZE's 0.1 % hysteresis (both now the Core's defaults, for the Plugin too), and the SDRAM at 3 / 3, paid for by `Engine::queueEdge`, `Engine::onGesture` and `Drift::control` built for size (−744 B). `-fno-math-errno` and `-fmove-loop-invariants` on the Core's objects made release larger (+240 / +440 B at -O3), so they're not used. Sizes: release 112,412 B (228 B under the 110 KB budget), profile 123,756 B, m0test 83,420 B; ITCM 52,136 B (release) / 52,152 B (profile) of 63 KB.

### Round 13 and the knob deadband (26 Oct 2026)

The owner played v1.0.37: crackle in MASSIVE with SIZE and DECAY high (now and then) and constant while frozen, and no freeze tint on the LEDs (the LEDs update in the main loop, which gets no time when the audio callback takes it all). The profile never played the release's knob sends, and the rack jitter check showed the module's readings change all the time, which defeats every memo keyed on an exact value (ERA's stages, the loss filters, the drift).

- **Round 13 (profile):** after the headroom test, `REALTIME` lines: MASSIVE with the knobs where the owner heard it (SIZE and DECAY 0.9, MIX / TONE / DRIFT / TIME 0.5, BLOOM), ERA at the Neve (0.1) or the cassette (0.7), playing or tap-frozen, each run in the audio callback with the seven knob parameters sent every block, **still** and with **jitter** (±2·10⁻⁴ of noise on each, inside what the rack check found). 3 s each (frozen 6 s); late blocks are drop-outs. `still` is also what the deadband build gives. Profile 125,008 B (95 %: profile-only, the warning is expected).
- **Release, to try: a knob deadband.** A knob's value goes to the Engine only once it has moved more than 0.0005 from the last value sent, or reaches an end stop (exact 0 / 1), so a still knob is still to the Engine and the memos work. Moves come in steps of 0.05 % of the travel. Release 112,500 B (+88; 140 B left).
- **v1.0.38: ERA's tape out at 2×** (the owner's pick on `renders/tape_os`); `TapeOut::process` is one shared copy (`IV_NOINLINE`). Release 111,652 B (988 B left), profile 124,008 B; ITCM 51,296 / 51,312 B.
- **Diagnostic (26 Oct 2026; the owner heard crackle in MASSIVE at noon on v1.0.38, even at ERA 0, which round 13 didn't show):** the playing build times its own audio callback; the **rightmost LED flashes red for 150 ms after a late block** (over its 1 ms period) and amber after one over 90 %. `make RELEASE_SDRAM_TIGHT=0` builds `sublustris_versio-sdlibdaisy.bin`, the same with libDaisy's SDRAM timing, in case 3 / 3 misbehaves on rack power (round 11's `SDRAMCHK` ran on USB power). Release 111,892 B, the libDaisy-timing one 111,804 B.
- **The crackle hunt (26 Oct 2026):** the late-block light showed the crackle is the CPU; the playing build's own serial report (`RELEASE_DIAG=1`, knobs held at noon, USB power) ran MASSIVE at 63 % with no late blocks; libDaisy's SDRAM timing didn't cure it; a blue / green flash on each SIZE / TIME send showed **playing notes moves SIZE's reading past the deadband**: the audio gets into the knob readings, and each move started a SIZE crossfade in MASSIVE (up to ~110 %). PLATE glides and never crackled. Fix: each reading smoothed by a 10 ms one-pole (on top of libDaisy's 2 ms) and sent past a deadband of 0.001, or 0.003 for SIZE and TIME (whose changes crossfade). The blue / green flashes stay in for the next rack test. Release 112,236 B.
- **Round 14 (26 Oct 2026, `docs/research/m3-round14.log`):** `Fdn::lineSlow` (the SIZE crossfade's line path) built for speed instead of size cut MASSIVE's SIZE-move worst block only 106.3 → 105.0 % (S1 neve's 114.4 → 111.9 %) for +2.9 KB of flash: reverted. The crossfade's cost is its memory (every line read twice, the old early taps re-read: `tkloop` 29 and `tkout` 31 % at the peak against ~13 and ~12), not its code. The cassette's tape at 2× (v1.0.38) shows: MASSIVE cassette 77.5 → 70.6 %, frozen cassette in real time 111 → 104.5 %.
- **Round 15 (26 Oct 2026, `docs/research/m3-round15.log`):** v1.0.39's staggered SIZE crossfade works on the module: MASSIVE's SIZE .3<>.8 worst block 105.0 → 87.7 %, S1 neve's SIZE 0 → 1 jump 111.9 → 96.8 %; every other corner within 1.4 points of round 14. Still over: a frozen MASSIVE with the cassette in real time (104.8 % avg, 109.2 % max, late on 5,929 of 6,000 blocks); close: S1 neve FROZEN+twin 100.6 % (one block, at the freeze), SHIMMER 99.0 %, neve FROZEN jitter 98.0 %. Headroom unchanged (+55 % with no late blocks).
- **v1.0.39 (26 Oct 2026):** SIZE's crossfade in two groups of lines (the owner's pick on `renders/size_stagger`, ADR 0025 amendment), in the Core, so in every build. To fit: the knob-send flashes removed, the new per-line arrays without initialisers (`reset()` sets them), the waiting lines' early taps folded into the old-position sum, and the late-block light made a build option (`make RELEASE_LATE_LIGHT=1` → `sublustris_versio-latelight.bin`, 112,804 B). Release 112,540 B (100 B under the 110 KB budget), profile 124,824 B. Round 15 (the profile) measures the SIZE crossfade's peak.

```
REALTIME <setting>                          still|jitter avg  <%> max  <%> | late blocks <n> of <blocks>
```

### The SIZE-spike lap (27 Oct 2026)

The owner on v1.0.48's late light: "the size knob is still triggering some amber and red dropouts ... even when there's nothing in the reverb tank". `make -C firmware MODE=profile SIZESPIKE=1` builds `sublustris_versio_profile-sizespike.bin`: in place of the corner lap, 59 lines (~11 min a lap), each timed in the audio callback on the shipping Core, SIZE turned through release's knob path (10 ms one-pole, 0.003 deadband, end stops), ONSET BLOOM, DECAY 0.9, BLEND and DEPTH at noon, no flush-to-zero (as release). Each line resets the Engine and plays 6 s untimed first (the lines' first trip after a reset reads them as filling, 5.5 s). Groups: `SPACES` (MASSIVE and HALL: silence (exact zeros) and music in, playing and frozen, SIZE swept 0.2 <-> 1, twisted 0.5 <-> 0.9, crept 0.7 -> 1, nudged two deadband steps every 1.5 s; PLATE swept and twisted), `KNOBS` (MASSIVE, music, crept: WEAR 0 / 0.5 / 1 x MOTION noon / full left, playing and frozen; TONE's ends; twists near the top), `STILL` (SIZE still: the reference).

```
SPIKE SPACES  MASSIVE playing silence sweep   W0.0 M0.5 T0.5 avg  63.0% max  80.1% | late 0 near 0 of 6000 | max at block 2345 start 0 lines 4 tanks 2 | fading 3600 avg  72.0% max  80.1%
  SPLIT ctl  0.4 in  9.0 shim  2.1 swell  8.0 twin  0.0 tank  3.0 tkctl  2.0 tkloop 20.0 tkout 18.0 tkmix  4.0 wear  6.0 out  3.0
SIZESPIKE LAP 1 DONE: 59 lines, worst max  80.1% (SPIKE SPACES  MASSIVE ...), late blocks in all 0, near 0
```

`late` = blocks over the 1 ms period (release's red), `near` = 90-100 % (its amber). `max at block` = the worst block, counted from the line's start; `start` / `lines` / `tanks` = SIZE crossfades started in it / line-blocks reading twice / tanks crossfading (0 0 0: no crossfade was running). `fading` = the blocks a SIZE crossfade ran in, their average and worst. `SPLIT` = the worst block's sections, % of one block (the knob path and the input aren't in a section; they are in its total).

`SIZESPIKE=2` builds `sublustris_versio_profile-sizespikeab.bin`, the A / B lap (38 lines, ~8 min): every SIZE move that crossfades (MASSIVE and HALL, playing, silence and music, the four hands; twists near the top; WEAR / MOTION up) as v1.0.48 ran its crossfade (`AS-SHIP`: `Fdn::lineSlow`, `Fdn::tapsOld`) and straight after on the faster paths (`FASTER`: `Fdn::lineXf` and the carried old taps, the same output to the bit), switched by `prof::xfFast`.

Flashing and reading it: **rack power unplugged, USB only**; flash the `.bin` (Firmware Swap: Select Custom File, CONNECT, CHANGE FIRMWARE); `screen /dev/tty.usbmodem* 115200`, then `Ctrl-A`, `Shift-H` to log to `screenlog.0`; the first line (`SIZESPIKE START` / `SIZESPIKE A/B START`) comes ~3 s after boot; wait for `SIZESPIKE LAP 1 DONE` (LED 1 blue while a lap runs, then green, or LED 4 red if any block came late), then `Ctrl-A`, `Shift-H` again and `Ctrl-A`, `K`, `Y`.

### CPU save 2 (28 Oct 2026, claude/cpu-save2)

The second round against the CPU-save lap (`SIZESPIKE=4`, `docs/research/m3-cpusave-67ea6b8-lap2.log`), every change the
same output to the bit on the desktop (`tools/same_render.py` SAME 60/60 against 68a4ad7):
- **SPACE flips:** the twin room's 42 KB of D2 SRAM is no longer zeroed in the restart block (it was most of SPLIT's
  `flip` ~23 %, and its evictions slowed the tank's reads after it); it is zeroed a slice a chunk while the room rests,
  the rest at its first block (a freeze). HALL's and MASSIVE's early-tap layouts are worked out once.
- **Per sample:** ERA's wear runs over the chunk with both sides side by side; MOTION's transport over the chunk with
  its state in registers; the swell reads two taps a pass, one load a read; the shimmer heads issue their divide
  before the reads; TONE's bell divides only while it moves.
- **Per block:** control maths kept while their arguments hold (`Engine::blockK`, the knobs' smoothing, TONE's,
  ERA's texture constants, MOTION's depths, the swell's glide); the loss filters' shared half once a block.
- **Flash:** `LedPwm` writes its DMA streams' registers itself (HAL_DMA_Start and HAL_DMA_Abort no longer link, −1.8 KB).
- The lap ends with three `ABSEXP` lines: the worst MASSIVE lines again with the loss filters on `std::exp` instead of
  `std::pow` (`prof::absExp`; the build option `IV_ABSORB_EXP`): not the same bits (desktop: ~1e-6 apart, up to ~1e-3
  where WEAR's grain rounds the other way), a trial. Compare each with its `SHIMMER` line.

### The audit lap (SIZESPIKE=5; review 2, 28-29 Oct 2026)

`make -C firmware MODE=profile SIZESPIKE=5` → `sublustris_versio_profile-audit.bin` (LEAN is the default since 29 Oct 2026; `LEAN=0` → `-nolean-audit`). Release builds take the SAI's DMA straight to the callback (AUDIO_DIRECT, the default since 30 Oct 2026: −3,240 B, the same samples, the owner's rack check clean); `AUDIO_DIRECT=0` → `sublustris_versio-libaudio.bin` through libDaisy's AudioHandle. Profile and m0test always use the AudioHandle. Release also refreshes the SDRAM at the datasheet's rate (SDRAM_REFRESH, the default since v1.0.63: +16 B, the same samples); `SDRAM_REFRESH=0` → `sublustris_versio-norefresh.bin`.
At boot, before `AUDIT START`: `HASH xxxxxxxx (3000 blocks, ...) | refresh COUNT n`: a CRC-32 of a fixed 3,000-block
render (MASSIVE, SHIMMER, WEAR 0.5, MOTION left, DECAY 0.9, a SIZE jump), so two builds that print the same HASH give
the same output to the bit on the module; `refresh COUNT` is the SDRAM refresh libDaisy left (2054). Then 16
`SPIKE` lines (the format above): `UNFRZ` x 5, `PAINT`, `CVALL` x 2, `GRIT` x 2 (the audit's untimed combinations), then
MASSIVE's SHIMMER sweep (WEAR 0.5, MOTION left, DECAY 0.9, music) as built (`SHIMMER`), with the loss filters on
`std::exp` (`ABSEXP`), with the shimmer window as a polynomial (`SHPOLY`), with both (`ABS+SHP`), the creep unfreeze
with both (`UNFRZ+AS`), and as built with the SDRAM refreshed at the datasheet's rate (`REFRESH`, COUNT 761). The
`DONE` line adds `| STACK n B of 60176` (the deepest the stack has reached since boot) and the HASH again.

Release map, v1.0.55 with review2/neutral's B1-B4 (29 Oct 2026; `arm-none-eabi-size -A` / `nm -S` on `build/sublustris_versio.elf`):

| Region | Used | What |
|---|---:|---|
| DTCM 0x20000000 (128 KB, zero-wait, never cached) | 70,896 B `.dtcmram_bss` | `gFast` 36,864 B (the input diffusers), the Engine object 34,032 B (`gEngineStorage`, placement-new at boot). **The stack is here too**: it grows down from `_estack` 0x20020000 into the 60,176 B above `.dtcmram_bss` (libDaisy's linker script, not AXI SRAM as this section once said). The review 2 audit's estimate of the deepest path (audio interrupt over the main loop): ~6.6 KB; the `SIZESPIKE=5` lap's `STACK` field measures it on the module. |
| AXI SRAM 0x24000000 (512 KB, cached) | 502,752 B (`.data` 232 + `.bss` 502,520) | `gAxi` 493,584 B: the tank's and the low tank's in-loop allpass lines, SHIMMER's right ring, staggered across the cache's sets; libDaisy's handles. |
| D2 SRAM 0x30000000 (288 KB) | 26,944 B uncached (`.sram1_bss`: DMA buffers, LED PWM tables) + 213,256 B `.d2_bss` | `gD2` 204,340 B: SHIMMER's left ring, the twin room. |
| ITCM 0x400 (63 KB) | 56,464 B | the Core's hot code (Engine.o, Fdn.o, Plate.o), copied from flash at boot. |
| SDRAM 0xC0000000 (64 MB, 100 MHz, 3 / 3 in release) | 19,952,232 B `gLarge` | FDN lines, plates, swells, predelay. Release refreshes it at the datasheet's rate since v1.0.63 (FMC_SDRTR COUNT 761: all 8192 rows every 64 ms), not libDaisy's (COUNT 2054: a row every 20.6 us, the whole chip every 168 ms); `SDRAM_REFRESH=0` builds release at libDaisy's rate (`-norefresh`); profile and m0test keep libDaisy's. |
| Heap | 0 | `IV_NO_HEAP`: no malloc linked. |

## Carried over from Resilio's M0 check (re-checked in Immensio's)

- `kPotKnob`: pots P1..P7 are libDaisy knobs 0, 4, 2, 1, 5, 3, 6 (not in reading order).
- Both switches read left (`Switch3::POS_LEFT`) pointing left, so neither is inverted.
- `kOutputTrim` −0.874: the module's analog path is polarity-inverted and 1.17 dB hot; the release build undoes both so MIX 0 matches a cable.
- `kMeterLed` assumes LED_0..LED_3 run left to right.
