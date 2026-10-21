# Handoff

**Written:** 21 Oct 2026, end of session 1 (M1, M4–M7 built from the frozen spec; review pages rebuilt; M5 notes and the freeze pick acted on). Start the next session with `/immensio-start`. **Nothing running:** no agents, no worktrees, no branches besides `main`. Remote: private repo https://github.com/jeffebauer/immensio-versio (`origin`, main tracked).

## State

| Milestone | State |
|---|---|
| Spec | **v1.0 frozen**, ADRs 0001–0017; build amendments v1.0.1–v1.0.4 (changelog), two waiting on the owner (ADR 0007, 0008 amendments) |
| M0 desktop | Built: Core (passthrough `iv::Engine`) + Renderer + tests + Plugin (AU/VST3) |
| M0 firmware | Built: release / m0test / profile |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | **Built**: FDN (8 lines), diffusers, predelay; `test_fdn` passes; listening page `renders/m1_hall/` (owner check pending) |
| M4 MASSIVE + ATTACK | **Built**: MASSIVE voicing, swell (BLOOM), REVERSE, floor governor, in-loop diffusion; `test_massive` passes; listening page `renders/m4_massive/` |
| M5 PLATE + freeze | **Built**, M5 notes fixed (Plate tap bug, calmer frozen Plate, washier MASSIVE paint); freeze voicing **awaiting the owner's confirmation** on `renders/freeze_twin/` (prototype voicings 0-3 still in the code) |
| M6 character | **Built**: ERA (5 stacked layers), DRIFT, soft saturation, μ-law output, Midiverb box, hiss, crackle; `test_era` passes; listening page `renders/m6_character/` |
| M7 duck + limiter | **Built**: DARKEN on the wet, soft limiter; `test_output` passes (incl. a 40-run random integration sweep); listening page `renders/m7_duck/` |
| M2 | Criteria pass in `plugin_host_test`; **not installed in Ableton** (the owner decides when: `tools/install_plugin.sh`) |
| M3 | Needs the owner at the rack (profile build ready) |
| M8, M9 | Not started (M8 = tuning from the owner listening notes) |

- **Gates (wrap, `7906f5c`):** ctest "100% tests passed out of 12".
- **Firmware sizes (21 Oct, wrap, `7906f5c`):** release 88,204 B (67 %), profile 89,208 B (68 %), m0test 82,356 B (62 %). DTCM 121,080 B of 128 KB (the companion plate is still in; it can go if the twin wins). SDRAM 25.2 MB (twin FDN + Plate added). ~22 KB left to the 110 KB budget. Budget for release at v1.0 is 110 KB (ADR 0002). History in `firmware/README.md`.
- **Plugin in Ableton:** none installed.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
**The freeze voicing (prototype, committed in `7906f5c`).** The owner picked B (voicing 1, "feed the pad") on every `freeze_options` card, with notes: subtler; recede back to the original frozen pad rather than persist; Hall and MASSIVE decay too steeply after unfreeze (MASSIVE drops immediately); MASSIVE's secondary has too much predelay; an upper resonance builds in frozen MASSIVE; PLATE on skank gives two audible repeats before it diffuses.
- Done: voicing 3, the **twin tank** (`Engine.h` `twinFdn_` / `twinPlate_` / `twinDiffuser_`; `SpaceVoicing.h` `kTwinLevel` 0.5, `kTwinMaxT60` 8 s): input goes to a second tank of the same space and size with a finite decay, own input diffusers, no predelay or swell; heard beside the untouched pad; painting copies it into the pad. Unfreeze drop fixed for all voicings (governor weight no longer glides with freezeMix; `Engine.cpp`).
- Page `renders/freeze_twin/` (A = voicing 1 as picked, B = voicing 3), level-matched, decision page. The WAVs differ (checked).
- **Not done:** measure and fix the frozen-MASSIVE upper resonance and the PLATE-skank early repeats (likely the input diffusers / first reflections of the voice used while frozen); no tests yet for voicing 3.
- `build-plugin/` is a scratch plugin build dir (gitignored; safe to delete).

## Next steps
1. **Owner confirms on `renders/freeze_twin`.** Then make the winner the only behaviour: remove the losing voicings (the companion plate and its DTCM if the twin wins; `SpaceVoicing.h` "FREEZE VOICING PROTOTYPE"; `Engine::setFreezeVoicing`; the `IV_FREEZE_VOICING` hook in `host/render/main.cpp:122`), amend ADRs 0009/0010, SPEC changelog v1.0.6, and add tests in `test_plate_freeze` (input recedes back to the pad within ~T60; no drop on unfreeze; paint stays bounded). Raise or remove clamps consistently ([[verify-voicing-labels]]).
2. **Frozen MASSIVE resonance and PLATE skank repeats:** measure first (spectrum of a 60 s frozen MASSIVE pad over time for a growing peak; onset envelope of `skank` into the frozen PLATE), fix, re-render `freeze_twin` and `m5_plate_freeze`.
3. **Owner runs the M0 hardware check** (`docs/m0-hardware-check.md`). Fixes go in `firmware/main.cpp` (`kPotKnob` hardware fact, `kPotParams` ADR 0017).
4. **Owner listening** on M1/M4/M5/M6/M7 pages, then **M8 tuning**: *(start)* numbers live in `core/params/SpaceVoicing.h` and `EraVoicing.h`.
5. **Owner decisions:** ADR 0008 makeup; MASSIVE SIZE glide speed (ADR 0007 amendment); plugin identity.
6. **At the rack:** M3 (flash `immensio_versio_profile.bin`, read the serial CPU report). The twin tank roughly doubles the frozen-state DSP cost: M3 must measure the frozen case.

