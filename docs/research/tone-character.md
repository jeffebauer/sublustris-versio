# TONE with character: a soft, sweet resonance

27 Oct 2026, session 9, for the owner's request ("add more character to the tone control - maybe some soft, sweet resonance"). Read-only research; nothing built.

**Today (v1.0.43):** TONE scales the high-end T60 share in every loop line (`toneHighT60Ratio`, `absorbent`) and drives a one-pole tilt on the wet (`dsp/Output.h` `Tilt`, 600 Hz; SPEC's P3 row says ~800 Hz, stale). Wet order: WEAR → tilt → duck → limiter.

**Placement (the main finding):** resonance in the loop compounds per pass; with loop gain ≤ 1 the peak outlives the rest of the tail and whistles at infinite DECAY or freeze (the metallic failure SPEC's check guards). Valhalla's Supermassive keeps its EQ out of the loop (newer modes put only plain 6 dB/oct cuts in). On the input, diffusion smears it and a frozen pad ignores it. **On the wet, with the loop keeping its tilt** (split): nothing compounds, holds and freeze are unaffected, CV sweeps act at once, TONE still sweeps a frozen pad. Strymon NightSky does this split (Regen darkens in the tank; a resonant 4-pole low-pass on the output); Eventide Blackhole has Q-able shelves on the tail.

**Keeping it soft:** Q ≤ ~1.4-2 (+3-6 dB bump, never self-oscillating), 12 dB slope, level make-up, optionally a cheap cubic clip on the band state. Click-free under CV: trapezoidal (TPT/ZDF) SVF or ladder, never a direct-form biquad; coefficients per block (the existing `EraSvf::gain`, sin/cos already linked; `bilinearK` is only accurate below ~3 kHz), g ramped per sample. Harsh families to avoid: MS-20 / Korg 35, diode ladder, Steiner-Parker.

| Candidate (on the wet, replacing today's wet tilt) | Sound | Est. CPU | Est. flash |
|---|---|---|---|
| **A. Resonant tilt** (Pultec / 1073 shelf idea): one SVF per side, low/band/high mix, corner moves with the knob, Q 0.707 → ~1.3 | today's TONE plus a sweet lift at the corner; CCW a warm low-mid swell under the darkening, CW a sheen | ~0.3-0.4 points | ~0.3-0.5 KB |
| **B. SEM sweep** (Oberheim 12 dB): CCW low-pass ~16 kHz → 350 Hz, CW high-pass ~20 → 300 Hz + air shelf, Q to ~1.4 at the ends | the dub-desk filter throw: CCW sinks underwater with a soft vowel bump, CW thins to airy wisps | ~0.4-0.6 | ~0.5 KB |
| **C. Ladder CCW + resonant air shelf CW** (ZDF 4-pole, one soft clip, bass-compensated) | the creamiest, most synth-like; resonance blooms and saturates gently | ~1 | ~1-1.5 KB |

**Interactions:** WEAR's band-pass top (~3 kHz) can stack with a low-pass bump (honk: lower Q while the band is in); its wandering dip crossing a bump flickers like a phaser. SHIMMER + a CW bump may turn glassy.

**Checks a prototype must pass:** the metallic check at TONE's ends and quarters with SHIMMER full and WEAR on; 10-minute infinite and 5-minute freeze holds (bit-identical in the loop); 1 ms click check on TONE CV sweeps 0.1-20 Hz; level within ±1 dB on music; firmware sizes and a profile run.

Sources: Stilson & Smith (ccrma.stanford.edu/~stilti/papers/moogvcf.pdf), Huovilainen DAFx-04, Zavalishin *The Art of VA Filter Design* 2.1, Simper's SVF (cytomic.com), RBJ audio EQ cookbook, Strymon NightSky manual, Eventide Blackhole guide, Valhalla Supermassive controls.
