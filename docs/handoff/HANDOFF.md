# Handoff

**Written:** 21 Oct 2026, end of session 1. Start the next session with `/immensio-start`. **Nothing running:** every agent finished and was reviewed.

## State

| Milestone | State |
|---|---|
| Spec | **v1.0 frozen**, ADRs 0001–0017 |
| M0 desktop | Built: Core (passthrough `iv::Engine`) + Renderer + tests + Plugin (AU/VST3) |
| M0 firmware | Built: release / m0test / profile |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1–M9 | Not started |

- **Gates:** see the commit message of this handoff's commit for the ctest summary line and firmware sizes at wrap.
- **Firmware sizes (21 Oct):** release 53,852 B (41 %), m0test 82,356 B (62 %), profile 52,224 B (39 %). That's the platform floor before DSP; budget for release at v1.0 is 110 KB (ADR 0002).
- **Plugin in Ableton:** none installed.
- **Seed provenance:** copied from Resilio Versio at `2ab71a9` (NOTICE, ADR 0001).

## In flight
Nothing. `build-plugin/` is the plugin agent's scratch build dir (gitignored; safe to delete).

## Next steps
1. **Owner runs the M0 hardware check.** If anything lands in the wrong place, the fix is in `firmware/main.cpp` (`kPotKnob` is a hardware fact from Resilio; `kPotParams` is ADR 0017).
2. **M1: FDN Hall, clean** (SPEC §4.3, §7 M1). New `core/dsp/Fdn.h/.cpp`: 8 lines, Hadamard, Jot per-line gains, modulated cubic reads with the modulation floor, damping. Mappings for DECAY (T60 table, SPEC §3) and SIZE (Hall: 40–180 ms, slew-limited) in `core/params/Mappings.h`. Tests: stability grid, T60 within 10 %, the metallic check (resonance ratio, `host/common/Metrics`), determinism, Size-sweep clicks. Listening: review page on the stimulus set.
3. Early **M3 microbenchmark** (SPEC §5.2): SDRAM sequential vs modulated reads on the chip, before features stack.

## Waiting on the owner
- M0 hardware check results.
- Plugin identity (`docs/TASKS.md`): the AU shows as "Resilio: Immensio Versio" (company name "Resilio", manufacturer `Rslo`, plugin code `ImVs`). Keep, or name the company differently?

## Will bite
- **Flash:** Resilio ended at 97.7 %. Record sizes at every wrap; any Core change can move them.
- `plugin/PluginEditor.cpp` includes `firmware/LedMeter.h` (shared freeze tint). Change its API and the plugin breaks.
- The editor hard-codes DRIFT's 0.47–0.53 dead zone; move it to `core/params/Mappings.h` when DRIFT's mapping lands.
- `ProfileHook.h` defines `markHook` inline in the header (no core `.cpp` existed). Fine, but don't also define it in a `.cpp`.
- `test_audio/stimulus/*.wav` are generated (`python3 tools/make_stimulus.py`), not in git; `test_metrics` fails without them.
- The P7/SW1 demo sketch (`docs/prototypes/p7-sw1-demos/`) is throwaway: none of it belongs in `core/`.

## Where to look
`SPEC.md` §4.3, §5, §7 · `docs/adr/README.md` · `core/dsp/Engine.h` · `core/params/ParamSpec.h` · `firmware/README.md` · `docs/TASKS.md`
