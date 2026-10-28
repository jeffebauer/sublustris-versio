# Handoff

**Written:** 28 Oct 2026, end of session 10. That session:
- built v1.0.51-54 from the owner's picks and rack notes: level D + glue E, HALL's endless tail +3 dB (C), DECAY's range B, TIME in BLOOM by crossfade, two rounds of CPU savings;
- ran six CPU laps with the owner.

Start with `/sublustris-start`.

**Nothing running:** no agents, no nested sessions. The owner worked over Remote Control, where `start_session` is refused. All delegated work ran as background `Agent` subagents in hand-made worktrees (memory "Remote Control: no nested sessions"). The LAN page server (`preview_start` `review-pages-lan`, port 8766) was started this session; assume it is gone. The module was still looping its profile lap into `screenlog.0` at the end.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to **v1.0.54**, on main |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `05122e3` (v1.0.54, Sublustris Versio), 28 Oct |
| M3 | v1.0.49 confirmed at the rack. The v1.0.52 rack test found overloads with SHIMMER + WEAR + MOTION's tape side; v1.0.54's laps show 0 late blocks. **v1.0.54 not yet heard at the rack** |
| M9 | Not started |

**Update 3, 29 Oct 2026 (session 11):** overnight review 2 ran (report `docs/research/review-2026-10-29.md`, now on main with the agents' notes; prototype branches `review2/*`, pages `renders/review2_endless`, `review2_bump`, `review2_halltail`, draft `review2_hole_draft`). The owner ran its audit lap and the LEAN HASH lap (`docs/research/m3-review2-audit*-212b68b.log`): HASH matched, so `review2/neutral` was merged into main (`248ec3d`, owner's OK) with **LEAN=1 the default**: 22 of 22 tests, SAME 60/60 against the pre-merge renderer, **release 110,476 B (2,164 B under budget)**, m0test 83,556, profile 127,408. Rack builds `dist/sublustris_versio-v1.0.55-248ec3d{,-latelight}.bin` (same sound as v1.0.55). Plugin not reinstalled (same sound). **The one overload left:** unfreezing after SIZE moved while frozen, MASSIVE up to 107.5 % (next: a prototype). AUDIO_DIRECT=1 (−3,240 B) waits for its rack boot test. Sections below this paragraph predate the review.

**Update 2, end of session 10:** v1.0.55 (the owner's pick B on `renders/decay_swell`: DECAY's make-up rises no faster than the old tail fades) merged into main (`4658d91`): 22 of 22 tests, **release 112,564 B (76 B under budget)**, m0test 83,556, profile 127,808. Plugin installed `4658d91` (AU check passed). Rack builds `dist/sublustris_versio-v1.0.55-3eb5e1c{,-latelight}.bin` (and `-profile-cpusave.bin`), not yet heard at the rack. **Next: the overnight review**, prompt in `docs/handoff/overnight-review-2-prompt.md`; the morning session reads its report (`docs/research/review-2026-10-29.md` on `review/overnight-2`), starts the page server for the owner and files TASKS. Sizes and versions below this paragraph are from v1.0.54.

**Update 1, end of session 10:** at the owner's request `claude/combined` (674a2ca) is **merged into main** (`05122e3`), so main holds everything now: 22 of 22 tests (main builds the plugin tests too), release 112,588 B, the same bytes as `dist/sublustris_versio-v1.0.54-7ee389e.bin`. Plugin installed `05122e3` (AU check passed; old Immensio bundles to the Trash). Friend package `dist/release/v1.0.54-05122e3/` (universal, read-me with what's new since v1.0.41). Judged pages, old builds and log tails are in `~/.Trash/sublustris-cleanup-2026-10-28`. The `combined` worktree can be removed; new work branches from main.

**Gates on `674a2ca`, re-run by me:**
- ctest: **"100% tests passed out of 21"**.
- Desktop `same_render`: the CPU savings are SAME 60/60 against the pre-savings code.

Firmware:

| Variant | Bytes | |
|---|---|---|
| release | **112,588** | **52 B under the 112,640 budget** |
| m0test | 83,556 | 63 % |
| profile | 127,832 | 97 %; warning expected |

Rack builds in `dist/`:
- `sublustris_versio-v1.0.54-7ee389e.bin` and `-latelight.bin`. Same code as `674a2ca`: that commit only adds docs and renumbering.
- Older ones can go to the Trash: v1.0.48-53 and every `-profile-cpu*` file. Keep the v1.0.54 pair.

## What changed this session (all on `claude/combined`)

- **v1.0.51**, ADR 0014 amendment and ADR 0033:
  - ducking 2:1, eased on held playing (40 %), fast 0.08 s release after held sounds;
  - endless floor −21 dBFS;
  - 816 B freed by building the control-rate maths `-Os`. The laps showed this isn't a CPU cost.
- **v1.0.52** (ADR 0033 amendment):
  - HALL's floor at −18 dBFS;
  - WEAR's compander clamp is now a soft knee; it was the cause of a tick on louder tails.
- **v1.0.53** (ADR 0034, ADR 0020 amendment): DECAY tops PLATE 30 s, HALL 60 s. Same curve, endless still from 0.85. Longer tails are trimmed to today's level while playing.
- **v1.0.54** (ADR 0025 amendment, phase 2):
  - BLOOM's swell taps crossfade on TIME moves instead of gliding.
  - CPU savings (`firmware/README.md` "CPU save" and "CPU save 2"):
    - shimmer rings in D2/AXI SRAM;
    - tank control maths once a block;
    - the twin room zeroed while it rests (that was the flip spike);
    - WEAR's two sides interleaved;
    - LED DMA without HAL.
