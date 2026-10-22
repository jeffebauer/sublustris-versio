# Recording your reverbs for comparison

Recordings of your Ultrasheer, Mercury7, H9, Swells and Valhalla plugins let every listening page carry a "Compare with" version, and let me measure how their tails decay. The device details behind this guide, with sources, are in `docs/research/reference-devices.md`.

**What to play through each one:** the files in `test_audio/reference_kit/`. If they're missing, I rebuild them with `tools/make_reference_kit.py`.

| File | Length | What it's for |
|---|---|---|
| `calibrate.wav` | 2 min | Knob-only units: five clicks 22 s apart. Turn DECAY to the next position in each gap |
| `measure.wav` | 1.5 min | A click, a noise burst and a sweep, each with 25 s to ring out: decay per band, and the reverb's response |
| `music.wav` | 5 min | All nine pieces of listening material, each with its tail |

**Where recordings go:** `test_audio/reference/incoming/<unit>_<algorithm>_<target>_<file>.wav`. For example, `mercury7_cathedra_medium_music.wav` or `supermassive_sagittarius_calibrate.wav`. Tell me when a batch is in. I align, split and measure them, then add them to the pages. They're never committed or published.

## The same conditions on every unit

- **100 % wet.** Use the kill-dry setting where the unit has one (the Mercury7 global KILL DRY, the H9 KILDRY).
- **Predelay at zero**, where it can be turned off.
- **Tone and EQ neutral.** Where a unit has no neutral, use the setting in the table and keep it.
- **Modulation at the unit's default.**
- **Every extra effect off:** vibrato, pitch, shimmer, echo, drive, lo-fi.
- **Levels:**
  - Plugins: unity gain.
  - Pedals: line level where there's a switch (the Mercury7). The H9 clips at +4 dBu, so set its input gain to 0 dB.
  - Swells: Eurorack levels are hotter, so use its INPUT and TRIM, and keep DRIVE off.
  - Recordings should peak around −6 dBFS and never clip.
- **48 kHz / 24-bit, stereo.**

## The decay targets

Record `measure.wav` at each target and `music.wav` at **medium** only. Add **infinite** where a unit has a true freeze or infinite setting.

