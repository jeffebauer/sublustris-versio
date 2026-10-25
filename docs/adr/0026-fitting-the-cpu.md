# 0026 — Fitting the module's CPU: the HALL / MASSIVE tank at 24 kHz, a 12-point swell

**Status:** Accepted, 25 Oct 2026. SPEC v1.0.35. Owner's picks:
- `renders/fit_cpu`: E (all three) on every card; "Not a huge sonic difference, especially when not at 100% wet. Ideally we'd only reduce the early reflections to 1 instead of 3 if needed".
- `renders/swell_points`: C (a true 12 points) on every card. That page was a correction: fit_cpu's "12 points" had rendered 24 points with a longer smear (a prototype bug).

Amends ADR 0007 (the swell) and ADR 0012 (the still-lows tanks).

**Context (M3 rounds 1-8 on the module, `dist/immensio_versio_profile_M3_round*.bin`):**
- At worst (MASSIVE with BLOOM) the Engine needed ~2.6× the 10,000 cycles a sample.
- The speed-ups that keep the sound to the bit took it to ~2.3×:
  - lazy line clears;
  - the swell read tap by tap over each chunk;
  - SDRAM lines staggered across the cache's sets;
  - SDRAM write-allocate;
  - block maths only when inputs change.
- Round 8 settled the rest: the same small tank costs 4,049 cycles a sample in AXI SRAM and 6,925 in SDRAM. The memory adds ~70 %, but the arithmetic alone is most of it (the M7 does about one operation a cycle).
- Measured building blocks (round 6, cycles):
  - SDRAM: 10 a float for one stream, 33 for 32 streams, ~100 for a delay line's read and write;
  - AXI: 3;
  - expf 113, sinf 90, powf 209, log10f 158.

**Decision:**
- **The HALL / MASSIVE tank (`fdn_`) and its freeze twin (`twinFdn_`) run at 24 kHz** (`dsp/HalfRate.h`):
  - halfbands either side (`dsp/Oversampler.h`: flat to ~10.5 kHz, 85 dB down from 13.4 kHz);
  - the still-lows tank stays at 12 kHz;
  - everything in the tanks is set in seconds, so lengths, T60s and levels hold (T60 within 3 %, level on music within 0.9 dB).
- **ERA's Midiverb box in a 24 kHz tank:** its ½ one-pole loop low-pass and its bits, without the halfbands (it was already 24 kHz inside). The 12 kHz low tank keeps only the bits, as before.
- **The swell has 12 taps per side** (was 24), over the same span, with the same jitter draws (24 drawn a side, the first 12 used), total energy 1.
- **Held back:** one early reflection per line (fit_cpu's D) stays a prototype (`IV_FIT_EARLY_TAPS`), "if needed".

**Considered:**
- Moving the lines into internal SRAM: MASSIVE's ~4.6 MB doesn't fit.
- Fetching each block's lines into the cache first (round 7): it cost ~30 % and saved less.
- Fewer FDN lines.
- Oversampling the in-loop saturation at 24 kHz (CPU).

**Consequences:**
- **Sound:**
  - the HALL / MASSIVE tail stops around 11-12 kHz;
  - on white noise HALL is 2.7 dB and MASSIVE 1.9 dB quieter (test offset), on music 0.2-0.9 dB;
  - BLOOM's swell is built from 12 points;
  - a hot 7 kHz tone at the cassette stage folds its 3rd harmonic (21 kHz at the 24 kHz rate) to 3 kHz: -38 dB below 5 kHz (the test's limit moved from -40 to -36).
- **Painting:** at the paint test's old white-noise level the pad froze under the floor, where painting may grow it to the floor + 1.5 dB by design. The test now uses louder noise.
- **Goldens updated.** The click's clarity (C80) and smoothness times moved most, on the onset the metric finds; a click rendered both ways starts at 11 ms with C80 within 0.8 dB.
- **CPU:** to be measured in round 9, together with the real limit (the owner's idea: extra load in the audio callback, step by step, counting late blocks).
- **Memory:** the two tanks at half the rate need ~8 MB less SDRAM.
