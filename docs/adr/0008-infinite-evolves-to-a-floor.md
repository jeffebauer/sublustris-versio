# 0008 — Infinite tails evolve to a warm floor

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decision 8.

**Context:** Valhalla's "Infinite Supermassive" post keeps EQ outside the loop and sets mod depth to 0, so a frozen tail keeps its pitch and brightness. The brief puts damping, saturation and ERA inside the loop, so the tail darkens and grains as it decays. The brief also wants tails that "sustain near-infinitely without running away".

**Decision:**
- At DECAY 0.85–1 the loop gain reaches 1 at the floor. TONE, ERA and the saturation **stay in the loop**, so the tail keeps slowly darkening and graining.
- The always-on soft saturator plus a slow RMS loop-level governor hold it at a **warm floor** (−24 dBFS RMS, a starting guess). It never turns to mud and never falls silent.
- Test: a 10-minute render at DECAY 1 holds the floor (±3 dB, no growth, no NaN) at every ERA stage.
- The frozen pad is different: it holds still (ADR 0009).

**Rejected:** holding still at infinite (in-loop colour bypassed); Andromeda-style fade over thousands of seconds instead of true infinite.

**Amendment (21 Oct 2026, M5/M6 build; SPEC v1.0.2 and v1.0.3; the makeup is for the owner to approve):**
- **Held tails hold.** Once the input has been quiet for 2 s, an infinite tail's own level (on an 8 s average, settled over 10 s) becomes its ceiling, so it can't creep up toward the floor. A frozen pad's ceiling is its own level 20 s after the freeze (ADR 0009). Above the ceiling the governor adds loss in proportion, as at the floor.
- **Makeup (an exception to SPEC §2.3 rule 2, loop gain ≤ 1):** a held tail that sags below its ceiling is raised back toward it, never above it, at most ~0.3 dB/s. Moving allpass-interpolated reads (fast DRIFT, the cassette's wow and flutter) lose a little energy, and without makeup an infinite tail at the cassette stage faded ~0.1 dB/s (60 dB in 10 minutes). The exception is bounded: only a held tail, only up to its own level, slowly.
- **Measured:** infinite tails hold within 0.5 dB for 10 minutes at every ERA stage (`test_era`), pads within 1 dB for 5 minutes in every SPACE (`test_plate_freeze`), and the levels stay put across random seeds.
- **Rejected:** a one-sided or symmetric-capped correction (it overshot into dips of several dB while Neve's even harmonics were still in the loop at infinite); keeping Neve's even harmonics at infinite (they made the tail jump 2-3 dB by themselves).

**Approved (23 Oct 2026, owner's pick on `renders/makeup`, A on all 4 cards; SPEC v1.0.17):** the held-tail makeup stays. Measured then: without it an infinite MASSIVE with tape wobble and the cassette sagged about 3 dB over 10 minutes and a HALL about 2.4 dB; frozen pads held either way.

**Amendment (24 Oct 2026, owner's OK; SPEC v1.0.24):** the governor's loss, and ERA's cassette compression, judge a falling tail fast. Each takes the lower of its 1.5 s level and a 50 ms level, so a tail that drops when the input stops is never judged as still loud. A held tail's hold level is taken only once the tank's own input is quiet (in BLOOM the swell keeps feeding the tank after the input stops). Before this, a loud input stopping made an infinite tail plunge (PLATE −27 dB, fed 12 dB hot) and creep back over minutes. Now every dip is within ~2 dB. Found by the overnight soak (`docs/research/soak-audit.md`).

**Amendment (29 Oct 2026, owner's pick B on every card of `renders/review2_bump`; SPEC v1.0.56):** **in PLATE and MASSIVE the governor eases in from DECAY 0.70**, its weight t³ over 0.70-0.85 (`voicing::kGovernorEaseFrom`, `map::floorWeight`); HALL's comes in across the 0.80-0.85 crossover as before.
- **Why:** the owner heard MASSIVE ~4 dB louder just before endless than at endless while playing, and a drop at about 3 o'clock on a slow turn. Measured (overnight review 2, `docs/research/review2-notes/bump.md`): at a fifth of its weight the governor already out-loses a 120 s tail, so ~75 % of the change sat between 0.80 and 0.81 (2.5-4.6 dB per 0.01 of DECAY in MASSIVE, 1.0-2.3 dB in PLATE). Its size depends on the playing level (endless is level-governed, finite settings are linear), so no fixed gain could fix it; the cliff could go.
- **PLATE's long-tail trim (ADR 0034) lets go on the same curve** ((1 − the governor's weight)³). With the governor alone eased, the trim's release stepped PLATE back up 0.7-0.9 dB at 0.80 → 0.81.
- **Unchanged:** the duck, the hold, the darkening and everything else DECAY's crossover fades keep the crossover's own weight; every DECAY at or below 0.70 and from 0.85 is bit-identical, as is HALL. The governor only adds loss in this range (the hold's make-up needs an endless or frozen tail), so nothing new can grow.
- **Rejected (the review's prototypes):** a fixed make-up trim before endless (it flipped the step's direction for quiet playing and stabs); the ramp inside 0.80-0.85 only (still a 1.9 dB step, and 0.74-0.80 as loud); easing the duck as well (within 0.1-0.6 dB of the governor alone, 8 B more flash).
