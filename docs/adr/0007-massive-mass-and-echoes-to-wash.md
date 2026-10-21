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

**Amendment (21 Oct 2026, M4 build; SPEC v1.0.1, for the owner to confirm by ear):**
- SPEC §3 gave MASSIVE's SIZE a mean line of 60 ms → 2 s, which can't produce "long repeating echoes at low SIZE". Built as **0.35 → 2 s**, with the spread 0.08 → 1 and diffusion 0.15 → 1. Measured: the tail's envelope spikiness (how echo-like it is) falls steadily from 2.9 dB at SIZE 0 to 0.36 dB at SIZE 1, as smooth as HALL (`test_massive`).
- Long lines alone don't blur into a wash for many seconds, so MASSIVE gets **in-loop diffusion**, two allpasses inside each line, scaled by SIZE (Supermassive's Density does the same job).
- SIZE moves glide at the one-semitone limit (ADR 0012), so a full SIZE sweep in MASSIVE takes ~40 s (its lines change by up to 2.5 s). Owner question: keep the gentle bend, or allow a faster swoop with more bend in MASSIVE?
