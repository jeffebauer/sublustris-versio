# Handoff

**Written:** 21 Oct 2026, session 1 (owner away; Claude built M1 and M4–M7 from the frozen spec). Start the next session with `/immensio-start`. **Nothing running:** every agent finished and was reviewed.

## State

| Milestone | State |
|---|---|
| Spec | **v1.0 frozen**, ADRs 0001–0017; build amendments v1.0.1–v1.0.4 (changelog), two waiting on the owner (ADR 0007, 0008 amendments) |
| M0 desktop | Built: Core (passthrough `iv::Engine`) + Renderer + tests + Plugin (AU/VST3) |
| M0 firmware | Built: release / m0test / profile |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | **Built**: FDN (8 lines), diffusers, predelay; `test_fdn` passes; listening page `renders/m1_hall/` (owner check pending) |
| M4 MASSIVE + ATTACK | **Built**: MASSIVE voicing, swell (BLOOM), REVERSE, floor governor, in-loop diffusion; `test_massive` passes; listening page `renders/m4_massive/` |
| M5 PLATE + freeze | **Built**: Dattorro-family PLATE, companion (DTCM), gesture reader, freeze / paint / unfreeze; `test_plate_freeze` passes; listening page `renders/m5_plate_freeze/` |
| M6 character | **Built**: ERA (5 stacked layers), DRIFT, soft saturation, μ-law output, Midiverb box, hiss, crackle; `test_era` passes; listening page `renders/m6_character/` |
| M7 duck + limiter | **Built**: DARKEN on the wet, soft limiter; `test_output` passes (incl. a 40-run random integration sweep); listening page `renders/m7_duck/` |
| M2 | Criteria pass in `plugin_host_test`; **not installed in Ableton** (the owner decides when: `tools/install_plugin.sh`) |
| M3 | Needs the owner at the rack (profile build ready) |
| M8, M9 | Not started (M8 = tuning from the owner listening notes) |

- **Gates:** see the commit message of this handoff's commit for the ctest summary line and firmware sizes at wrap.
- **Firmware sizes (21 Oct, M7):** release 85,076 B (64 %), profile 86,008 B (65 %), m0test 82,356 B (62 %). DTCM 100.6 KB of 128 KB. ~25 KB left to the 110 KB budget. Budget for release at v1.0 is 110 KB (ADR 0002). History in `firmware/README.md`.
- **Plugin in Ableton:** none installed.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
Nothing. `build-plugin/` is the plugin agent's scratch build dir (gitignored; safe to delete).

## Next steps
1. **Owner runs the M0 hardware check.** If anything lands in the wrong place, the fix is in `firmware/main.cpp` (`kPotKnob` is a hardware fact from Resilio; `kPotParams` is ADR 0017).
2. **Owner listening** on the five pages (`tools/render_review_pages.sh` remakes them after any change), then **M8 tuning** from the notes: every *(start)* number is in `core/params/SpaceVoicing.h` and `EraVoicing.h`.
3. **Owner decisions pending:** the ADR 0008 makeup; MASSIVE's SIZE glide speed (ADR 0007 amendment); plugin identity.
4. **At the rack:** the M0 check, then M3 (flash `immensio_versio_profile.bin`, read the serial CPU report; an SDRAM microbenchmark decides whether the PLATE moves to AXI SRAM).

## Waiting on the owner
- M0 hardware check results.
- Plugin identity (`docs/TASKS.md`): the AU shows as "Resilio: Immensio Versio" (company name "Resilio", manufacturer `Rslo`, plugin code `ImVs`). Keep, or name the company differently?

## Will bite
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
