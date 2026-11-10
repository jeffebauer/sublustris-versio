# Handoff

**Written:** 10 Nov 2026, end of session 19 (10 Nov).

- **The crackle when SIZE turns right after letting go of FREEZE (HALL, CANYON) is fixed on the module, as two picks on `proto/unfreeze-size`:**
  - **A, "the room makes way"** (all 24 cards, `renders/unfreeze_size`): a SIZE change after an unfreeze waits while the twin room fades in 0.3 s.
  - **"The room keeps its size"** (the owner's idea, `3786ef9`): the twin plays the SIZE the pad froze with, and its glide is cut from the firmware.
  - Laps at the owner's knobs with SHIMMER on: control 105.7-110.7 % with ~10k late blocks; A and fixed room 0 late. Fixed room has the best CANYON headroom (95.0 %).
  - Not ported, not released.
- **A new crackle, found by the owner on the fixed-room build:** flip SPACE PLATE <-> CANYON and flick SIZE end to end right after. It's in v1.0.79 too (laps `docs/research/m3-flipflick-{fixedroom,control}-lap.log`: 101.4-115.2 % on all 11 lines, both builds).
  - Cause, from the late-block counts (1,201 / 300 ms = CANYON's / PLATE's SIZE crossfade): the new tank's SIZE crossfade running under a two-tank morph. Research: `docs/research/flipflick-size-2026-11-10.md`.
- **The owner picked B, "SIZE waits", on all 17 cards of `renders/flipflick_size`:** during a PLATE <-> HALL / CANYON morph the new tank keeps the SIZE it started with; the old tail fades over its 3 s, then SIZE lands once.
  - Built into firmware on `proto/flipflick-size` (`fcecb00`, on top of the two picks above).
  - **Two 47-line laps are built and wait for the owner in the morning.**

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments; `main` at v1.0.68; v1.0.69-v1.0.79 on `origin/beta/player-feedback` (PR 1, not merged). This session's three picks are on `proto/` branches, with no ADR, SPEC line or goldens yet |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; the beta's plugin hasn't been played by the owner |
| M3 | Laps today on the module: unfreeze-SIZE (A, fixed room, control, SHIMMER on) and flip-flick (fixed room, control). B's laps are built, not run. The owner played A's and fixed room's release builds on the rack |
| M9 | Released v1.0.68 (Latest); `v1.0.79-beta.1` (10 Nov, pre-release) is the newest |

**main:** docs only since v1.0.68; **pushed to origin at wrap** (the owner's OK, 10 Nov; no `site/` change, so no site rebuild). **`beta/player-feedback`:** `e9bc141`, pushed, equal to origin.

## Start here

**Update, 11 Nov morning (session 20): the B laps are run and read** (`docs/research/m3-flipflick-B-lap.log`, `-B-OFF-lap.log`). B clears 12 of 15 flip-flick lines (OFF: all 15 late). Left: PLATE -> CANYON, SHIMMER on, SIZE flicked 0.1-0.3 s after the flip (3 lines, 100.8-102.5 %, peak ~3.4 s, when the waiting SIZE lands at the morph's end), and the XFFLIP lines (3 of 4 late in both builds, up to 125.5 %). The usual 19 and the unfreeze lines 0 late. The owner's call on what next is pending. The page server on 8766 is no longer running: restart it from `main` before the next page.

**(Done) Read the owner's two B laps** (`docs/TASKS.md`, "At the rack", first item). The owner flashes `dist/v1.0.79-flipflick/sublustris_versio_profile-flipflick-B-MEASURING-47lines.bin`, then `...-OFF-MEASURING-47lines.bin`. Each takes about 13 min with the corner lines.

- **How to wait for it:** take a `wc -l screenlog.0` baseline and wait for the first `LAP .* DONE \[FLIPFLICK-B\]` after it (memory "watching a lap"). Slice START→DONE into `docs/research/m3-flipflick-B-lap.log` / `-OFF-lap.log`.
- **Pass:**
  - lines 20-34 (the flip-flicks, PLATE <-> HALL included) 0 late under B;
  - the usual 19 and the unfreeze lines still 0 late;
  - worst ideally under 98 %.
- **The XFFLIP lines (35-38)** flip SPACE while a SIZE crossfade still runs. B doesn't touch them (a flip doesn't wait for a SIZE crossfade today, `Engine.cpp` ~1118). If they overload, the house-style fix is for a flip to wait for the crossfade. That would be a prototype, a page and a lap.

Then, in order:
1. **Beta 2:** port the three picks (room makes way, room keeps its size, SIZE waits) from `proto/flipflick-size` onto `beta/player-feedback` as v1.0.80, once the laps pass.
   - Needs an ADR per pick (or one for the pair on the unfreeze), SPEC changelog lines, one golden update, and the gates.
   - Release notes are hand-written and shown to the owner first.
   - The ADR 0055 left-over (CANYON -> PLATE, freeze within 220 ms of the flip, 101.5 %) is still open.
2. Players' feedback (PR 1, Modwiggler, issue 2).
3. The red output LEDs.
4. The full release once players and the owner are happy.
5. The plugin UI.

## Waiting on Jesse

See `docs/TASKS.md`:
- **The two B laps** (morning).
- As before:
  - lfad credit in the release notes;
  - a Modwiggler post about the beta;
  - issue 2's recording;
  - the older clean-up list, now also `proto/unfreeze-size` (superseded by `proto/flipflick-size`, which contains it);
  - the rack list and the beta's plugin.

## In flight

**Nothing running.** Both agents (the flip-flick prototype and its firmware build) are finished and reviewed. I read both diffs and re-ran SAME 60/60 (hook off vs `7e1ef2c`) and a clean firmware rebuild: release and B profile `cmp`-identical to the dist files.

Kept on purpose:
- **`proto/flipflick-size`** (`fcecb00`, worktree `.claude/worktrees/flipflick-size`): holds all three picks plus the 47-line lap. It's what beta 2 ports from (without the measuring lines). Untracked there: `ff-build/` (its CMake build) and `ff-base/` (7e1ef2c's renderer and `.bin` files, the SAME baseline).
- **`proto/unfreeze-size`** (`7e1ef2c`, worktree `.claude/worktrees/unfreeze-size`): an ancestor of the above. Keep until the port; then it's for the clean-up list.
- **`beta/player-feedback`** (worktree `.claude/worktrees/beta`); `.claude/worktrees/release` (the release script's).
- `claude/input-stop`, `claude/rack-levels`, `proto/size-blast`, and the older clean-up list's branches (see TASKS).

Local only: `dist/v1.0.79-unfreeze-size/` (this session's earlier builds), `dist/v1.0.79-flipflick/` (B's builds and README), the page server on 8766 (`review-pages-lan`, still running), and the scratchpad SAME outputs.

## Will bite

- **B's flash:**
  - release 111,660 B, **980 B under budget**;
  - profile (47 lines) 126,860 B, 4,212 B under the chip;
  - `FLIPFLICK=0` (the control) gives release 111,532 B, byte-identical to `7e1ef2c`.
- **A port has to take the measuring lines out of `firmware/main.cpp`** (the FLIPFLICK / XFFLIP / UFS lines). Port from the Core and the Makefile, not the lap.
- **B makes SIZE feel dead for up to 3 s after a PLATE <-> FDN flip.** The owner picked it by ear on the desktop and hasn't played it on the rack. The release build is in `dist/v1.0.79-flipflick/` if they want to.
- **A flip during a SIZE crossfade isn't guarded** (the XFFLIP lines measure it).
- **CPU is at the edge:** the usual 19 lines are 97.5-98.6 % worst across laps; drops start near 99 %.
- **The desktop can't hear these crackles.** Judge them with a lap, not the click check.
- **A morph's fast drop (`morphFast_`) also cuts the old tank's input,** and that clicked (+13.3 dB) when the prototype reused it for SIZE. A, desktop only, keeps the input.
- **Lap logs:** each DONE line now carries its build's tag. The first START banner after a flash may be the previous build's.
- As before:
  - `screenlog.0` appends: copy it, never move it;
  - never connect USB and rack power at once;
  - `ls` is eza (use `/bin/ls`);
  - a new worktree needs `tools/make_stimulus.py`, and firmware built with `LIBDAISY_DIR=` pointing at main's `libs/libDaisy`;
  - disk is at 18 GB free (both judged pages deleted).

## Gates

- **`proto/flipflick-size` `fcecb00`:**
  - ctest: **"100% tests passed out of 28"** (re-run by me at wrap, `ff-build/ctest.log`);
  - SAME 60/60 with `IV_FLIPFLICK=off` vs `7e1ef2c` (re-run by me);
  - the page's 17 cards: the built B is bit-identical to the prototype's B (the agent's check);
  - firmware, clean rebuild by me: release **111,660 B**, m0test 83,556 B, profile **126,860 B**, release and profile `cmp`-identical to the dist files.
- **main (`59ace73`, docs only since `cdae028`, which had no code change):** ctest "100% tests passed out of 24"; release 111,828 B, m0test 83,556, profile 130,956. Not re-run.
- **`beta/player-feedback` v1.0.79 (`e9bc141`):** ctest 100 % of 26; release 111,236 B; profile 130,628 B. Not re-run.
- **Installed plugin:** `4140997` (v1.0.68), unchanged.

## Where to look

- `docs/TASKS.md`, "At the rack", first item, and `dist/v1.0.79-flipflick/README.txt`.
- `docs/research/flipflick-size-2026-11-10.md` and `unfreeze-size-2026-11-10.md`.
- `core/dsp/Engine.cpp` on `proto/flipflick-size`: `ffHold`, `ufsSize` / `ufsRamp`, `morphFlip` / `morphAdvance`.
- `firmware/main.cpp` on that branch: `RunMorphLap` and the FLIPFLICK / XFFLIP lines.
