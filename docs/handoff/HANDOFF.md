# Handoff

**Written:** 29 Oct 2026, end of session 11.

This session covered six pieces of work:
- **Overnight review 2,** with its report, prototypes and the module's audit laps.
- **Five versions merged:**
  - v1.0.56, DECAY's gentle ramp into endless;
  - v1.0.57, HALL's long tails;
  - v1.0.58, endless tails keep still notes, only bright tails settle, the bass and DC stay clean;
  - v1.0.59, tape dropouts from about 7:30;
  - the review's same-sound savings, with LEAN the default.
- **Three pages judged:**
  - `rack_levels` (the HALL/MASSIVE level match picked, not built yet);
  - `review2_hole` (as now);
  - `input_stop` (D picked; refined on `input_stop2`, waiting for the owner).
- **The GitHub repo renamed** to `sublustris-versio`, with a new README.
- **A website brief and a prompt** for a new session.
- **Old builds and judged pages moved to the Trash.**

Start with `/sublustris-start`.

**Nothing running:** no agents, no nested sessions, no worktrees. The owner works over Remote Control from their phone: `start_session` is refused, so delegated work runs as background `Agent` subagents in hand-made worktrees (memory "Remote Control: no nested sessions"). The LAN page server may still be running from earlier; check with `preview_list`.

## Folder move (not done yet; owner's choice)

The GitHub repo is already `jeffebauer/sublustris-versio` (ADR 0032 amendment; `origin` points at it). The local folder moves `~/Documents/Sites/immensio-versio` → `~/Documents/Sites/sublustris-versio`, but **only at a wrap-up when the owner is at the computer and will open a new session in the new folder**. Never before a `/clear` or from the phone: a cleared session stays in the old folder (memory "Folder move needs a new session"). Checklist:

1. **Prepare:** everything committed and pushed; no agents running; no worktrees; the page server stopped.
2. **Paths:**
   - Replace `/Users/jesse/Documents/Sites/immensio-versio` with `/Users/jesse/Documents/Sites/sublustris-versio` in tracked files that aren't history: CLAUDE.md, `.claude/skills/*`, `tools/**/*.py`, SPEC §6's tree. `git grep -l immensio-versio` finds them; leave `docs/research/` and past handoff prompts as history.
   - Change the memory path in `.claude/skills/sublustris-wrap/SKILL.md` to `-Users-jesse-Documents-Sites-sublustris-versio`.
   - Commit and push.
3. **Memory:** `mkdir -p ~/.claude/projects/-Users-jesse-Documents-Sites-sublustris-versio && cp -R ~/.claude/projects/-Users-jesse-Documents-Sites-immensio-versio/memory ~/.claude/projects/-Users-jesse-Documents-Sites-sublustris-versio/`.
4. **Last command:** `mv ~/Documents/Sites/immensio-versio ~/Documents/Sites/sublustris-versio`. Then tell the owner to open a new session in the new folder and run `/sublustris-start`.
5. **The new session's first steps:**
   - `git worktree prune`.
   - Recreate the `.venv` (`python3 -m venv .venv && .venv/bin/pip install numpy soundfile`): a moved venv's scripts still point at the old path.
   - `build/`'s CMake cache records the old path, so remove only `build/CMakeCache.txt` and `build/CMakeFiles`. Then `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` and ctest (expect 22).
   - Run `make -C firmware all-variants` and `cmp` the release against `dist/sublustris_versio-v1.0.59-4371335.bin`.
   - Update CLAUDE.md's first paragraph to say the folder has moved.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to **v1.0.59**, on main |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4371335` (v1.0.59), 29 Oct 17:27; AU check passed |
