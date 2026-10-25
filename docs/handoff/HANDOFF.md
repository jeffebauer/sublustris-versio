# Handoff

**Written:** 25 Oct 2026, end of session 5. Start the next session with `/immensio-start`.

**Nothing running:** no agents, a clean tree, `main` pushed to `origin` with this handoff.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments **v1.0.1–v1.0.30** (SPEC changelog). This session: v1.0.28–v1.0.30, below |
| M0 desktop / firmware | Built |
| M0 hardware | **Passed** (owner, 25 Oct 2026: "everything worked as expected") |
| M1 Hall, M4 MASSIVE, M5 PLATE | Built and listened |
| M2 | Criteria pass. **Not installed** in Ableton |
| M3 | Needs the owner at the rack (CPU). New wet stages since the last estimate: the BBD and the radio |
| M6 character | **Redesigned and listened:** ERA's five eras ("M6 also sounding excellent"; the ERA sweep "sounds excellent") |
| M7 duck | Done |
| M8 | Knob feel done; DECAY keeps the loudness (v1.0.28) |
| M9 | Not started |

**This session's spec changes:**
- **v1.0.28 DECAY keeps the loudness** (ADR 0020). A shorter tail is made up to its SPACE's longest finite tail (`map::decayLevelMakeup`), MASSIVE included, and a short REVERSE too (`map::reverseLevelMakeup`). PLATE is +0.4 dB. Every finite setting sits within ~2 dB on music (it spread 13 dB).
- **v1.0.29 bug fix:** turning ERA into or out of the cassette clicked (a spike ~250× the surrounding level, since v1.0.22). Tape out now fades in and out over 20 ms (`Engine` `tapeMix_`). `test_era` sweeps the knob.
- **v1.0.30 ERA runs from sweet to gritty** (new ADR 0021; ADR 0003 superseded, 0004 and 0019 amended):
  - **Five eras in equal stretches,** each on its own (only neighbours crossfade): Neve, Midiverb, bucket-brigade, cassette, AM radio.
  - **The Neve is the base under all of them,** and alone at ERA fully CCW. ERA's default is now 0.1.
  - **Bucket-brigade** (`dsp/EraStages.h` `BbdStage`, on the wet after the loop): its grit reaches round 2's "more" at two-thirds of its stretch (`kBbdGritAt`).
  - **Radio** (`RadioStage`, on the wet after the loop):
    - a Rician station fade, given back by an AGC that follows the carrier, so the static swells as the station dips;
    - round 1's wandering dip;
    - soft hiss, three-layer static, a slow detune;
    - at the gritty end, drift-offs and a faint hum;
    - a slow level hold (~8 s, ±12 dB). Without it, a held tail drained 6 dB in six minutes through the band as it darkened.
    - No whistles or polar flutter (owner).
  - **Removed:** μ-law, vintage reads, stacking.

**Turned down after listening** (in the ADRs and `docs/TASKS.md` Done):
- ERA on the dry too: "too intense" (ADR 0004 amendment);
- stacked ERA made stronger (superseded by the redesign);
- the early-sampler era;
- the round-1 and round-2 radios, whistles and polar flutter.

- **`main` HEAD:** this handoff's commit.
- **Plugin in Ableton:** none installed.
- **Gates:** ctest **"100% tests passed out of 15"** on the merge commit `2d6364c`. Only `docs/TASKS.md` changed after it.
- **Firmware** (110 KB budget = 112,640 B; **1,068 B left**):

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 111,572 | 85 % |
  | profile | 112,560 | 85 % |
  | m0test | 82,356 | 62 % |

