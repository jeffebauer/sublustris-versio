# Handoff

**Written:** 11 Nov 2026, end of session 20 (11 Nov).

- **The two B laps were read:** B ("SIZE waits for the morph") cleared 12 of 15 flip-flick lines. Two crackles were left: the PLATE -> CANYON landing, and a SPACE flip during a SIZE crossfade (`docs/research/flipflick-landing-2026-11-11.md`).
- **The owner picked A, "the flip waits for SIZE", on all 14 cards of `renders/flip_waits_size`.** Built with fix 1a (SIZE's fast crossfade paths on lines still filling, bit-identical), the 47-line lap was all 0 late, worst 98.9 % (`docs/research/m3-flipflick-landing-lap.log`).
- **Beta 2 is out:** v1.0.80 (ADRs 0056-0058) ported, merged into `beta/player-feedback` (`4f6f950`, pushed) and published as [`v1.0.80-beta.1`](https://github.com/jeffebauer/sublustris-versio/releases/tag/v1.0.80-beta.1) (pre-release; the site stays on v1.0.68). Linked on PR 1; the owner posted it on Modwiggler (BBCode draft written here). The notes credit lfad and list one known issue.
- **Clean-up** (the owner's OK): judged pages, test firmware v1.0.70-v1.0.79, older beta packages, 11 worktrees and 12 branches (bundled first in `dist/branches-backup-2026-11-11.bundle`). Disk 7.8 to 11 GB free.

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | `main` at v1.0.68; v1.0.69-v1.0.80 on `beta/player-feedback` (PR 1, open, not merged). ADRs 0047-0058 live on the beta branch only |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; the beta's plugin (now v1.0.80) not played by the owner |
| M3 | The v1.0.80 code lapped on the module as the landing build (all 47 lines 0 late, worst 98.9 %); the published v1.0.80 release itself is the same machine code (agent's disassembly check) but not lapped. The owner was flashing the beta firmware at wrap |
| M9 | v1.0.68 Latest; `v1.0.80-beta.1` (11 Nov) the newest pre-release |

**main:** `e02e8c5` plus this wrap's commit, docs only since v1.0.68; ahead of origin by the session's docs commits, **push pending the owner's answer** (see the end of this file). **`beta/player-feedback`:** `4f6f950`, pushed, equal to origin.

## Start here

**The owner plays beta 2 on the rack, and players' feedback comes in** (`docs/TASKS.md`, "At the rack", first item). Read PR 1's comments, issue 2 and anything the owner brings from Modwiggler first.

Then, in order:
1. **The known issue:** CANYON -> PLATE with FREEZE pressed within ~220 ms of the flip, 101.5 %, 96 late blocks (`docs/research/m3-freeze-on-change-2-lap.log`). Research first (memory "research before designing"), then prototype, page if it changes the sound, and lap.
2. Issue 2's recording (still waiting on chriisu).
3. The red output LEDs.
4. The full release (merge PR 1) once players and the owner are happy.
5. The plugin UI.

## Waiting on Jesse

See `docs/TASKS.md`:
- playing beta 2 on the rack (first "At the rack" item);
- measuring the module's input level;
- playing the beta's plugin in Ableton (say when, and it gets installed);
- emptying the Trash of the older clean-ups.

## In flight

**Nothing running.** Both agents this session (the lap firmware, the v1.0.80 port) finished; I read both diffs and re-ran their gates (below). No nested sessions were started.

Kept:
- `beta/player-feedback` (worktree `.claude/worktrees/beta`) and `.claude/worktrees/release` (the release script's, detached at `4f6f950`).
- `claude/input-stop`, `claude/rack-levels`: open work, unmerged.
- `dist/branches-backup-2026-11-08.bundle` and `-2026-11-11.bundle` (the 12 branches removed today, `proto/flipflick-landing` among them).
- No page server is running (8766 is free): start `review-pages-lan` from `main` before the next page.

## Will bite

- **Flash:** v1.0.80 release 112,236 B, **404 B under budget**; profile 126,960 B. The profile only fits because the boot micro-tests are out by default (`IV_PROFILE_MICRO=0` in `firmware/main.cpp`, ported from the prototype); turning them back on overflows by ~530 B.
- **CPU is at the edge:** worst lap line 98.9 % (flip-flick and XFFLIP lines); drops start near 99 %. Anything that adds work in a SPACE or SIZE transition needs a lap.
- **The XFFLIP lap lines watch 3.4 s from the switch;** with the flip now waiting up to ~1.2 s, they don't see the end of its 3 s morph.
- **`LIBDAISY_DIR` must go on the make command line** (the Makefile assigns it hard).
- **The desktop can't hear these crackles.** Judge them with a lap.
- As before: `screenlog.0` appends (copy, never move); never USB and rack power at once; `ls` is eza (`/bin/ls`); a new worktree needs `tools/make_stimulus.py`; a `while … $(…)` loop in Bash trips the rm safety check even with no rm: use plain commands.

## Gates

- **`beta/player-feedback` `4f6f950` (tree = `c1c2dc7`, v1.0.80):** ctest **"100% tests passed out of 30"** (re-run by me in the port worktree); goldens unchanged; `same_render` 60/60 vs v1.0.79 and 55/55 page cards identical to the prototype with its hooks on (agent); firmware clean rebuild by me: release **112,236 B** (99.6 % of budget), m0test 83,556 B, profile **126,960 B** (96.9 % of the chip). The published `.bin` is byte-identical to my rebuild.
- **main:** at `e02e8c5` (docs only since v1.0.68): ctest **"100% tests passed out of 24"** (`build/ctest.log`, run at wrap); firmware release **111,828 B** (99.3 %), m0test 83,556 B, profile **130,956 B** (99.9 % of the chip), the same as before.
- **Installed plugin:** `4140997` (v1.0.68), unchanged; main's HEAD differs only by docs.

## Where to look

- `docs/TASKS.md`, "At the rack", first item.
- `docs/adr/0056-unfreeze-size.md`, `0057-size-waits-for-morph.md`, `0058-flip-waits-for-size.md` (beta branch).
- `docs/research/m3-flipflick-landing-lap.log`, `docs/research/m3-freeze-on-change-2-lap.log` (the known issue).
- PR 1: https://github.com/jeffebauer/sublustris-versio/pull/1
