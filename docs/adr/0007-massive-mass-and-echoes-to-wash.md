# 0007 — MASSIVE: mass, and SIZE from echoes to wash

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 6, 7, 9, as amended by decision 29 (ADR 0015) before the freeze.

**Context:** Three of the owner's four favourite Supermassive presets are the Sagittarius family (CygnusX1, DarkMatter, BensonArizona). Their defining trait is **mass**: "slow to get going, slow to go away" (Costello), with the attack tied to Feedback. The fourth, Ice Floe Nowhere To Go, is Large Magellanic Cloud: "LOOOONG repeating echoes, long decay, medium attack", echoes at 4× DELAY at Density 0, a lush wash with Density up and Warp ≥ 50 %. The first draft gave Massive a 1 s swell at low Decay, faster with Decay, and up to ~3 s with Size. Once ATTACK became a switch for every space (ADR 0015), the long swells moved to BLOOM, where TIME sets them; the owner wants a bloom whose timing they control so it doesn't "interfere with harmonies".

**Decision:**
- **SIZE in Massive goes from echoes to wash:** low SIZE = few, long, near-equal line ratios with low diffusion, giving distinct repeating echoes up to ~2 s (Large Magellanic Cloud). Turning up spreads the ratios (Warp-style) and raises diffusion into a boundless wash. Predelay grows with it, 0 → 500 ms (Andromeda).
- **Mass:** a **swell stage**, a feedforward cascade of long modulated delays and allpasses before the tank, makes the attack a slab, not a hit. It is shared with BLOOM in every space.
- **In ATTACK = NORMAL, Massive keeps a gentle built-in mass:** A = A₀(Decay) · m(Size), A₀ 0.5 → 0.2 s across Decay (Sagittarius: higher Decay, faster swell), m 1 → 1.5 across Size.
- **In BLOOM:** TIME sets the swell, still × m(Size), so Size adds a little mass.
- Andromeda-scale tails stay tame through ADR 0008.

**Rejected:**
- Swell ~3 s or ~6 s at low Decay (the owner chose ~1 s, a "playable bloom", before ADR 0015 moved long swells to BLOOM).
- SIZE "just bigger" in Massive (always a wash).
- Swell set by Decay only, or a fixed swell.
- Massive keeping its own Decay/Size coupling and ignoring the ATTACK switch.
