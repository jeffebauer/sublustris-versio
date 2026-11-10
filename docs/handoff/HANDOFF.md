# Handoff

**Written:** 10 Nov 2026, checkpoint in session 19 (10 Nov).

- **`v1.0.79-beta.1` is published** (10 Nov, from `e9bc141` on `beta/player-feedback`, pushed; pre-release, Latest stays v1.0.68; PR 1 has a comment linking it; the owner approved the notes as written). It carries everything since v1.0.68: v1.0.77's SIZE limit, v1.0.78's SPACE morph (ADR 0054) and v1.0.79's freeze-on-flip fix (ADR 0055). The released firmware is byte-identical to the round 2 build the owner heard on the rack.
- **The freeze-on-flip crackle is fixed by the owner's ear.** Their note on v1.0.78: SPACE flipped then FREEZE a moment later gave one crackle (every PLATE change, and HALL -> CANYON); FREEZE first was clean. Cause: a module overload, nothing the desktop hears (measuring lap `docs/research/m3-freeze-at-flip-lap.log`: 104-120.7 % on all 9 lines). Their pick: the old space holds as the pad, the change waits for the unfreeze. Round 2 (`proto/unfreeze-reshape`, `3bfeb76`) lapped 31 of 32 lines clean, worst 99.0 % (`docs/research/m3-freeze-on-change-2-lap.log`). Ported without the lap's lines as v1.0.79 on `claude/port-freeze-on-flip` (`d4f0a5d`), merged into the beta (`e9bc141`).
- **Left on the freeze-on-flip lap:** CANYON -> PLATE with FREEZE pressed and let go within about 220 ms of the flip, 101.5 %, 96 late blocks, cause unknown (the same change at the owner's knobs is 90.9 %, a 2 s hold 90.5 %). Untimed: a freeze about 2.5 s after the flip. The owner didn't report hearing either.
- **The owner heard a second crackle on round 2:** turning SIZE during the second the pad fades after letting go of FREEZE, in HALL and CANYON. It's the known unfreeze-after-SIZE overload (`docs/research/m3-review2-audit-212b68b.log`, 29 Oct: CANYON up to 107.5 %, HALL 97.5 %), in every build since before v1.0.68; SIZE's change lands while the twin room is still fading. Noted as a known issue in the beta's notes. **The owner's call (10 Nov): this is next.**
- **Module CPU on the usual 19 lines** stays 0 late, worst 97.6 % (round 2's lap); s6's three laps were 98.0-98.6 %. Not under the 95 % target; the module drops audio near 99 %, and a line varies by up to 1.5 points between laps.
- **Issue 2's blast was not reproduced on a module.** Don't tell chriisu it's solved; they've been asked for a recording.

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments; `main` at v1.0.68; v1.0.69-v1.0.79 on `origin/beta/player-feedback` (PR 1, not merged) |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; the beta's plugin hasn't been played by the owner |
| M3 | v1.0.79's release build played on the module by the owner (10 Nov); its Core lapped as round 2 |
| M9 | Released v1.0.68 (Latest); `v1.0.76-beta.1` 6 Nov; `v1.0.77-beta.1` 9 Nov; **`v1.0.79-beta.1` 10 Nov** from `e9bc141` (v1.0.78 never released on its own) |

**main:** `452baab` plus this commit, docs only since v1.0.68; **10 commits ahead of origin, not pushed** (pushing needs the owner's OK; no `site/` change, so the site won't rebuild). **`beta/player-feedback`:** `e9bc141`, pushed, equal to origin.

## Start here

**The unfreeze-after-SIZE crackle** (the owner's call, 10 Nov: beta now, this next). In HALL and CANYON, turning SIZE in the second after letting go of FREEZE overloads the module: the whole SIZE move lands as one crossfade while the twin room is still running. Research first how established units handle a size change while the reverb is busy (`docs/research/`, memory "Research before designing"). Candidate directions from `docs/TASKS.md`: glide SIZE after an unfreeze, or hold SIZE's change until the twin has faded. Then a prototype on a `proto/` branch off `beta/player-feedback`, energy and click checks, a listening page for the owner, and a module lap with lines that turn SIZE within 0.5 s of an unfreeze (HALL and CANYON). It would ship as a beta 2 on the same branch.

Then, in order:
1. Players' feedback on the beta (PR 1, the Modwiggler thread, which still links `v1.0.76-beta.1`; issue 2).
2. The red output LEDs (`firmware/LedMeter.h`, `core/dsp/Output.h`), from the rack list.
3. The full release once players and the owner are happy: merge PR 1 (owner's OK), `tools/make_release.sh --publish` from `main`, re-render the site's demos, check the presets' starting points at DECAY 0.6-0.75, the site changelog, and a release note that saved sets at DECAY 0.85-0.95 now open as long tails.
4. The plugin UI (danishchairs' DM; Figma first, parameter keys unchanged). lfad's notes on ONSET's modes and PLATE vs HALL contrast. `input_stop2` needs re-rendering first.

## Waiting on Jesse

See `docs/TASKS.md`:
- **Should the GitHub release notes credit lfad by name?**
- **A Modwiggler post about `v1.0.79-beta.1`,** if wanted.
- **Pushing `main`'s 10 docs commits.**
- **Issue 2:** chriisu's recording, if it comes.
- **The clean-up list, not yet OK'd:** worktrees and branches `proto/morph-peak-hunt` (diagnostic, to drop), `proto/morph-last-98`, `proto/space-morph-c`, `claude/port-space-morph`, `claude/port-size-blast`, and now `proto/unfreeze-reshape` (holds the measuring lap's lines) and `claude/port-freeze-on-flip` (merged into the beta); `renders/morph_last_98` (293 MB, judged); `dist/v1.0.76-morph-s*`, `-morph-peak-hunt`, `dist/v1.0.78-unfreeze-hunt/`, `dist/v1.0.78-freeze-on-change/`, `dist/v1.0.78-morph-port/`. `proto/morph-last-98` alone holds the per-card chapters and shading on the page tool (`6df116d`), so bring those to `main` first if wanted.
- **The rest of the rack list** and trying the beta's plugin.

## In flight

**Nothing running, nothing uncommitted on `main`.** Untracked and harmless, mine: `review-build/`, `Testing/`, `review-build.log`, `review-ctest.log` in `.claude/worktrees/unfreeze-reshape`; `port-build/`, `port-*.log`, `test_audio/stimulus` in `.claude/worktrees/port-freeze`. The helper subagent from the freeze hunt is idle and not needed.

Kept on purpose:
- `beta/player-feedback`, worktree `.claude/worktrees/beta`; `.claude/worktrees/release` is the release script's own (detached at `e9bc141`).
- `claude/input-stop`, `claude/rack-levels`, `proto/size-blast` (not pushed): prototypes for open items, or their history.
- The branches on the clean-up list above, until the owner OKs it.

Local only: `dist/` builds, `dist/release/v1.0.79-e9bc141-beta.1/`; `dist/branches-backup-2026-11-08.bundle`. Unsure: the memory backup in `~/.Trash/sublustris-memory-2026-11-08/`.

## Will bite

- **CPU is at the edge:** 97.6-98.6 % worst on the usual lines against drops near 99 %. Anything that adds per-sample work during a change, a freeze or an unfreeze needs a lap. The unfreeze-after-SIZE fix must not add load inside the twin's fade.
- **The desktop can't hear these crackles:** both this session's crackles were module overloads with no click in desktop renders. Judge them with a lap (late blocks), not the click check.
- **A lap line can hide a gesture:** a FREEZE held 120 ms and one held 2 s behave differently (the C->P corner). Time the owner's real gesture, and ask them to say when in the gesture the crackle lands.
- **Flash on v1.0.79:** release 111,236 B (1,404 B under budget), **profile 130,628 B (444 B under the chip)**. Measuring lap lines cost about 400 B of profile, so a new lap may need to drop other lines (round 2's 32-line build left 12 B).
- **Known, in the pick to the bit:** a freeze in the first half-second of HALL -> PLATE with SHIMMER on leaves about -59 dBFS above 5 kHz for about 0.1 s.
- **Level at transitions, kept on purpose:** worst +3.8 dB over the louder steady space at CANYON -> HALL, SIZE 0, DECAY 0.5; an in-place tail at endless settles about 4 dB above today's.
- **The trims change boot code** (clocks, SDRAM, codec I2C, ADC, DMA), proven so far by listening at the rack. Cold starts and an hour warm aren't confirmed.
- **Unexplained:** output depends on what ran earlier in the process, even after `Engine::reset()`. SIZE's residual rises (a low note into a short CANYON, about +10-12 dB; a big PLATE shrink on a steady 70 Hz, up to +13.7 dB) aren't addressed.
- **Flash on `main` is untrimmed** (116 B free on profile) until the beta merges.
- **PR 1 conflicts, if they show,** are usually `docs/TASKS.md` (take main's) or `docs/adr/README.md` (keep both, in number order).
- **Release notes for a beta** are hand-written (`--release-notes <file>`), following the last beta's shape (GitHub: `gh release view v1.0.79-beta.1`), and shown to the owner before publishing.
- **Page server:** use `preview_start` "review-pages-lan" (8766) and check the link answers before sending it.
- **`/research` can't run here** (`uv` missing, wrong repo path in the skill).
- **Disk: 18 GB free (10 Nov).** Each page is 0.3-0.6 GB.
- As before: `screenlog.0` appends across flashes; never connect USB and rack power at once; in zsh `echo =====` fails and `ls` is eza; a new worktree needs `tools/make_stimulus.py` and firmware built with `LIBDAISY_DIR=` the main checkout's built `libs/libDaisy`.

## Gates

- **main (`cdae028`, docs only since):** ctest **"100% tests passed out of 24"**; release **111,828 B**, m0test 83,556, profile 130,956. Installed in Ableton: `4140997` (v1.0.68).
- **`beta/player-feedback` v1.0.79 (`d4f0a5d`, the same tree as `e9bc141`):** ctest **"100% tests passed out of 26"** (re-run by me); fresh firmware build in a new worktree: release **111,236 B** (`cmp` identical to round 2's and to the published firmware), m0test 83,556, profile **130,628 B**. Not re-run on the port (the Core is identical to `3bfeb76`): `same_render` SAME 60/60 against v1.0.78, click check 7,680 renders (peak 0.814 vs 0.805), energy 216 renders finite, endless 0.00 dB. Goldens unchanged. Plugin test not re-run since v1.0.78.
- **Published `v1.0.77-beta.1` (`f4814a7`):** release 112,228 B, profile 131,068 B.

## Where to look

- `docs/TASKS.md`, top of "At the rack" and the unfreeze-after-SIZE item.
- `docs/adr/0054-space-morph.md`, `0055-freeze-on-flip.md` (on the beta branch).
- `docs/research/m3-review2-audit-212b68b.log` (the SIZE overload), `m3-freeze-on-change-2-lap.log`.
- `firmware/main.cpp` around `RunMorphLap` (the beta's), and `proto/unfreeze-reshape`'s version for the ATFLIP/FRZMOVE lap lines.