| M3 | v1.0.57 played at the rack (the owner's three notes: levels and dropouts, both since picked). **v1.0.59 not yet at the rack** |
| M9 | Not started |

- **main HEAD:** the commits after this handoff; code as of the v1.0.59 merge `4371335`. Pushed to `origin`.
- **Gates, re-run at wrap-up on main:**
  - ctest: **"100% tests passed out of 22"**.
  - `same_render`: SAME 60/60 against the v1.0.58 renderer at the v1.0.59 merge.
- **Firmware:**

  | Variant | Bytes | |
  |---|---|---|
  | release | **112,012** | 85 %; **628 B under the 112,640 budget** |
  | m0test | 83,556 | |
  | profile | 128,848 | 98 %; warning as usual |
  | release `AUDIO_DIRECT=1` | 108,772 | |

- **Rack builds in `dist/`:**
  - `sublustris_versio-v1.0.59-4371335{,-latelight}.bin`;
  - `...-4371335-audiodirect{,-latelight}.bin`, the owner's next flash: it combines the AUDIO_DIRECT boot test with v1.0.59's rack check.
  - Also kept: `dist/release/v1.0.54-05122e3/`, the friend package the owner sent.

## What changed this session (on main)

- **Review 2** (report `docs/research/review-2026-10-29.md`, agents' notes in `docs/research/review2-notes/`, laps in `docs/research/m3-review2-audit*-212b68b.log`).
- **`review2/neutral` merged** (`248ec3d`):
  - B1-B4 same-output CPU savings;
  - trial switches done properly: `ABSORB_EXP`, `SHIMMER_POLY`, `LEAN`, `AUDIO_DIRECT`, `SDRAM_REFRESH` (each needs `=1`, own tag and obj dir);
  - the audit lap `SIZESPIKE=5`;
  - **`LEAN=1` the default:** its HASH matched on the module, `0x3b18a2ac`, and its timing is the same within 0.4 points; `LEAN=0` → `-nolean`.
- **v1.0.56** (ADR 0008 amendment): PLATE and MASSIVE ease the floor governor in from DECAY 0.70 (`voicing::kGovernorEaseFrom`, `map::floorWeight`).
- **v1.0.57** (ADR 0035, ADR 0012 amendment): HALL's loss shelf at 6 kHz; HALL's modulation floor deepens ×1 → ×2 from 5 s to 15 s tails, centred.
- **v1.0.58** (ADR 0008/0009 amendments):
  - the governor's correction is a flat gain;
  - the still lows take no hold correction;
  - a 5 Hz DC blocker on the wet input;
  - endless PLATE and HALL calm their modulation over 10 s once held ("still notes");
  - the light settle (`kEndlessTop*`) applies only as far as the playing was bright (`Engine::trackBrightness`);
  - a new test checks that a held tail keeps its spectrum.
- **v1.0.59** (ADR 0030 amendment): `kTransportDipsFrom = 0.026`, so dips start at about 7:30.
- **Repo and docs:**
  - README rewritten in Resilio's style;
  - NOTICE and `tools/make_release.sh` point at the new repo URL;
  - the website brief is `docs/minisite/BRIEF.md`, and the new session's prompt `docs/handoff/website-prompt.md`.

## In flight (branches kept on purpose, no worktrees)

- **`claude/rack-levels`** (`6eab077`): `IV_SPACE_TRIM="plate,hall,massive"` dB at endless, plus `IV_MIX_TAPER`. **The owner picked C, meet halfway: `0,-2,2`**, and MIX as built. Next: build it as v1.0.60 (step 1 below). Notes: `docs/research/rack-levels.md` on the branch.
- **`claude/input-stop`** (`1ef34bc`, on top of v1.0.58): `IV_INPUT_STOP` modes.

  | Mode | What it is | Flash |
  |---|---|---|
  | 1 = B | quick limiter release | |
  | 2 = C | B + the cushion at endless | |
  | 3 = D | the owner's pick on `input_stop`: the cushion at every DECAY | +352 |
  | 4 = E | the cushion follows the tail closely and holds 0.8 s | +392 |
  | 5 = F | E + a +1.5 dB bloom as the dry leaves | +440 |

  - Diagnostic hooks: `IV_LIMIT_OFF`, `IV_TRACE`.
  - Its page `renders/input_stop2` (A/D/E/F) waits for the owner.
  - Notes: `docs/research/input-stop.md` on the branch.
  - The branch is built on v1.0.58, not v1.0.59, so merge main into it before building.
- **Shelved or done prototypes** (keep): `claude/motion-dips`, `review2/endless`, `review2/endless2`, `review2/hole` and `review2/holepage` (the owner kept MASSIVE's low mids as now), `review2/bump(page)`, `review2/halltail`, `review2/knobs` (`IV_KNOBPATH`: the firmware's knob path in the renderer), `review2/robust` (`IV_DC_BLOCK_HZ`, `IV_INPUT_GUARD`, the robustness harness), `review2/valhalla`, and the rest of `review2/*` and `review/overnight-2`. Older: `claude/level-duck`, `claude/drift-rethink-8838a7`, `claude/swell-motion`, `claude/wear-onset`.
- **Pages in `renders/`:** `input_stop2` (waiting), `endless_bright_settle`, `rack_levels`, `motion_dips` and `review2_hole` (all judged: can go to the Trash on the owner's OK).

## Next steps

1. **v1.0.60, the HALL/MASSIVE level match:** at endless, HALL −2 dB and MASSIVE +2 dB, coming in as DECAY nears endless (the owner's pick C on `rack_levels`).
   - Port `claude/rack-levels`'s `IV_SPACE_TRIM` (only the trim, not `IV_MIX_TAPER`: MIX stays as built) into a voicing constant on main, now at v1.0.58/59.
   - Prove it equals the prototype with `"0,-2,2"` on a branch rebased onto main; everything below the trim's range is bit-identical to main.
   - Goldens, SPEC line, and an ADR 0033 amendment (it takes back 2 of `hall_level` pick C's 3 dB).
   - About +90 B; it fits in 628 B.
2. **The owner's pick on `input_stop2`,** built as the next version from `claude/input-stop` (merge main in first).
   - Flash: D +352, E +392, F +440 B. After v1.0.60 that leaves about 180 B for D and almost nothing for F, so **AUDIO_DIRECT's rack test (−3,240 B) decides the room.**
   - If the owner picks F, check its bloom at MIX 0.3 / 0.8 once more.
3. **The unfreeze-after-SIZE overload:** MASSIVE with SHIMMER, WEAR 0.5 and MOTION tape reaches 100.6-107.5 % in the audit lap's UNFRZ lines, up to two-thirds of the blocks late, a crackle.
   - Cause: the frozen tank holds its SIZE, so the unfreeze lands the whole move as one crossfade while the twin room still runs.
   - Options (sound changes, so a page): glide SIZE after an unfreeze; wait for the twin to stop; stop the twin sooner.
   - `ABSORB_EXP` + `SHIMMER_POLY` only take it to 103.4 %.
   - Profile lap: `make MODE=profile SIZESPIKE=5`.
4. **AUDIO_DIRECT the default** once the owner's rack boot test is clean: `firmware/Makefile` `AUDIO_DIRECT ?= 1`, with a `-libaudio` tag for the old path, the same way LEAN was done.
5. **The website** runs in its own session from `docs/handoff/website-prompt.md`. This repo's sessions shouldn't touch `site/` or `docs/minisite/` while it runs.
6. **Later** (TASKS "Waiting on Claude"):
   - the review's housekeeping list;
   - "Found by review 2, for later";
   - PLATE's SIZE by crossfade;
   - the SPACE-flip morph;
   - the Plugin input NaN guard;
   - desktop `-ffp-contract=off`.

## Waiting on the owner (`docs/TASKS.md`)

- **Listen:** `input_stop2`.
- **At the rack:** v1.0.59 with the AUDIO_DIRECT boot test, in one flash.
- **Try in Ableton:** v1.0.59, and check Ableton's sample rate (at 96 kHz HALL and MASSIVE come out much brighter than the module).
- **When convenient:**
  - start the website session;
  - empty the Trash (`sublustris-cleanup-2026-10-29`, `sublustris-old-builds-2026-10-29b`);
  - reference recordings.

## Will bite

- **Flash: 628 B left.** v1.0.60 (~90 B) fits. The `input_stop` pick needs ~350-440 B, which is tight to impossible without `AUDIO_DIRECT` (−3,240 B, waiting on the rack test). Profile is at 98 % of 128 KB: every new profile line counts.
- **CPU:**
  - The unfreeze case above is the one known overload.
  - v1.0.58 adds a per-chunk brightness measure (`trackBrightness`, every 4th sample, out of line) and the endless modulation calm. Neither has been profiled on the module; the next profile lap should include them.
  - Playing MASSIVE's worst SIZE-move line is 90.8 % (88.4 % with both shortcuts).
- **Agents and the main checkout:**
  - An agent once reconfigured main's `build/` by losing its `cd`. Briefs must say: absolute paths only (`cmake -S <worktree> -B <worktree>/build`), never cmake or make in the main checkout. Check `grep IV_BUILD_PLUGIN build/CMakeCache.txt` reads ON after agents finish.
  - The scratchpad is shared: give each agent its own named folder.
- **Explain owner notes only after a control run.** This session's first explanation of `input_stop`'s dip (the first reflections) was wrong; it was the limiter (memory "Measure owner notes first").
- **Version numbers on branches:** build a version's rack files from main after the merge, not from a branch that lacks the version before it (v1.0.59's branch builds were v1.0.57 + dips; renamed and retired).
- **Goldens:** `IV_GOLDEN_UPDATE` refuses a second update at the same SPEC version.
- **Desktop vs module:** release has **no** flush-to-zero and no fused multiply-adds; the desktop fuses (901 `vfma` in `libiv_core.a`). `same_render` proves desktop against desktop; firmware-only claims need `cmp` or the module's HASH line (`SIZESPIKE=5` lap).
- **`screenlog.0`** in the main checkout is the module's serial log (local only; laps from it are filed in `docs/research/`). It appends across flashes: slice laps by their START and DONE lines.
- **The shell's cwd drifts into worktrees;** commit main's docs with `git -C <main checkout>`.

## Where to look

- `docs/TASKS.md`: "Waiting on Claude", whose first item is "Next, in order".
- `SPEC.md`: changelog v1.0.56-v1.0.59.
- `docs/research/review-2026-10-29.md`: the review, including "The module's answer".
- `docs/research/m3-review2-audit-212b68b.log`: the UNFRZ lines.
- Branches `claude/rack-levels` and `claude/input-stop`: their notes `docs/research/rack-levels.md` and `input-stop.md`.
- `docs/minisite/BRIEF.md` and `docs/handoff/website-prompt.md`, for the website session.
