# 0012 — MIX, SIZE, TONE and DRIFT

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 14, 15, 18, 20. Resolves the brief's [OPEN] kill-dry and SIZE zipper items.

**Context:** Four knob behaviours from the spec session. Ranges and curves: SPEC §3.

**Decision:**
- **MIX:** equal-power dry/wet. Fully left = a clean passthrough (the μ-law layer is wet-only); fully right = 100 % wet for a send/return. **No separate kill-dry setting.**
- **SIZE moves bend the pitch, tape-like** (Cathedra/Supermassive): the delays glide, **slew-limited** to about ±1 semitone (≤ 6 % delay change per second), so fast knob or CV moves become a smooth swoop over up to ~1.5 s, never zipper. While frozen, SIZE plays the companion, so the pad never bends.
- **TONE:** a tilt in the loop; **noon is flat**. Left darkens the tail each pass, right brightens it; loop gain is renormalised so bright never exceeds 1.
- **DRIFT is bipolar:** noon is still (only the anti-metallic modulation floor). Left: random tape wow/flutter. Right: smooth Cathedra-style pitch drift and bend. ERA's Cassette wow/flutter adds on top of the left side.

**Rejected:**
- MIX "dub curve" (dry at full up to noon).
- SIZE crossfading with no pitch change (smoother, less playable).
- TONE noon gently dark.
- One-way DRIFT (tape wobble left to ERA only).
