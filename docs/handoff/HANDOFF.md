# Handoff

**Written:** 22 Oct 2026, end of session 1. That session built M1 and M4–M7 from the frozen spec, rebuilt the review pages, and acted on the owner's M1, M4 and M5 notes plus both freeze picks. Start the next session with `/immensio-start`. **Nothing running:** no agents, no worktrees, no branches besides `main`, clean tree. Remote: private repo https://github.com/jeffebauer/immensio-versio (`origin`, `main` tracked, pushed).

## State

| Milestone | State |
|---|---|
| Spec | **v1.0 frozen**, ADRs 0001–0017; build amendments v1.0.1–v1.0.8 (SPEC changelog). Owner-driven changes are amendments on ADR 0007, 0009, 0010, 0012 and 0015. ADR 0008's makeup is still for the owner to approve |
| M0 desktop | Built: Core + Renderer + tests + Plugin (AU/VST3) |
| M0 firmware | Built: release / m0test / profile |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | **Built**. M1 notes acted on (v1.0.8): SIZE 20 → 260 ms, small rooms with shorter tails, TONE wet tilt. Page re-rendered for a listen |
| M4 MASSIVE + ATTACK | **Built**. M4 notes acted on (v1.0.6): early taps, 0–100 ms SIZE predelay, no echoes at SIZE 0. REVERSE **awaiting the owner's pick** on `renders/reverse_options/` (prototype `Reverse::setSmooth`, `IV_REVERSE_SMOOTH`) |
| M5 PLATE + freeze | **Built**. Freeze settled on the **twin** (v1.0.7; companion removed). Still to fix: the upper resonance building in a frozen MASSIVE, and the PLATE's early repeats on skank |
| M6 character | **Built**: ERA (5 stacked layers), DRIFT, saturation, μ-law, Midiverb box, hiss, crackle. Owner hasn't listened yet |
| M7 duck + limiter | **Built**: DARKEN on the wet, soft limiter. Owner hasn't listened yet |
| M2 | Criteria pass in `plugin_host_test`; **not installed in Ableton** (the owner decides when: `tools/install_plugin.sh`) |
| M3 | Needs the owner at the rack (profile build ready) |
| M8, M9 | Not started (M8 = tuning from the owner's listening notes) |

- **`main` HEAD at wrap:** `d661e81` (this handoff's commit follows it).
- **Gates (wrap):** ctest "100% tests passed out of 12".
- **Firmware:**

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 88,564 | 67 % |
  | profile | 89,616 | 68 % |
  | m0test | 82,356 | 62 % |

  DTCM 36,864 B of 128 KB; SDRAM 25.2 MB. ~21 KB left to the 110 KB release budget.
- **Plugin in Ableton:** none installed.
- **Listening pages:** `renders/` holds m1_hall, m4_massive, m5_plate_freeze, m6_character, m7_duck, reverse_options and p7_sw1_demos. All but p7_sw1_demos are rendered from the current build. They take 3.1 GB, gitignored; 50 GB free on disk.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
- **REVERSE prototype:** `Reverse::setSmooth`, `Engine::setReverseSmooth` and the `IV_REVERSE_SMOOTH` hook in `host/render/main.cpp`. Waiting on the owner's pick on `renders/reverse_options` (A today, B smooth).
- `build-plugin/` is a scratch plugin build dir (gitignored; safe to delete).

## Next steps
1. **Frozen MASSIVE resonance and PLATE skank repeats.** From the owner's freeze_options notes: "a resonant frequency, an upper harmonic, builds up when frozen" (MASSIVE); "two audible repeats before the sound diffuses" (PLATE on skank). Measure first:
   - **MASSIVE:** the spectrum of a 60 s frozen pad over time, looking for a peak that grows (frozen MASSIVE goes to full in-loop diffusion, `Engine::applySpace`).
   - **PLATE:** the onset envelope of skank into a frozen PLATE (it now goes through the twin's diffusers, then `twinPlate_`).

   Then fix, add a test, and re-render `m5_plate_freeze`. Memory [[measure-owner-notes-first]].
2. **REVERSE, after the owner's pick.**
   - **If B:** replace the per-sample `std::sin` in `Reverse.h` with a table or recursive oscillator (two sines a sample is costly on the M7); make it the only REVERSE; change `test_massive`'s "segments rise to their cut" to "no cuts"; drop `setSmooth`, `setReverseSmooth` and the hook; delete `renders/reverse_options` and its block in `tools/render_review_pages.sh`; finish ADR 0015's amendment and a SPEC changelog line.
   - **If A:** drop the prototype the same way.
3. **Owner listening** on M1/M4/M5 (again) and M6/M7 (first time), then **M8 tuning** from the notes. Every *(start)* number is in `core/params/SpaceVoicing.h` and `EraVoicing.h`.
4. **Owner at the rack:** M0 check (fixes in `firmware/main.cpp`: `kPotKnob` is a hardware fact, `kPotParams` is ADR 0017), then M3. Flash `immensio_versio_profile.bin` and read the serial CPU report. Measure the frozen case: a frozen MASSIVE runs the pad plus the twin FDN plus 24 early-tap reads a sample.
5. **Owner decisions:** ADR 0008 makeup; MASSIVE SIZE glide speed (ADR 0007 amendment); plugin identity.

## Waiting on the owner
All in `docs/TASKS.md`, grouped by kind:
- **Decisions:** REVERSE, ADR 0008 makeup, MASSIVE SIZE glide, plugin identity. On identity: the AU shows as "Resilio: Immensio Versio", and `tools/install_plugin.sh` uses CODE `RsVs`; settle it before any Ableton set saves the plugin.
- **Listening:** M1, M4 and M5 again; M6 and M7 for the first time.
- **At the rack:** M0, then M3.
- **When convenient:** Mercury7 reference recordings.

## Will bite
- **Anything that changes at the freeze must glide** with `freezeMix_` (`Engine::controlTick` / `applySpace`). The duck, the floor and the governor weight each stepped the wet in one sample at a freeze (fixed in v1.0.8). The click detector only caught it once the dark TONE tilt took the music's highs out, so test clicks at TONE fully CCW too.
- **Painted pads:** painting copies only the twin, scaled by `paintRoom_` (it eases off within 1 dB of +1.5 dB). On PaintStop the pad gets **no ceiling** until it has settled again (`frozeAt_` is reset). A ceiling at the paint headroom made the hold makeup lift the pad after the paint. The makeup compares against the *slow* (8 s) level, so never set a hold target from a level that is still moving.
- **A fresh (or freshly painted) pad wanders a few dB while it settles.** HALL drifts about +3 dB over its first ~10 s with no input at all; MASSIVE more. It has no ceiling until `kPadSettleSeconds` (20 s), then holds within ±0.5 dB. Judge paint against a no-paint run, not against the level at the freeze.
- **HALL's SIZE now scales its finite T60** (`map::hallT60Scale`), with level made up by `map::hallSizeLevelTrim`. Any test that expects DECAY's raw mapping in HALL must include the scale (`test_fdn` does).
- **MASSIVE's early taps** (`Fdn::setEarlyTaps`, `kEarlyTap*` in `SpaceVoicing.h`) are 24 extra SDRAM reads a sample. MASSIVE's rise can't be shorter than ~1 s (`kMassiveBuildSeconds` in `test_massive`).
- **The twin costs memory:** a second FDN and Plate in SDRAM, used only while frozen. `Engine::largeFloats` / `fastFloats` must count every buffer the Engine attaches, with the same size factors (the twin's diffusers use 1.13). A mismatch fails 7 suites with arena overruns.
- **TONE's tilt is on the wet output** (`dsp::Tilt` in `Output.h`), not in the loop, so it's safe for infinite holds. The loop part is the high-end T60 ratio (0.08 → 1.5).
- **Review pages:** `make_review.py --showcase` (no picks), `--level-match`, `--dry <dir> --wet-only` (MIX fader). `tools/render_review_pages.sh [page]` renders everything. Playback is Web Audio from `_audio/*.js` sidecars, which work from Finder. Serve the pages with `.claude/launch.json` "review-pages" (port 8765) to test.
- **In-loop allpass reads must keep their fraction in [0.5, 1.5)** (`Memory.h` readAllpass), or frozen pads drift toward hiss.
- The ringing metric flags **pitched** material (vibes) as ringing. Judge metallic-ness on noise bursts (`06_noise_bursts.wav`), not on held notes.
- Per-pass effects compound 10–20 times a second: anything added inside a loop must pass the infinite-hold tests (`test_era`, `test_massive`, `test_plate_freeze`).
- The tank system is chaotic: judge hold behaviour across seeds, not one run.
- REVERSE: a TIME change takes effect at the next segment; a big TIME jump while in REVERSE may glitch once. Not yet tested.
- **Flash:** Resilio ended at 97.7 %. Record sizes at every wrap.
- `plugin/PluginEditor.cpp` includes `firmware/LedMeter.h`; the editor hard-codes DRIFT's 0.47–0.53 dead zone.
- `ProfileHook.h` defines `markHook` inline; its section enum now says `kTwin` (was `kCompanion`).
- `test_audio/stimulus/*.wav` are generated (`python3 tools/make_stimulus.py`), not in git; `test_metrics` fails without them. The vibes material comes from Resilio's `test_audio/demo_sources` (CC0, not in either repo).
- No numpy on this Mac (pip is externally managed). Analyse with `ffmpeg … -f f32le -` plus the stdlib `array` module, `iv_render --analyze`, or a C++ harness against `build/libiv_core.a`.

## Where to look
`docs/TASKS.md` · `SPEC.md` changelog v1.0.5–v1.0.8 · `core/dsp/Engine.cpp` (freeze, twin, paint, `applySpace`) · `core/params/SpaceVoicing.h` · `core/dsp/Fdn.cpp` · `host/tests/test_plate_freeze.cpp`
