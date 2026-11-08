# Handoff

**Written:** 8 Nov 2026, end of session 17 (8 Nov).

- **Smooth SPACE changes, picked and built as a prototype.** Today a SPACE change fades out, wipes the tanks and fades in. The owner picked **S on all 20 cards** of `renders/space_morph2` (after C on all 20 scored cards of `renders/space_morph`): a 3 s morph between PLATE and HALL/CANYON (both tanks run for 3 s), and HALL ↔ CANYON reshaped in place in the one tank, spread over SIZE's four groups of 0.3 s (never more than 2 of 8 lines reading twice).
- **The owner's call:** a held endless tail with nothing playing dies away in the 3 s on a PLATE change, as built.
- **Flash found, no sound change:** STM32's HAL set-up files and DMA driver built `-Os` (`HAL_OS=1`), plus 61 member initialisers dropped (`IV_RI`). Release 112,172 → 106,564 B, without the morph. The constructors lead was only 360 B, not 1.7 KB.
- **The trimmed v1.0.76 played on the rack:** "sounding great! No issues at all", rapid knob moves at settings that push the unit included.
- **The shared `/session-start` had its first real run;** nothing from the old routine is missing. This is `/session-wrap`'s first. The old wrap pushed on its own; pushing is now ask-first.
- **The Modwiggler post is up** (owner). PR 1 had only Vercel's bot at this session's start; the thread itself wasn't read. **Corrected 9 Nov:** player feedback had come in. [Issue 2](https://github.com/jeffebauer/sublustris-versio/issues/2) (chriisu, 6 Nov, v1.0.68 on the module, no reply yet): a fast SIZE move from big to small makes a very loud blast, in HALL or CANYON. Not reproduced; unknown on v1.0.76.

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments; `main` at v1.0.68; v1.0.69-v1.0.76 on `beta/player-feedback` (PR 1), not merged. The morph is prototype only, no SPEC version yet |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; the beta's plugin hasn't been played by the owner |
| M3 | Trimmed v1.0.76 on the module and liked (first listen); the morph has never run on a module; the red-LED recheck and the rest of the rack list open |
| M9 | Released v1.0.68 (Latest); `v1.0.76-beta.1` published 6 Nov |

**main HEAD:** a clean-up commit on top of `f107e4e` (the wrap). Docs only since session 15. All of it is pushed. Nothing merged or released this session. All gates green.

## Start here

**Corrected 9 Nov:** the first lap (flashed from `dist/v1.0.76-morph-s/`) never flipped SPACE: `gMorphEv` wasn't volatile, so the compiler dropped the event times (checked in the ELF). Its lines are only steady loads for each start space, about PLATE 50 %, HALL 60 %, CANYON 69-78 % (`docs/research/m3-morph-s-lap-no-flips.log`). The lap is fixed on `proto/space-morph-c` (19 lines, each printing `fired N`; expect 1 on a plain change, 2 on REFLIP, 3 on UNFREEZE); profile 130,996 B, **76 B under the chip**. A desktop trace puts the worst moment at 2.4-3.05 s after a PLATE → CANYON change, inside the window, and guesses a peak of 80-100 %: only the module can say.

**1. Read the owner's SPACE CPU lap.** Build: `dist/v1.0.76-morph-s2/sublustris_versio-v1.0.76-morph-s2-profile.bin`, USB power. Copy `screenlog.0`, never move it; slice `SPACE START` to `SPACE LAP n DONE` (19 lines, ~3 min); a subagent files it as `docs/research/m3-…log`. Each line: `SPACE <FROM>-><TO> <heavy|SHIMMER|REFLIP|UNFREEZE> … avg % max % | late blocks`. The pass: no late blocks, and max under about 95 % (the module starts dropping audio near 99 %). PLATE → CANYON with SHIMMER is the likeliest to go over; my sum of averages was 89-101 %.

