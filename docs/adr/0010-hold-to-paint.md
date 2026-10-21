# 0010 — Hold to paint into the frozen pad

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 11. Resolves the brief's [OPEN] "slow feed of the small reverb into the frozen pad".

**Context:** Layering what you play into the pad is musical, but a pad that absorbs everything all the time stops being a stable background.

**Decision:**
- While frozen, **holding the button** (≥ 0.3 s, ADR 0011) blends what is playing, through the companion, into the pad over ~3 s (a starting guess).
- On release the pad **keeps its new layers**. With no hold, the pad is untouched.
- LEDs: painting pulses the output pair slowly (ADR 0016).

**Rejected:** a slow feed all the time; no layering at all.
