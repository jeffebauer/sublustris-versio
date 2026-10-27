# Handoff

**Written:** 28 Oct 2026, end of session 9. That session did the following:
- built v1.0.41-49, the owner's long run of picks: twin, WEAR, small room, TONE, SIZE crossfade groups, MOTION, room wander, the rename to **Sublustris Versio**, and WEAR's tuning plus the faster SIZE crossfade;
- fixed the ONSET/SPACE burst (v1.0.50);
- measured the level at endless DECAY;
- got the owner's level pick (D).

Start with `/sublustris-start`.

**Nothing running:** no agents and no nested sessions. A LAN page server (port 8765) was running as a background task of session 9; assume it is gone. To serve pages, use `preview_start` `review-pages-lan` (port 8766, `.claude/launch.json`) and give the owner `http://<ipconfig getifaddr en0>:8766/<page>/`. Memory "owner reviews on phone" has the details.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus amendments to **v1.0.50**, on `claude/combined` only. Main's SPEC stops at v1.0.33 |
| M0 | Passed (25 Oct) |
| M1/M4-M8 | Built and listened |
| M2 | Plugin installed `b39d451` (v1.0.34, still named Immensio Versio). v1.0.35-50 not installed |
| M3 | **Done on paper, confirmed at the rack for v1.0.49** (owner, 27 Oct: "really sounding excellent"). The SIZE A/B lap put every SIZE move at ≤ 89.6 %, with no near-late blocks. v1.0.50 has not been on the module yet |
| M9 | Not started |

**All the code is on `claude/combined`** (worktree `.claude/worktrees/combined`, HEAD `8f5124e`). `main` holds docs and skills only. The merge into main waits for the owner's OK (TASKS "Decide"), after the next rack build.

**Gates on `claude/combined`.** Its tree equals `dcc7112`, re-run by me in `space-burst`'s build:
- ctest: **"100% tests passed out of 21"**.
- `same_render` against v1.0.49: only `flips` differs.

Firmware:

| Variant | Bytes | |
|---|---|---|
| release | **112,628** | **12 B under the 112,640 budget** |
| m0test | 83,556 | 63 % |
| profile | 125,024 | 95 %; the warning is expected |

The v1.0.49 builds in `dist/` (`sublustris_versio-v1.0.49-0b58b25{,-latelight}.bin`) were heard at the rack. The v1.0.48 builds in `dist/` can go to the Trash.

## In flight

- **`claude/level-duck`** (worktree `.claude/worktrees/level-duck`, HEAD `5c4d683`, off `e3c4cb4`, **not rebased onto the burst fix**). It holds renderer-only prototype hooks; firmware is byte-identical and the hooks are bit-identical when off:
  - `IV_DUCK_RATIO`, `IV_FLOOR_DB` (Engine `setDuckRatioPrototype` / `setFloorPrototype`, with `floorDbfs()` / `duckSlope()` accessors that are constexpr in the firmware);
  - `IV_DUCK_RELEASE`, `IV_DUCK_ADAPTIVE`, `IV_DUCK_SUSTAIN_CUT`;
  - page scripts `tools/pages/level_duck.py` and `level_glue.py`, which include `--safety` checks.
- **The owner's pick on `level_duck`: D**, duck 2:1 and governor floor −21 dBFS. Their words: "Duck 2:1 floor21 sounds the best, but ... still an audible dip once the dry signal stops". Not built yet.
- **`renders/level_glue`** (593 MB) is waiting for the owner's picks. Every option is D plus one of:
  - **B:** duck release 0.15 s instead of 0.4. Fits in flash at 0 B.
  - **C:** fast release only after held sounds. Roughly 80-120 B.
  - **D′:** held sounds ducked less. Roughly 90-130 B.
  - **E:** C + D′. Roughly 100-140 B.

  Dip numbers are in its `_verify.txt`. Research sources: `docs/research/level-at-infinite.md` covers the cause; the agent's sources (Eventide, Valhalla, TC 2290, Giannoulis/Massberg/Reiss JAES 2012/2013) are not written up yet. Put them in that file when building the pick.
