# 0016 — LEDs: level meters plus the freeze state

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 21. Follows Resilio ADR 0031.

**Context:** The Versio has 4 RGB LEDs. Level meters proved useful on Resilio; the freeze and painting states have no other visible sign.

**Decision:**
- Level meters as Resilio: LED 1–2 In L / In R, LED 3–4 Out L / Out R; green → amber with level, red on clip or when the limiter pulls down.
- **Frozen:** the output pair turns cool blue under the meter. **Painting:** it pulses slowly (~1 Hz).

**Rejected:** LEDs glowing ERA's stage colour with freeze shown on top (no level meters).
