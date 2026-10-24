# 0019 — ERA gains vintage reads and tape out

**Status:** Accepted, 24 Oct 2026 (owner's picks on `renders/dsp_audit`, B on every card). SPEC v1.0.22.

**Context:** The DSP audit (`docs/research/dsp-audit.md`, E4 and E5) measured two of ERA's stages against the gear they evoke:
- **μ-law stretch (E4).** Our tank reads its delays with clean allpass interpolation at every ERA position. The Lexicon 224 and PCM70 that ERA's middle stretch evokes read theirs coarsely: linear interpolation with the fraction quantised to 32 or 64 steps, and a noisy modulation (Costello).
- **Cassette (E5).** The cassette's in-loop drive never reaches its knee on music: renders were bit-identical with it bypassed. So the cassette stage had no audible saturation.

**Decision:**
- **Vintage reads** (HALL and MASSIVE): across ERA's μ-law stretch, every FDN line's read crossfades to the 224 / PCM70 way: linear interpolation with its fraction quantised to 32 steps, plus a small random read jitter each control block, smoothed once (`Fdn::setVintage`, `kVintageSteps`, `kVintageJitterSamples`). This is in-loop colour, so a freeze glides it out (ADR 0009).
  - The 32 steps follow Costello on the 224.
  - The jitter's size is our own, tuned on the measured noise halo around a held note: −39 → −30 dB in HALL, −42 → −39 dB in MASSIVE.
- **Tape out:** the cassette stage saturates the wet on its way out, outside the loop (`Engine::tapeOut`).
  - An asymmetric tanh gives even and odd harmonics as tape does, at unity gain for small signals, with drive growing with the cassette layer.
  - It runs at 4× (2× inside 2×), followed by a 5 Hz DC blocker.
  - It adds distortion about 24–26 dB under the signal. Our own design, after dub tape returns and ChowTape.

**Consequences:**
- **Energy.** Vintage reads were energy-checked (sweep and 10-minute holds at ERA 0.8 and 1 pass). Tape out is outside the loop.
- **Cost.**
  - Desktop CPU: about +2 % for vintage reads and +1 % for tape out.
  - Release flash: about +2 KB.
- **Testing aliasing.** Tape's asymmetry puts real difference tones below 5 kHz on a hot 7 kHz test tone (−35 dB). So `test_era`'s aliasing check now compares 48 kHz against 96 kHz: aliasing is what the lower rate adds, and here it adds nothing.
- **PLATE.** Vintage reads are FDN only; the PLATE keeps its reads.

**Amendment (24 Oct 2026, owner's pick on `renders/tape_bias`; SPEC v1.0.26):** tape out's asymmetry is a third of the first one's. The full bias bent even quiet signals, so held notes picked up a strong 2nd harmonic: "saturation-like distortion on resonances". At a third the held notes stay clean enough, and loud peaks keep the tape's even warmth. A test hook `setTapeOut(false)` bypasses the tape for measuring.
