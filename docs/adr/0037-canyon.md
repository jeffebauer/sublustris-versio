# 0037 — SPACE's third position is CANYON (was MASSIVE)

**Status:** Accepted, 1 Nov 2026 (owner). SPEC v1.0.62. The name only: no sound change.

**Context:**
- **SPACE read as two nouns and an adjective:** PLATE · HALL · MASSIVE. The owner wanted three places: PLATE · HALL · CANYON, a space so big its echoes blur into a wash, which is what the position does (ADR 0007).
- **The panel art, the website copy (`docs/minisite/content/`) and the README** already said CANYON; the plugin, the renderer and the firmware's tables still said MASSIVE (`core/params/ParamSpec.h`).
- **What a rename can break**, as for the product's (ADR 0032): saved sets, automation, presets, sweeps and the tools' scripts.
  - The plugin saves SPACE by its key (`space`) and its position (0, 1, 2), never by its label: the JUCE choice parameter's value is the index, and VST3 / AU parameter IDs come from the key.
  - Presets, sweeps (`presets/sweeps/*.json`), the review-page scripts and `tools/same_render.py` name the position by its label: `"space": "MASSIVE"`, `--set space=MASSIVE`.

**Decision:**
- **The shown label is CANYON:** ParamSpec's SPACE choices are PLATE / HALL / CANYON, so the plugin's switch and its host text, the renderer's help and its sidecars say CANYON.
- **MASSIVE still reads:** the renderer and its presets accept `MASSIVE` as SPACE's right position, as `NORMAL` still reads for ONSET (`host/common/ParamsJson.cpp`). Labels stay case-sensitive.
- **Kept on purpose (what the code and the tools depend on):**
  - the ParamSpec key `space` and the plugin codes;
  - internal identifiers: `voicing::kMassive*`, `map::massive*`, `kTwinRoomMassiveDb`, `test_massive`, the golden fingerprints' `massive_*` names, file names;
  - comments in the DSP code, the firmware's profiling-lap labels (`MASSIVE S1 ...`, `fdnMassive24k`) and debug strings, the renderer's `IV_*` hooks;
  - sweep files, the tools' and pages' scripts (`tools/pages/`, `tools/same_render.py`, `tools/sweetspot.py`), which go on passing `MASSIVE`;
  - history: the changelog lines, ADRs 0001-0036, `docs/research/`, the handoff's past sessions and the round logs.

**Rejected:**
- **Renaming the identifiers with the label:** hundreds of edits across the Core, the tests and the tools, for nothing anyone hears or sees, and every old page script would stop reading.
- **Dropping MASSIVE as an input:** every preset, sweep and page script written before v1.0.62 would fail to render.

**Consequences:**
- **Sound:** none. `tools/same_render.py` against v1.0.61: every render the same, sample for sample; the goldens pass unchanged.
- **Saved sets:** a set saved with MASSIVE reopens on the same position, now labelled CANYON.
- **Sidecars** written from v1.0.62 say `"space": "CANYON"`; older ones say MASSIVE, and both read back.
- **The module:** nothing on it shows the name; release flash changes by a few bytes (the string).