- **Listening pages:** none waiting (`renders/` holds only `_material` and `_material_sweep`, 626 MB). The ERA signatures reference page (https://claude.ai/artifact/AiR2JXPAaLbT8GeJseK3Vj) measures the **old** stacked ERA. The owner declined a re-measure for now.
- **Phone listening:** `.claude/launch.json` "review-pages-lan" serves `renders/` on the Wi-Fi at port 8766 (http://192.168.1.100:8766/…; the IP comes from `ipconfig getifaddr en0`). See memory [[owner-reviews-on-phone]].

## In flight

- **Nothing running.**
- **No branches kept:** the prototype branches `claude/era-dry` (7ec5af8), `claude/era-strength` (3608c89) and `claude/listen` (fbb283c) were deleted at the owner's request after the wrap; the decisions are in ADR 0004 and 0021.
- `.claude/worktrees/serene-snyder-30d3a2` and `swell-curve`: old sessions' worktrees, already merged. The app cleans them.

## Next steps

1. **Owner at the rack: M3** (M0 passed 25 Oct 2026). Profile the heaviest cases, which now include the new wet stages:
   - a frozen MASSIVE, DRIFT CW, BLOOM, ERA at the radio (0.9–1.0), and at the BBD (0.5);
   - the cassette with tape out at 4×;
   - a long silent tail, for subnormals (the firmware doesn't flush to zero).

   The radio runs ~8 RNG calls, 2 SVF sets and 5 filters per sample. If CPU is short, its per-block `std::pow` calls and the BBD's three SVFs are the first places to look.
2. **The rest of the Valhalla comparison** (TASKS "Waiting on Claude"). MASSIVE's highs die sooner than Supermassive's, and the Neve base now shortens every space's highs a little more (×0.7 high T60 share at every ERA). Re-measure with `tools/compare_reference.py` first. Supermassive's pitch movement at long settings is the other item.
3. **Flash before any Core addition:** 1 KB left. See memory [[flash-libm-costs]] for where the bytes go (`arm-none-eabi-nm --size-sort -S -C -t d firmware/build/immensio_versio.elf`). Candidates: the radio's per-sample `process` (2.6 KB, -O3) and REVERSE / old prototype code paths.
4. **ERA signatures page**, only if the owner asks: `tools/era_reference.py` still lists the old stage positions (`STAGES`). Update it to the five eras and republish the artifact (Dashboard type, `ArtifactData` writes; one long-form JSON asset).
5. Resilio → Ombra Audio (its own repo; check the owner's Ableton sets first).

## Waiting on the owner (all in `docs/TASKS.md`)

- No picks or pages waiting.
- **At the rack:** M3 (M0 passed).
- **Recordings:**
  - re-export three clipped Valhalla takes 6 dB lower;
  - re-record Supermassive's short settings;
  - check Supermassive's DELAY;
  - H9 Reverse, Mercury7, VintageVerb.
- **M2:** install the plugin when they want it in Ableton.

## Will bite

- **Flash:** 1,068 B left. A new libm function (`tan` was 2.7 KB) or an inlined helper can blow it. Mark control-rate code `IV_SIZE_OPT`.
- **Goldens update once per SPEC version.** `IV_GOLDEN_UPDATE` refuses unless SPEC's newest changelog version is newer than the goldens' (now v1.0.30). A second sound change needs its own changelog line first.
- **ERA fully CCW is no longer clean** (the Neve base, darkening ×0.7). Tests that used ERA 0 as "clean" now hear the Neve. Mostly fine, but keep it in mind when reading old measurements and research notes.
- **The radio is random by design** (storm, fades, crashes, drift-offs), with a fixed seed per Engine. Two renders of the same settings match, but different material lengths shift the events. Its hold is judged over 25 s windows in `test_era` (its fades are meant to be heard).
- **The click detector flags the music's own attacks** (bass plucks, keys) on looped material. Check click times against era boundaries before blaming the engine. The radio's crackle is clicks by design; `test_era`'s sweep stops before the radio.
- **Page chapters are per row** (the material). `make_review.py --chapters` sets one chapter bar for every row, for knob sweeps on their own page.
- **Earlier traps still apply:**
  - the tape soft-limits the wet's peaks from the cassette up;
  - level-matched pages hide loudness;
  - don't rebuild `build/` while a page renders from it;
  - iv_render sidecars don't record `IV_*` hooks (none are left in the renderer now except `IV_STILL_LOWS`, `IV_TAPE_OUT`, `IV_DIFF_INDEPENDENT`, `IV_HALL_DIFFUSION`, `IV_MOD_SCALE`, `IV_DECAY_MAKEUP`);
  - picks relayed by other sessions need the owner's confirmation.
- **Removals built from shell variables are refused** by the harness (memory [[rm-literal-paths]]).

## Where to look

- `docs/TASKS.md`
- `docs/adr/0021-era-sweet-to-gritty.md` and `docs/adr/0020-decay-keeps-the-loudness.md`
- `core/dsp/EraStages.h` (BBD, radio), `core/params/Mappings.h` (`eraMix`, `decayLevelMakeup`), `core/dsp/Engine.cpp` (wet path)
- `docs/research/new-eras.md`, `docs/research/radio-artifacts.md`
- `host/tests/test_era.cpp` (what ERA promises)
- `SPEC.md` changelog v1.0.28–v1.0.30 and §4.7
