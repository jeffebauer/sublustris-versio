# 0033 — The endless tail settles 3 dB higher: the floor at −21 dBFS

**Status:** Accepted, 28 Oct 2026 (owner, pick D on `renders/level_duck`). SPEC v1.0.51. Amends ADR 0008's floor level (the mechanism is unchanged).

**Context:**
- **The owner's note** after the v1.0.49 rack test: "on hall and massive with Long decays, I feel the limiter is keeping the level a bit too low", and sweeping BLEND from 50:50 to fully wet "the reverb gets noticeably louder".
- **Measured** (`docs/research/level-at-infinite.md`): not the limiter (it takes 0.00-0.13 dB on music; nothing reaches it at infinite DECAY). At infinite DECAY the wet sat 7-12 dB under the dry while playing: the floor governor's −24 dBFS floor gave 4.3 dB (HALL) / 7.5 dB (MASSIVE) of it, the 4:1 duck the rest.
- **The page** (`renders/level_duck`) offered the duck at 2:1 and 1.5:1, the floor at −24, −21 and −18, and combinations. The owner picked D: "Duck 2:1 floor21 sounds the best".

**Decision:**
- **The floor governor's floor is −21 dBFS RMS** (`voicing::kFloorDbfs`, was −24): an infinite tail settles about 3 dB higher; the darkening toward the floor stops 3 dB earlier.
- Everything that follows the floor moves with it: a painted pad's room to grow (the floor + `kPaintHeadroomDb`), and the floor's glide out with a freeze.
- The duck's 2:1 and its following the playing are ADR 0014's amendment of the same day.

**Checked before building** (`tools/pages/level_duck.py --safety`, and again on the build with `tools/pages/level_glue.py --safety --built`): fed 10-minute infinite holds in HALL and MASSIVE don't grow and their tails hold; a 5-minute freeze with a paint holds still; WEAR full with SHIMMER at infinite doesn't grow; no clicks through the duck's and the floor's onset (DECAY swept 0.7 → 1 → 0.7 while playing).

**Not picked (heard on the page):** the floor at −18 dBFS, the duck at 1.5:1, and easing the duck alone with the floor left at −24.

**Amendment (28 Oct 2026, owner's pick C on every card of `renders/hall_level`; SPEC v1.0.52):** **HALL's floor is −18 dBFS** (`voicing::kHallTailDb`, 3 dB over `kFloorDbfs`); PLATE and MASSIVE stay at −21.
- **Why:** on `level_glue` the owner wrote "Hall infinite examples feel slightly less level matched than massive, particularly the first example - reverb feels a bit too quiet so when the set stops, there's a less seamless transition". Measured on the held chord: HALL's tail wasn't quieter than MASSIVE's overall (both ~3.9 dB under the dry) but darker (above 2 kHz 10.8 dB under the dry, MASSIVE 7.1).
- **The page** offered HALL's tail +1.5 dB (B), +3 dB (C) and a brighter tail at the same level (D, TONE's tilt nudged for HALL at infinite), with MASSIVE as a reference. The owner picked C: the level, not the brightness. HALL's tail now sits 0.7 dB under the dry on that chord.
- **Found on the way:** C's louder tail reached the hard clamp at the input of WEAR's compander (±2, there for onsets' overshoot), and the corner it put in the wave was a tick on the tail's biggest peaks. The clamp is now a soft knee (slope 1 at ±2, to ±2.5): every sample within ±2 is as before, and the few that overshoot change by a few hundredths of a dB of the mix (nulls −50 dB and deeper).
- A painted HALL pad's room to grow still uses the shared −21 floor (or the pad's own level, whichever is higher), as on the page.
