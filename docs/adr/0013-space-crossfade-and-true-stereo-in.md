# 0013 — SPACE flips crossfade; true stereo in

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 17, 23.

**Context:** Letting the old space ring out while the new one starts would need two big tanks running at once, costing CPU headroom and maybe Massive's size. Stereo sources should keep their placement in the reverb; the Versio normals In L to In R in hardware.

**Decision:**
- **SPACE flips crossfade over ~100 ms:** the old tail fades out while the new space takes the input. **Only one big tank ever runs.**
- **True stereo in:** L and R enter the tank at different points (different FDN lines / Plate inputs), so stereo placement survives. A signal on L alone reaches both inputs through the hardware normal.

**Rejected:** the old tail ringing out (two tanks at once); summing L and R into a mono send (as Resilio).

**Amendment (24 Oct 2026, owner's picks on `renders/valhalla_round2`; SPEC v1.0.25):** the PLATE's cross-branch output taps are added, not subtracted as in Dattorro's table. With true-stereo input his signs made left and right lows near mirror images, which sounded vague and less centred than Valhalla's plates. Added, the lows centre and the highs stay wide. The HALL now takes MASSIVE's early taps too, so it starts ~10 ms after the note instead of ~80 ms.

**Amendment (9 Nov 2026, owner's picks on `renders/space_flip`, `space_morph`, `space_morph2`, `morph_last_98`; SPEC v1.0.78; ADR 0054):** a SPACE change no longer cuts the tail, so "only one big tank ever runs" no longer holds: PLATE <-> HALL or CANYON runs both big tanks for the 3 s of the morph. HALL <-> CANYON reshapes the one FDN. True stereo in is unchanged.