## Waiting on the owner
- M0 hardware check results.
- Plugin identity (`docs/TASKS.md`): the AU shows as "Resilio: Immensio Versio" (company name "Resilio", manufacturer `Rslo`, plugin code `ImVs`). Keep, or name the company differently?

## Will bite
- **The twin tank costs:** a second FDN or Plate runs while frozen (only then). `Engine::largeFloats`/`fastFloats` must count every buffer the Engine attaches, with the same size factors (the twin's diffusers use 1.13): a mismatch fails 7 suites with arena overruns.
- **Review pages:** `tools/review/make_review.py` options `--showcase` (no picks), `--level-match`, `--dry <dir> --wet-only` (MIX fader). Render everything with `tools/render_review_pages.sh [page]`. Memory [[owner-listens-before-deciding]].
- **Review pages** (`tools/review/`): playback is Web Audio, fed from `_audio/*.js` next to each page (a page opened from Finder can't read WAVs into Web Audio, but can load scripts). That roughly doubles a page's size on disk (renders/ is ~2.8 GB, gitignored). Serving them (`.claude/launch.json` "review-pages", http://localhost:8765) also works and `?audio=js` tests the Finder path. Click-free switching was measured, not assumed: the output's second-difference spikes stay at the music's own level for every transition, against 52× for a deliberate instant switch.
- **In-loop allpass reads must keep their fraction in [0.5, 1.5)** (`Memory.h` readAllpass). Near 0 the interpolator's pole sits by Nyquist and moving reads leak energy into the top octave: frozen pads drift toward hiss.
- The ringing metric flags long or infinite renders of **pitched** material (vibes) as "steady tone": the reverb holding the notes, not metallic modes. On broadband noise the Hall reads ≤ 5 dB at every length. Judge metallic-ness on noise / hits, not on held notes.
- TONE and ERA's darkening are length-independent high-end T60 ratios through the Jot loss, not per-pass filters (SPEC v1.0.3): any filter or nonlinearity added inside a loop must be checked against the infinite-hold tests (`test_era`, `test_massive`, `test_plate_freeze`), since per-pass effects compound 10-20 times a second.
- The tank system is chaotic: builds with tiny float differences take different paths. Judge hold behaviour across seeds (a probe in the scratchpad did 5), not one run.
- The governor now does three jobs (floor for fed infinite tails, a held pad's ceiling, an infinite tail's ceiling once quiet); see `core/dsp/Governor.h` and the Engine's `applySpace`. Allpass-interpolated reads can nudge a lossless loop's energy up or down; the holds stop it creeping up.
- PLATE's shortest T60s are longer than the mapping at large SIZE (structural; SpaceVoicing.h).
- SPACE flips wait while frozen (the pad would be lost); they happen after the unfreeze.
- MASSIVE's SIZE range (0.35 → 2 s) is my reading of ADR 0007 (SPEC v1.0.1); the owner hasn't heard it yet.
- REVERSE: a TIME change takes effect at the next segment; growing the segment reads past the previous one for one segment (possible small glitch on a big TIME jump while in REVERSE). Not yet tested.
- `test_massive` takes ~15 s (two 10-minute infinite renders).
- **Flash:** Resilio ended at 97.7 %. Record sizes at every wrap; any Core change can move them.
- `plugin/PluginEditor.cpp` includes `firmware/LedMeter.h` (shared freeze tint). Change its API and the plugin breaks.
- The editor hard-codes DRIFT's 0.47–0.53 dead zone; move it to `core/params/Mappings.h` when DRIFT's mapping lands.
- `ProfileHook.h` defines `markHook` inline in the header (no core `.cpp` existed). Fine, but don't also define it in a `.cpp`.
- `test_audio/stimulus/*.wav` are generated (`python3 tools/make_stimulus.py`), not in git; `test_metrics` fails without them.
- The P7/SW1 demo sketch (`docs/prototypes/p7-sw1-demos/`) is throwaway: none of it belongs in `core/`.

## Where to look
`SPEC.md` §4.3, §5, §7 · `docs/adr/README.md` · `core/dsp/Engine.h` · `core/params/ParamSpec.h` · `firmware/README.md` · `docs/TASKS.md`
