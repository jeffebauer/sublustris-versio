# 0009 — Freeze: a tap latches, the pad holds still, unfreeze glides

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 10, 25, 26.

**Context:** The button is a two-layer freeze: the big tank locks as a background pad, and the input moves to a small companion reverb so playing on top isn't dry. It has to work hands-free, and the knobs that played the companion while frozen may have moved by the time the tank comes back.

**Decision:**
- **A tap latches the freeze**; the next tap unfreezes, and the pad decays naturally. While frozen, SIZE and DECAY play the companion (scale 0.25 → 0.6, T60 0.3 → 6 s).
- **The pad holds still:** loop gain exactly 1, input closed (20 ms ramp), in-loop colour glided out over ~0.2 s. Only the modulation floor and DRIFT remain. It is a pad you play over, not the evolving floor of ADR 0008.
- **Unfreeze glides to the knobs:** the big tank moves to the current SIZE and DECAY over ~1 s as the pad releases, a tape-like swoop if SIZE moved (as SIZE's bend, ADR 0012).
- Press timing and the gate: ADR 0011. Painting: ADR 0010.

**Rejected:**
- Freeze held only while the button is held; tap-latch plus momentary hold.
- A pad that evolves to the floor like infinite Decay.
- Soft takeover on unfreeze (no swoop, but a knob can feel dead for a while).

**Amendment (21 Oct 2026, owner's picks on `freeze_options` and `freeze_twin`; SPEC v1.0.7):** the M5 note "the frozen and secondary reverbs sound too disparate" led to a level-matched choice: the companion on top (today), the input feeding the pad, or both. The owner picked the feed everywhere, "more subtle, and should eventually recede back to the original frozen buffer state". Built as the **twin**: what you play goes into a second tank of the frozen space (no predelay or swell), heard beside the untouched pad at −10.5 dB and fading within 8 s, so the pad returns to what it was. Confirmed on every card ("a more subtle layer on top of the frozen pad, not the main focus"). Size and Decay play the twin while frozen, as they played the companion. The companion plate and its 84 KB of DTCM are gone.

**Amendment (23 Oct 2026, owner's pick on `renders/twin_level`; SPEC v1.0.14):** the twin sits 6 dB under the pad (level 0.5, was 0.3 / −10.5 dB): playing over a frozen pad dropped 6–7 dB at the tap. C (+6 dB) on all 24 cards, "the sweet spot is between plus3 and plus6", so +4.4 dB. MASSIVE's twin swells in on an S-curve (v1.0.13, `renders/swell_curve`, B on all 18).
