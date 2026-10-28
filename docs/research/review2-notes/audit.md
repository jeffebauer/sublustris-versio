# Review 2: CPU, flash and RAM on the module (audit)

28-29 Oct 2026, branch `review2/audit` (worktree `.claude/worktrees/r2-audit`, off `review/overnight-2` at d1db5e7, v1.0.55).
No hardware: every CPU figure here is either read from the existing laps (measured, on the module) or estimated from
them and from the code. Sizes are measured (this worktree's `make -C firmware all-variants`, GCC 15.3.1).
Tags: **[measured]**, **[estimated]**, **[literature]**.

Tools written for this audit (all under `tools/review2/`):
- `map_by_object.py <map>`: flash bytes per archive / object / input section from the ld map.
- `fn_diff.py a.elf b.elf`: per-function size differences.
- `flag_trial.sh <fw.log> <dir> "<flags>"`: recompile the release Core objects with extra flags and relink (exactly
  112,564 B with no flags).
- `stack_depth.py <root> *.ci`: worst call path's stack from GCC's `-fstack-usage -fcallgraph-info=su`.

Defaults are untouched: the release, m0test, profile and `SIZESPIKE=4` `.bin` files built on this branch are
byte-identical to v1.0.55's (cmp; the cpusave lap against `dist/sublustris_versio-v1.0.55-3eb5e1c-profile-cpusave.bin`).
Desktop: "100% tests passed out of 21"; `same_render` against the pinned ref-main renderer: **SAME 60/60**.

---

## 1. Flash

### 1.1 Where the 112,564 B go [measured, release map]

| Part | Bytes | |
|---|---:|---|
| Core in ITCM (Engine.o 36,958 incl. rodata, Fdn.o 15,214, Plate.o 5,342; loaded from flash) | 56,408 | `.itcm_text`, 8,104 B of ITCM left |
| libDaisy + HAL | 43,597 | RCCEx 5,924, DMA 5,260, audio.o 3,764, RCC 2,900, ADC 2,848, I2C 2,516 + 1,656, SAI 2,136 + 2,064 ... |
| main.o (-Os) | 5,703 | `main` 2,208, `Engine()` 892, `AudioCallback` 752, `Plate()` 404, `Fdn()` 280 |
| libm | 4,240 | powf 792 + tables, sinf 508, cosf 508, sincosf table 320, logf 216 + 288, log10f 188 + 96, expf 228, exp2f 212, exp2 table 328, sqrtf wrapper 80, math_errf 160 |
| startup, vectors, libc (memset/memcpy/errno), crtbegin, veneers | ~1,550 | 16 long-branch veneers (flash <-> ITCM) = 128 B |

- **libgcc pulls in nothing** now: the one helper, `__aeabi_l2d` (40 B), is the firmware's own VFP version
  (`no_uart_spi.cpp`), called by newlib's sinf / cosf range reduction.
- libm call sites (via veneers, all from ITCM): expf 47, powf 23, sinf 15, log10f 14, sqrtf 12, cosf 10, logf 2, exp2f 1.
  Callers: powf / sinf / expf / sqrtf / log10f from Engine.o, Fdn.o, Plate.o (control rate); cosf, expf, log10f also
  from main.o (LED meter); exp2f only `EraTexture::set` (the grain's step, `EraTexture.h:135`).
- **`queueEdge`**: 98 B + an 8 B veneer (it is already `IV_SIZE_OPT`): nothing left there. The "~1-2 KB in queueEdge and
  the constructors" (HANDOFF) is really the **constructors: 1,712 B** (`Engine()` 892, `Plate()` 404, `Fdn()` 280,
  `DaisyVersio()` 136) plus 92 B of static constructor for `gLedPwm`. `Engine()` is ~120 stores of non-zero defaults,
  16 calls (Fdn x2, Plate x2, Swell x2, Diffuser x4, HalfRate x2, memset x4).
- `vmla.f32` appears only in the `-Os` functions (TwinRoom::process 19, Fdn::lineXf 14, lineSlow 10, applySpace 31,
  Transport::control 34 ...): GCC uses the VFP's chained multiply-accumulate when optimising for size. VMLA rounds
  the product before the add (not fused), so the output is the same as vmul + vadd; no `vfma` anywhere in our code
  (only inside newlib's sinf / cosf / powf / logf / expf). Fine, just noting it is not a `-ffp-contract` leak.

### 1.2 Trims, measured on this branch (all off by default)

`make -C firmware LEAN=1` and/or `AUDIO_DIRECT=1` (Makefile trial switches, own `.bin` names `-lean`, `-direct`):

| Trim | Bytes | Sound | How proven | CPU | Risk |
|---|---:|---|---|---|---|
| **AUDIO_DIRECT**: the SAI's DMA straight to our callback (`SaiHandle::StartDma` on `hw.seed.AudioSaiHandle()`), dropping libDaisy's `AudioHandle::Impl::InternalCallback` (3,500 B: every bit depth, interleaved, 4-channel) | **-3,240** | none | same conversion code (`s242f` / `f2s24` from daisy_core.h, the AudioHandle config's postgain and output_compensation), same buffer layout; not provable on the desktop | ~0 (same work; maybe -0.1 %) | needs one rack boot: audio in, out, both channels, levels (low risk; it is libDaisy's own SAI path, 1.5 KB of new DMA buffers in the uncached D2 window, 28,480 of 32,768 B) |
| LEAN: `Engine::process` without loop unswitching (the tank-input loop was copied for ERA's hiss / crackle test, `Engine.cpp:1033`) | -540 | none | control flow only (same FP ops, same order) | + one predictable branch a sample: ~0.01-0.02 % [estimated] | low |
| LEAN: `Fdn::runBlock` without loop splitting (`s < q` in the early taps' last loop, `s >= lag` in the filling taps, `Fdn.cpp:732, 787`) | -474 | none | as above | + a compare a sample a tank: ~0.02 % [estimated] | low |
| LEAN: `-Wl,--wrap=HAL_I2C_Slave_Transmit` to a stub (`I2CHandle::TransmitBlocking`'s slave branch; the only I2C use is the Seed 1.1 codec's set-up, as master) | -1,014 (with its only caller of `I2C_WaitOnTXISFlagUntilTimeout`) | none | code never reached | 0 | low |
| LEAN: `sqrtf` as one `vsqrt.f32` (newlib's is an errno wrapper around the same instruction) | -80 | none | VSQRT is IEEE correctly rounded: same bits for every x >= 0 | faster (control rate) | none |
| LEAN: `LedPwm`'s member initialisers dropped (static storage, zeroed by startup; `start()` sets each port before reading it) | -48 | none (LEDs only) | static zero-init | 0 | none |
| **LEAN total** | **-2,144** | | release **110,420 B** | | |
| **LEAN + AUDIO_DIRECT** | **-5,384** | | release **107,180 B** (5,460 B under the 112,640 budget) | | |

The LEAN Core changes are macros that expand to nothing outside the firmware (`SizeOpt.h` `IV_LEAN_PROCESS`,
`IV_LEAN_RUNBLOCK`), so the desktop is unchanged (SAME 60/60). On the module, "same output" rests on the argument that
unswitching and splitting are control-flow transforms; **the audit lap's HASH line (section 3.4) turns it into a cmp
on the module**: build `MODE=profile SIZESPIKE=5` with and without `LEAN=1`, the two `HASH` lines must match.

Caveat found on the way [measured]: the profile build's hot loops are not shaped like release's. Its `prof::mark`
calls change GCC's loop decisions: `Engine::process` is 7,252 B in the cpusave profile, 7,288 in release, and LEAN
shrinks it by only 34 B in the profile (540 in release). So a lap can't time LEAN's `Engine::process` change; the
late-light release build is the only check. The estimate (one predictable branch a sample) says it doesn't matter.

### 1.3 Trims measured but not adopted, and ones not built

| Candidate | Bytes | Sound | Note |
|---|---:|---|---|
| `-fno-ipa-sra` on the Core | -112 [measured] | none | CPU unknown (argument passing changes) |
| `-fno-tree-loop-vectorize -fno-tree-slp-vectorize` | -84 [measured] | none | `processTwoTanks`, `Plate::process`: SLP pairs loads/stores; may cost speed |
| `-fno-tree-partial-pre` | -48 [measured] | none | CPU unknown |
| `LoopEra::run` without unswitching | -144 [measured] | none | the branch lands in the tank loop, every line every sample: ~0.15 % [estimated]; not worth it |
| libm into ITCM | -16 [measured], ITCM +2,968 | none | not worth it (new veneers for errno and `__aeabi_l2d`) |
| `log10f` -> `logf(x) * (1 / ln 10)` (14 sites) | ~-240 [estimated] | tiny: not bit-identical (dB levels for the governor, ducking, LEDs) | |
| `exp2f` -> `expf(x ln 2)` (the grain step) | ~-212 [estimated] | tiny: not bit-identical | |
| Constructors: non-zero defaults that `reset()` / `prepare()` set again, made `IV_ZI`-like | -300..-700 [estimated] | none if complete | medium risk: a member read before reset only shows on the module; provable only by the HASH line, since desktop builds keep the initialisers |

### 1.4 The flags libDaisy passes

The Core objects compile with libDaisy's `CPPFLAGS`, including **`-fno-move-loop-invariants`** (seen in every compile
line). That turns off GCC's RTL loop-invariant motion for our per-sample loops (tree-level LICM still runs). v1.0.37
tried turning it back on for flash (+440 B then; **+272 B now** [measured]: `Engine::process` +104, `Fdn::runBlock` +28,
`Swell::processBlock` +24, `Plate::process` -32 ...) but nobody timed it. It is sound-neutral by construction (no FP
reassociation); worth one A / B lap if LEAN frees the bytes.

---

## 2. RAM [measured, release map / `size -A`]

| Region | Size | Used | Free | What |
|---|---:|---:|---:|---|
| DTCM 0x20000000 | 131,072 | 70,896 `.dtcmram_bss` | **60,176** for the stack (top `_estack` 0x20020000) | `gFast` 36,864 (diffusers), the Engine object **34,032** (not 24,232 as `main.cpp:223` says) |
| AXI SRAM 0x24000000 | 524,288 | 502,776 (`.data` 232 + `.bss` 502,520 + alignment) | 21,512 | `gAxi` 493,584 (tank + low-tank allpasses 294,912, SHIMMER right ring 153,840, stagger), libDaisy handles ~8.7 KB (incl. 2 KB of USB handle structs never used in release) |
| D2 SRAM1-3 0x30000000 | 294,912 | 26,944 uncached window (`.sram1_bss`) + 213,256 `.d2_bss` from 0x30008000 | 5,824 in the uncached 32 KB, **48,888** above `.d2_bss` | LED PWM tables 10,240; libDaisy audio DMA buffers 16,384 (uses 1,536 of them); `gD2` 204,340 (SHIMMER left ring 153,840, twin room 42,372, stagger); prime sieve 8,916 |
| D3 SRAM4 0x38000000 | 65,536 | 0 | **65,536** | unused |
| Backup SRAM | 4,096 | 12 | | libDaisy |
| ITCM 0x400 | 64,512 | 56,408 | 8,104 (9,416 with LEAN) | Engine.o, Fdn.o, Plate.o code |
| SDRAM | 64 MB | 19,952,232 (`gLarge`) | ~45 MB | FDN lines, plates, swells, predelay |
| Heap | 0 | | | `IV_NO_HEAP`; no malloc linked |

### Stack [estimated from `-fstack-usage` / callgraph, release objects recompiled bit-identically]

Deepest path from the audio callback: `AudioCallback` 56 -> `Engine::process` 288 -> `processTwoTanks` 232 ->
`Fdn::process` 56 -> **`Fdn::runBlock` 4,368** (its `y`, `in`, `e` block arrays) -> `lineXf` 184 -> lambda 20 =
**5,204 B**. Add libDaisy's `InternalCallback` (80 B frame + two 96-float VLAs = ~850 B), the DMA IRQ and HAL frames
(~150 B), the FPU exception frame (104 B) and the main loop under it (~300-500 B, deepest at boot: `Engine::prepare`
520 B before audio starts): **~6.6 KB worst case** against 60,176 B. About 53 KB of DTCM is unused. (Not measured:
the audit lap's `STACK` field paints the gap at boot and reports the high-water mark each lap.)

Stale docs: `main.cpp:223-224` (Engine 24,232 B, "~68 KB" for the stack: now 34,032 and 60,176) and
firmware/README.md's "Memory" section ("libDaisy's linker script already puts the stack in AXI SRAM": it is in DTCM).

---

## 3. CPU

### 3.1 Where the time goes now [measured: `docs/research/m3-cpusave2-7ee389e-full.log`, v1.0.54 code]

Worst block's sections, % of the 1 ms block, SHIMMER, WEAR 0.5, MOTION full left, DECAY 0.9, music, SIZE still:

| SPACE | avg / max | biggest sections |
|---|---|---|
| PLATE | 50.3 / 51.4 | shim 14.7, tank 13.5, in 6.2, out 4.6, wear 4.4, ctl 3.7 |
| HALL | 60.3 / 61.2 | shim 14.6, tkloop 7.9, tkout 5.6, in 5.6, tkctl 4.9, wear 4.4, tkabs 3.4 |
| MASSIVE | 78.6 / 79.7 | shim 14.7, tkloop 10.3, swell 10.2, tkout 10.0, in 6.9, tkctl 5.3, wear 4.4, tkabs 3.5 |

With SIZE moving (the crossfade reads every line twice): MASSIVE sweep max 94.8-95.3 %, twists avg 89.6 % with
4,042 of 6,000 blocks "near" (90-100 %); HALL sweep 85.1 % (all 32 line-blocks fading at once). SHIMMER is the dearest
add-on everywhere (shim 14.7 against 1.8 at BLOOM); MOTION's tape side +2.1 (out 4.7 vs 2.6); WEAR 0.5 4.4. SPACE flips
no longer spike (worst 80.3 %, at 3.4 s: MASSIVE's own steady state). Late blocks: 0 in all six laps.

Release against the profile's numbers [estimated]: the profile times from its callback's entry, so libDaisy's
conversion (~1.5 k cycles a block) and release's `ProcessAllControls` / meters (~0.5 k) are outside its figure (+~0.4
points), while its marks add ~1 % inside it: release is the profile **within about +-1 point**. The late light uses
the same callback-internal timing, so a "near" block at 99.6 % may already be late.

### 3.2 What a late block sounds like (firmware/main.cpp, libDaisy sai.cpp / audio.cpp)

The SAI's RX and TX DMA run circular double buffers of 2 x 48 frames on their own; the RX half / full interrupts call
libDaisy's `InternalCallback`, which reads the input half at its start and writes the output half at its very end
(~2 us for 96 words). If a block takes 1 + f periods, the TX DMA starts playing that half at the deadline while it
still holds the samples written two blocks earlier; the late write lands f ms in. So **the first f x 48 samples are a
replay of 2 ms old output, then a splice into the right audio: a click per late block, a crackle at up to 1 kHz while
the overload lasts**. The Engine itself sees every input sample (the input half is read in time unless a block runs
over two periods), so the reverb's state stays continuous: the glitch is only in what reaches the DAC. Past two
periods the interrupts' pending flag merges two events into one: a whole input block is skipped (a 1 ms jump in what
the tank hears) and a whole output block replays. The main loop gets no time meanwhile, so the LEDs freeze (as the
owner saw at round 13). AUDIO_DIRECT behaves the same.

### 3.3 Combinations no lap has timed [estimated from the laps]

Inputs: SHIMMER MASSIVE W0.5 M0 still 78.6 / 79.7; its SIZE crossfade +10..15 (fading avg 90.9-91.4, sweep max 94.8);
the twin (the small room, its diffusers and half-rate converters) +5..8 while it runs (candidate lap, BLOOM, room
frozen vs playing: +8.0 avg, +2..4 on the worst block; code count: room ~250 cycles a sample at -Os, diffusers ~60,
half-band up / down ~110 = ~4-5 %).

| Combination | Why it is plausible | Estimate (worst block) |
|---|---|---|
| **Unfreeze after SIZE moved while frozen**, MASSIVE, SHIMMER, WEAR 0.5, MOTION left, music | while frozen the tank keeps `heldSize_` (`Engine.cpp:415`), so the unfreeze lands the whole SIZE move as one crossfade; the twin keeps running after the unfreeze until its input has closed and its 50 ms level is under -100 dB (`Engine.cpp:1078`), seconds at DECAY 0.9. Every SIZE move in those seconds crossfades with the twin on | **96-103 %: the most likely late blocks left** |
| the same in HALL (sweep) | HALL fades all 32 line-blocks at once | 90-93 % |
| Frozen + PAINT (held), MASSIVE SHIMMER W0.5 M0 | no SIZE crossfade while frozen; paint adds 2 multiply-adds a sample | 84-88 % |
| Every knob moving (CV on several inputs), MASSIVE SHIMMER, SIZE creeping | each moving knob misses its control memo every chunk (EraTexture::set's pow / exp, TONE's bell divides "only while it moves", the swell's glide): +1..2 points of `ctl` | 91-96 % |
| WEAR past 0.65 (the BBD clock and the grain) | +2 x (cvt, cvt, mul, mul, abs, add) a sample | +0.3..0.6 |
| DEPTH (TIME) moved during a SIZE crossfade in BLOOM / MASSIVE | the swell's TIME crossfade fades one tap at a time (one extra tap read a side, the smear's old-length reads, `Swell::addRamped` at -Os) | +1..2 |
| SPACE switched while frozen, then unfreeze | the flip waits for the 1 s unfreeze glide: that second is the first row, then a normal flip | as the first row for 1 s |

### 3.4 The lap to time them (built here only to check it compiles: `MODE=profile SIZESPIKE=5`, 120,016 B)

On this branch, `firmware/main.cpp` / `Makefile` (`SIZESPIKE=5` -> `sublustris_versio_profile-audit.bin`, ~3.5 min a
lap, the cpusave lap's machinery): 10 lines, SHIMMER, WEAR 0.5, MOTION left, DECAY 0.9, music unless named:
- `UNFRZ` x 5: frozen at SIZE 0.3; the hand moves under the freeze; a tap unfreezes at block 200 (`gUnfreezeAt`):
  MASSIVE still (one jump to 0.7), creep, twists; HALL sweep; MASSIVE BLOOM WEAR 1 creep.
- `PAINT`: MASSIVE frozen, button held from block 200 (`gPaintFrom`), released after the line.
- `CVALL` x 2: SIZE's hand plus MIX, DECAY, TONE, WEAR, MOTION, DEPTH each +-0.04 at 0.7-2.3 Hz through release's
  10 ms one-pole and deadbands (`gCvAll`): MASSIVE creep, HALL sweep.
- `GRIT` x 2: WEAR 1, MASSIVE creep and still.
- `HASH` once at boot: CRC-32 of 3,000 blocks from a reset at fixed settings (a SIZE jump at block 1000), the proof
  that two builds give the same output on the module (LEAN=1 against default, or any later trim).
- ` | STACK n B of 60176` on each DONE line: the deepest the stack has reached since boot (DTCM gap painted with
  0xA5A5A5A5 first thing in `main`).

The orchestrator / owner flashes it; `LEAN=1 SIZESPIKE=5` builds the LEAN twin (`-lean-audit`, 119,552 B) for the
HASH comparison.

### 3.5 If UNFRZ comes late: options [estimated]

| Option | CPU | Flash | Sound | Needs |
|---|---|---|---|---|
| `TwinRoom::process` at -O3 (it is `IV_SIZE_OPT` although it runs every sample: `TwinRoom.h:106` chose -Os for 1.4 KB) | -0.5..-1.5 in freeze / after-freeze cases | +1.4 KB (from LEAN's headroom) | none | a lap |
| The twin room's 42,372 B in DTCM instead of D2 (zero-wait, out of the 16 KB D-cache the tank's SDRAM lines need) | -0.5..-1.5 same cases | ~+60-100 B (its own arena) | none | the STACK line first: leaves ~17.8 KB of stack for an estimated 6.6 KB |
| `-fmove-loop-invariants` back on for the Core | 0..-2 everywhere (unknown) | +272 B | none | an A / B lap |
| `IV_ABSORB_EXP` (the cpu agent's) | -1.5..-4 on SIZE moves (measured) | ? | not bit-identical (~1e-6) | owner |
| Hold the unfreeze's SIZE jump until the twin has rested, or spread it over more groups | removes the overlap | small | audible (timing of the jump) | owner's ears |

---

## 4. Established techniques for the rest [literature + this project's own measurements]

Already in use: ITCM for the Core (+2.5-3.5 points, round 12), DTCM for the Engine and diffusers, SDRAM write-back
write-allocate (ST AN4839) and 3 / 3 timing (round 11: -25 % a tank sample), allpasses and SHIMMER rings on chip, no
fused multiply-add (Resilio M3), block-wise line processing, staggered crossfades, cache-set stagger, flush-to-zero
found unnecessary (`subX` = `norm`).

| Technique | Fit here | Estimate | Risk |
|---|---|---|---|
| **MDMA prefetch of the tank's per-block read windows** (SDRAM -> DTCM, double-buffered: each line's block of reads is one contiguous run while SIZE is still) | the tank is memory-bound (round 14: the crossfade's cost is its reads, `tkloop` + `tkout` 20-27 % at MASSIVE's peaks); the M7's D-cache doesn't prefetch, and PLD measured slower (round 11 `pld32` > `sdr32`) | -5..-10 points in MASSIVE crossfades | high: a new data path, coherency (DTCM isn't cached: fine), 1.5-3 KB flash; not v1.0 |
| CMSIS-DSP kernels | f32 on the M7 is the same scalar FPU (no SIMD for float); the hot code is feedback (FDN, allpasses), and CMSIS's biquads use a different operation order | ~0 | not bit-identical; flash |
| `-funroll-loops` on compute-bound loops (`Plate::process`, `TwinRoom`) | dual-issue helps compute-bound code; the tanks are memory-bound | small | flash (KBs) |
| More code in ITCM (libm) | measured: -16 B flash, +2,968 B ITCM; control-rate only | ~0 | none |
| Larger blocks (96) | halves per-block costs (`ctl` 4-5 %, `tkctl` 5 %); `PROFILE_BLOCK=96` exists | -3..-5 points | +1 ms latency, control timing changes (not bit-identical) |
| D3 SRAM4 (64 KB, empty) | on the D3 AHB, slower than D2 from the M7; useful only to free D2 / AXI | - | - |

## 5. Not checked

- Nothing ran on the module: LEAN's and AUDIO_DIRECT's CPU, AUDIO_DIRECT's boot, the HASH equality, the real stack
  depth, every section 3.3 estimate.
- The twin's current cost is inferred from the pre-CPU-save candidate lap and a code count, not timed on v1.0.54+.
- Other Seed revisions (1.1 WM8731, 1.2 / 2 DFM PCM3060, Seed 3): AUDIO_DIRECT reads `output_compensation` from the
  AudioHandle config as libDaisy does, so it follows whatever DaisySeed set; not tested on any of them.
