# Handoff

**Written:** 9 Nov 2026, end of session 18 (9 Nov).

- **The SPACE morph is ported as v1.0.78 and merged into `beta/player-feedback`** (`40850ee`, local, not pushed or published; ADR 0054 amending 0013). A SPACE change keeps the tail: PLATE with HALL or CANYON morphs over 3 s on two tanks, HALL with CANYON reshapes one tank over 1.5 s, a change during one waits for it to end. Nobody has heard the port on a module, and it hasn't had its own lap.
- **The owner picked C on every card of `renders/morph_last_98`:** during a change into CANYON with SHIMMER on, 4 shimmer heads a side instead of 8. Its first build spiked to 99.5 % at the handover; the fix keeps C's code out of the handover's block.
- **Update, 10 Nov:** the owner's first listen to v1.0.78 found one repeatable crackle: FREEZE pressed as SPACE flips (the four PLATE changes, and HALL -> CANYON in the first report), one dropout after the unfreeze. The desktop finds no click. The measuring lap (`docs/research/m3-freeze-at-flip-lap.log`; `proto/unfreeze-reshape`, `fa838a7`) overruns on all 9 lines with FREEZE 100 ms after a flip (104-120.7 %, 30-843 late blocks), peaking within about 100 ms of the press, not at the unfreeze, so whether that is the crackle heard is still open. The usual 19 lines: 0 late, worst 97.6 %. The beta waits for this (`docs/TASKS.md`).
- **Module CPU (s6, `docs/research/m3-morph-s6-lap.log`): 0 late blocks, worst 98.2 %.** `screenlog.0` holds three full s6 laps, worst 98.2, 98.6 and 98.0 % (corrected 10 Nov: the log note filed one). Level with before C (s4), not under the 95 % target. The module starts dropping audio near 99 %, and a line varies by up to 1.5 points between laps.
- **`v1.0.77-beta.1` is published** (pre-release; the site's downloads stay on v1.0.68): the SIZE safety limit for issue 2. PR 1 has a comment linking it.
- **Issue 2's blast was not reproduced on a module** (v1.0.68, input LEDs red, CANYON, BLOOM, full to none: a slight swell only). The desktop cause is real but not shown to be what chriisu heard, so don't tell them it's solved. They've been asked for a recording.
- **Page tool:** a card's length is now its versions', not the longer dry's (`7f20fbd`, on `main`).

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments; `main` at v1.0.68; v1.0.69-v1.0.77 on `origin/beta/player-feedback` (PR 1, mergeable, not merged); v1.0.78 on the local beta branch only |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; the beta's plugin hasn't been played by the owner |
| M3 | Trimmed v1.0.76 played on the module and liked. Morph prototypes lapped on the module (s6 the last). v1.0.78 itself never run on a module |
| M9 | Released v1.0.68 (Latest); `v1.0.76-beta.1` 6 Nov; `v1.0.77-beta.1` 9 Nov from `f4814a7` |

**main:** this commit, docs and page-tool work only since v1.0.68; pushed at this wrap (the site doesn't rebuild: no `site/` or `docs/minisite/` changes). **`beta/player-feedback`:** `40850ee`, 2 commits ahead of origin, deliberately not pushed until the owner has heard it. All gates green.

## Start here

**Read the owner's rack check of v1.0.78.** Two builds in `dist/v1.0.78-morph-port/`: `sublustris_versio-v1.0.78-morph-port.bin` to hear (rack power) and `…-profile.bin` for the lap (USB power only). Copy `screenlog.0`, never move it; 19 lines; a lap's end is a rise in the count of `SPACE LAP [0-9]+ DONE`. Compare with `docs/research/m3-morph-s6-lap.log`: M7 timing can shift when code moves in memory, even with the same sound.

If it's good, with the owner's OK: push `beta/player-feedback`, then `tools/make_release.sh --beta <ref> 1 --publish` as `v1.0.78-beta.1` **from `.claude/worktrees/beta`** (the script reads that checkout's `releases/README.md`, and `--dry-run` needs the branch pushed). Comment on PR 1 linking it.

If CPU is worse or a late block shows up: the handoff's `apply` stage (about 4 points of its block, needs a settings struct across about 440 lines) and `CANYON->PLATE->CANYON` are what's left; other options give up sound and go back to the owner by ear.

Then, in order:
1. Players' feedback on the beta (PR 1, the Modwiggler thread, issue 2).
2. The red output LEDs (`firmware/LedMeter.h`, `core/dsp/Output.h`), from the rack list.
3. The full release once players and the owner are happy: merge PR 1 (owner's OK), `tools/make_release.sh --publish` from `main`, re-render the site's demos, check the presets' starting points at DECAY 0.6-0.75, the site changelog, and a release note that saved sets at DECAY 0.85-0.95 now open as long tails.
4. The unfreeze-after-SIZE overload in CANYON (`docs/research/m3-review2-audit-212b68b.log`). The owner said "not now" on 8 Nov.
5. The plugin UI (danishchairs' DM; Figma first, parameter keys unchanged). lfad's notes on ONSET's modes and PLATE vs HALL contrast. `input_stop2` needs re-rendering first.

## Waiting on Jesse

See `docs/TASKS.md`:
- **Listen to v1.0.78 and run its lap** (top of "At the rack").
- **Should the GitHub release notes credit lfad by name?**
- **Issue 2:** chriisu's recording, if it comes.
- **The clean-up list, not yet OK'd:** worktrees and branches `proto/morph-peak-hunt` (diagnostic, to drop), `proto/morph-last-98`, `proto/space-morph-c`, `claude/port-space-morph`, `claude/port-size-blast`; `renders/morph_last_98` (293 MB, judged); `dist/v1.0.76-morph-s*`, `-morph-peak-hunt`. `proto/morph-last-98` alone holds the per-card chapters and shading on the page tool (`6df116d`), so bring those to `main` first if wanted.
- **The rest of the rack list** and trying the beta's plugin.

## In flight

**Nothing running.** The port agent finished and was reviewed: diff read, ctest and a clean firmware rebuild re-run on the commit, the merged beta's tree confirmed equal to `c1e54d2`. Not re-run by me (the agent's numbers): `same_render` 59/60 against `f4814a7` (only `flips` differs, the one case that changes SPACE); bit-identical to the pick in 116/116 cases; the Plugin's test against main's JUCE; click check, energy and infinite holds. Goldens passed unchanged, so v1.0.78 has no golden update.

Kept on purpose:
- `beta/player-feedback`, worktree `.claude/worktrees/beta`; `.claude/worktrees/release` is the release script's own.
- `claude/input-stop`, `claude/rack-levels`, `proto/size-blast` (not pushed): prototypes for open items, or their history.
- The branches on the clean-up list above, until the owner OKs it.

Local only: `dist/` builds; `dist/branches-backup-2026-11-08.bundle` (12 deleted prototype branches). Unsure: the memory backup in `~/.Trash/sublustris-memory-2026-11-08/`.

## Will bite

- **CPU is at the edge:** 98.2 % worst on one lap against drops near 99 %. The diagnostic laps once hit 100.1 % with 1 late block (C still in the handover's block then). Anything that adds per-sample work during a change needs a lap.
- **Flash on v1.0.78:** release 111,164 B (1,476 B under budget), **profile 130,580 B (492 B under the chip)**, ITCM 62,848 of 64,512 B. Morph profile builds leave out SIZEHAND, the HEADROOM steps and some MICRO tests.
- **The one sound difference from the pick:** v1.0.77's SIZE limit also applies in the reshape, moving CANYON -> HALL at DECAY 0.15 and under (largest sample difference 0.31 at DECAY 0.05, SIZE 1; peak and loudest 50 ms about the same). From DECAY 0.3 up nothing moves.
- **Known, in the pick to the bit:** a freeze in the first half-second of HALL -> PLATE with SHIMMER on leaves about -59 dBFS above 5 kHz for about 0.1 s.
- **Level at transitions, kept on purpose:** worst +3.8 dB over the louder steady space at CANYON -> HALL, SIZE 0, DECAY 0.5; an in-place tail at endless settles about 4 dB above today's.
- **The trims change boot code** (clocks, SDRAM, codec I2C, ADC, DMA), proven so far by listening at the rack. Cold starts and an hour warm aren't confirmed; `IV_RI`'s proof ran on the host.
- **Unexplained:** output depends on what ran earlier in the process, even after `Engine::reset()`. Harness or a real gap in `reset()`; not looked at. SIZE's residual rises (a low note into a short CANYON, about +10-12 dB; a big PLATE shrink on a steady 70 Hz, up to +13.7 dB) aren't addressed.
- **Flash on `main` is untrimmed** (116 B free on profile) until the beta merges; the trims came with the morph.
- **PR 1 conflicts, if they show,** are usually `docs/TASKS.md` (take main's) or `docs/adr/README.md` (keep both, in number order).
- **Page server:** use `preview_start` "review-pages-lan" (8766) and check the link answers before sending it; 8765 is sometimes held by `pearls`.
- **`/research` can't run here** (`uv` missing, wrong repo path in the skill).
- **Disk: 20 GB free (10 Nov).** Each page is 0.3-0.6 GB.
- As before: `screenlog.0` appends across flashes; never connect USB and rack power at once; in zsh `echo =====` fails (`=word` expansion) and `ls` is eza.

## Gates

- **main (`cdae028`, docs only since):** ctest **"100% tests passed out of 24"**; release **111,828 B** (85 %), m0test 83,556 (63 %), profile 130,956 (99 %). Installed in Ableton: `4140997` (v1.0.68), the same sound as `main`.
- **`beta/player-feedback` v1.0.78 (`c1e54d2`, the same tree as `40850ee`):** ctest **"100% tests passed out of 26"**; clean rebuild release **111,164 B** (84 %, budget 112,640), m0test 83,556 (63 %), profile **130,580 B** (99 %). Plugin's test "100% of 1" (agent, against main's JUCE).
- **Published `v1.0.77-beta.1` (`f4814a7`):** release 112,228 B, profile 131,068 B.

## Where to look

- `docs/TASKS.md`, top of "At the rack".
- `docs/adr/0054-space-morph.md` (on the beta branch).
- `docs/research/m3-morph-s6-lap.log`, `m3-morph-s4-lap.log`, `m3-morph-peak-hunt.log`.
- `firmware/main.cpp` around `RunMorphLap`, and `tools/make_release.sh`.
