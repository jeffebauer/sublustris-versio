# Handoff

**Written:** 3 Nov 2026, end of session 13 (2 to 3 Nov).

This session:
- **v1.0.62 to v1.0.68 merged** after the owner's rack check ("sounding excellent"):
  - CANYON as the name of SPACE's third position (ADR 0037);
  - WEAR's tape drive back (pick B on `tape_fold`);
  - the radio lighter (pick C);
  - PLATE's SIZE by crossfade;
  - the small fixes and WEAR without the BBD clock.
- **v1.0.68 released, the first public release.**
  - The repository is public. The owner's ruleset on `main` blocks deletions and force pushes, and private vulnerability reporting is on.
  - GitHub Release `v1.0.68` was built from `133e0e0`:
    - firmware 111,828 B, the same bytes as the rack build;
    - the universal AU/VST3 zip, 7.8 MB, not notarised;
    - checksums.
  - The site is public at https://sublustris-versio.vercel.app with the downloads live:
    - `DOWNLOADS_LIVE=1` is set for production;
    - Vercel Authentication covers previews only.
- **Release audit:** all nine site demos were re-rendered on v1.0.68, and the docs, presets and README were brought current.
- **The owner's site review and iPhone pass** were merged. The download buttons now read "Download", with the format under each title and the size at the right.
- **Resilio's site links to Sublustris.** A Resilio session did it in Resilio's repo; its main is `39347d3`.
- **Launch posts** for Modwiggler, r/modular and the NE Discord were drafted in `docs/launch/`. The owner has posted all three.
- **Plugin v1.0.68 is in Ableton** (`4140997`); the AU check passed.

Start with `/sublustris-start`.

**Nothing running:** no agents and no preview servers; the nine finished worktrees were removed. No worktrees left; the launch-posts branch merged at the wrap.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to **v1.0.68**, on main |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4140997` (v1.0.68), 3 Nov; AU check passed |
| M3 | **v1.0.68 on the module**, the rack build (`dist/v1.0.68-rack-63-68`); module CPU not profiled since v1.0.58 |
| M9 | **Released** v1.0.68, 3 Nov 2026 |

- **main HEAD:** `12ec45b` plus the wrap-up commit; pushed to `origin/main`.
- **Gates on main (3 Nov, wrap-up):** ctest **"100% tests passed out of 24"**.

  | Variant | Bytes | |
  |---|---|---|
  | release | **111,828** | 85 %; **812 B under the 112,640 budget** |
  | m0test | 83,556 | 63 % |
  | profile | 130,956 | 99 %: **116 B under the 131,072 limit** |
- **Rack builds and releases in `dist/`:**
  - `dist/release/v1.0.68-133e0e0`: what's on GitHub;
  - `dist/v1.0.68-rack-63-68`: what's on the module;
  - the older v1.0.60-v1.0.67 builds, which can go to the Trash.

## In flight (kept on purpose)

- **`claude/rack-levels`** (`6eab077`): the HALL/CANYON level match, pick C (`0,-2,2`: HALL −2 dB, CANYON +2 dB at endless). It was measured before v1.0.61, so re-measure first.
- **`claude/input-stop`** (`1ef34bc`, built on v1.0.58): the `input_stop2` page is stale. Merge main, re-measure the dip on v1.0.68, rebuild E and F, and re-render.
- **`proto/more-tape`, `proto/radio-lighter`, `proto/tape-fold`, `proto/wear-no-bbd`:**
  - prototype branches whose picks were ported into v1.0.64-v1.0.67;
  - kept for their pages' notes only;
  - delete them with the owner's OK.

## Next steps

1. **The unfreeze-after-SIZE overload in CANYON**, the most important sound fix left.
   - The evidence is the audit lap's UNFRZ lines in `docs/research/m3-review2-audit-212b68b.log`.
   - Prototype a SIZE glide after an unfreeze, or wait for the twin room, then make a page.
2. **The HALL/CANYON level match:** re-measure on v1.0.68, then port the trim (branch `claude/rack-levels`).
3. **`input_stop2`:** re-render as described above, then ask for the owner's pick.
4. **SPACE flips morph** (the owner's pick: "Morph 3s"). It needs a plan first: flash is tight (see below).
5. **Housekeeping from the reviews**, now that the repo is public:
   - the Plugin's NaN input guard;
   - SPEC §3-§7 brought in line with the code;
   - the test gaps (`docs/TASKS.md`, "Waiting on Claude").
6. **A profile lap on the module**, covering v1.0.58-v1.0.68.
7. **Answering the launch threads:** relay issues the owner brings in. Bug reports and sound suggestions arrive as GitHub Issues (`.github/ISSUE_TEMPLATE/`).

## Waiting on the owner (`docs/TASKS.md`)

- **Listen:** `input_stop2`, after its re-render.
- **When convenient:**
  - check Ableton's sample rate: at 96 kHz HALL and CANYON are brighter;
  - empty the Trash;
  - the reference recordings;
  - Resilio's move to Ombra Audio, in Resilio's own repo.

## Will bite

- **The repo and the site are public.** Every push to `main` deploys production on Vercel (connected to GitHub), so main must always be releasable.
  - Branch work stays in worktrees.
  - The download buttons use `releases/latest/download/...`, so a new GitHub Release changes what people download at once.
  - Update `site/src/data/release.json` with each release.
- **Flash:** the release is 111,828 B against the 112,640 budget (ADR 0002), about 800 B left. Anything new (morph, input stop) needs a flash plan or a trim first. **Profile has only 116 B left**: a profile lap may need a trimmed profile build first.
- **Releases:**
  - the release script is `tools/make_release.sh`;
  - the plugin isn't notarised, so the install guide's Terminal step stays;
  - Vercel's `create_deployment` needed the owner's explicit OK this session ("Allow you to redeploy").
- **The site's dependencies:** `site/node_modules` must be installed with `npm ci` in a fresh worktree; don't symlink it, because Astro breaks. The fonts come from `FONTS_DIR` (MD IO, never committed). An orphan dev server once held port 4321.
- **Two worktrees showed `libs/JUCE` "not to be a symbolic link"** errors in `git status`. They were harmless and those worktrees are gone, but a new worktree may print it.
- **Desktop vs module, goldens and `screenlog.0`:** as before.
  - `IV_GOLDEN_UPDATE` allows one update per SPEC version.
  - `screenlog.0` appends across flashes.
- **Nested sessions:** `start_session` works from the computer, not over Remote Control. Follow-ups go by SendMessage to the session's `local_…` id.

## Where to look

- `docs/TASKS.md`: "Waiting on Claude".
- `docs/research/m3-review2-audit-212b68b.log` (UNFRZ) for step 1.
- `SPEC.md`'s changelog v1.0.62-v1.0.68; `docs/adr/0037-*`.
- `site/src/data/release.json` and `tools/make_release.sh` for the next release.
- `docs/launch/` for what was posted.
