# 0015 — P7 is TIME, SW1 is ATTACK (NORMAL / BLOOM / REVERSE)

**Status:** Accepted, 21 Oct 2026 (owner, after listening). Spec-session decisions 27–30 (amends 6 and 9, see ADR 0007; supersedes 19, DESK).

**Context:** The draft put DESK on P7 (a resonant peaking EQ in the loop, 150 Hz–4 kHz, with console drive rising with it). The owner wanted to hear it before committing, and suggested a Desmodus-style modulation speed knob instead. In the demos (`renders/p7_sw1_demos/`) the owner picked Desk **off** on the skank and the Desk **sweep** on vibes, the **slowest** Speed (0.1 Hz) on both, the low cut **off** on both, and **bloom** on vibes, with this note: "I like the sound of the bloom, but it's something I'd want to control the timing of. When the swell is too long, it can interfere with harmonies between the incoming signal and delayed bloom." And: "Another idea to consider for the second switch is toggling between bloom and reverse."

**Decision:**
- **P7 = TIME.** Its meaning follows ATTACK:
  - NORMAL: predelay 0 → 250 ms (∝ x²), so the knob always does something.
  - BLOOM: swell time 0.1 → 4 s, exponential.
  - REVERSE: segment length 0.2 → 2 s, exponential.
- **SW1 = ATTACK: NORMAL / BLOOM / REVERSE, governing the attack in every space**, Massive included (ADR 0007: NORMAL keeps Massive's gentle built-in mass; BLOOM uses TIME × Size's mass).
- **BLOOM:** the swell stage is set so the tail's envelope rises over TIME. A short TIME keeps a swell from clashing with the harmony coming in.
- **REVERSE, the classic reverse swell:** the tank output is written into TIME-long segments; each finished segment plays backwards, so each stretch of reverb rises to a cut, then the next. Edges crossfade over ~5 ms. The reversed wet is **TIME late** (inherent to real-time reverse reverb); the dry is not delayed. Two stereo segment buffers in SDRAM (~0.75 MB). **Bypassed while frozen** (a still pad reversed is the same pad), with a 50 ms crossfade; the companion is never reversed.
- ATTACK changes crossfade ~50 ms on the wet.
- The dub-desk inspiration lives on through TONE, the always-on saturation and ERA.

**Rejected:**
- **DESK** on P7: the resonant peak you sweep by hand is lost; the owner preferred timing control.
- **SPEED** on P7: the slowest rate was picked both times, and DRIFT's right side already covers slow drift.
- **Low cut on the send** for SW1: off picked both times.
- **Width** (mono / stereo / wide) for SW1.
- A two-meaning switch (BLOOM / REVERSE only).
- A reversed wash (short reversed input chunks fed into the tank) instead of the classic reverse swell.

**Amendment (21 Oct 2026, owner's M4 listening notes, pending the owner's pick):** "Reverse is sounding really choppy and disjointed rather than an actual reversed reverb." Measured on the skank: the largest envelope step is 9.5 dB (the cuts). Prototype B plays two backwards readers half a segment apart, each faded in and out over its whole length (sin², summing to one): no cuts, largest step 4.5 dB, still TIME late. Page `renders/reverse_options` (A today, B smooth). If B wins, the per-sample sine becomes a table or recursive oscillator (CPU), the M4 "segments rise to their cut" test becomes "no cuts", and setSmooth / `IV_REVERSE_SMOOTH` go.
