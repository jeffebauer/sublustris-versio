# 0004 — ERA's grain in two places; the Midiverb box at 24 kHz

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 2, 4.

**Context:** μ-law makes loud peaks gritty and keeps the quiet tail smooth (Resilio ADR 0042). Midiverb-style bit loss inside the loop does the opposite: the tail gets grainier as it fades. The original Midiverb is 12-bit, ~10 kHz bandwidth, ~23.4 kHz rate (the rate is an emulator's figure, not a factory spec). The brief hoped a lower internal rate at the lo-fi end would also save CPU.

**Decision:**
- **Both:** from the μ-law stage on, μ-law sits on the **wet output** (24 kHz, 12 → 10-bit). At the Midiverb stage, bit loss also runs **in the loop** (12 → 8-bit, truncated toward zero so it can't hold a buzz; the dither was dropped in SPEC v1.0.3).
- **The Midiverb end at the old rate:** the loop passes through a **48 → 24 → 48 kHz box** (polyphase IIR halfbands, Resilio `Oversampler`/`EchoBits` style). Its images are left a little (stopband ~−40 dB, a tuning number) for some aliasing grit. True ~10 kHz ceiling.
- The tank itself keeps running at 48 kHz, so this stage **saves no CPU**: a live tail's whole tank rate can't be switched with a clean crossfade.
- Midiverb **character only** (rate, bits, halving accumulator maths). No ROM microcode. emeb/MIDIVerb_RE (MIT) may inform the code; thement/midiverb_emulator (GPL-3.0) is reference only.

**Rejected:**
- μ-law in the loop as well (grittier, harder to keep sweet).
- Output-only grain (safest for infinite tails, but no grain growing as the tail fades).
- 48 kHz with filters only (no aliasing character).
- Running the whole tank at 24 kHz at the lo-fi end (can't crossfade a live tail).