- **New lap:** `SIZESPIKE=4` (CPUSAVE) in the profile build.

## In flight

- **Nothing unmerged that matters.** Every branch made this session is contained in `claude/combined`: `glue-e`, `time-noise`, `cpu-hunt`, `cpu-save`, `cpu-save2`, `decay-range`, `v153`. Their worktrees are removed and the branches kept. `claude/level-duck` (the prototype hooks, superseded by v1.0.51) is shelved, its worktree removed.
- **Pages judged this session**, in the main checkout's `renders/`, can go to the Trash after the owner's OK: `level_glue`, `hall_level`, `decay_range`, `_material_level_glue`, `_material_decay_range`. That's about 1.8 GB.
- **Local-only artefacts in the main checkout**, not committed: `screenlog.0` (the cpusave2 lap still looping) and `screenlog-2026-10-2*.0` (leftover lap tails). All can go.
- **The optional `IV_ABSORB_EXP`** (loss filters on `exp`, not bit-identical, ~1e-6): measured −1.5 to −4 points on MASSIVE's SIZE moves (ABSEXP lines in `docs/research/m3-cpusave2-7ee389e-full.log`). It's off in release.
  - Turning it on via `make 'TRIAL_DEFS= -DIV_ABSORB_EXP'` gave a byte-identical binary, so that route doesn't work.
  - To enable it properly: add a Makefile option to `C_DEFS`, then measure the size.

## Next steps

1. **The owner plays v1.0.54 at the rack** (TASKS "At the rack" lists what to check): the LEDs (driver changed), SHIMMER (memory moved), SIZE moves with SHIMMER + WEAR + MOTION, SPACE switches, TIME in HALL BLOOM, DECAY's range.
   - If MASSIVE still glitches or flashes amber a lot, turn on `IV_ABSORB_EXP` in release (see In flight). It needs bytes found first if it grows the build.
2. ~~Merge into main, install the plugin~~ done (05122e3).
   - Take main's TASKS / HANDOFF / CLAUDE.md / skills / `docs/research/m3-*` logs.
   - `docs/research/level-at-infinite.md` exists on both sides; keep both halves.
   - Then install the plugin with `tools/install_plugin.sh <commit>` (Ableton closed; `auval -v aumf ImVs Ombr` only), tell the owner to remove the old "Immensio Versio" bundles, and update the TASKS "Plugin installed" line.
3. **Later** (TASKS "Waiting on Claude"):
   - the sub-20 Hz rumble at endless DECAY with TONE CW;
   - PLATE's SIZE by crossfade;
   - the SPACE-flip morph (needs ~0.5-1 KB of flash);
   - the review's housekeeping list;
   - the low tank's crossover reset on a SPACE flip.

## Waiting on the owner (`docs/TASKS.md`)

- At the rack: v1.0.54.
- Decide: the merge into main.
- Empty the Trash: old `immensio-*` folders.
- Optional reference recordings and Ribbons/Magneto takes.

## Will bite

- **Flash: 52 B left.** Anything new needs bytes found first. Known tools:
  - `IV_ZI`, `IV_SIZE_OPT` / `IV_CTL_OPT`, `IV_NOINLINE`, no new libm calls;
  - check what links from libgcc (`__aeabi_*`): the VFP `__aeabi_l2d` saved 840 B;
  - ~1-2 KB is still in `queueEdge` and the constructors (review item).
- **CPU headroom is thin in MASSIVE.** With SHIMMER + WEAR 0.5 + MOTION's tape side while SIZE moves, the profile lap peaks at 93-95 %. Release may be a point or two heavier.
  - Any per-sample addition in MASSIVE needs a lap.
  - D2 SRAM now holds the left shimmer ring, the twin room and the prime sieve (`.d2_bss`, clocks on at boot). AXI SRAM is nearly full.
- **Lap logs.** Moving `screenlog.0` mid-run stops logging. Laps repeat, so logs mix builds: slice one lap between START and DONE, and trust the DONE line's late count (memory "Moving screenlog stops logging").
- **The late-light build's output file** is `firmware/build/sublustris_versio-latelight.bin`; `cmp` the dist pair (memory).
- **The shell's cwd drifts into worktrees.** Commit main's docs with `git -C <main checkout>`.
- **The click check misses broadband bursts:** compare energy above 3 kHz against an unswitched render.
- **The level governor holds an absolute level** (HALL −18, PLATE/MASSIVE −21 dBFS at endless); the module's jack calibration is unknown.
- **Test changes this session to keep in mind:**
  - `test_plate_freeze` feeds PLATE at 0.5 and carries a 0.8 dB white-noise offset for the DECAY trim.
  - `test_motion` and `test_twin_level` set DECAY via `host/tests/DecayV1052.h` to keep the old tail lengths.

## Where to look

- `docs/TASKS.md`
- `SPEC.md` changelog v1.0.51-54 on `claude/combined`
- `firmware/README.md` "CPU save" / "CPU save 2" (lap `SIZESPIKE=4`, memory placement)
- `docs/research/m3-cpusave2-7ee389e-full.log` (latest lap, main)
- ADRs 0014, 0025, 0033, 0034 (on `claude/combined`)
