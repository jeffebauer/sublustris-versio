# 0001 — A new repo, seeded from Resilio Versio

**Status:** Accepted, 21 Oct 2026 (owner approved spec v1.0). Phase 1 finding; `docs/spec-session.md` "Phase 1 findings".

**Context:** Resilio Versio already solved most of the plumbing: one DSP Core with no platform code, three Hosts (Renderer, JUCE Plugin, Versio firmware) that share one `ParamSpec` table, a dependency-free test runner, review pages and a 128 KB flash discipline. But Resilio is frozen (v1.0), public and released, and its DSP blocks include its own voicing headers (`params/*Voicing.h`).

**Decision:**
- Immensio is a **new repo**. Its generic pieces are **copied** from Resilio, with the source commit noted in `NOTICE`. Namespace `iv::` instead of `rv::`.
- **Taken as they are:** the three-Host layout and `ParamSpec` pattern (keys never renamed), `host/common` (Wav, Json, Fft, Spectrogram, Metrics, Automation, Sweep), the test runner and ctest wiring, the Plugin shell and `plugin_host_test`, firmware `PotEndStops`, `LedMeter`, LED PWM, the release / m0test / profile build variants, `ProfileHook`, `Filters`, `Oversampler`, `Select`, `Seed`, `SizeOpt`, and the tools (stimulus, review pages, sweetspot, install and release scripts).
- **Refactored (maths kept, Resilio's voicings dropped):** μ-law (`OutputBits` → Era's output layer, `EchoBits` → the Midiverb box), `Drive` / `EchoWear` (saturation, tape), `Wobble` (→ DRIFT).
- Our code is MIT, as Resilio's.

**Rejected:**
- **Monorepo:** every Immensio change could touch a shipped, frozen product.
- **Shared DSP library now** (submodule): it would churn both repos while Immensio's code is still moving. It can be pulled out once both firmwares settle on the same code.
