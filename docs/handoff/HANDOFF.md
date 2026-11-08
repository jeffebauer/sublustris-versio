# Handoff

**Written:** 8 Nov 2026, end of session 15 (6 to 8 Nov); checkpoint later on 8 Nov, session 16 (the session skills moved over; no sound or code work); corrected against the repo by session 17's start, 8 Nov.

This session took the launch feedback to a **public beta**:
- **Every pick ported, one SPEC version each,** stacked v1.0.69 → v1.0.76: WEAR's taper (0044), the tape's drive (0045), a fuller tail (0047), DECAY's curve (0048), the louder wet (0049), PLATE's hit (0050), WEAR's ceiling (0051), HALL's low notes (0052), plus the tape's one-sample tick fix (`ff52544`).
- **HALL's left-side build-up** (the owner's Serge patch on the rack) was traced to the still low tank's narrow, chance-panned resonances. The fix was picked on `renders/hall_lows` (D: the lows narrowed to 0.30, in-loop diffusion 0.60 at every SIZE, +3.3 dB). Its port added one change the page didn't have: the low tank's allpass lengths follow the SIZE the lines crossfade to, which stops a smear on SIZE flicks.
- **v1.0.76 profiled on the module** (`docs/research/m3-v1076-hall-lows.log`): HALL 52-58 % average, worst 78 %; it fits. The owner played the release build on the rack: "sounding really good".
- **`v1.0.76-beta.1` published** (pre-release, never Latest; https://github.com/jeffebauer/sublustris-versio/releases/tag/v1.0.76-beta.1) from `beta/player-feedback` (`e4a9738`: `b6f325e` plus a merge of `origin/main` for ADR 0046's tooling, no code change). Its firmware is byte-identical to the rack build; checksums verified on the downloaded assets. **PR 1** into `main`: https://github.com/jeffebauer/sublustris-versio/pull/1 (mergeable, clean).
- **The owner's Modwiggler post** (BBCode, crediting lfad, danishchairs and dubsackdude, in their own reply voice) is posted (owner, 8 Nov).
- **Clean-up:** judged pages, 16 worktrees and eight merged branches gone; 17 GB free when session 17 started.
- **Session 16 (docs only):** `/sublustris-start` and `/sublustris-wrap` retired for the shared `/session-start`, `/session-checkpoint` and `/session-wrap`; this project's facts are in `.claude/session.md`, the orchestrator guide in `docs/orchestrator.md`, the handoff's shape in `docs/handoff/TEMPLATE.md`. Then a writing-for-agents pass over `CLAUDE.md`, the session file and the memory (32 files to 28).

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments; `main` at v1.0.68; **v1.0.69-v1.0.76 on `beta/player-feedback`** (PR 1), not merged |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; **the beta's plugin hasn't been played by the owner** (its Plugin test passes) |
| M3 | v1.0.76 on the module, profiled (it fits) and liked; the red-LED recheck and the rest of the rack list open |
| M9 | Released v1.0.68 (Latest); **v1.0.76-beta.1 published** 6 Nov |

- **main HEAD:** docs commits on top of `3de8889` (the agent-docs review) and `949593f` (the session skills). `3de8889` and everything before it is pushed; the later docs commits (the checkpoint, two task-list tidies, session 17's corrections) aren't unless the owner says so. Main changed no sound in sessions 15 or 16.

## In flight

- **Nothing running.** Linked sessions are all idle (decay-curve, wear-taper, wet-loudness, fuller-tail, port sessions, hall-lows-page, launch-posts, resilio-site-links, plateau ones); they can be archived with the owner's OK.
- **A backup of the memory folder** from before the pruning is in `~/.Trash/sublustris-memory-2026-11-08/` (all 32 files plus the index). The Trash can go once the memory has proved fine.
- **`beta/player-feedback`** (pushed, PR 1): the beta. Its worktree `.claude/worktrees/beta`; `.claude/worktrees/release` is the release script's own (keep both).
- **Prototype and research branches kept** (not pushed; each holds only its hook and page scripts or notes): `claude/fuller-tail-6dcc8d`, `claude/proto-decay-curve-01af2c`, `claude/wet-loudness-8e55d6`, `claude/plate-hit`, `proto/wear-ceiling`, `claude/proto-hall-lows-f1f7e5`, `claude/hall-left-hunt`, `claude/plateau-research`. Per ADR 0046 they go with the owner's OK once the beta merges. Older: `claude/rack-levels`, `claude/input-stop` (to re-measure), `proto/more-tape`, `proto/radio-lighter`, `proto/tape-fold`, `proto/wear-no-bbd` (ported long ago; delete with OK).

## Start here

**First:** `/session-start` had its first real run in session 17 and nothing from the old routine is missing; `/session-wrap` has yet to have one (the session-skills README asks for one of each). The old wrap pushed on its own; pushing is now ask-first. Fix `.claude/session.md`, not the shared skills, if a gap is a project fact. Then:

1. **Players' feedback on the beta** (PR 1, the Modwiggler thread). A tuning change is a new SPEC version on `beta/player-feedback` (v1.0.77) and a new beta (`v1.0.77-beta.1`), with a comment on PR 1; a repackage is `v1.0.76-beta.2`. Run the release script from the beta worktree (memory "betas from the beta worktree").
2. **The red output LEDs** (owner's recheck): if they hear squash or crackle, bring level options (the limiter catches peaks since v1.0.73's +3 dB, +1.5 at short tails); if only the light, maybe leave it or make the LED less jumpy.
3. **The full release** once players and the owner are happy: merge PR 1 (owner's OK), `tools/make_release.sh --publish` from `main`, re-render the site's demos on v1.0.76, check the presets' starting points at DECAY 0.6-0.75 (the curve moved), the site changelog, and a release note that saved sets at DECAY 0.85-0.95 now open as long tails (the owner accepted this).
4. **The unfreeze-after-SIZE overload in CANYON:** still the most important crackle fix (`docs/research/m3-review2-audit-212b68b.log`).
5. **The plugin UI** (danishchairs' DM; Figma first, compact DAW layout; parameter keys unchanged).
6. **lfad's other notes not yet acted on:** ONSET's modes sounding alike, and PLATE vs HALL contrast beyond PLATE's hit; ask after the beta.
7. As before: `input_stop2` (re-render on the current build), the SPACE flip morph (a flash plan first: 468 B left), the housekeeping list, the module's volts.

## Waiting on Jesse (`docs/TASKS.md`)

- **At the rack:** the red-LED recheck and the rest of the check list on v1.0.76; the module's input volts.
- **Try the beta's plugin** in Ableton (`tools/install_plugin.sh e4a9738` installs it; update the "Plugin installed" line).
- **Answer:** whether the GitHub release notes should credit lfad by name (they say "feedback on Modwiggler and r/modular"; the launch posts promised changelog credits).

## Will bite

- **Flash:** release 468 B left on v1.0.76, profile 84 B. Anything new needs a trim plan first.
- **The profile lap has no v1.0.75 baseline**, so D's exact CPU cost is unknown, and it never moves SIZE in HALL; the owner heard no drop-outs by hand.
- **The beta branch has a merge of `origin/main`** (`e4a9738`); main is now ahead with docs only. If PR 1 shows conflicts later, they'll be in `docs/TASKS.md` (take main's) or `docs/adr/README.md` (keep both lists, in number order).
- **`make_release.sh` reads `releases/README.md` from the checkout it runs in**, not the ref: run betas from the beta worktree. `--dry-run` also needs the branch pushed.
- **The release notes are the GitHub release body**, edited with `gh release edit v1.0.76-beta.1 --notes-file`; the zip's read-me has its own copy (`THIS IS A BETA` section).
- **reddit is blocked in the built-in browser;** ask the owner for screenshots (memory "owner forum voice").
- **The new session setup is unproven:** `.claude/session.md` was written from the old skills line by line, but no session has opened from it yet.
- **The preview server's port (8765) is held by the `pearls` project's `beads/test/listen.py`** (session 17's start). Ask before stopping it, or serve the next page from `main` on another port.
- As before: one golden update per SPEC version; `screenlog.0` appends across flashes (copy, don't move); never connect USB and rack power at once.

## Gates

- **Gates on main (8 Nov, session 15's wrap-up at `667e99b`; not re-run since: only docs and `CLAUDE.md` changed):** ctest **"100% tests passed out of 24"**; firmware unchanged from v1.0.68: release **111,828 B** (85 %), m0test 83,556 (63 %), profile 130,956 (99 %, 116 B under the chip). Installed in Ableton: `4140997` (v1.0.68), the same sound as `main`.
- **v1.0.76 (`b6f325e`, the beta's sound):** ctest "100% tests passed out of 25" (the port's session; Plugin test included); release **112,172 B** (468 B under the 112,640 budget), m0test 83,556, profile **130,988 B (84 B under the chip)**; my rebuild from `git archive` byte-identical.

## Where to look

- `docs/TASKS.md` ("Share the beta", "At the rack", "Waiting on Claude").
- `docs/adr/0046-betas-from-branches.md`, then 0051 and 0052 on `beta/player-feedback`.
- PR 1 and the release page (links above).
- `tools/make_release.sh` (`--beta`).
- `firmware/LedMeter.h` and `core/dsp/Output.h` for the red-LED question.
