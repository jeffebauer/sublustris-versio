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

**Amendment (24 Oct 2026, owner's pick on `renders/dsp_audit`; SPEC v1.0.22):** a frozen pad now holds truly still. The tank's own modulation floor (FDN) and the PLATE's chorus and floor glide out with the freeze too: Costello's "mod depth to zero" for a freeze. Before, a frozen chord slowly shimmered out of tune over minutes. Its purity (energy on the chord's own notes) fell from 0.88 to 0.66 in HALL over 5 minutes; it now holds 0.93. DRIFT still moves a frozen pad if you turn it.

**Amendment (26 Oct 2026, owner's pick on `renders/twin_louder`; SPEC v1.0.41):** "Let's shoot for 8db under the pad" (B, louder, on every card, the repeated cards agreeing; MIX 50 %). The owner could hardly hear the twin over a frozen MASSIVE: measured 12-15 dB(A) under the pad, not 6, because its tail is cut at 8 s while HALL's and MASSIVE's tank run to 14 s or forever. In HALL and MASSIVE the twin's output is lifted by the square root of how much its tail is cut (sqrt(tank T60 / 8 s), at most `kTwinFdnGain` 1.862, +5.4 dB); an uncut twin (a short HALL) is unchanged, PLATE too. Over SIZE 0.3/0.9 × DECAY 0.6/0.9/1 × Neve/cassette the twin now sits MASSIVE −5.5 to −11.3 dB(A) under the pad (mean −8.5), HALL −2.0 to −11.1 (mean −7.8). A flat +5.4 dB was rejected: a short HALL's twin came level with the pad and Paint overshot its ceiling (+2.3 dB); with the lift Paint stays at ≤ +1.3 dB. Rejected on the way (`renders/freeze_moment`, owner: "A today sounds more seamless"): holding or gliding the ducking at the freeze; the step heard at a freeze is the ducking letting go, kept.

**Amendment (26-27 Oct 2026, owner's picks on `renders/twin_alternatives`, `small_room`, `room_level` and `room_motion`; SPEC v1.0.43, v1.0.47; ADR 0028):** in HALL and MASSIVE what you play over a frozen pad goes into a small room (`dsp/TwinRoom.h`: a 150 Hz low cut, eight short lines, a slightly softer top), not a second big tank. Its level follows the pad as the twin's did, and its movement scales with SIZE. PLATE keeps its twin. The button reads FREEZE / PAINT on the panel and the gate FREEZE (ADR 0029).