**Lap result, 9 Nov (`docs/research/m3-morph-s2-lap.log`, every line fired as expected): not clean.** Worst 105.9 %, 966 late blocks, all on two swap lines where SPACE changes again 1.2 s into a morph out of CANYON (`CANYON->PLATE->HALL` 101.1 %, 100 late; `CANYON->PLATE->CANYON` 105.9 %, 866 late). Also over 95 % with no late blocks: `PLATE->CANYON` SHIMMER 99.2 %, `HALL->CANYON` SHIMMER 97.5 %, and two other REFLIP lines (97.6 %, 98.1 %). The other 13 lines are at 94.6 % or below; the UNFREEZE lines 91 %. Owner by hand on the release build: no dropouts. Not ported. **The owner's call (9 Nov): a second flip during a morph waits for it to finish, then goes;** when it ends, the switch's latest position decides, so a there-and-back morphs there, then back (up to about 6 s). Built as `ec361fd` on `proto/space-morph-c` (release 110,484 B, profile 130,804 B; clean rebuild byte-identical; with the morph on, ctest 96 % of 25, only `test_control_latch`). The lap on it: `dist/v1.0.76-morph-s3/`, REFLIP windows lengthened. SHIMMER into CANYON (97.5-99.2 %, 98.2-98.4 % warm) is still to deal with.

**2. If it's clean, port S** to `beta/player-feedback` as a new SPEC version (v1.0.77) and `v1.0.77-beta.1` (run the release script from the beta worktree). The port needs:
- an ADR amending 0013 ("only one big tank ever runs"), a SPEC changelog line, one golden update;
- `test_control_latch` changed: it asserts the tail is wiped, the only test that fails with S on;
- S always on: the A/B/C/S hook, `IV_SPACE_MORPH` and its log removed, `SPACE_MORPH` no longer a measuring flag;
- the trims `4327229`, `f9912f9`, `faf55db`; `firmware/README.md` documents `HAL_OS`; `firmware/Makefile`'s comment still says the morph is C only;
- the Plugin's test (24 of 25 pass in worktrees, which have no JUCE; run the Plugin against main's JUCE).

**If the lap isn't clean:** spread the PLATE-side load or shorten its 3 s, or cut SHIMMER's share during the change, and bring it back to the owner by ear.

