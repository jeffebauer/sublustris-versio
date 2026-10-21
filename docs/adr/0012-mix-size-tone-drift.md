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

**Amendment (21 Oct 2026, owner's M1 listening notes; SPEC v1.0.8):**
- "Difference between small, medium, and large halls seem quite subtle. Small doesn't read as small." HALL's SIZE is now 20 → 260 ms of mean line, and it scales HALL's finite T60s (× 0.4 at SIZE fully CCW, × 1 at CW; a small room can't ring for 16 s), with the level made up. DECAY still sets the tail; infinite stays infinite.
- "Size small, vibes: long has a buzzy resonance build up": the same change (a small room's long, sparse tail let pitched notes build up; vibes ringing score 43 → 26 dB, noise unchanged).
- "Dark and bright don't feel like extreme ends... lower lows, higher highs": TONE also tilts the wet round 600 Hz (CCW +4 / −9 dB, CW −4 / +6 dB, flat at noon), and its loop damping range widens (high-end T60 ratio 0.08 → 1.5). Noon stays flat.
