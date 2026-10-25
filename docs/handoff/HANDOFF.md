# Handoff

**Written:** 26 Oct 2026, end of session 7 (the overnight review and its merge). Start the next session with `/immensio-start`.

**Nothing running:** no agents, no workflows. Both trees are clean. `main` holds the docs (this handoff, `docs/TASKS.md`); **all the code is on branch `claude/massive-air`**, which is not merged into main yet (the owner's Decide item).

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments up to **v1.0.36**. v1.0.34-36 are on the branch only; main's SPEC stops at v1.0.33. The review's merged changes are sound-neutral and have no changelog line |
| M0 desktop / firmware | Built |
| M0 hardware | **Passed** (owner, 25 Oct 2026) |
| M1 Hall, M4 MASSIVE, M5 PLATE | Built and listened |
| M2 | Plugin installed: `b39d451` (v1.0.34). The branch HEAD (v1.0.36 + the review) is **not** installed |
| M3 | **Over budget on the last measurement (round 9b: MASSIVE 115-129 % avg, target ~90 % peak); the review's speed-ups are built but unmeasured on the module.** By instruction count and the review's cache/SDRAM model MASSIVE+BLOOM should now read ~85-100 %, and a libDaisy SDRAM timing trial could take another 9-19 points. **Round 11 is waiting for the owner to build and flash** (`docs/TASKS.md`) |
| M6 character | Five eras, listened |
| M7 duck | Done |
| M8 | Knob feel done |
| M9 | Not started |

**This session (the overnight review, 26 Oct 2026; report `docs/research/review-2026-10-26.md` on the branch):**
- Eleven background agents (a workflow in two phases) surveyed the literature (Jot, Schlecht & Habets, Dattorro, Costello, Barr, MI, ST's AN4839/AN4891/RM0433, the Cortex-M7 TRM) and costed every stage from the release disassembly; four then built prototypes in their own worktrees. Everything they claimed was re-checked before it went into the report.
- **Merged into `claude/massive-air` (owner: "merge it"), all bit-identical to v1.0.36** on `tools/same_render.py`'s 55 renders: the HALL/MASSIVE tank rewritten line by line (`Fdn::runBlock` / `lineFast` / `lineSlow`; MASSIVE 2,967 → 1,488 M7 instructions a tank sample), the twin's input side off while there is no twin, the Plate's full-ring reads and per-block taps, each tank's ERA sent only when it changed, the still-lows split on locals, the BBD/radio `set()` memoised and the radio's `sinf` skipped when the hum is off, `LoopEra`'s divide skipped at drive 1, the Engine object in DTCM, the two tanks' in-loop allpass lines in AXI SRAM (`Fdn::prepare` takes a second arena; `Engine::attachMemory` takes an `axi` array).
- **The profile build is round 11** (`firmware/README.md`): FZ on, the SDRAM timing trial (TRP/TRCD 3/3 vs libDaisy's 16/10) with `SDRAMCHK`, new MICRO tests (VDIV latency/throughput/overlap, PLD, the swell per block, a real-size MASSIVE tank with its allpasses in SDRAM vs AXI, subnormals), three new corners (frozen MASSIVE with the twin, MASSIVE SHIMMER, MASSIVE with SIZE flicked), the Core linked first (since round 10 the profile timed `-Os` copies of the tape and radio), an untested ITCM build behind `PROFILE_ITCM=1`.
- **Behind renderer hooks, off:** four sound options (`IV_TAPE_OS=2`, `IV_LOW_DECIMATE=8`, `IV_SWELL_TAPS=24`, `IV_SHIMMER_LAZY=1`) and three bug fixes (`IV_SIZE_HYST`, `IV_HALFRATE_PRIME`, `IV_TWIN_STOP_FAST`), with tests `test_size_noise`, `test_halfrate_prime`, `test_twin_stop`.
- **Tools:** `tools/same_render.py` (the bit-identity harness: two renderers, 55 float renders, `SAME n/n`), `tools/pages/review_cpu_pages.py`.

**Gates** (on `claude/massive-air` HEAD `1dd35bc`, run in `.claude/worktrees/massive-air`):
- ctest **"100% tests passed out of 17"** (`build-scratch/ctest.log`).
- `tools/same_render.py` (baseline `0bf5505` renderer vs HEAD): **SAME 55/55**.
- Firmware (110 KB budget = 112,640 B; **156 B left**):

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 112,484 | 86 % |
  | profile | 123,684 | 94 % (profile-only tests; the all-variants warning at 95 % is near) |
  | m0test | 82,476 | 62 % |

  RAM (release): DTCM 61,376 B of 128 KB (`gFast` + the Engine), AXI SRAM 452,024 B of 512 KB (the allpass lines), SDRAM 24.9 MB.
- **main HEAD:** this handoff's commit (docs only). **Plugin installed:** `b39d451` (v1.0.34).
- **Pages waiting** in main's `renders/`: `tape_os`, `still_lows_rate` (the review's two options), `swell_seed` (from session 6). `renders/` is now 2.3 GB; delete judged pages.

## In flight

- **Nothing running.**
- **Branch `claude/massive-air`** (worktree `.claude/worktrees/massive-air`, HEAD `1dd35bc`): kept on purpose until the owner OKs the merge into main (TASKS "Decide"). Its `docs/TASKS.md` and `docs/handoff/HANDOFF.md` are the session-5 versions plus the review's pointers; **main's are the current ones** (they will conflict at the merge into main: take main's and add the review's Done line).
- The review's branches (`review/overnight`, `review/p2-*`) are merged and deleted; its worktree is removed. The agents' full notes lived in the session's scratchpad (temp) and are not kept; everything that mattered is in the report.
- `.claude/worktrees/serene-snyder-30d3a2`, `swell-curve`: other sessions' worktrees, already merged.

## Next steps

1. **Read round 11 when the owner flashes it** (TASKS). In order: `BENCH` (FZ on; `sdram TRP 3 TRCD 3`; the Engine's address in DTCM), **`SDRAMCHK` 0 / 0 / 0 or forget the timing**, `sdr32` vs `sdr32t` (33 → ~21 expected), `fdnMassive24k` vs `fdnMassiveApAxi` (−900 to −1,700 expected), `vdivIndep` vs `vdivThr`, `pld32` vs `sdr32`, `subX` vs `norm`, the corners (MASSIVE S1 neve BLOOM ~85-100 % expected; **FROZEN+twin**; **SIZE .3<>.8's `max`**), the headroom test. Expected values for every field: report section 3 (F1-F4) and `firmware/README.md`. Then the ITCM build.
2. **Adopt in release what the module confirms** (report section 8, item 2): the 3/3 SDRAM timing in `firmware/main.cpp` after `hw.Init` (~40 B) if `SDRAMCHK` is clean; the FZ two-liner if `subX ≫ norm`; the ITCM linker script if it booted and gained; the block size (`PROFILE_BLOCK=96`, −7.5 points model, +1 ms) is the owner's latency call.
3. **The SIZE bug before the module is played with knobs:** adopt `IV_SIZE_HYST`'s hysteresis (`Engine::setSizeHysteresis(1e-3f)` as the default; +248 B, so first free flash: the tape at 2× is −808 B if the owner picks it on `tape_os`, or `-fmove-loop-invariants` / `-fno-math-errno` for the DSP objects, or `Engine::queueEdge` and the constructors `IV_SIZE_OPT`, ~1-2 KB). Confirm SIZE moves in MASSIVE on the rack. Decide the CV question (a slow LFO into SIZE holds the tank until it rests).
4. **Merge `claude/massive-air` into main** on the owner's OK (take main's TASKS/HANDOFF in the conflict). Install v1.0.36 + the review when the owner wants it in Ableton (`tools/install_plugin.sh <commit>`, Ableton closed).
5. **If the target is still short after 1-3:** the swell inside the 24 kHz path (−5 to −7 %, or the 24 points back at today's cost; report section 4), control once per block, the crossfade peak mitigations if `SIZE .3<>.8`'s `max` is the peak, the owner's pages, and only then the structural option (4 lines with a velvet feedback matrix: −25 to −35 %, a new voice).
6. **Housekeeping** (report section 8, item 7), each small: `-ffp-contract=off` on the desktop `iv_core` (then a golden refresh), the stimulus as a CTest fixture, `ctest -j8` in CLAUDE.md, the Plugin's mono-input aliasing fix, the NaN guard, the sieve as a static initialiser, the dead headers and stale Resilio comments, SPEC §5.2/§6.2 vs the code; then the Engine's decomposition with the harness.

## Waiting on the owner (`docs/TASKS.md`)

- **Build and flash round 11** from the `claude/massive-air` worktree (the command in TASKS uses the main checkout's built libDaisy), run a lap and the headroom test, send the serial output; then the ITCM build.
- **Decide:** merge `claude/massive-air` into main; adopt the SIZE fix (needs flash).
- **Listen** (only if round 11 says the module is still short): `tape_os`, `still_lows_rate`; and `swell_seed` from session 6.
- Recordings, as before.

## Will bite

- **CPU is still unmeasured after the review.** Every figure for the merged set is an instruction count (solid) or the review's model (reads 0-25 % low, relative only). The SIZE crossfade is the one place the desktop (+4-18 % slower blocks) and the M7 count (~15 % cheaper) disagree in direction: the `SIZE .3<>.8` corner's `max` decides. A frozen MASSIVE with the twin (estimated 1.5-1.65× a plain MASSIVE) was never in a corner before round 11.
- **The SDRAM timing trial rewrites `FMC_SDTR1` while the FMC is live** (precharge-all before and after; RM0433 doesn't explicitly allow it). `SDRAMCHK` is the guard: any non-zero count means don't adopt, whatever the speed-up. The profile lap runs at 3/3 by default (`PROFILE_SDRAM_TIGHT=0` for libDaisy's timing).
- **Flash: 156 B left.** The SIZE fix needs 248 B; the HalfRate prime 32; the twin stop 96. `RadioStage::set`'s memo cost +328 B and is the item to trim if bytes are short.
- **Two tank paths to keep in step:** `Fdn::lineFast` / the still taps (the everyday case) and `lineSlow` / `tapsMoving` / `tapsOld` (crossfades, gliding or filling allpasses). Any change to the tank's per-sample maths goes into both; `tools/same_render.py` against a reference renderer built from the previous commit is the gate, plus forcing the slow paths (`IV_FORCE_ONE`, `IV_FORCE_SLOW` existed in the agent's scratch copy, not in the repo: add them if the tank is touched again).
- **`runBlock` uses ~4.2 KB of stack inside the audio callback;** the stack is in DTCM with ~70 KB of room after the Engine moved there.
- **The profile's sections moved:** the line-output sums and the governor are in `kTankMix` now (they were in `kTankOut`), marks are per block (no ~1 % marking cost). Compare round 11 with round 9b with that in mind.
- **Knob noise on the module defeats every memo keyed on exact float equality** (ERA's `LoopEra::set`, the BBD/radio `set()`s, `AbsorbMemo`, `Drift`): the "still" savings may not happen on hardware until the readings get a deadband. Unmeasured; a jitter readout in m0test would settle it.
- **The desktop Core is built with fused multiply-adds, the firmware isn't** (`-ffp-contract=off` only in `firmware/Makefile`), so minute-long holds measured on the desktop are a close cousin of the module's, not the same bits. `tools/same_render.py` compares desktop with desktop.
- **A fresh worktree fails two tests** until `python3 tools/make_stimulus.py` has run (the WAVs aren't in git), and its `libs/libDaisy` submodule is empty: build firmware with `LIBDAISY_DIR=<a built one>`.
- **Round 6's `axi32 = 3.0` measured nothing** (the array was optimised away); the review's AXI estimates rest on round 8's real AXI tank. Round 11's `axi32` is the first real figure.
- **Goldens:** one update per SPEC version (v1.0.36 now); the review added no changelog line (nothing audible changed). The `IV_HALFRATE_PRIME` fix, if adopted, changes every HALL/MASSIVE golden's bits (one sample later) and needs its own line.
- **Earlier traps still apply:** picks relayed by other sessions need the owner's confirmation; prototypes that change prepare()-time layout must be set before prepare (the review's hooks re-prepare and print what they built).

## Where to look

- `docs/research/review-2026-10-26.md` (Summary, sections 3 and 8 first) and `docs/TASKS.md`
- `firmware/README.md` (round 11's serial fields, expected values, build commands) and `firmware/main.cpp`
- `core/dsp/Fdn.cpp` (`runBlock`, `lineFast`, `lineSlow`, `blockFits`), `core/dsp/Engine.cpp` (`process`, `processTwoTanks`, `applySpace`'s ERA memo, the hooks)
- `tools/same_render.py` (the gate for anything called sound-neutral)
- `docs/adr/0026-fitting-the-cpu.md`, `0025-size-and-time-crossfade.md`
