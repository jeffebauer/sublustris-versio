# 0005 — Cassette hiss goes into the tank, ducked by the input

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 3.

**Context:** ERA's Cassette stage adds hiss. A steady hiss bed on the output sits outside the reverb and doesn't move with it; hiss inside the reverb sounds like a noisy desk send. But with infinite Decay, hiss fed into the tank would fill the tail with noise.

**Decision:**
- Hiss enters at the **tank input** (up to ~−66 dBFS, a tuning number), so it is reverberated and shaped by TONE.
- It is **ducked by the input level**: with no input, it falls away, so an infinite tail doesn't fill with noise. Test: a 2-minute silent render keeps hiss below −90 dBFS.

**Rejected:** hiss on the output (simple, constant, but outside the reverb); both, in small amounts.
