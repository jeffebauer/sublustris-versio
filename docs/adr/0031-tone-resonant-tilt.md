# 0031 — TONE gets a resonant tilt on the wet

**Status:** Accepted, 27 Oct 2026 (owner: "A resonant tilt is the clear winner — adds a little character without creating too much resonance or rounding that SEM and ladder filters add" on `renders/tone_character`, MIX 50 %). SPEC v1.0.44. Amends ADR 0012 (TONE). `docs/research/tone-character.md` (main checkout).

**Context:**
- **The owner asked** to "add more character to the tone control - maybe some soft, sweet resonance ... a balance between performance and quality so we don't reintroduce cpu overloads or run out of flash".
- **TONE before** (v1.0.8, v1.0.15): in every loop line, the highs' T60 share, a shelf crossing at 2.5 kHz. On the wet, a one-pole tilt round 600 Hz (SPEC's P3 row said ~800 Hz, stale): CCW +4 dB lows / −9 dB highs, CW −4 / +6 dB, flat at noon.
- **Where resonance can go** (the research's main finding):
  - **In the loop** it compounds every pass. With loop gain ≤ 1 the peak outlives the rest of the tail and whistles at infinite DECAY or freeze: the metallic failure SPEC's checks guard. Valhalla's Supermassive keeps its EQ out of the loop.
  - **On the input**, the diffusion smears it, and a frozen pad ignores it.
  - **On the wet**, with the loop keeping its share, nothing compounds and holds and freeze are unaffected. CV sweeps act at once, and TONE still sweeps a frozen pad. Strymon NightSky splits it this way (Regen darkens in the tank, a resonant filter on the output), and Eventide Blackhole has Q-able shelves on the tail.
- **Keeping it soft:** Q at most ~1.4-2 (a +3-6 dB bump, never self-oscillating), level make-up, and a trapezoidal (TPT) SVF, never a direct-form biquad, so CV sweeps don't click.

**Decision:**
- **A soft bell after the wet's tilt** (`dsp/Output.h` `ToneBell`), one trapezoidal SVF a side riding the tilt's bend (the Pultec / Neve 1073 idea: out = x + (A − 1) k · band):
  - **turned left**, its corner moves 3 kHz → 600 Hz, a warm swell under the darkening;
  - **turned right**, 1 → 5 kHz, a sheen on top;
  - **Q** 0.707 at noon → 1.3 at the ends, and the bell's lift 0 → +4.5 dB;
  - **a make-up** keeps music's loudness (LUFS) where the tilt alone had it.
- **Noon is flat:** the bell passes the tilt's output bit for bit.
- **The loop's TONE share is untouched,** so holds and freeze don't change.
- **Coefficients** are worked out once per control chunk (`EraSvf::gain`: sin and cos, already linked, no tan) and ramped per sample. Across noon the old side's bell fades out before the new side's starts.
- **Removed:** B, C, the `IV_TONE_CHAR` hook and its trial switch (the renderer says it ignores `IV_TONE_CHAR`).

**Rejected:**
- **B, an SEM sweep** (an Oberheim-style 12 dB filter: low-pass ~16 kHz → 350 Hz turned left, high-pass ~20 → 300 Hz plus an air shelf turned right, Q to ~1.4). It is the dub-desk filter throw, but it rounds the sound off.
- **C, a ladder turned left and a resonant air shelf turned right** (ZDF 4-pole, one soft clip). The creamiest and most synth-like, but it cost about three times A's CPU and flash.
- **Resonance in the loop or on the input:** see Context.
- **Harsh filter families** (MS-20 / Korg 35, diode ladder, Steiner-Parker): never prototyped.

**Consequences:**
- **Sound:** only renders with TONE off noon change. Against v1.0.42 all 55 `tools/same_render.py` renders (all at noon) are the same, and the 48 TONE cases (ends, quarters, near noon, CV at 0.1 / 2 / 20 Hz, a frozen sweep, SHIMMER, WEAR) all differ. Of the goldens, only `plate_bright` and `hall_dark` (TONE 1 and 0) moved.
- **Watch:** WEAR's band-pass top (~3 kHz) can stack with the bell, and its wandering dip crossing a bump can flicker like a phaser. SHIMMER plus a bright bell may turn glassy. All three were on the page.
- **Flash:** release +1,064 B (105,356 → 106,420 B on its branch; 105,532 → 106,588 B merged onto v1.0.45).
- **CPU:** ~0.3-0.4 points (estimate).
- **Tests:** `test_tone`: the bell's shape, level within 1 dB on music, no 1 ms spikes on TONE CV, and the tanks bit-identical with and without it (`Engine::setToneBellForTests`, desktop only).
