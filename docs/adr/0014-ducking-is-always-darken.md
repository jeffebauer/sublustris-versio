# 0014 — Ducking is always DARKEN, growing with DECAY

**Status:** Accepted, 21 Oct 2026 (owner, after listening). Spec-session decisions 16, 24, 27. Supersedes the draft's DUCK switch (Darken / Push back / Level, after the brief's Warble was swapped for Level).

**Context:** The brief borrows the concept of ducking past a feedback threshold from Desmodus (not its "scream"): past ~3 o'clock DECAY goes infinite and the tail should get out of the way while you play. The draft gave the ducking its own switch. The owner wasn't sure that was the best use of a switch and asked to hear each style. In the demos (`renders/p7_sw1_demos/`, sketch in `docs/prototypes/p7-sw1-demos/`) the owner picked **DARKEN on both materials** (dub skank and vibes).

**Decision:**
- Ducking has **no switch: it is always DARKEN.** While you play, a loop low-pass closes (down to ~1.5 kHz at full depth) and the tail dips ~3 dB; when you stop, it brightens back.
- **Depth grows with DECAY:** 0 below DECAY 0.85, rising to full at DECAY 1.
- Envelope follower on the input: attack 5 ms, release 400 ms, −42 dBFS → no duck, −24 dBFS → full (starting guesses, from the demos).
- SW1 is freed for ATTACK (ADR 0015).

**Rejected (heard in the demos):** PUSH BACK (more smear, narrower image), LEVEL (plain dip), and a switch to choose between them; a duck depth fixed once infinite.

**Amendment (21 Oct 2026, M7 build; SPEC v1.0.4):** DARKEN's low-pass sits on the wet output, not inside the loop. A filter in the loop removes the highs from the tail for good, so the tail could never "brighten back when you stop"; on the output it does (measured: highs above 4 kHz 4 % of the wet while playing at infinite, 31 % a second after). There's no ducking while frozen: playing over a pad mustn't darken the pad.

**Amendment (23 Oct 2026, owner's M1 notes and pick on `renders/ducking`; SPEC v1.0.17):** ducking is now **4:1 compression of the wet**, not DARKEN. The owner heard DARKEN as "too extreme", holding "too long", with the held tail jumping up when the input stopped, and asked for something "closer to 4:1 compression rather than extreme ducking, so it feels cohesive" (the usual studio practice: a compressor on the reverb return keyed by the dry). Five versions were compared at infinite DECAY on six materials in all three SPACEs; D (attack 30 ms, release 0.4 s, 4:1 above −24 dBFS of input, no darkening) won all 18 cards. Still no switch, still only from Decay 0.85 up and not while frozen. The title stands for the history; the decision is the amendment.

