# 0023 — MASSIVE's shortest tail lasts six trips round its lines

**Status:** Accepted, 25 Oct 2026 (owner, on `renders/massive_drop`: "Overall E (at least 6 trips) sounds best — it solves the abrupt level drop, and extends the tail of the reverb which helps further set it apart from the other reverb types, and thematically works making it truly MASSIVE"). SPEC v1.0.32.

**Context:** On `renders/massive_highs` the owner heard "an audible and visible drop in amplitude after the input stops" in a 5 s MASSIVE, most on the skank. Measured (SIZE 0.7, mean line ~1.2 s): a 5 s tail loses ~14 dB a trip round the lines. The impulse holds near full level for the first trip (~1.6 s; the early taps rise along each line), then falls 10 dB in 0.4 s as the second trip takes over; after the music stops the tail fades at up to 24 dB/s, twice a 5 s tail's 12. At 15 s (~5 dB a trip) it is smooth. Supermassive steps harder (Sagittarius bursts of 24 dB/s, Andromeda ~9 dB in 0.1 s). Researched: published designs keep each trip short against T60 (Costello: mostly short lines, lots of feedback), tie the shortest decay to size, or spread the loss along the loop (Dattorro's decay multipliers, Jot); nothing makes 14–20 dB a trip sound smooth.

**Decision:** `kMassiveMinPasses` 3 → 6: MASSIVE's shortest T60 is at least six mean lines, so a trip loses at most ~10 dB. The knob's long end is unchanged (120 s, then infinite).

**Considered (same page):** B, the early taps carrying their share of each trip's loss (smoother, 2–4 dB quieter, still 5 s); C, a short MASSIVE's lines shortened to T60 / 8 (smoothest at 5 s, but turning DECAY bends pitch); D, B and C. The owner preferred E's longer, smoother tail as MASSIVE's identity.

**Consequences:**
- **The shortest MASSIVE grows with SIZE:** ~2.1 s at SIZE 0, ~5 s at noon, ~7 s at 0.7, 12 s at full (was 2 / 2.5 / 3.6 / 6 s). Shorter tails are the HALL's.
- **Smoother:** at SIZE 0.7's shortest, the steepest quarter-second fall on the skank 9.3 → 4.8 dB, the click's fade ±1.3 → ±0.5 dB about a straight line.
- **Level:** kept by `massiveDecayLevelTrim` (the extra length's loudness taken off), so DECAY's first notch is as loud as before.
- **No code:** a constant; flash unchanged.
