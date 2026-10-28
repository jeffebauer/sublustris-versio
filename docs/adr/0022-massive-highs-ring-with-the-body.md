# 0022 — MASSIVE's highs ring as long as its body

**Status:** Accepted, 25 Oct 2026 (owner's picks on `renders/massive_highs`: D "as long as the body" on every card, 5 s and 15 s, the repeated cards agreeing). SPEC v1.0.31.

**Context:** The Valhalla comparison (`docs/research/valhalla-comparison.md`, suggestion 3) found MASSIVE's top end dying well before its body, against Supermassive's, which rings as long as its body. Re-measured on v1.0.30 (impulse T60, 8 kHz over 1 kHz): MASSIVE 0.43 at both 5 s and 15 s; Sagittarius and Andromeda 0.99–1.06. In a 15 s tail our 8 kHz was gone after ~6–8 s.
- **The cause:** MASSIVE took the shared damping share (`kDampingHighT60Ratio` 0.45 at TONE noon), and since v1.0.30 the Neve under every era shortens it a further ×0.7. HALL and PLATE got "air" in v1.0.25 (a steady dB/s); MASSIVE kept the share.
- **How Supermassive does it** (valhalladsp.com, the Supermassive page): its High EQ is a −6 dB/oct low-pass on the *output* of the networks; only the newer "realistic" modes (Scorpio, Libra, Leo, Virgo, Pleiades, Sirius) filter inside the loop. Andromeda and Sagittarius don't damp in the loop at all.

**Decision:**
- **MASSIVE has its own high-end share,** `kMassiveHighT60Ratio` 1.43: under the Neve's ×0.7 it's 1, so at TONE noon and ERA's Neve the highs ring as long as the body (`map::massiveHighRatio`).
- **TONE and ERA scale it as everywhere:** TONE CCW still darkens it (0.18 of the body at fully CCW), the cassette's ×0.4 still darkens it; TONE CW lets the highs outlast the body more than in HALL (3.3×).
- **Infinite is unchanged:** across DECAY's crossover the share glides back to the shared one (by `governorWeight`), so an infinite MASSIVE still darkens towards its floor (ADR 0008), and a frozen pad is as before.
- **The twin follows MASSIVE** (its own DECAY), so what you play over a frozen MASSIVE rings with the same top. REVERSE keeps the shared share (it's its own short reverb).

**Considered:** B (highs ~0.6 of the body) and C (~0.75): not picked. Air as in HALL (a steady dB/s): at 15 s it barely differs from today.

**Consequences:**
- **Measured** (impulse, SIZE 0.7): 8 kHz / 1 kHz T60 0.43 → 1.0 at 5 and 15 s; 125 Hz–1 kHz unchanged. Wet balance at 8 kHz on music −4.9 → −1.2 dB re 1 kHz at 15 s (Supermassive −0.2).
- **Energy:** level within +0.4 dB across SIZE 0 / 0.6 / 1 × DECAY 0.5 / 0.75 / 0.95 × DRIFT 0 / 0.25 / 1; no build-up; infinite renders identical. A finite tail's last seconds are up to ~3 dB louder (the highs are still there).
- **Found on the way:** a short MASSIVE (5 s) at large SIZE holds about one pass round its ~1.2 s lines after the input stops, then drops (~14 dB lost per pass); the owner asked for it to be smoothed. Separate from this decision (see the handoff / TASKS).
