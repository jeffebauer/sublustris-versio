# Handoff

**Written:** 25 Oct 2026, end of session 6. Start the next session with `/immensio-start`.

**Nothing running:** no agents. Both trees are clean. `main` is pushed with this handoff. **All the code is on branch `claude/massive-air`**, which is not merged into main yet.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments up to **v1.0.36**. v1.0.34-36 are on the branch only; main's SPEC stops at v1.0.33 |
| M0 desktop / firmware | Built |
| M0 hardware | **Passed** (owner, 25 Oct 2026) |
| M1 Hall, M4 MASSIVE, M5 PLATE | Built and listened |
| M2 | Plugin installed: `b39d451` (v1.0.34). The branch HEAD (v1.0.36) is **not** installed |
| M3 | **Over budget, being fitted.** Real limit measured (owner's headroom test, round 9b): clean to 96 % of a block, late from 101 %, so the target is **~90 % at peak**. After v1.0.35: PLATE 59-69 % avg / ~88 % peak (fits); HALL 101-123 % avg; MASSIVE 115-129 % avg (worst live 122 % avg / 127 % peak). v1.0.36 (fewer early reflections, the prime table) is not measured yet. **Round 10 is waiting for the owner to flash it** (`dist/immensio_versio_profile_M3_round10.bin`) |
| M6 character | Five eras, listened |
| M7 duck | Done |
| M8 | Knob feel done |
| M9 | Not started |

**This session (all on `claude/massive-air`):**
- **v1.0.31-33** MASSIVE highs (ADR 0022), the shortest tail (0023), SHIMMER replaces REVERSE (0024). Installed earlier.
- **v1.0.34 SIZE and TIME by crossfade** (ADR 0025, installed as `b39d451`):
  - inside the loop the fade gains sum to one, scaled by how well the old and new reads agree (Bristow-Johnson);
  - a small HALL's diffusion joins or leaves on its own read;
  - the loss filter fades with the lengths;
  - the level that follows SIZE is on the tank's input, with ERA's in-loop stages scaled back to their old level.
- **v1.0.35 fitting the CPU** (ADR 0026):
  - the HALL / MASSIVE tank and its freeze twin run at 24 kHz (`dsp/HalfRate.h`);
  - the swell has 12 points (a true 12: fit_cpu's "12" had rendered 24 because of a prototype bug, so a re-listen settled it);
  - the Midiverb box keeps its loop low-pass and bits inside the 24 kHz tank.
- **v1.0.36 early reflections by SPACE: HALL 2, MASSIVE 4** of six (`Engine::setEarlyTapsForSpace`). One per line reopened MASSIVE's dip before a hit's tail (16.9 dB at SIZE full). Also a prime table for the line lengths, which removed 150-210 % spikes at SIZE changes and SPACE flips.
- **Sound-neutral speed-ups (bit-identical, checked against 66 float renders each):**
  - lazy line clears (no ~130 ms stall at a flip or freeze);
  - the swell read tap by tap;
  - SDRAM lines staggered across the cache's sets;
  - SDRAM write-back with write-allocate (`firmware/main.cpp` `SdramWriteAllocate`);
  - block maths only when its inputs change (`AbsorbMemo`, `Memo1`, the drift's depths);
  - the 6k±1 prime test, then the sieve.
- **Tried and removed:** fetching each block's lines into the cache before the loop (round 7: cost ~30 %, saved less).

**Gates** (on the branch HEAD `0bf5505`):
- ctest **"100% tests passed out of 14"** (`build-scratch` in the worktree).
- Firmware (110 KB budget = 112,640 B; **460 B left**):

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 112,180 | 85 % |
  | profile | 116,432 | 88 % |
  | m0test | 82,476 | 62 % |

- **main HEAD:** this handoff's commit (tasks and handoff only).
- **Phone listening:** "review-pages-lan" (port 8766); the Mac's IP changes (`ipconfig getifaddr en0`; .238 on 25 Oct).
- **Pages waiting:** `renders/swell_seed` (other seeds for the 12-point swell; the owner's note on its mids). Measured first: 12 and 24 points are equally even on noise and note by note (~5.3 dB comb), so a seed can only move which notes stand out.

## In flight

- **Nothing running.**
- **Branch `claude/massive-air`** (worktree `.claude/worktrees/massive-air`, 33 commits ahead of main): kept on purpose until the owner OKs the merge (TASKS "Decide").
- **Round 10 profile build** in `dist/` (from `0bf5505`), not flashed yet. What it shows:
  - corners: SPACE × SIZE 0/1 × the centre of each era (Neve 0.1, Midiverb 0.3, BBD 0.5, cassette 0.7, radio 0.9), DECAY full, BLOOM;
  - ERA's wet split into `tape` / `bbd` / `radio`;
  - MICRO: `tape/smp`, `bbd/smp`, `radio/smp`, `bbdSet/blk`, `radioSet/blk`, `loopNeve/smp`, `loopMidi/smp`;
  - the headroom test (heaviest corner as is, then PLATE neve stepped up 5 % a time).
- `.claude/worktrees/serene-snyder-30d3a2`, `swell-curve`: other sessions' worktrees, already merged; not this session's.

## Next steps

1. **Read round 10 when the owner flashes it.**
   - On the desktop the cassette was the costliest era (+35 % of the Engine), the radio +20 %. The module's split decides.
   - Sound-neutral ERA fixes first:
     - the radio's per-sample `std::sin` for the hum (computed even when the hum is 0): a rotation oscillator;
     - its ~7 divisions a sample: reciprocals worked out at set();
     - `BbdStage::set` and the radio's SVF `set` (`sin`+`cos` each) only when their inputs change;
     - TapeOut: `/ (1 - tb²) / drive_` as a precomputed product.
   - Then sound-changing options as a page: the tape's 4× → 2× oversampling; per-sample vs per-block RMS in the tape.
2. **The tank's loop** (round 9b: tkloop ~28 %, tkout ~31 % of the budget in MASSIVE). Ideas not tried:
   - unrolling the 8-line loop by two to hide the FP latency;
   - removing the per-line `readAllpass` divide (eta per block);
   - the per-sample `era_[i]` work when ERA is at the Neve;
   - in-loop allpasses or the Plate in AXI SRAM (~200 KB free);
   - hot code in ITCM (64 KB, unused).
   - Check every change bit-identical with the 66-render comparison: a reference renderer built from the previous commit, float WAV material, `cmp`. The scripts from this session were in the scratchpad (`same.sh`, `same_ff.sh`); rebuild them.
3. **Install the plugin** when the owner wants v1.0.36 in Ableton (`tools/install_plugin.sh <commit>`, Ableton closed, `auval -v aumf ImVs Ombr`).
4. Later, from TASKS: BLOOM's TIME and PLATE's SIZE by crossfade (phase 2); the SPACE-flip 3 s morph (needs flash and CPU); the Valhalla comparison rest.

## Waiting on the owner (`docs/TASKS.md`)

- **Flash round 10** and run a lap.
- **Merge** `claude/massive-air` into main (Decide).
- **Listen:** `swell_seed`.
- Recordings, as before.

## Will bite

- **CPU:** MASSIVE is still ~1.3× over the ~90 % target.
  - Blocks over 100 % in the audio callback starve the main loop. The profile build times off the audio clock and the headroom step ends from the callback, for that reason.
  - A knob jump or a SPACE flip can spike: check PEAK, not just avg.
- **Flash: 460 B left.**
  - Per-block functions must be `IV_SIZE_OPT`.
  - Shared helpers called from two places get inlined twice; `HalfRate::process` is `IV_NOINLINE` for that.
  - Prototype code goes under `#ifndef IV_FIXED_VOICINGS`.
- **Prototypes that change prepare()-time layout must be set before prepare lays it out.** fit_cpu's swell count wasn't, so the owner picked a mislabelled version (memory [[verify-voicing-labels]]).
  - Verify the thing itself, not just "different".
  - The Engine's re-preparing setters: `setEarlyTapPrototype`, `setSwellSeedPrototype` (renderer `IV_FIT_EARLY_TAPS`, `IV_SWELL_SEED`).
- **Owner firmware goes in `dist/` with a round name** (memory [[profile-builds-in-dist]]).
- **Narrow-band tests through sparse taps are comb luck.** The early taps and the swell comb a single tone by ±5 dB. The cassette alias test averages five tones for that reason (−34 dB with the 24 kHz tank, limit −32).
- **The 24 kHz tank changed what white-noise tests read:** HALL −2.7 dB, MASSIVE −1.9 dB (`kHalfRateWhiteNoiseOffsetDb`). The paint test now uses louder noise, because the pad must freeze above the floor.
- **Goldens:** one update per SPEC version (v1.0.36 now).
- **Earlier traps still apply:** see the previous handoffs in git history; picks relayed by other sessions need the owner's confirmation ([[relayed-picks-need-owner]]).

## Where to look

- `docs/TASKS.md`
- `docs/adr/0026-fitting-the-cpu.md`, `docs/adr/0025-size-and-time-crossfade.md` (on the branch)
- `firmware/main.cpp` (the profile build: MeasureCorner, MICRO tests, headroom)
- `core/dsp/Fdn.cpp` (the tank: control, process, the crossfade), `core/dsp/HalfRate.h`, `core/dsp/Swell.h`
- `core/dsp/EraStages.h`, `core/dsp/Tape.h` (the ERA stages to optimise next)
- `docs/handoff/overnight-review-prompt.md` (the owner's overnight review brief)
