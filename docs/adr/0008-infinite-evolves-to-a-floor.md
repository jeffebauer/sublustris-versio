# 0008 — Infinite tails evolve to a warm floor

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 8.

**Context:** Valhalla's "Infinite Supermassive" post keeps EQ outside the loop and sets mod depth to 0, so a frozen tail keeps its pitch and brightness. The brief puts damping, saturation and ERA inside the loop, so the tail darkens and grains as it decays. The brief also wants tails that "sustain near-infinitely without running away".

**Decision:**
- At DECAY 0.85–1 the loop gain reaches 1 at the floor. TONE, ERA and the saturation **stay in the loop**, so the tail keeps slowly darkening and graining.
- The always-on soft saturator plus a slow RMS loop-level governor hold it at a **warm floor** (−24 dBFS RMS, a starting guess). It never turns to mud and never falls silent.
- Test: a 10-minute render at DECAY 1 holds the floor (±3 dB, no growth, no NaN) at every ERA stage.
- The frozen pad is different: it holds still (ADR 0009).

**Rejected:** holding still at infinite (in-loop colour bypassed); Andromeda-style fade over thousands of seconds instead of true infinite.
