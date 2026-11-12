# Handoff

**Written:** 12 Nov 2026, end of session 21 (12 Nov).

- **Fable's review** (brief and report on `proto/fable-review`, `docs/research/fable-review-2026-11-12.md` there; page `renders/fable_review`). The owner picked:
  - the radio band arriving earlier on MOTION's right;
  - PLATE's long tails keeping their mids;
  - CANYON's bass centred and fuller in mono;
  - three CPU savings nobody can hear.
- **CANYON's whole wet level:** the owner picked C on all 25 cards of `renders/canyon_level`.
- **v1.0.81:** those picks were ported on `claude/port-fable` (ADR 0059, plus amendments). It was lapped twice, on the 55-line landing set and the 47-line ATFLIP set: 0 late blocks, worst 98.4 %.
- **CANYON endless highs:**
  - The whole-tank turnover page (`renders/canyon_turnover`) was rejected ("highs remove the lows").
  - The owner then picked **C, "mellow and wear"**, on all 7 cards of `renders/canyon_darken`: a held tail's top mellows, and old highs wear while played; the lows stay endless.
  - It was fitted into flash with bit-identical savings (`proto/canyon-darken-fit`, `0ddbfcc`).
- **The rack crackle** (FREEZE right after a CANYON <-> PLATE flip) is found and fixed on `proto/flip-freeze` (`3d5a3a9`):
  - Cause: the flip saved SIZE together with the pot's jitter, so a frozen CANYON crossfaded all its lines for ~1.7 s.
  - It was lapped: those lines went from up to 98.4 % to 92-93 %.
  - Audible change: the move into PLATE starts ~1 s sooner. The owner said that's fine.
- **v1.0.82 candidate:** both changes are combined on `claude/v1082` (`08e1ba3`, which carries v1.0.81). Its lap build is `proto/lap-v1082` (`a331a08`): 2 laps, 0 late blocks, worst 97.7 % then 96.5 %.

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | `main` at v1.0.68. `beta/player-feedback` at v1.0.80 (PR 1, open). `claude/v1082` at v1.0.82 (v1.0.81 + v1.0.82, not merged into the beta). ADRs 0059 and the 0008/0022 amendments live on that branch only |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; no later plugin installed |
| M3 | The module runs the v1.0.82 lap build `a331a08` (profile); lapped clean |
| M9 | v1.0.68 Latest; `v1.0.80-beta.1` the newest pre-release; beta 3 (v1.0.82) not published |

**main:** `01d7752` plus this wrap's commit: docs, research logs and the task list only (no code change). Pushed to origin on 13 Nov (docs only, so the site was not rebuilt). **`beta/player-feedback`:** `4f6f950`, equal to origin.

## Start here

**The owner decides CANYON endless's level after a rack listen.** C plays bright material 2-2.5 LU under v1.0.81 while played and up to 4.6 LU under once held; they listened level-matched. The choice is keep it quieter, or add make-up back to v1.0.81's level.
- If they choose make-up: there are only 12 B of flash. It's a constant change in `core/params/SpaceVoicing.h` (`kCanyonWear*`, or a trim next to `kCanyonWetTrimEndless`). After it, measure, re-pin the goldens, and update the v1.0.82 SPEC line.

Then, in order:
1. **Merging into the beta:** with the owner's OK, merge `claude/v1082` (not `proto/lap-v1082`, which only adds the lap lines) into `beta/player-feedback`, re-run the gates, and publish beta 3 from the beta worktree (`tools/make_release.sh --beta`; memory "betas from the beta worktree").
2. **The owner's second question, still unanswered:** with very hot, bright noise, CANYON's lows rise ~4 dB as the highs wear (under 0.4 dB on music). Is that fine, or should the lows hold exactly?
3. **Still open from Fable's report:** should the three SPACEs add the same warmth at the same playing level? Is SHIMMER's octave in CANYON, 7 dB stronger at full DEPTH, wanted?
4. **Later and separate:** CANYON's held-tail smear (5-7 dB over two minutes). The SHIMMER-recorder saving on its own is still not measured on the module.

## Waiting on Jesse

See `docs/TASKS.md`, "Waiting on Claude", under Fable's review:
- the rack listen on CANYON's endless level;
- the lows question above;
- the OK to merge into the beta and publish beta 3;
- the clean-up list below.

## In flight

**Nothing running.** All four subagents this session finished, and I reviewed each one's diff and re-ran its gate:
- the Fable review;
- the CANYON darken page;
- the CANYON fit;
- the flip-freeze fix.