Then, in order:
3. Offer the trims to the beta on their own: 5.6 KB of room, already played. The owner's call.
4. Players' feedback on the beta (PR 1, the Modwiggler thread). Issue 2, the SIZE blast, is the urgent one: it's on the public release. The owner put the CPU lap first on 9 Nov; where the hunt falls against the port is still to agree.
5. The red output LEDs (`firmware/LedMeter.h`, `core/dsp/Output.h`), from the rack list.
6. The full release once players and the owner are happy: merge PR 1 (owner's OK), `tools/make_release.sh --publish` from `main`, re-render the site's demos on the new sound, check the presets' starting points at DECAY 0.6-0.75, the site changelog, and a release note that saved sets at DECAY 0.85-0.95 now open as long tails.
7. The unfreeze-after-SIZE overload in CANYON, the most important crackle fix (`docs/research/m3-review2-audit-212b68b.log`). The owner said "not now" on 8 Nov.
8. The plugin UI (danishchairs' DM; Figma first, parameter keys unchanged). lfad's notes on ONSET's modes and PLATE vs HALL contrast. As before: `input_stop2`, the housekeeping list.

## Waiting on Jesse

See `docs/TASKS.md`:
- **The CPU lap for the morph** (planned for the morning of 9 Nov). The release build `dist/v1.0.76-morph-s/sublustris_versio-v1.0.76-morph-s.bin` is there to play by hand on rack power.
- **At the rack:** the red-LED recheck, the rest of the v1.0.76 list on the trimmed build (`dist/v1.0.76-trimmed/`), cold starts and an hour warm, the module's input volts.
- **Try the beta's plugin** (`tools/install_plugin.sh e4a9738`; update the "Plugin installed" line).
- **Answer:** should the GitHub release notes credit lfad by name?

## In flight

**Nothing running.** All three prototype agents finished and were reviewed: firmware rebuilt from scratch, `SAME 60/60` re-run, default release `cmp`-identical to the build played on the rack. The 11 finished sessions started earlier were archived.

Kept on purpose:
- **`proto/space-morph-c`** (`b6b9b0a` plus the 9 Nov lap fix, not pushed, worktree `.claude/worktrees/space-morph-flash`; the folder name is older than the branch): S, the trims (`faf55db` and before) and the SPACE lap. **This is the one to port.**
- `beta/player-feedback` (pushed, PR 1), worktree `.claude/worktrees/beta`; `.claude/worktrees/release` is the release script's own. Keep both.
- `claude/input-stop` and `claude/rack-levels` (not pushed): prototypes for open items, to re-measure on the current build.

Local only: `dist/v1.0.76-trimmed/`, `dist/v1.0.76-morph-s/`, `dist/branches-backup-2026-11-08.bundle` (the 12 prototype branches deleted in the clean-up; `git fetch <bundle>` restores any). `renders/space_morph` and `space_morph2` are deleted; re-render from `proto/space-morph-c` (`tools/pages/space_morph2.py`) if a comparison is needed. Unsure: the memory backup in `~/.Trash/sublustris-memory-2026-11-08/` (the shell can't list the Trash).

## Will bite

- **Morph flash, on `proto/space-morph-c`:** release 110,708 B (1,932 B under budget), **profile 130,796 B (276 B under the chip)**, ITCM 62,432 of 64,512 B. To fit, the morph's profile build leaves out SIZEHAND and the HEADROOM steps. There are more bytes in libDaisy's C++ (about 10 KB at `-O3`), leaving `audio.cpp` out because it runs per sample.
- **Module CPU for the morph is unmeasured.** `morphRunOld` is `-O3` in ITCM, but `morphFlip` and `morphAdvance` run from flash, and `morphAdvance` runs every block, a small cost even when idle.
- **The trims change boot code** (clocks, SDRAM, codec I2C, ADC, DMA), proven so far by one good listen at the rack. Cold starts and an hour warm aren't confirmed. `IV_RI`'s proof ran on the host, not on ARM.
- **Level at transitions, kept on purpose (the owner picked S having heard them on SIZE-full cards):** worst +3.8 dB over the louder steady space at CANYON → HALL, SIZE 0, DECAY 0.5, which the page didn't play; +2.2 dB at SIZE full. An in-place tail at endless settles about 4 dB above today's.
- **The half builds** (`SPACE_MORPH=1` and `2`) are size measurements only: never listened to, and the wet dips on the other half's flips.
- **Not tested:** freeze or re-flips combined with BLOOM or SHIMMER ONSET; the 10-minute holds with a flip inside them.
- **Page server:** port 8765 was held by `pearls`' `beads/test/listen.py` on 8 Nov and was free on 9 Nov. Use `preview_start` "review-pages-lan" (8766), and check the link answers before sending it, because the app stopped it once within about an hour.
- **`/research` can't run here:** `uv` isn't installed and the skill's stated repo path is wrong (the script is in `~/.claude/skills/obsidian-second-brain`). Nothing from this session's research went to the vault.
- **Disk: 18 GB free (9 Nov).** Each page was 0.5-0.6 GB.
- **Flash on the beta and on `main` is untrimmed:** 468 B left on v1.0.76 release, 84 B on profile, until the trims are ported.
- As before: one golden update per SPEC version; `screenlog.0` appends across flashes; never connect USB and rack power at once; `make_release.sh` reads `releases/README.md` from the checkout it runs in, and `--dry-run` needs the branch pushed; if PR 1 shows conflicts they'll be in `docs/TASKS.md` (take main's) or `docs/adr/README.md` (keep both, in number order). **It conflicts now (9 Nov), in other files:** `docs/research/decay-curve-2026-11-06.md` and `wet-loudness-2026-11-05.md`, which the clean-up commit also added to `main`. Docs only; not resolved yet.

## Gates

- **main (`c677ec5`, 8 Nov):** ctest **"100% tests passed out of 24"**; release **111,828 B** (85 %), m0test 83,556 (63 %), profile 130,956 (99 %, 116 B under the chip). Installed in Ableton: `4140997` (v1.0.68), the same sound as `main`.
- **v1.0.76 beta (`e4a9738`, from session 15, unchanged):** ctest "100% tests passed out of 25"; release 112,172 B, m0test 83,556, profile 130,988 B.
- **`proto/space-morph-c` (`b6b9b0a`), default build, sound of v1.0.76 (re-run this session):** release 106,564 B, `cmp`-identical to the rack build; m0test 83,556; profile 126,532 B; `SAME 60/60` against `e4a9738`. ctest "100% tests passed out of 24" (agent; no JUCE in the worktree).
- **`SPACE_MORPH=3` on the same commit:** release 110,708 B, profile 130,796 B (re-run this session). With S on, ctest "96% tests passed, 1 tests failed out of 24", only `test_control_latch` (agent).

## Where to look

- `docs/TASKS.md` ("At the rack", "SPACE flips morph between spaces" under Waiting on Claude).
- `git log e4a9738..proto/space-morph-c` and `core/dsp/SizeOpt.h` (`IV_MORPH_*`).
- `docs/adr/0013-space-crossfade-and-true-stereo-in.md`, `docs/adr/0025-size-and-time-crossfade.md`.
- `firmware/main.cpp` around the `SPACE START` lap.