| Target | Decay | Why |
|---|---|---|
| short | about 2 s | a small, tight space |
| medium | about 5 s | the everyday setting; the music is recorded here |
| long | about 15 s | big washes |
| infinite | held | for comparing frozen pads (H9, Swells; the Mercury7's SWELL hold comes close) |

**Plugins and the H9 show decay in seconds:** set 2.0 / 5.0 / 15.0 directly.

**The Ultrasheer, Mercury7, Supermassive and Swells (and H9 Blackhole) don't show seconds.** Calibrate those once:
1. Record `calibrate.wav`.
2. Before each of the five clicks, set DECAY to the next position: 8, 10, 12, 2 and 4 o'clock (on a slider: 20, 40, 60, 80, 95 %). There are 22 s between clicks.
3. Save it as `<unit>_<algorithm>_calibrate.wav`.
4. I measure each click's decay and tell you which positions give 2, 5 and 15 s. Then record the targets.

A photo of the knobs for each recording helps me label them.

## Each unit

### Valhalla plugins (record these first: exact, recallable)

Render offline at 48 kHz. Mix 100 % (with the lock on), predelay 0, modulation at default.

| Plugin | Mode(s) | Decay | Neutral settings |
|---|---|---|---|
| **Room** | Large Room; Dark Chamber | 2 / 5 / 15 s | Bass Mult 1.0×, High Mult 1.0×, High Cut max, Lo Cut min, DEPTH 100 % (late only) |
| **VintageVerb** | Concert Hall, Smooth Plate, Cathedral; COLOR **NOW** | 2 / 5 / 15 s | Bass Mult 1.0×, damping open, High Cut max, Low Cut min. Also try **Nonlin** with high Attack (its reverse) |
| **FutureVerb** | Hall, Cathedral, Space; COLOR **Neutral** | 2 / 5 / 15 s | Echo Level 0, Delay 0, Feedback 0, Echo Mode Modern, High Cut max, Low Cut min |
| **Plate** | its default mode | 2 / 5 / 15 s | EQ gains 0 dB, Width 100 % |
| **Supermassive** | Sagittarius, Andromeda, Large Magellanic Cloud | FEEDBACK %: calibrate (guesses ~45 / 70 / 88 %) | WARP 100 %, DENSITY 100 %, High Cut max, Low Cut min, Width 100 %, MOD at the Default preset's values. Infinite: FEEDBACK 100 % |

Supermassive has no predelay control, so its onset will differ; Andromeda has built-in predelay. That's expected.

### Eventide H9 (use H9 Control; save a preset per target)

**Global settings:**
- **Tempo off:** decay then shows in seconds, not note values.
- **KILDRY** = YES, and MIX 100.
- **Routing:** NORMAL.
- **Input gain** 0 dB, **output** 0 dB.
- Make sure no HotSwitch is engaged.

**Algorithms:**

| Algorithm | Decay | Neutral settings |
|---|---|---|
| **Hall** | 2 / 5 / 15 s | PREDLY 0; LO, MID and HI levels 0; LO and HI decay at ×1 |
| **Plate** | 2 / 5 / 15 s | PREDLY 0, TONE centre |
| **Room** | 2 / 5 / 15 s | PREDLY 0, levels 0 (if 15 s is out of range, record its maximum) |
| **Blackhole** | GRAVITY: calibrate | FEEDBK 0, levels 0. Infinite: FEEDBK at its maximum |
| **Reverse** | the build-up is in ms: 500 ms and 2000 ms (our TIME range) | SIZE 0, FEEDBK 0, LATE 0. **Our yardstick for REVERSE** |

### Meris Mercury7 (MIDI CCs give exact values; otherwise presets)

- **Setup:**
  - Global settings: **LINE** level, **KILL DRY = DRY MUTED**.
  - MIX max; predelay (ALT + SPACE DECAY) at minimum.
- **Neutral settings:**
  - PITCH VECTOR min (off), VIBRATO DEPTH (ALT + HI FREQ) min, SWELL off, DENSITY max.
  - LO FREQ and HI FREQ max ("no attenuation"; note that this is its brightest).
  - MODULATE about 9 o'clock and MOD SPEED about 10 o'clock: preset 1's values.
- **Algorithms:** both **Cathedra** and **Ultraplate**.
- **Decay:** calibrate. Guesses, Ultraplate: 9, 11:30 and 2:30 o'clock. Cathedra runs longer per position.
- **Infinite:** hold SWELL.

### Vongon Ultrasheer

- MIX full; vibrato switched off; BRIGHTNESS at noon (it acts in the feedback, so keep it there).
- **Decay:** calibrate. Guesses: 9:30, 12 and 2:30 o'clock.
- No infinite. Check that MIX fully up has no dry leaking through on a click.

### Intellijel Swells

- **Setup:**
  - All ten attenuverters centred, so its SWELL generator modulates nothing.
  - LO-FI off, DRIVE off. FREEZE, REVERSE and BURST off. Nothing patched to TRIG.
  - PREDLY minimum, EQ tilt centre, damping at the quick-start guide's neutral positions.
  - MIX fully wet; use its send/return mode if you know where it is.
- **Models:** **Fog** (lush) and **Shadow** (dark hall).
- **Decay:** calibrate. Guesses: 30–35 %, 50–55 % and 75–80 % of the slider.
- **Infinite:** FREEZE. **Reverse:** the REVERSE button. Record it too: a second yardstick for REVERSE.

## Order (most useful first)

1. H9 **Reverse**: the REVERSE yardstick.
2. **Supermassive** Sagittarius, Andromeda and LMC: for MASSIVE.
3. **Mercury7** Cathedra: for HALL and DRIFT.
4. Valhalla **Plate** and **VintageVerb**: for PLATE and ERA.
5. The rest as you have time: Room, FutureVerb, H9 Hall and Blackhole, Ultrasheer, Swells.

**Before recording pedals or Swells:** a loopback of `measure.wav` (interface out straight to in) tells me your interface's own latency, so I can line the files up.