The earlier port and lap agents were reviewed before the context summary. No nested sessions were started.

The page server `review-pages-lan` (port 8766, serving main's `renders/`, LAN 192.168.1.238) was started from the desktop app's preview this session. It may still be running.

**Kept on purpose:**
- `claude/v1082` (worktree `.claude/worktrees/v1082`, which is checked out on `proto/lap-v1082`): the beta 3 candidate. Switch the worktree back with `git -C .claude/worktrees/v1082 checkout claude/v1082` before working there.
- `proto/lap-v1082`: the lap lines for v1.0.82.
- `beta/player-feedback` (worktree `beta`) and `.claude/worktrees/release` (detached at `4f6f950`).
- `claude/input-stop`, `claude/rack-levels`: older open work.

**Proposed clean-up, awaiting the owner's OK:**
- Branches and worktrees whose work is in `claude/v1082` or was dropped:
  - `proto/canyon-turnover`, `proto/canyon-darken`, `proto/canyon-darken-fit`, `proto/flip-freeze`, `proto/lap-flipfix`;
  - `claude/port-fable`, `proto/lap-v1081`, `proto/lap-v1081-atflip`, `proto/unfreeze-reshape`, `proto/flipflick-landing`;
  - `proto/fable-review`: keep it until the report is copied to main, or keep it as is.
- Judged pages: `renders/canyon_turnover` (707 MB), `canyon_darken` (869 MB), `canyon_level` (1.5 GB), `shimmer_level` (432 MB), `fable_review` (689 MB).
- `dist/` lap builds older than v1.0.82.
- Disk: 20 GB free (96 % used).

## Will bite

- **Flash: v1.0.82's release is 112,628 B, only 12 B under budget.** The flip fix cost +8 B alone but +48 B once merged (memory "flash-libm-costs": branch sizes don't add). Any make-up or new feature needs bytes freed first. The CANYON fit's savings list is in its commit message (`0ddbfcc`).
- **CPU:** the worst lap line is 96.5-97.7 %.
  - HALL->CANYON FREEZE +50 ms reads 97.7 % on a fresh flash's first lap and 95.1 % after.
  - PLATE->CANYON (the direction the fix doesn't touch) is 96.4-96.7 %.
  - Drops start near 99 %.
- **Desktop renders have no knob jitter**, so `same_render` can't see the flip fix (memory "desktop has no knob jitter"). `host/tests/test_flip_freeze_size.cpp` adds the lap's noise.
- **The beta 3 merge brings in v1.0.81 too:** `claude/v1082` contains `claude/port-fable`. Its ADR 0059 and the amendments come with it.
- **The flip fix has no ADR of its own:** it's described in the v1.0.82 SPEC line only. Add an ADR amendment (the morph's ADR) if the next session wants one.
- **`LIBDAISY_DIR` must go on the make command line in worktrees** (their submodules aren't initialised).

## Gates

**`main`, at `01d7752`** (v1.0.68; this session changed docs only):
- ctest: "100% tests passed out of 24".
- Firmware:
  - release 111,828 B (99.3 % of 112,640);
  - m0test 83,556 B;
  - profile 130,956 B (99.9 % of 131,072).
  - The `-direct` and `-latelight` bins in `firmware/build` are left over from older builds, not from this run's targets: unsure.
- Plugin installed `4140997` (v1.0.68), against main's HEAD `01d7752`: docs-only difference.

**`claude/v1082`, at `08e1ba3`** (the beta 3 candidate; its sound differs from main's):
- ctest: "100% tests passed out of 30".
- Firmware (clean build):
  - release 112,628 B (99.99 %, 12 B left);
  - m0test 83,556 B;
  - profile 127,336 B.
- `same_render` against the CANYON fit build (`0ddbfcc`): SAME 60/60.
- Lap build `a331a08`: profile 127,972 B, the same release size, 2 laps 0 late (`docs/research/m3-v1082-lap.log`).
- No plugin installed for it.

## Where to look

- `docs/TASKS.md` ("Waiting on Claude", Fable's review)
- `SPEC.md` on `claude/v1082`: the v1.0.82 changelog line
- `docs/research/m3-v1082-lap.log`, `m3-v1081-flipfix-lap.log`, `m3-v1081-atflip-lap.log`
- `docs/research/canyon-endless-turnover.md`
- `tools/pages/canyon_darken_README.txt` on `claude/v1082`