- Remaining worktrees: `combined` and `level-duck` only. Old branches still exist (`git branch --no-merged main`); all are folded into `claude/combined` or shelved (`claude/swell-motion` holds swell motion D).

## Next steps

1. **The owner's `level_glue` picks.** Then build into `claude/combined`, in one change:
   - D: `kDuckRatio` 2 and `kFloorDbfs` −21 in `core/params/SpaceVoicing.h`;
   - the glue pick.

   If the pick isn't B, find the bytes first (a flash hunt subagent: `nm -S` diff; candidates in `firmware/README.md`). Then:
   - SPEC v1.0.51 line plus an ADR 0014 amendment for the duck, and the governor floor's ADR;
   - golden update with its reason;
   - the safety checks via `tools/pages/level_glue.py --safety`.
2. **A rack build** of v1.0.51 (release and `-latelight`, unique `dist/` names). The owner checks:
   - ONSET switches in and out of BLOOM;
   - MASSIVE ↔ HALL switches;
   - the level at 50:50 with endless DECAY;
   - the dip when they stop.
3. **Merge `claude/combined` into main** with the owner's OK. Take main's TASKS / HANDOFF / CLAUDE.md / skills. Then install the plugin with `tools/install_plugin.sh <commit>`, with Ableton closed; validate only with `auval -v aumf ImVs Ombr`. Tell the owner to remove the old "Immensio Versio" bundles, and update the TASKS "Plugin installed" line.
4. Later:
   - TIME moves while in BLOOM still glide the swell's taps (gentler than the ONSET burst; unmeasured);
   - the sub-20 Hz rumble at endless DECAY with TONE CW;
   - the low tank's crossover reset on a SPACE flip (−9 dB more early highs MASSIVE→HALL, costs bytes);
   - the PLATE flip not covered by the burst test.

## Waiting on the owner (`docs/TASKS.md`)

- Listen: `level_glue`.
- Decide: the merge into main, after the rack build.
- Empty the Trash: `immensio-*-2026-10-27*` and `sublustris-judged-pages-2026-10-28`.
- Optional Ribbons/Magneto recordings and reference recordings, as before.

## Will bite

- **Flash: 12 B left.** Any new Core code needs bytes found first, including C/D′/E on `level_glue` and the D pick itself if it isn't free. D should be free, since it only changes constants. Known techniques: `IV_ZI`, `IV_SIZE_OPT`, `IV_NOINLINE`, no new libm calls (memory "flash: libm costs").
- **`claude/level-duck` is based on `e3c4cb4`, before the burst fix.** Both touch `Engine.cpp`: the duck target ~663 and `applySpace`. Rebase or port by hand, then re-run SAME and the click check.
- **The shell's cwd drifts into worktrees.** A TASKS commit landed on `claude/combined` once this session. Commit main's docs from the main checkout path in the same command (memory "keep checkout on main").
- **The click check misses broadband bursts.** The ONSET burst was about 50 dB under the music with no sample steps. Use energy above 3 kHz against an unswitched render, as `onsetAndSpaceChangesDontBurst` does.
- **The level governor holds an absolute level.** The endless wet doesn't follow the input, so level numbers depend on input level. The module's jack-to-dBFS calibration is unknown; the "hot" tier (peaks at −1 dBFS) is the upper bound.
- From before: knob readings are noisy on the module (deadband); the two tank paths plus `lineXf` need any crossfade change in all three; USB power leaves the knobs unpowered.

## Where to look

- `docs/TASKS.md`, `docs/research/level-at-infinite.md`
- `.claude/worktrees/level-duck`: `tools/pages/level_glue.py` and its `Engine.cpp` hooks (duck target ~663)
- `core/params/SpaceVoicing.h` (duck and limiter constants ~257-272, `kFloorDbfs` ~153), `core/dsp/Governor.h`
- SPEC.md on `claude/combined` (changelog v1.0.42-50), `docs/adr/0014-*` (ducking), `0013-*` (SPACE flip)
- `firmware/README.md` (flash techniques)
