# 0013 — SPACE flips crossfade; true stereo in

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 17, 23.

**Context:** Letting the old space ring out while the new one starts would need two big tanks running at once, costing CPU headroom and maybe Massive's size. Stereo sources should keep their placement in the reverb; the Versio normals In L to In R in hardware.

**Decision:**
- **SPACE flips crossfade over ~100 ms:** the old tail fades out while the new space takes the input. **Only one big tank ever runs.**
- **True stereo in:** L and R enter the tank at different points (different FDN lines / Plate inputs), so stereo placement survives. A signal on L alone reaches both inputs through the hardware normal.

**Rejected:** the old tail ringing out (two tanks at once); summing L and R into a mono send (as Resilio).
