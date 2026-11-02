# 0030 — MOTION: a tape transport on the left, a stronger Wander on the right

**Status:** Accepted, 27 Oct 2026. Owner's picks, all on `renders/drift_rethink` unless named:
- **Right page** (level-matched, MIX 100 %): "B stronger = winner".
- **Left page, round 2:** D (the transport with crinkle) on the MASSIVE dub cards, with "Tape transport could introduce a bit more variance at the extreme end — Kinotone Ribbons pedal ... we should use this pedal as a reference". Of the crinkle: "The crinkle seems more like light-weight dropouts rather than a true crinkle — or at least what I'm used to ... on the Kinotone Ribbons pedal and Strymon Magneto".
- **Round 3:** E, the Magneto crinkle, on every card, the repeated cards agreeing: "I like the magneto crinkle, but compared to C there are less dropouts. I think C's dropouts are a little extreme, so let's find a middle ground".
- **`left_tape_final`:** B, a third of C's crinkle dips, on every card, the repeated cards agreeing.
- **`renders/motion_entry`** (MIX 50 %): A, as built, on every card.

SPEC v1.0.46. Amends ADR 0012 (DRIFT's two sides; the knob is MOTION on the panel, ADR 0029) and the right side of v1.0.20. `docs/research/drift-rethink.md`.

**Context:**
- **The ends got lost in a mix.** The owner: "In a realistic setting amongst a broader mix, the extreme ends of the modulation knob don't create obvious results ... we should make them stronger to at least be more noticeable in a mix". Of the left side: "the wow and flutter side also needs to be more obvious as the deviations get a bit lost in the diffuse reverb".
- **Measured: per-line movement averages out.** At full left the tank's per-line wobble measures ~20 cents on a held tone, but the lines move on their own. Two tones played together through HALL bend together with a correlation of only 0.0-0.38, and a shared ±10-cent transport laid on top barely showed. Even sines in step on every line (right side, round 2) came out of the loop with their phase smeared (correlation −0.01 to +0.26). Only a movement after the tank bends the tail as one (0.82-0.93).
- **ERA's cassette wow and flutter were gone** with WEAR (ADR 0027), so tape movement now belongs to MOTION alone, as Strymon files wow and crinkle under "Mechanics".
- **The references:** ChowTape's wow and flutter (numbers only, GPL), Kinotone Ribbons (variance, contact-loss crinkle) and Strymon Magneto (friction and crease crinkle). Recordings of the owner's units are for analysis only and never committed (ADR 0006).

**Decision:**
- **Left side: a tape transport on the wet** (`dsp/Transport.h`), after the tank and before WEAR's chain. It is one short delay a channel, both read at the same moving point, so the whole tail bends together as one tape speed would. From the dead zone to full left:
  - **wow:** mostly random (two smoothed one-poles) over a sine whose rate drifts, 0.5 → 1.2 Hz, up to ±12 cents;
  - **flutter:** two sines at 6 → 10 Hz, up to ±3 cents, moving the level ±0.3 dB too;
  - **variance** (round 3's C), over the side's last half, eased in. The wow's speed is re-picked every 0.5-2 s, from 40 % slower to twice as quick, with a slow drift under it and now and then a swing half as big again. Wow grows to ~±20 cents and flutter to ±4.5;
  - **crinkle, as on Magneto** (E), over the last 40 % of the side, rising to 0.6 bursts a second: a bed of scrape roughness (fast random pitch around 100-200 Hz), and bursts of 0.1-0.6 s of jittery pitch (~45 Hz, up to ±12 cents);
  - **the dips** (B): a counted share of the bursts (0.43, `kTransportDipShare`) also bring round 2's dip. That is about a third of C's dips, 15 a minute at full left. Each dip roughens the level by 1-3.5 dB (median −1.6), takes the highs toward ~5 kHz, lasts 50-300 ms, and runs on random numbers of its own, so E's movement stays exactly as picked.
- **The tank's own per-line wow and flutter keep a third** on the left side (`kTransportLineShare` 0.33): at full, their incoherent ~20 cents buried the transport.
- **How it comes in** (`motion_entry` A). Off the left side the transport rests and the wet is bit for bit as before (the dead zone and the right side never touch it). Turned onto the left side, it crossfades in over ~5 ms, then its centre glides from 8 to 1000 samples over 5 s: a tape coming up to speed, at most ~11 cents slow. On the left side the wet arrives 1000 samples (20.8 ms) later than at noon.
- **Right side: Wander, stronger** (B). Over the side's last half its depth grows to ×1.4 and its rate to ×1.2 at the end (× (1 + 0.4 x²) and × (1 + 0.2 x²), x from 0 at the side's middle to 1 at its end; `kWanderGrow`, `kWanderRateGrow`). The first half is as before.
- **The dead zone** (0.47-0.53, the modulation floor only) and the still lows (ADR 0012, v1.0.18) are unchanged.

**Rejected:**
- **Sines on the right side**, each line on its own phase (D fast, E slow), all lines in step (F, G), or one sine on the output (H). The loop smears the in-step versions' phase, and the output sine is a vibrato, not a reverb's movement.
- **C's stronger ends** (×1.8 depth, ×1.4 rate) in place of B's.
- **Dropouts** (round 2's mode 3: 20-150 ms falls of 6-18 dB): round 2's pick was the transport with crinkle, without them.
- **Round 2's crinkle:** "more like light-weight dropouts rather than a true crinkle".
- **Ribbons crinkle** (round 3's D): brief contact losses of 5-40 ms, 6-20 dB, with no pitch. E was picked on every card.
- **C's full share of dips, or half of it** (`left_tape_final` A and C). A, E without the dips, read as fewer "dropouts" than the owner wanted.
- **The transport always on** at its centre (`motion_entry` B): the wet would be 21 ms later everywhere, for no gain at noon or on the right side.

**Consequences:**
- **Sound:** only the MOTION ends change (`tools/same_render.py` against v1.0.45: six of 55 renders). Noon and the right side's first half are as before.
- **Latency:** 20.8 ms more wet delay on the left side.
- **Flash:** release +3,992 B (106,588 → 110,580 B), no new libm.
- **RAM:** +16 KB DTCM (two 2048-float buffers).
- **CPU** [I]: about 2 % of the M7 on the left side; off it, one buffer write a sample. Not yet timed on the module.
- **Tests:** `test_motion`: the dead zone exact, the page's p95 cents and crinkle and dip counts, the right side, latency, the glide out, clicks on sweeps, flicks and dips, energy, 10-minute holds, level. The goldens are all at MOTION noon.
- **Removed:** the sines (`DriftAmounts::sine*`), the in-step and output-sine versions, Ribbons and round 2's crinkle, the dropouts, and every `IV_DRIFT_*`, `IV_TAPE_TRANSPORT` and `IV_TRANSPORT_ALWAYS` hook (the renderer says it ignores them).

**Amendment (29 Oct 2026, owner's pick C, "from 7:30", on every card of `renders/motion_dips`; SPEC v1.0.59; `docs/research/motion-dips.md`):** the owner at v1.0.57: "it'd be good if the dropouts start entering closer to fully CCW instead of ~9 o'clock." **The dips come in from about 7:30, not with the crinkle.** Their share of the crinkle bursts is 0 above MOTION 0.026 (`voicing::kTransportDipsFrom`, about 7:30 on the module with the pots' end stops) and rises in a straight line to `kTransportDipShare` (0.43) at fully left, so fully left keeps `left_tape_final`'s B to the bit (15 a minute). The wow, flutter, variance, crinkle and scrape are unchanged everywhere (the dips draw their own random numbers). Rejected: as built (from ~9 o'clock, A), from 8 o'clock (B), and only the last ~10 degrees (D, on the module mostly the end stop). Consequences: from 7:30 up the left side has no dips (renders equal a no-dip control to the bit); `test_motion` pins the share; release +64 B.

**Amendment (1 Nov 2026, owner's pick F on every card of `renders/dip_motion`; SPEC v1.0.60; ADR 0036):** the right side is no longer Wander. It is the AM radio's band, moved from WEAR, across the whole side: "The wander is still pretty subtle, and I like the idea of having independent control over that phasing-like effect." Right of noon the lines move no more than at noon; the left side, the dead zone and the floor are unchanged. Wander's constants, its half of `map::driftAmounts` and `DriftAmounts::rateSpread` are removed.

**Amendment (3 Nov 2026, owner's pick D on every card of `renders/more_tape`; SPEC v1.0.65; ADR 0040):** a more worn cassette. Wow and flutter ×1.25; the crinkle from about 9:35 (was about 8:50) at up to 0.9 bursts a second (was 0.6), ±18 cents a burst (was ±12); the dips from about 9:30 (were from 7:30, v1.0.59), on half the bursts at full left (were 43 %), up to about 8 dB deep (was 3.5) and dulled toward ~3.5 kHz (was ~5 kHz).
