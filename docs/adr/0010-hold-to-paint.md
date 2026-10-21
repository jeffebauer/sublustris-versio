# 0010 — Hold to paint into the frozen pad

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 11. Resolves the brief's [OPEN] "slow feed of the small reverb into the frozen pad".

**Context:** Layering what you play into the pad is musical, but a pad that absorbs everything all the time stops being a stable background.

**Decision:**
- While frozen, **holding the button** (≥ 0.3 s, ADR 0011) blends what is playing, through the companion, into the pad over ~3 s (a starting guess).
- On release the pad **keeps its new layers**. With no hold, the pad is untouched.
- LEDs: painting pulses the output pair slowly (ADR 0016).

**Rejected:** a slow feed all the time; no layering at all.

**Amendment (21 Oct 2026, owner's freeze_twin notes; SPEC v1.0.7):** "Once it's painted into the pad, it becomes dominant." Measured: painting opened the pad's input to the raw playing and the pad grew 7 dB; after release its makeup kept lifting it toward the paint headroom. Now painting copies a little of the **twin** (never the raw input) and eases off over the last 1 dB below a ceiling 1.5 dB over the frozen level; a painted pad then settles without a ceiling, like a fresh one. Painting adds ~1 dB; the pad's own layers stay on top.
