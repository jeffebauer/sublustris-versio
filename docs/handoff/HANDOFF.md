# Handoff

**Written:** 2 Nov 2026, end of session 12 (30 Oct to 2 Nov).

This session:
- **Two versions merged after the owner's rack checks:**
  - v1.0.60, the AM radio band moved from WEAR to MOTION's right (pick F on `dip_motion`, ADR 0036);
  - v1.0.61, endless follows the playing (pick C on `decay_endless_match`, ADR 0008 amendment). Below that: the duck is removed, and the radio's band stays whole at endless (it was halved there, which the owner heard at the rack).
- **AUDIO_DIRECT made the default** (30 Oct, after a clean rack boot).
- **The website** (`site/`, Astro), merged on main:
  - the Paper Mono layout and copy;
  - the motion-radio copy, presets, demos and the "Old radio" tile;
  - "Two ways to play";
  - the hero film and panel stills with the revised panel art (CAN, the new type);
  - the story image with one tape across both decks (Eno's Discreet Music diagram);
  - issue forms in `.github/ISSUE_TEMPLATE/`;
  - a Vercel project (`sublustris-versio`, behind Vercel Authentication, not linked to GitHub).
- **Research:** `docs/research/wet-dry-balance.md` (BLEND is equal-power, as Valhalla and Clouds are; noon is where level steps show most) and `docs/research/endless-match.md`.
- **Clean-up:**
  - every judged page, old build and finished worktree is in the Trash, `sublustris-wrap-2026-11-02`;
  - 22 superseded branches deleted, with a backup bundle in the same folder.

Start with `/sublustris-start`.

**Nothing running:** no agents and no worktrees. The nested sessions this session started are all idle and their branches merged or deleted; they can be archived. The LAN page server (`renders/` on :8765) may still be running. The laptop's LAN address is now 192.168.1.238.

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
   - Run `make -C firmware all-variants` and `cmp` the release against `dist/sublustris_versio-v1.0.61-radio-endless-33a1be3.bin`.
   - Update CLAUDE.md's first paragraph to say the folder has moved.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to **v1.0.61**, on main |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `4371335` (v1.0.59), 29 Oct; **main is v1.0.61, not installed** (owner runs `tools/install_plugin.sh`) |
| M3 | **v1.0.61 played at the rack** (2 Nov: "MUCH better"); module CPU not profiled since v1.0.58 |
| M9 | Not started |

- **main HEAD:** `5a54f8e` plus the wrap-up commit; the v1.0.61 merge is `520b329`.
- **Not pushed:** main is about 45 commits ahead of `origin/main` (`dcf211d`). The owner's standing rule is no push without their OK; asked at wrap-up.
- **Gates on merged main (2 Nov):** ctest **"100% tests passed out of 22"**; the site builds (13 pages) with `astro check` at 0 errors and 0 warnings.
- **Firmware:**

  | Variant | Bytes | |
  |---|---|---|
  | release | **108,660** | 83 %; **3,980 B under the 112,640 budget** |
  | m0test | 83,556 | |
  | profile | 128,720 | 98 %; warning as usual |

- **Rack builds in `dist/`:**
  - `sublustris_versio-v1.0.61-radio-endless-33a1be3{,-latelight}.bin`, what's on the module. It equals main's code; `33a1be3` is its branch commit.
  - `sublustris_versio-v1.0.60-motion-radio-7e08cb1{,-latelight}.bin`, the fallback.

## In flight (branches kept on purpose, no worktrees)

- **`claude/rack-levels`** (`6eab077`): `IV_SPACE_TRIM="plate,hall,massive"` dB at endless. The owner's pick is C, `0,-2,2` (HALL −2 dB, MASSIVE +2 dB at endless). It was measured before v1.0.61, and endless now plays at DECAY 0.80's level, so **re-measure HALL against MASSIVE on v1.0.61 before porting**: the gap may have changed. Notes are in `docs/research/rack-levels.md` on the branch.
- **`claude/input-stop`** (`1ef34bc`, built on v1.0.58): `IV_INPUT_STOP` modes 1-5 (D = 3 is the owner's earlier pick; E = 4 and F = 5 are on the `input_stop2` page). The page `renders/input_stop2` is still waiting for the owner. It was rendered on v1.0.58, so re-render it on v1.0.61 before asking (the duck it partly works around is gone). Merge main in first.

## Next steps

1. **v1.0.62, the HALL/MASSIVE level match:** measure endless HALL against MASSIVE on v1.0.61 first. If they still differ by about 4 dB, port the trim as a voicing constant (not `IV_MIX_TAPER`), then the goldens, a SPEC line and an ADR 0033 amendment. If the gap has changed, show the owner a short page.
2. **`input_stop2`:** re-render it on v1.0.61, then ask for the owner's pick and build it. Flash fits any mode.
3. **The unfreeze-after-SIZE overload** in MASSIVE (SHIMMER, WEAR 0.5, MOTION tape): the audit lap's UNFRZ lines in `docs/research/m3-review2-audit-212b68b.log`. Options in the session 11 handoff (git history): glide SIZE after an unfreeze, or stop the twin sooner.
4. **MASSIVE → CANYON in the firmware and plugin:** labels only (ParamSpec's shown label, the plugin, CONTEXT, the tools), keys and codes unchanged, plus an ADR and a SPEC line. The panel art and site already say CANYON.
5. **A profile lap on the module** (`make MODE=profile SIZESPIKE=5`), covering v1.0.58-v1.0.61. Profile is at 98 % of flash.
6. **Website:** the AI-tells copy review, the Safari and iPhone checks, a fresh Vercel preview once main is pushed.

## Waiting on the owner (`docs/TASKS.md`)

- **The push to origin** (asked at wrap-up).
- **Install v1.0.61 in Ableton** when wanted.
- **Listen:** `input_stop2` (after its re-render).
- **When convenient:** empty the Trash (`sublustris-wrap-2026-11-02` and the older folders); reference recordings; walk through the site.

## Will bite

- **Flash is 3,980 B under the budget, but profile is at 98 %**: every profile line counts.
- **v1.0.61 changed the endless level:** endless now carries the long-tail trim, about 2.1 dB lower in HALL, so its twin room is lower too. Older measurements taken at endless (`rack_levels`, `input_stop2`, `hall_level`) predate this.
- **PLATE right after you stop at endless:** about 7 dB of early sound leaves in the first second, then it holds. With the radio near full it's slightly brighter than at 0.80, from the louder held tail driving the over-modulation. Neither has been flagged by the owner.
- **Old renderer hooks** `IV_ENDLESS_MATCH`, `IV_DECAY_CROSS`, `IV_DUCK_*` and `IV_FREEZE_DUCK` are now ignored with a warning, so old sweeps using them render v1.0.61's sound.
- **Nested sessions:**
  - `start_session` works from the computer, but not over Remote Control from the phone. Use background Agent subagents there (memory).
  - Follow-ups go by SendMessage to the session's `local_…` id.
- **Main's `site/` has no `node_modules`.** Build the site in a scratch worktree with `cp -cR` of a node_modules folder (a symlink breaks Astro), plus `FONTS_DIR=/Users/jesse/Downloads/Fonts/MD_IO-V2-FutureFonts/Fonts/Variable`. The papermono worktree that held one is gone, so run `npm ci` in `site/` once.
- **Desktop vs module, goldens and `screenlog.0`:** as before. `IV_GOLDEN_UPDATE` allows one update per SPEC version, and `screenlog.0` appends across flashes.

## Where to look

- `docs/TASKS.md`: "Waiting on Claude", "Next, in order".
- `SPEC.md` changelog v1.0.60-v1.0.61; `docs/adr/0008-*` (its v1.0.61 amendment), `docs/adr/0036-radio-band-on-motion.md`.
- `docs/research/endless-match.md` and `docs/research/wet-dry-balance.md`.
- The branches `claude/rack-levels` and `claude/input-stop`, with their notes.
