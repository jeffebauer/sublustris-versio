# Handoff

**Written:** 6 Nov 2026, end of session 14 (5 to 6 Nov).

This session acted on the launch feedback (Modwiggler and r/modular; lfad's, danishchairs' and dubsackdude's posts; summary in `docs/TASKS.md` "Waiting on Claude", "Launch feedback"):
- **WEAR, two picks, built for the rack but not yet tried on the module:**
  - v1.0.69: the taper, D on every card of `wear_taper` (ADR 0044). The intensities follow 1-(1-v)^2.2 and the grit starts at noon.
  - v1.0.70: the tape's drive back to 16.25 dB at full, C on every card of `wear_drive` (ADR 0045).
  - Both are on `claude/proto-wear-taper-78e1da`, with the rack build in `dist/v1.0.70-wear/`. I rebuilt it and it's byte-identical.
- **Betas from branches, each with a PR** (ADR 0046):
  - `tools/make_release.sh --beta <ref> <n>` publishes a pre-release, never Latest, so the site's downloads don't move.
  - Branch names: `beta/`, `proto/`, `claude/`. Tags are never deleted.
  - CLAUDE.md has the rule. Merged and pushed (`da014f1`).
- **Two investigations, each with a page ready:**
  - `fuller_tail`: the "thin/reedy" note. Its causes are a low-mid cancellation between the still bass tank and the main tank, and too little slow movement.
  - `wet_loudness`: about 1 LU under the dry at full wet, and 4-5 LU under Valhalla Room at short decays.
- **A DECAY curve prototype is running overnight** (the owner's note after playing; see In flight).
- **A porting note** for other Daisy Seed hardware (README and FAQ) waits on `claude/install-check` for the owner's merge OK.
- **The task list:** brought into main from that branch at this wrap.

Start with `/sublustris-start`.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to v1.0.68 on main; **v1.0.69 and v1.0.70 on `claude/proto-wear-taper-78e1da`**, not merged (the beta first, ADR 0046) |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; AU check passed |
| M3 | v1.0.68 on the module; **v1.0.70 rack check pending** (`dist/v1.0.70-wear/`); module CPU not profiled since v1.0.58 |
| M9 | Released v1.0.68 (3 Nov). No betas yet; the first is WEAR, after the rack check |

- **main HEAD:** `da014f1` plus the wrap-up commit, pushed to `origin/main`. Main changed only in docs and `tools/make_release.sh` this session; no sound change on main.
- **Gates on main (6 Nov, wrap-up):** ctest **"100% tests passed out of 24"**; firmware unchanged from v1.0.68: release **111,828** B (812 B under budget), m0test 83,556, profile 130,956 (116 B under the chip).
- **v1.0.70 on its branch** (rebuilt and compared here):

  | Variant | Bytes | |
  |---|---|---|
  | release | **111,844** | 796 B under the 112,640 budget |
  | m0test | 83,556 | |
  | profile | 131,028 | **44 B under the 131,072 limit** |

  Its session reported ctest 24/24, SAME 40/60 against v1.0.69 (only mid-WEAR cases differ; WEAR 1 checked SAME 12/12 separately), click and endless-hold checks clean.

## In flight

- **`decay-curve`** ([its session](#local_03e43d72-4190-4923-82e1-4144e43eada5), `local_03e43d72-4190-4923-82e1-4144e43eada5`, branch `claude/proto-decay-curve-01af2c`, off v1.0.70): started at the wrap, running overnight.
  - The owner's note: the long decays are squeezed into noon to 3 o'clock, and 3:30 to fully clockwise is all identical endless.
  - Today: `kDecayFiniteTop` 0.80, `kDecayInfiniteAt` 0.85, exponential T60 (HALL is 12 s at noon, 60 s at 3).
  - The brief: research where established units put infinite; audit everything tied to the endless point (endless follows your playing, the governor's ease, the radio at endless, SHIMMER, `decayLevelMakeup`, `rack-levels`, `input-stop`, tests); prototype `IV_DECAY_CURVE` (as now, endless in the last ~5 %, plus a long-end taper, endless at the end stop); build the page `renders/decay_curve` with a T60-per-hour table.
  - **Check when it reports:**
    - read its diff, confirm everything is fenced with `#ifndef IV_FIXED_VOICINGS`, and that the `#else` paths are today's code;
    - build its release firmware in a scratch worktree and `cmp` it against `dist/v1.0.70-wear/…bin`;
    - open the page and HEAD every `_audio/*.wav.js`;
    - the audit list must be complete.
  - Its branch lacks main's ADR 0046 commits, which it doesn't need.
- **Branches waiting on the owner's picks or OK** (all off v1.0.70 unless stated; none pushed):
  - `claude/proto-wear-taper-78e1da`: v1.0.69 + v1.0.70 (`4b95b8a`), plus the `wear_drive` prototype commit `88a5148` under them. After the rack check it becomes `beta/wear`: push it, run `tools/make_release.sh --beta beta/wear 1 --publish` (the owner's OK first), open the PR (the script prints the commands), and comment on the Modwiggler thread.
  - `claude/fuller-tail-6dcc8d` (`a3478b8`): hook `IV_FULLER` B/C/D. Firmware checked byte-identical here.
  - `claude/wet-loudness-8e55d6` (`41d13ae`): hook `IV_WET_MAKEUP` +2/+3/+4.5 dB. Firmware checked byte-identical here. Its lean is +3 dB with the `rack-levels` trim.
  - `claude/install-check` (`997a412`): the README and FAQ porting note (`95abc67`). Its task-list commits are already in main, so on merge take main's `docs/TASKS.md`.
  - `claude/rack-levels` (`6eab077`) and `claude/input-stop` (`1ef34bc`): as before (re-measure on the current build). Both now interact with `wet_loudness` and `decay_curve`.
  - `proto/more-tape`, `proto/radio-lighter`, `proto/tape-fold`, `proto/wear-no-bbd`: their picks are already ported; delete them with the owner's OK.
- **Worktrees:**
  - each prototype's under `.claude/worktrees/` (keep until its pick is ported);
  - `.claude/worktrees/release`, the release script's own reusable worktree (local);
  - `../sublustris-install-check`.
- **Idle linked sessions:** `wear-taper`, `wet-loudness` and `fuller-tail` have the context for follow-ups (SendMessage to their `local_…` ids, in "Linked sessions"). `resilio-site-links` and `launch-posts` are done; archive them with the owner's OK.

## Next steps

1. **Review `decay_curve`** when it reports (the checks above), then send the page.
2. **Owner picks:** `fuller_tail`, `wet_loudness` and `decay_curve`.
   - Listen to fuller first: fuller tone sounds a little bigger, so it's worth settling before choosing a loudness.
   - Port each pick on top of v1.0.70 as new SPEC versions (one golden update per version), each with an ADR.
   - Decide with the owner whether they ride the WEAR beta (beta.2 or v1.0.7x-beta.1) or wait for their own beta.
   - The loudness pick also settles the `rack-levels` trim.
3. **The WEAR beta** after the owner's rack check of v1.0.70 (ADR 0046):
   - rename or create `beta/wear` at `4b95b8a`, or later if more picks join;
   - push it, `--beta … 1 --publish`, then the PR with links to lfad's post, ADRs 0044/0045 and what to listen for.
   - **First real publish of the beta mode: dry-run it first.**
4. **The unfreeze-after-SIZE overload in CANYON:** still the most important crackle fix (`docs/research/m3-review2-audit-212b68b.log`, UNFRZ lines).
5. **The plugin UI** (danishchairs' DM; the owner's backlog, shared with Resilio): Figma first, in a compact DAW layout. Parameter keys must not change.
6. **The module's level in volts** (the owner's 10-minute rack task) feeds the loudness report.
7. As before: `input_stop2`, the SPACE flip morph (a flash plan first), the housekeeping list, a profile lap (the profile build has 44 B left).

## Waiting on the owner (`docs/TASKS.md`)

- **At the rack:**
  - try v1.0.70 (`dist/v1.0.70-wear/sublustris_versio-v1.0.70-wear.bin`);
  - measure the module's input level (sine to the red In LED, volts peak to peak).
- **Listen:** `fuller_tail` and `wet_loudness` (ready), `decay_curve` (overnight).
- **Decide:** merge `claude/install-check`.
- **When convenient:** trash `wear_taper` and `wear_drive` (3.4 GB) and the old `dist/` builds; the sample-rate check in Ableton; the reference recordings.

## Will bite

- **Main is public and deploys the site**, so sound changes go out as betas from branches first (ADR 0046). `--beta` has been tested by building a package but has never actually published. Its test package is `dist/release/v1.0.70-4b95b8a-beta.1/`, built before the stable-name fix; I removed the stray copies by hand.
- **One golden update per SPEC version:** stacking picks means one new version per port (v1.0.71, v1.0.72…).
- **Flash:** release has 796 B left on v1.0.70, the profile build only 44 B. Faster CANYON movement (fuller C/D) costs about 40-100 B and won't fit the profile build without a trim.
- **Several prototypes share v1.0.70 and touch the same areas.** `fuller_tail` and `decay_curve` both touch `Engine`/`Fdn`, and `wet_loudness` the output. Port them one at a time, re-running the others' checks after each.
- **The preview server** (`review-pages`, port 8765) stops overnight. The pane's first navigation after a restart can come up empty, so reload. Pages store audio as `_audio/*.wav.js`, not `.wav`. Curl to localhost is blocked by the sandbox; use the browser pane.
- **`docs/TASKS.md` is edited on several branches** (each prototype's session added a line). Main's copy is the master: on merging a prototype branch, take main's version and re-apply only what's new.
- **`IV_WEAR_TAPER`/`IV_WEAR_DRIVE` are gone** on v1.0.70; old notes that mention them refer to the prototypes.
- As before: `screenlog.0` appends across flashes; never connect USB and rack power at once; `site/node_modules` with `npm ci` in a fresh worktree.

## Where to look

- `docs/TASKS.md` ("Listen", "At the rack", "Waiting on Claude" > "Launch feedback").
- `docs/adr/0044-wear-taper.md`, `0045-wear-drive.md` (on the WEAR branch), `0046-betas-from-branches.md`.
- `docs/research/fuller-tail-2026-11-05.md` and `wet-loudness-2026-11-05.md` (on their branches).
- `tools/make_release.sh` (`--beta`).
- `core/params/Mappings.h` (`decayRate`, `kDecayFiniteTop`, `eraMix`) for the DECAY review.
