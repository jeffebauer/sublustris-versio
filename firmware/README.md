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

Memory at M1: `gLarge` 8.5 MB in SDRAM (FDN lines + predelay, power-of-two lines sized for MASSIVE), `gFast` 16 KB in DTCM (input diffusers), the Engine object ~1.3 KB in AXI SRAM.

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
- `SPLIT` / `PEAK` — each section's share (`dsp/ProfileHook.h`: ctl, in, swell, tank, comp, era, out), averaged and in the worst single block. Zero until the Engine marks its sections (M1 on).
- `BENCH` (once per lap) — clock speed and whether the caches are on.

On the module: **LED_0** brightens through the 24 corners; **LED_3 turns red** for good once any corner's `max` passes 75 %.

## Memory

Nothing yet (M0 passthrough). The plan (SPEC §5.2): the FDN lines, swell stage, predelay and REVERSE segments in the 64 MB SDRAM; the input diffusers and the companion plate in DTCM (`IV_DTCM` in `main.cpp`: zero-wait, NOLOAD, so cleared by whoever uses it); the Plate engine in AXI SRAM. libDaisy's linker script already puts the stack in AXI SRAM, leaving DTCM free (Resilio measured this).

## Carried over from Resilio's M0 check (re-checked in Immensio's)

- `kPotKnob`: pots P1..P7 are libDaisy knobs 0, 4, 2, 1, 5, 3, 6 (not in reading order).
- Both switches read left (`Switch3::POS_LEFT`) pointing left, so neither is inverted.
- `kOutputTrim` −0.874: the module's analog path is polarity-inverted and 1.17 dB hot; the release build undoes both so MIX 0 matches a cable.
- `kMeterLed` assumes LED_0..LED_3 run left to right.
