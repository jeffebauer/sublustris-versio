# Handoff

**Written:** 21 Oct 2026, end of session 1. Start the next session with `/immensio-start`. **Nothing running:** every agent finished and was reviewed.

## State

| Milestone | State |
|---|---|
| Spec | **v1.0 frozen**, ADRs 0001–0017 |
| M0 desktop | Built: Core (passthrough `iv::Engine`) + Renderer + tests + Plugin (AU/VST3) |
| M0 firmware | Built: release / m0test / profile |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | **Built**: FDN (8 lines), diffusers, predelay; `test_fdn` passes; listening page `renders/m1_hall/` (owner check pending) |
| M2–M9 | Not started (M2 plugin criteria already pass in `plugin_host_test`) |

- **Gates:** see the commit message of this handoff's commit for the ctest summary line and firmware sizes at wrap.
- **Firmware sizes (21 Oct, M1):** release 61,484 B (46 %), profile 62,312 B (47 %), m0test 82,356 B (62 %). Budget for release at v1.0 is 110 KB (ADR 0002). History in `firmware/README.md`.
- **Plugin in Ableton:** none installed.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
Nothing. `build-plugin/` is the plugin agent's scratch build dir (gitignored; safe to delete).

## Next steps
1. **Owner runs the M0 hardware check.** If anything lands in the wrong place, the fix is in `firmware/main.cpp` (`kPotKnob` is a hardware fact from Resilio; `kPotParams` is ADR 0017).
2. **M4: MASSIVE + ATTACK** (SPEC §4.5, §4.11): the swell stage, Massive's voicing on the FDN, BLOOM, REVERSE, then the floor governor for infinite (ADR 0008).
3. Early **M3 microbenchmark** (SPEC §5.2): SDRAM sequential vs modulated reads on the chip, before features stack.

## Waiting on the owner
- M0 hardware check results.
- Plugin identity (`docs/TASKS.md`): the AU shows as "Resilio: Immensio Versio" (company name "Resilio", manufacturer `Rslo`, plugin code `ImVs`). Keep, or name the company differently?

## Will bite
- The ringing metric flags long or infinite renders of **pitched** material (vibes) as "steady tone": the reverb holding the notes, not metallic modes. On broadband noise the Hall reads ≤ 5 dB at every length. Judge metallic-ness on noise / hits, not on held notes.
- TONE is an M1 placeholder: it moves the per-line high/low T60 ratio (`map::toneHighT60Ratio`), not yet SPEC §4.6's pivoted tilt.
- Every SPACE position plays the Hall until M4/M5.
- **Flash:** Resilio ended at 97.7 %. Record sizes at every wrap; any Core change can move them.
- `plugin/PluginEditor.cpp` includes `firmware/LedMeter.h` (shared freeze tint). Change its API and the plugin breaks.
- The editor hard-codes DRIFT's 0.47–0.53 dead zone; move it to `core/params/Mappings.h` when DRIFT's mapping lands.
- `ProfileHook.h` defines `markHook` inline in the header (no core `.cpp` existed). Fine, but don't also define it in a `.cpp`.
- `test_audio/stimulus/*.wav` are generated (`python3 tools/make_stimulus.py`), not in git; `test_metrics` fails without them.
- The P7/SW1 demo sketch (`docs/prototypes/p7-sw1-demos/`) is throwaway: none of it belongs in `core/`.

## Where to look
`SPEC.md` §4.3, §5, §7 · `docs/adr/README.md` · `core/dsp/Engine.h` · `core/params/ParamSpec.h` · `firmware/README.md` · `docs/TASKS.md`
