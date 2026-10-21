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
| M4 MASSIVE + ATTACK | **Built**, M4 notes acted on (SPEC v1.0.6: early taps, shorter SIZE predelay, no echoes at SIZE 0); REVERSE **awaiting the owner's pick** on `renders/reverse_options/` (prototype `Reverse::setSmooth`, `IV_REVERSE_SMOOTH`) |
| M5 PLATE + freeze | **Built**; freeze settled on the **twin** (owner, SPEC v1.0.7; companion removed); M5 page re-rendered for a listen. Still to fix: frozen-MASSIVE upper resonance, PLATE-skank early repeats |
| M6 character | **Built**: ERA (5 stacked layers), DRIFT, soft saturation, μ-law output, Midiverb box, hiss, crackle; `test_era` passes; listening page `renders/m6_character/` |
| M7 duck + limiter | **Built**: DARKEN on the wet, soft limiter; `test_output` passes (incl. a 40-run random integration sweep); listening page `renders/m7_duck/` |
| M2 | Criteria pass in `plugin_host_test`; **not installed in Ableton** (the owner decides when: `tools/install_plugin.sh`) |
| M3 | Needs the owner at the rack (profile build ready) |
| M8, M9 | Not started (M8 = tuning from the owner listening notes) |

- **Gates (wrap, `7906f5c`):** ctest "100% tests passed out of 12".
- **Firmware sizes (21 Oct, twin only):** release 87,884 B (67 %), profile 88,920 B (67 %), m0test 82,356 B (62 %). DTCM 36,864 B of 128 KB (the companion's 84 KB freed). SDRAM 25.2 MB. ~22 KB left to the 110 KB budget.
- **Plugin in Ableton:** none installed.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
- **REVERSE prototype** (`Reverse::setSmooth`, `IV_REVERSE_SMOOTH` hook in `host/render/main.cpp`): waiting on the owner's pick on `renders/reverse_options`.
- `build-plugin/` is a scratch plugin build dir (gitignored; safe to delete).

## Next steps
1. **Owner picks on `renders/reverse_options`.** If B (smooth): replace the per-sample `std::sin` in `Reverse.h` with a table or recursive oscillator (two sines a sample is costly on the M7), make it the only REVERSE, change `test_massive`'s "segments rise to their cut" to "no cuts", drop `setSmooth` and the hook, finish ADR 0015's amendment.
2. **Frozen MASSIVE resonance and PLATE skank repeats** (owner, freeze_options notes): measure first (spectrum of a 60 s frozen MASSIVE pad over time for a growing peak; onset envelope of `skank` into the frozen PLATE, which now goes through the twin), fix, re-render `m5_plate_freeze`.
3. **Owner runs the M0 hardware check** (`docs/m0-hardware-check.md`). Fixes go in `firmware/main.cpp` (`kPotKnob` hardware fact, `kPotParams` ADR 0017).
4. **Owner listening** on M1/M4/M5/M6/M7 pages, then **M8 tuning**: *(start)* numbers live in `core/params/SpaceVoicing.h` and `EraVoicing.h`.
5. **Owner decisions:** ADR 0008 makeup; MASSIVE SIZE glide speed (ADR 0007 amendment); plugin identity.
6. **At the rack:** M3 (flash `immensio_versio_profile.bin`, read the serial CPU report). The twin tank roughly doubles the frozen-state DSP cost: M3 must measure the frozen case.

## Waiting on the owner
- M0 hardware check results.
- Plugin identity (`docs/TASKS.md`): the AU shows as "Resilio: Immensio Versio" (company name "Resilio", manufacturer `Rslo`, plugin code `ImVs`). Keep, or name the company differently?

## Will bite
- **MASSIVE's early taps** (`Fdn::setEarlyTaps`, `SpaceVoicing.h` kEarlyTap*): 3 linear reads per line per sample (24 more reads, SDRAM) while in MASSIVE. M3 must measure MASSIVE, frozen and not. Its rise can't be shorter than ~1 s (`test_massive` kMassiveBuildSeconds).
- **A fresh (or freshly painted) pad wanders a few dB while it settles** (HALL: +3 dB over its first ~10 s with no input at all; MASSIVE more). It has no ceiling until `kPadSettleSeconds` (20 s), then holds within ±0.5 dB. Judge paint against a no-paint run, not against the level at the freeze.
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
