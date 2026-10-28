# Review 2: the sound-neutral flash and CPU package, and the one profile build (review2/neutral, 29 Oct 2026)

The owner's goal: "find room in the flash and a few points of CPU without changing the sound, and one firmware I can
flash in the morning that tells us whether MASSIVE fits and that the savings really are the same sound on the module."

Branch `review2/neutral` (worktree `.claude/worktrees/r2-neutral`), off `review2/cpu` (B1-B4, `ABSORB_EXP=1`,
`SHIMMER_POLY=1`) with `review2/audit` merged in (`LEAN=1`, `AUDIO_DIRECT=1`, the `SIZESPIKE=5` audit lap). Nothing here
ran on the module. Tags: **[measured]** (this worktree, GCC 15.3.1 / Apple clang), **[estimated]**, **[module]** (an
earlier lap's log).

## 1. What is on the branch

- **The merge.** One conflict (firmware/Makefile's trial-switch block and `TRIAL_TAG` / `TRIAL_DEFS`): both kept. While
  there, `LEAN` and `AUDIO_DIRECT` now need `=1` like the cpu agent's switches (before, `LEAN=0` turned LEAN *on*:
  the Makefile tested "not empty").
- **The audit lap times the CPU trials** (section 4): six lines after the audit's ten. The shimmer A / B
  (`IV_PROFILE_SHPOLY_AB`, defined only by `SIZESPIKE=5` without `SHIMMER_POLY=1`) chooses per sample between two
  copies of the head loop (`Shimmer::headsAB<bool>`), so neither side pays the other's branch per head. With both
  copies GCC stopped inlining `Shimmer::process` into `Engine::process` (a call per sample the shipped code doesn't
  make), so in that build only it is `IV_INLINE` (checked: no out-of-line `Shimmer::process`, the polynomial's
  constant present in `Engine::process`, 6 `vdiv` as in the default profile).
- **`SDRAM_REFRESH=1`** (release or profile, `-refresh`): the SDRAM refreshed at the datasheet's rate (section 6).
- **Docs:** firmware/README.md's "The release build never changes the timing" (wrong since v1.0.37) corrected; its
  Memory section rewritten from the release map (the stack is in DTCM, not AXI SRAM); the overnight review's dated
  memory paragraph marked as that day's numbers; main.cpp's Engine comment (34,032 B, not 24,232; 60,176 B for the
  stack, not ~68 KB); a README section for the audit lap.

## 2. Gates [measured]

- Desktop: `tools/same_render.py` against the pinned ref-main renderer: **SAME 60/60** (merged tree, all hooks off).
- ctest: **"100% tests passed out of 21"** (merged tree, and again at 86561de); SAME 60/60 again at 86561de.
- The shimmer A / B's two copies [measured, desktop]: `Shimmer.h` compiled three ways with clang, `-ffp-contract=off`,
  6 s of a chord through `Shimmer::process`: the A / B build with `prof::shimmerPoly` false is **bit-identical** to the
  shipped loop, with it true bit-identical to `IV_SHIMMER_POLY` (the two differ by up to 4.6e-4 at a 0.23 peak, as the
  cpu notes found). So the lap's `SHIMMER` and `SHPOLY` lines run exactly the shipped and the trial arithmetic.
- Firmware defaults after every edit: release / m0test / profile **byte-identical** (cmp) to the merge's own.
- Default release against main's v1.0.55 (`dist/sublustris_versio-v1.0.55-3eb5e1c.bin`, 112,564 B): **112,620 B,
  different bytes** by design: B1-B4 change code (B3 +32 B, B4 +24 B) but not output (SAME 60/60 on the desktop; on
  the module they skip work whose result is unread or provably 1, so no float operation that runs changes). The
  third HASH file (section 4) turns that into a cmp on the module.

## 3. Sizes [measured, release unless named; budget 112,640 B]

| Build | Bytes | vs default | vs budget |
|---|---:|---:|---:|
| main v1.0.55 | 112,564 | -56 | 76 under |
| default (B1-B4) | **112,620** | 0 | 20 under |
| `SDRAM_REFRESH=1` | 112,636 | +16 | 4 under |
| `ABSORB_EXP=1` / `SHIMMER_POLY=1` alone | 112,636 each | +16 | 4 under |
| `ABSORB_EXP=1 SHIMMER_POLY=1` | 112,652 | +32 | **12 over** |
| `LEAN=1` | **110,476** | -2,144 | 2,164 under |
| `LEAN=1 ABSORB_EXP=1 SHIMMER_POLY=1` | 110,508 | -2,112 | 2,132 under |
| `AUDIO_DIRECT=1` | 109,380 | -3,240 | 3,260 under |
| `LEAN=1 AUDIO_DIRECT=1` | **107,236** | -5,384 | 5,404 under |
| `LEAN=1 AUDIO_DIRECT=1 ABSORB_EXP=1 SHIMMER_POLY=1` | 107,268 | -5,352 | 5,372 under |
| `LEAN=1 AUDIO_DIRECT=1 SDRAM_REFRESH=1` | 107,252 | -5,368 | |
| all five | 107,284 | -5,336 | |
| m0test | 83,556 | | |
| profile (default) | 127,872 | | of 131,072 |
| profile `SIZESPIKE=5` (the morning's file) | **121,296** | | 9,776 under 128 KB |
| profile `SIZESPIKE=5 LEAN=1` | 120,824 | | |

LEAN pays for both CPU trials many times over: with LEAN, B1-B4 + ABSEXP + SHPOLY fit with 2.1 KB left.

## 4. The profile build for the morning

Files in `dist/`, built at **212b68b** (the commit after it only writes these names into the notes):
- `sublustris_versio-review2-212b68b-profile-audit.bin` (121,296 B): **the one to flash.** LEAN off, so its HASH is
  the reference.
- `sublustris_versio-review2-212b68b-profile-audit-lean.bin` (120,824 B): the same lap with LEAN=1. Its only job: its
  HASH line. Flash it after the first, wait for the HASH (or the first DONE line), compare.
- `sublustris_versio-review2-212b68b-profile-audit-v1.0.55.bin` (120,016 B): optional, the audit branch's own lap on
  v1.0.55's code (no B1-B4; built from review2/audit f2eea0e). Its HASH proves B1-B4 the same on the module.
  Its lap has only the first 10 lines, and it prints its HASH **only at boot** (3 s after power-up, not on the DONE
  line): have `screen` running before the 3 s are up (if missed: quit screen, unplug and replug USB, start screen at
  once). Its HASH line has no `refresh COUNT` part.

**Steps (USB power only: never with the rack power cable connected):**
1. Unplug the rack power cable from the Versio. Connect USB.
2. Flash `...-profile-audit.bin` (noiseengineering.us/portal/firmware → Select Custom File → CONNECT → CHANGE
   FIRMWARE).
3. In Terminal, from the main checkout (`cd ~/Documents/Sites/immensio-versio`): `screen /dev/tty.usbmodem* 115200`,
   then **Ctrl-A, Shift-H** to start logging (`screenlog.0` in that folder; copy it, don't move it, while it runs).
   The module waits 3 s after boot for the log; the HASH line comes out then, and again on every DONE line.
4. LED 1 is blue while a lap runs. Wait for **`SIZESPIKE LAP 1 DONE`**: about **4-6 minutes** (16 lines of ~6 s of
   untimed run-in, +2 s frozen, then 6 s timed; estimate, not measured). LED 1 turns green (no late block) or LED 4 red.
5. Quit screen (Ctrl-A, K, Y). Flash `...-profile-audit-lean.bin`, open screen again with logging, wait for its HASH
   line (3 s after boot) or its first DONE line (~5 min). Optionally the same with `...-audit-v1.0.55.bin`.

**What each line should read:**

| Line | Expect | Answers |
|---|---|---|
| `HASH xxxxxxxx (3000 blocks, ...) \| refresh COUNT 2054` | 8 hex digits; **the same on all two (three) files** | LEAN (and B1-B4) the same output on the module. COUNT 2054 confirms libDaisy's refresh. |
| `SPIKE UNFRZ ... still / creep / twists`, `HALL sweep`, `BLOOM W1.0 creep` | audit estimate 96-103 % max for MASSIVE's; HALL 90-93 | the untimed corners: any `late` > 0 is a click on the module |
| `SPIKE PAINT`, `CVALL` x 2, `GRIT` x 2 | 84-88 (PAINT), 91-96 (CVALL) [estimated] | |
| `SPIKE SHIMMER MASSIVE playing music sweep W0.5 M0.0 T0.5 D90` | ~93-95 % max (94.8 in CPU save 2, minus B1-B4's ~0.5-1) | the reference for the next four |
| `SPIKE ABSEXP ...sweep` | ~1.3-4 points under SHIMMER (90.7 measured in CPU save 2) | ABSORB_EXP's saving |
| `SPIKE SHPOLY ...sweep` | ~1.9-2.7 under SHIMMER [estimated] | SHIMMER_POLY's saving |
| `SPIKE ABS+SHP ...sweep` | ~4-6 under SHIMMER: **about 88-90 %**, the target | whether MASSIVE fits with both |
| `SPIKE UNFRZ+AS MASSIVE FROZEN ... creep` | the UNFRZ creep line minus ~4-6 | whether the worst untimed corner fits with both |
| `SPIKE REFRESH ...sweep` | SHIMMER + 0 to 0.5 [estimated] | what the datasheet refresh costs |
| `SIZESPIKE LAP 1 DONE: 16 lines, worst max ... \| STACK n B of 60176 \| HASH xxxxxxxx` | STACK ~5-7 KB (audit estimate 6.6 KB) | the stack's real depth |

Compare each trial line with the `SHIMMER` line of the **same lap** (the same build, the same minute); `max` is the
worst block, `avg` the mean, `near` the blocks over 90 %. A `late` count above 0 anywhere is the module running out.

Caveat [measured]: the profile's `prof::mark` hooks change GCC's loop decisions in `Engine::process` (it is shaped
differently from release: LEAN shrinks it by 40 B in the profile, 540 B in release). So **the HASH proves LEAN's
`Fdn::runBlock` change (-352 B in the profile too) and its `sqrtf`, not its `Engine::process` change**, which rests on
section 5's argument. The shimmer A / B makes the profile's head loop two copies chosen per sample: the `SHIMMER`
line's absolute figure may differ from the CPU-save lap's by a few tenths; compare within the lap.

## 5. LEAN's claims, re-verified

- **`sqrtf` as one `vsqrt.f32`** [measured, disassembly]: newlib's `sqrtf` is `__ieee754_sqrtf` (itself one
  `vsqrt.f32 s0, s0`) plus, for x < 0, errno and `0 / 0`. For every x >= 0 and NaN the result is the same instruction's;
  for x < 0 both give the default NaN (VSQRT's invalid operation and `0 / 0` both return it). Only errno differs, which
  nothing reads. Same bits, every input.
- **I2C slave path unused** [measured, source]: `HAL_I2C_Slave_Transmit` is reached only from
  `I2CHandle::TransmitBlocking`'s `else` (mode not master). The one `I2CHandle` in this build is `DaisySeed::Init`'s
  Seed 1.1 codec set-up, `Mode::I2C_MASTER` (libDaisy `daisy_seed.cpp:277`); main.cpp has no I2C. Unreachable.
- **`LedPwm` zero-initialised** [measured, map]: `gLedPwm` is in `.bss` (0x24000120, between `_sbss` and `_ebss`,
  zeroed by startup). Every read of a port's `id` / `numPins` / `counts` is for `p < numPorts_`, set as ports are added;
  `slot_`'s defaults were 0 anyway; `PORTX`'s non-zero default was never read. The LEDs only, either way.
- **No loop unswitching (`Engine::process`) / splitting (`Fdn::runBlock`)**: both are control-flow transforms in GCC
  (unswitching copies a loop per value of an invariant condition; splitting cuts the iteration range at a compare).
  Neither reorders, fuses or reassociates floating point; the Core is built with `-O3 -ffp-contract=off`, without
  `-ffast-math` / `-fassociative-math` (checked in the compile line), and the M7 has no float SIMD, so the
  vectoriser can't reassociate either. Turning them off changes where the branch is, not the arithmetic. I agree with
  the audit: output-identical in principle. Not provable on the desktop (the macros are firmware-only) and, for
  `Engine::process`, not by the HASH (above). No emulator here (no qemu / unicorn).
- Doubt worth noting: none on correctness. The CPU cost (a predictable branch a sample in those loops, ~0.03 %
  [estimated]) is untimed, and the profile can't time release's `Engine::process` shape; the late-light release with
  LEAN would be the check if anyone wants one.

## 6. AUDIO_DIRECT, read against libDaisy

`SaiCallback` matches `AudioHandle::Impl::InternalCallback`'s non-interleaved 24-bit stereo path line for line:
`s242f(in) * postgain_recip_` in, `f2s24(out * output_adjust_)` out (the same factors, from the AudioHandle's config:
`1 / postgain`, `postgain * output_compensation`), the callback given `size / 2` frames, the same buffer size
(`blocksize x 2 x 2`, uncached D2 window), the same `SaiHandle`. No sum in either conversion, so no fused
multiply-add difference between libDaisy's flags and ours. The same samples in principle; it needs the rack boot.
`...-lean-audiodirect.bin` in `dist/` is that test (section 8).

## 7. SDRAM refresh (task 6)

- **What libDaisy programs** [source]: `HAL_SDRAM_ProgramRefreshRate(0x81A - 20)` (`libs/libDaisy/src/dev/sdram.cpp`):
  COUNT 2054, a refresh every (COUNT + 1) / 100 MHz = **20.55 us**, all 8192 rows every **168 ms**. (0x81A = 2074 is
  64 ms / 4096 rows at ~133 MHz: it looks inherited from a 4K-row part or another clock.)
- **What the chip asks**: 8192 refreshes in 64 ms, 7.8 us apart. Evidence: the FMC is set up with 13 row bits
  (`RowBitsNumber 13`, sdram.cpp) = 8192 rows; 4 banks x 8192 rows x 512 columns x 32 bit = 512 Mb, the part's size;
  JEDEC SDR SDRAM of this density is 8K / 64 ms; a distributor listing for the AS4C16M32MS family states 8192 cycles /
  64 ms (via a search result summary; https://www.mouser.com/new/alliance-memory/alliance-memory-sdrams/ returned 403
  to a direct read). The Alliance datasheet PDF (the URL in robust.md section 8) could not be text-extracted here:
  **the 8K / 64 ms figure is not verified from the datasheet itself.**
- **The right COUNT**: RM0433: refresh rate = (COUNT + 1) x SDCLK, COUNT = period / rows x f - 20 (the 20 a margin for
  a refresh held behind a read): 7.8125 us x 100 MHz - 20 = **761**.
- **Does the tank's own traffic refresh it?** Partly. Activating a row (any read or write that opens it) restores
  every cell of that row, so rows the heads pass are refreshed by use. But with the FMC's mapping (32-bit, 9 column
  bits: column = address bits 10:2, row = 23:11, bank = 25:24) a row is 2 KB, 512 floats, ~11 ms of one line. A delay
  line's samples sit untouched between the write head and the read heads: for every line longer than ~64 ms (all of
  HALL's and MASSIVE's, seconds; a frozen pad holds them indefinitely, rewritten once a loop) most rows depend on the
  timer alone, i.e. on cells holding 168 ms where 64 ms is what's guaranteed (at 85 C). At room or rack temperature
  retention is typically far longer (it roughly doubles every ~10 C cooler), which is why every Daisy product gets away
  with it; nothing reported. What a failure would sound like: a bit flipping in a held float. In the mantissa, a tiny
  click; in the exponent, a huge sample, and robust.md finding 4 says one huge sample poisons the tanks for good. So
  rare, but not harmless.
- **The trial**: `SDRAM_REFRESH=1` writes COUNT 761 to `FMC_SDRTR` after the 3 / 3 timing at boot (main.cpp
  `SdramSetRefresh`): **+16 B** (release 112,636 B; 4 B under the budget alone, fine with LEAN). The same samples.
  Cost [estimated]: ~80,000 more refreshes a second, each a precharge-all + refresh + a later re-activate (~140 ns):
  +1.1 % of the SDRAM's time; the tank stalls on only part of it: ~0-0.5 points. The audit lap's `REFRESH` line
  measures it on MASSIVE's worst corner. Not default.

## 8. The optional rack-boot release

`dist/sublustris_versio-review2-212b68b-lean-audiodirect.bin` (107,236 B): LEAN=1 AUDIO_DIRECT=1, otherwise the
default release (B1-B4, no sound trials). Built cleanly; the code read against libDaisy (section 6). On **rack power**
(USB unplugged): it should boot to the usual LED pattern and sound exactly like v1.0.55. Listen for: both outputs
present and at the usual level (compare with v1.0.55 at the same knob settings), left and right not swapped (a
signal into the left input only), no hiss, no constant buzz or crackle (a wrong buffer layout would buzz at 1 kHz),
the knobs working. If it is silent or wrong, flash v1.0.55 back; nothing persistent changes.

## 9. Open risks, not checked

- Nothing ran on the module: every CPU figure is estimated or from earlier laps; the lap time is an estimate.
- LEAN's `Engine::process` part is proven by argument only (section 4's caveat).
- The 8K / 64 ms refresh figure is not read from the datasheet itself.
- SHPOLY and ABSEXP are sound changes (tiny: 62-123 dB under; the cpu notes); they need the owner's OK by the rules,
  even if the lap says they fit.
- The scratch `SIZESPIKE=4` (cpusave) lap was not rebuilt or compared here.
