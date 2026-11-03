# 0041 — The tape's drive back to 11.25 dB; the worn cassette stays

**Status:** Accepted, 3 Nov 2026 (owner, pick B on all 10 cards of `renders/tape_fold`). SPEC v1.0.66. Reverses ADR 0040's drive (B) and keeps its cassette (C). Amends ADR 0027.

**Context:**
- **The owner, at the rack on the combined build (v1.0.63 + v1.0.64 + v1.0.65), 3 Nov 2026:** "it feels like the saturation is too extreme now - by 3 o'clock on the wear knob, the saturation is folding back too much."
- **Measured** (`renders/tape_fold/_measure.txt`): the cause is squash and denser saturation, not aliasing.
  - 3 o'clock (WEAR 0.8) drove the tape 13 dB, more than v1.0.62's full WEAR (11.25). The clip's products on a chord rose about 7 dB there (HALL pad −23.4 → −16.1 dB re output, CANYON pad −16.7 → −9.4), landing between the notes as an out-of-tune haze.
  - The tail's crest fell: CANYON pad at 3 o'clock 12.5 dB (v1.0.62) → 11.3, at full 12.1 → 10.3.
  - The tape's own aliasing, on the real wet, was 100 dB or more under its output; an anti-aliased clip (first-order ADAA, Parker et al. DAFx-16) measured the same as the plain one, so it was not offered.
- **The page** (100 % wet, pad and stabs; HALL at WEAR 3 o'clock, full and noon, CANYON at 3 o'clock, CANYON endless at WEAR full and MOTION full left; all level within 0.05 dB):
  - A, the combined build (16.25 dB).
  - B, v1.0.62's drive (11.25 dB), the cassette kept.
  - C, 16.25 dB at full with the drive in dB following WEAR², so 3 o'clock lands near v1.0.62's.
  - D, 13.75 dB.

**Decision:**
- **B is the sound:** `tex::kTapeDriveDb` 11.25 dB (`dsp/EraTexture.h`), as from v1.0.49 to v1.0.64. The prototype's hook (`IV_TAPE_FOLD`, branch `proto/tape-fold`) is not merged.
- **ADR 0040's cassette stays:** MOTION's left side (wow and flutter ×1.25, crinkle and dips from about 9:30, deeper dips) is unchanged.

**Rejected:**
- **A:** too extreme on the module by 3 o'clock.
- **C and D:** the owner picked B on every card, including WEAR full, where C equals A.
- **Anti-aliasing the clip:** no audible gain (above), at about 3× the clip's cycles.

**Consequences:**
- **Sound:** WEAR's tape is v1.0.62's again; everything else is as in the combined build. WEAR 0 and MOTION are unchanged.
- **Aliasing:** `test_era`'s hot 7 kHz tone; its limit goes back down (see the test's comment for the reading).
- **Words:** the README, the manual and the release read-me no longer say the tape is "driven hard near the top".
