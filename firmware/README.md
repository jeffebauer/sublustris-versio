# Firmware — build guide

The Versio firmware: a standard libDaisy app in the Seed's internal flash, installed with Noise Engineering's Firmware Swap (SPEC §5.3, §8). One `main.cpp`, three variants. Seeded from Resilio Versio's firmware (its variants, flash measures and LED PWM); everything Resilio-specific is gone.

| Variant | Command | Output | What it is |
|---|---|---|---|
| **release** | `make -C firmware` (MODE defaults to `release`) | `build/immensio_versio.bin` | The instrument: P1–P7 MIX, DECAY, TONE, ERA, SIZE, DRIFT, TIME (+CV); SW0 SPACE, SW1 ATTACK; button and gate edges to the Engine (it reads tap / hold itself). LEDs: In L / In R / Out L / Out R level meters, green → amber with level, red on input clip or when the output limiter pulls down; while frozen the output pair shows cool blue under the meter, pulsing slowly while painting. LED dimming is DMA-driven (`LedPwm` in `main.cpp`, TIM5 + DMA, 512 steps at ~1 kHz), not libDaisy's flickery software PWM. No serial printing (flash). **At M0 the Engine is a passthrough**, so the release build is a calibrated cable with meters. |
| **m0test** | `make -C firmware MODE=m0test` | `build/immensio_versio_m0test.bin` | The M0 hardware check (`docs/m0-hardware-check.md`): dry passthrough (no trim), every control → LEDs and USB serial, in panel order. Doesn't link the Core. |
| **profile** | `make -C firmware MODE=profile` | `build/immensio_versio_profile.bin` | M3 CPU profiling: ignores the controls, cycles 24 setting corners (SPACE × DECAY × SIZE × ERA) on a synthetic click + noise signal, prints CPU load per corner and per section over USB serial. Until M1 it measures the callback's own overhead only. |

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

Memory since the overnight review (26 Oct 2026, release map): the Engine object (24,232 B) in DTCM with `gFast` (36,864 B): 61,096 B of 128 KB from 0x20000000, the stack grows down from 0x20020000 (~68 KB for it). `gAxi` (433,984 B, `Engine::axiFloats`) in AXI SRAM's `.bss`: the in-loop allpass lines of the tank and the low tank (16 × 16 KB + 16 × 8 KB, staggered across the cache's sets); AXI SRAM 452,024 B used of 512 KB. The twin's allpasses stay in SDRAM (all three tanks' don't fit). `gLarge` 24.9 MB in SDRAM.

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

- **M0 hardware check:** `build/immensio_versio_m0test.bin`.
- **CPU profiling (M3):** `build/immensio_versio_profile.bin`.
- **Playing it:** `build/immensio_versio.bin`.

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

- `FZ on/off (irq on/off)` — flush-to-zero, read from the FPU now: this thread's FPSCR (the corners run here) and FPDSCR (what the audio interrupt starts with: the headroom test). On by default in the profile build only; `make -C firmware MODE=profile PROFILE_FZ=0` builds `immensio_versio_profile-nofz.bin` to compare.
- `sdram TRP n TRCD n (libDaisy|tight)` — the SDRAM's precharge and row-to-column waits in SDRAM clock cycles, read from the FMC now. libDaisy sets 16 / 10; the profile build tries **3 / 3** (the AS4C16M32MSA-6 datasheet needs 18 ns each: 2 cycles at 100 MHz) and **keeps it for the corner lap** by default. `PROFILE_SDRAM_TIGHT=0` builds `immensio_versio_profile-sdlibdaisy.bin`, whose corners run at libDaisy's timing (its micro-tests still try 3 / 3 and then put 16 / 10 back). `sdclk` is the SDRAM clock worked out from PLL2 and the FMC divider. **The release build never changes the timing.**
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

**ITCM (release and profile since v1.0.37; `PROFILE_ITCM=0` builds `immensio_versio_profile-noitcm.bin` with the code in flash).** `Engine.o`, `Fdn.o` and `Plate.o` (~52 KB of code, with the swell, tape and radio stages they use) run from ITCM (`firmware/STM32H750IB_flash_itcm.lds`, ITCM from 0x400) instead of flash through the 16 KB instruction cache, copied there first thing in `main()` (`CopyItcm`). BENCH says `itcm 52152 B` (else `itcm off`). Round 12 booted it and measured 2.5-3.5 points off every corner (lit-m7 expected 3-6).

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

## Memory

Nothing yet (M0 passthrough). The plan (SPEC §5.2): the FDN lines, swell stage, predelay and REVERSE segments in the 64 MB SDRAM; the input diffusers and the companion plate in DTCM (`IV_DTCM` in `main.cpp`: zero-wait, NOLOAD, so cleared by whoever uses it); the Plate engine in AXI SRAM. libDaisy's linker script already puts the stack in AXI SRAM, leaving DTCM free (Resilio measured this).

## Carried over from Resilio's M0 check (re-checked in Immensio's)

- `kPotKnob`: pots P1..P7 are libDaisy knobs 0, 4, 2, 1, 5, 3, 6 (not in reading order).
- Both switches read left (`Switch3::POS_LEFT`) pointing left, so neither is inverted.
- `kOutputTrim` −0.874: the module's analog path is polarity-inverted and 1.17 dB hot; the release build undoes both so MIX 0 matches a cable.
- `kMeterLed` assumes LED_0..LED_3 run left to right.
