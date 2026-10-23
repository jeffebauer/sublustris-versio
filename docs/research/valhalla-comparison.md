# Valhalla comparison: the numbers beside `renders/valhalla_compare`

*24 Oct 2026. Measurement only: no Core change. It compares our HALL, PLATE and MASSIVE with the owner's recordings of Valhalla Room, Valhalla Plate and Valhalla Supermassive, at the page's matched decays (about 2, 5 and 15 s). The recordings stay private (ADR 0006). To re-run it, after a Core change or on new recordings (H9, Mercury7): `.venv/bin/python tools/compare_reference.py` (read only; see its docstring).*

**The short answer.**
- **The decays match.** In the mids, every pair is within 10 % at every target, so the page's DECAY settings are right.
- **The big difference is the treble.** Our short tails lose their highs much faster than Valhalla's. Over the first 2 s, a 2 s HALL tail gets 22 dB darker; Room's gets 1 to 4 dB darker. Theirs behaves like air: the highs fade at a steady few dB per second whatever the decay. Ours takes the same share off the highs at every decay. Supermassive doesn't darken at all.
- **Our HALL starts late.** Its first sound comes 80 ms after the note and it peaks at 130 ms. Room's comes after 9 to 32 ms and peaks at 35 to 57 ms.
- **Our PLATE builds more slowly.** It starts at the same moment as Valhalla's, but it peaks 2.5 to 4 times later and sounds less "up front" (C80 is 2.5 to 4 dB lower).
- **Our reverb gets quieter as DECAY goes down.** It drops up to 10 dB. Room and Supermassive keep the same loudness at any decay. (The page level-matches, so you hear this on the module's MIX knob, not on the page.)
- **What we do that they don't:** a smoother start (fewer separate early echoes than Room or Supermassive), a gradual swell in MASSIVE where Supermassive jumps in after a gap, and a slightly longer bass in the 15 s HALL.

---

## Method

Both sides were measured with the same code (`tools/compare_reference.py`).

- **Same input, same timeline.** We played the same kit files the Valhalla plugins played (`test_audio/reference_kit/music.wav` and `measure.wav`) through `iv_render`, at the page's settings: ERA 0, TIME 0.2 (10 ms predelay), TONE and DRIFT at noon, MIX full, ATTACK NORMAL.
- **Raw takes.** Both sides were read on the kit's own timeline, from the raw takes in `incoming/`, not from the ingested pieces. Those pieces are cut from the sync click's *response*, which hides any predelay. The raw takes are aligned: in every take, Room's and Plate's first sound comes 9 to 32 ms after the kit's click.
- **Same material.** The kit's pieces are bit-identical to `renders/_material`, so the page's A renders and the Valhalla takes heard the same audio.

What was measured:

| Measure | From | What it tells you |
|---|---|---|
| Decay per octave band (T60, s) | **Music tails.** After each piece's last note, each band's level is fitted from 5 to 25 dB below the tail's peak, ×3 (the same span as `ingest_reference.py`). The figure is the median of the nine pieces. | How long each frequency rings |
| Extra treble loss at 8 kHz (dB/s) | 60/T60(8 kHz) − 60/T60(1 kHz), from the row above | How fast the highs fade *beyond* the mids. A real room's air gives a steady few dB/s at any decay |
| Tail darkens 0.25 → 2 s (dB) | The level at 4 kHz minus the level at 250 Hz, over the music tails | How much darker the tail gets in its first 2 s |
| First sound, peak, rise, C80, grain, IACC | **The sync click** at the start of `music.wav`. It has 2.95 s before the first piece, the only click Room and Plate takes have. C80, IACC and grain come from `iv_render --analyze` (`host/common/Metrics`) | When the reverb starts, when it is loudest, how "up front" it is, how grainy its start is (`echo_density_grain_s`), how wide it is |
| Impulse T60 per band | Supermassive's `ir.wav`, and ours deconvolved from our measure render by `ingest_reference.py`'s own code, both through `--analyze` | Clean decay figures for MASSIVE vs Supermassive, whose delay repeats make music tails unreliable at 2 s |
| Wet balance (dB re 1 kHz) | Each piece's wet spectrum against its dry, per octave | The reverb's own tone colour, including the low end |
| Wet level re dry (dB) | The wet's energy against the dry's, per piece | How loud the reverb is for the same input |
| Tail L/R correlation, IACC | 0.2 to 3.2 s into each music tail | Width on real material |
| Pitch wobble (cents) | The vibes and pad tails, at the dry's last note's strongest partial. This is `tools/pitch_track.py`'s method with a filter a quarter-tone wide. Ours was also rendered with the tanks' modulation off (`IV_MOD_SCALE=0`) as a control | Modulation (see the caveat below) |

**Checks on the method.**
- **Music tails against clicks.** On our own renders, the music-tail decays agree with our click and impulse decays within about 6 % (they read slightly long). Example, HALL at 5 s, 125 Hz → 8 kHz:
  - music tails: 5.7 / 5.8 / 5.5 / 4.9 / 4.1 / 3.3 / 2.6 s;
  - click: 5.4 / 5.4 / 5.0 / 4.8 / 3.8 / 3.0 / 2.6 s.

  So the Room and Plate figures, which only exist from music, compare fairly.
- **Two exceptions:**
  - **MASSIVE and Supermassive at 2 s:** their delay repeats and swells break the music-tail fit (ours read 6.2 s where the impulse says 2.5 s), so the impulse is used there.
  - **Our PLATE at 15 s, impulse method:** the sweep reads only 4.7 s, because a strongly modulated tail defeats sweep deconvolution. The click reads the right 14 to 16 s, and is used.
- **Pitch wobble is inconclusive.** Even with our modulation switched off, the method reads 2 to 28 cents. The beating between a tail's many modes reads as wobble, and the result depends on which partial is followed. Ours with modulation, and theirs, mostly fall in that same range. The only clean gap is MASSIVE at 15 s: Supermassive reads 29 to 35 cents on the vibes, ours 6.6 (4.9 with modulation off). Judge modulation by ear (suggestion 6).

---

## HALL vs Valhalla Room

In each cell: **ours · Large Room · Dark Chamber**.

| | about 2 s | about 5 s | about 15 s |
|---|---|---|---|
| Decay, mids (s) | 2.1 · 2.1 · 2.2 | 5.2 · 5.3 · 5.3 | 15.5 · 15.4 · 15.1 |
| Decay 125 Hz (s) | 2.4 · 2.3 · 2.3 | 5.7 · 5.4 · 5.9 | **17.4** · 15.4 · 15.4 |
| Decay 4 kHz (s) | **1.4** · 2.0 · 1.9 | **3.3** · 4.7 · 4.3 | 9.7 · 12.5 · 10.3 |
| Decay 8 kHz (s) | **1.1** · 1.8 · 1.7 | **2.6** · 4.0 · 3.4 | 8.1 · 8.9 · 6.5 |
| Extra treble loss at 8 kHz (dB/s) | **23** · 4.8 · 6.7 | **11** · 3.7 · 5.9 | 3.3 · 2.8 · 5.1 |
| Tail darkens 0.25 → 2 s (dB) | **−22** · −1 · −4 | **−14** · −1 · −4 | −5.5 · −1 · −6 |
| First sound (ms) | **80** · 32 · 9 | **80** · 32 · 9 | **80** · 32 · 9 |
| Tail peaks at (ms) | **127** · 57 · 34 | **127** · 57 · 34 | **164** · 57 · 34 |
| C80 (dB) | −0.3 · −0.9 · +2.3 | −4.6 · −5.2 · −2.5 | −9.7 · −9.5 · −7.2 |
| Grain, the start (s) | 0.023 · 0.077 · 0.102 | 0.029 · 0.080 · 0.096 | 0.033 · 0.079 · 0.099 |
| IACC early / late | 0.15 · 0.08 · 0.02 / 0.03 · 0.02 · 0.03 | 0.12 · 0.08 · 0.02 / 0.02 · 0.02 · 0.03 | 0.10 · 0.06 · 0.02 / 0.02 · 0.01 · 0.02 |
| Wet balance 63 Hz re 1 kHz (dB) | 0.0 · −2.0 · −1.2 | −0.1 · −0.6 · −1.0 | −0.5 · −0.8 · −0.3 |
| Wet balance 8 kHz re 1 kHz (dB) | **−4.2** · −0.8 · −0.1 | −3.1 · −1.5 · −1.3 | −2.7 · −3.1 · −3.9 |
| Wet level re dry (dB) | **−12.4** · −0.2 · +0.1 | **−7.7** · +0.3 · 0.0 | −2.6 · +0.5 · +0.6 |

**In plain words.**
- **Room's short tails stay bright; ours go dull.** At 2 s, our highs fade four to five times faster than Room's: 23 dB a second beyond the mids, against 5 to 7. Even Dark Chamber, the dark one, stays brighter than our HALL at 2 and 5 s; only at 15 s is it the darker.
- **The reason.** Room's extra loss in the highs is about the same at every decay, 3 to 7 dB a second, as air in a real room. Ours is a fixed *share* of the decay, so the shorter the tail, the faster its highs vanish.
- **Room answers sooner.** Its first reflections arrive within 9 to 32 ms and its tail peaks within 60 ms. Ours is silent for 80 ms, then peaks at 130 to 160 ms. You hear a small gap between the note and our hall.
- **Room's start is a little grainier** (its early reflections are separate echoes). Ours is smoother, still well under the 0.1 s "grainy" bar. Dark Chamber's start sits right at the bar.
- **Our 15 s hall keeps its bass about 13 % longer** (17.4 s at 125 Hz against 15.4 s), a slightly warmer, fuller long tail.
- **Width is the same:** the late IACC is 0.01 to 0.03 for all three, and the music tails agree. Our early sound is a little narrower than Dark Chamber's.
- **Room is equally loud at every decay.** Ours is 12 dB quieter at 2 s than at 15 s.

## PLATE vs Valhalla Plate

In each cell: **ours · Chrome · Lithium**.

| | about 2 s | about 5 s | about 15 s |
|---|---|---|---|
| Decay, mids (s) | 2.2 · 1.9 · 1.9 | 5.1 · 4.8 · 5.0 | 15.2 · 14.0 · 15.2 |
| Decay 125 Hz (s) | 2.3 · 2.0 · 1.6 | 5.9 · 5.5 · 6.4 | 17.0 · 19.3 · **21.1** |
| Decay 4 kHz (s) | 1.5 · 1.5 · 1.7 | 3.3 · 2.7 · 4.2 | 8.8 · **5.3** · 8.8 |
| Decay 8 kHz (s) | 1.2 · 1.2 · 1.5 | 2.7 · 1.8 · 3.5 | 7.7 · **2.9** · 6.3 |
| Extra treble loss at 8 kHz (dB/s) | 20 · 18 · 8 | 10 · 21 · 5 | 3.6 · 16 · 5.4 |
| Tail darkens 0.25 → 2 s (dB) | **−24** · −9 · −4 | −16 · −15 · −6 | −9 · −13 · −9 |
| First sound (ms) | 19 · 22 · 18 | 19 · 22 · 18 | 19 · 21 · 18 |
| Tail peaks at (ms) | **120** · 50 · 45 | **120** · 52 · 45 | **192** · 52 · 50 |
| Rise, −20 → −3 dB (ms) | **49** · 19 · 16 | **49** · 20 · 17 | **52** · 23 · 18 |
| C80 (dB) | **−2.9** · +0.3 · +1.4 | **−6.2** · −3.6 · −2.5 | **−10.7** · −7.3 · −6.4 |
| Grain, the start (s) | 0.033 · 0.000 · 0 | 0.034 · 0 · 0 | 0.036 · 0.002 · 0 |
| Tail IACC (music) | 0.31 · 0.52 · 0.40 | 0.15 · 0.30 · 0.18 | 0.22 · 0.16 · 0.15 |
| Wet balance 125 Hz re 1 kHz (dB) | 0.2 · −0.3 · 0.3 | −0.3 · 2.0 · 1.1 | −0.5 · **2.1** · **2.0** |
| Wet balance 8 kHz re 1 kHz (dB) | −2.1 · −3.4 · −7.6 | −3.0 · −5.1 · −9.4 | −3.6 · −6.5 · **−11.5** |
| Wet level re dry (dB) | −8.7 · −4.7 · −3.7 | −5.8 · +1.6 · +0.6 | −4.1 · +8.2 (clipped) · +4.6 |

**In plain words.**
- **The decays are close.** Chrome is the dark plate: its highs fade fast at every decay (16 to 21 dB a second beyond the mids). Lithium is the bright one (5 to 8 dB a second). Ours sits near Chrome at 2 s, between the two at 5 s, and is the brightest of the three at 15 s. That is the same fixed-share behaviour as the HALL, but a plate is allowed to be dark, so it matters less here.
- **Valhalla's plates hit harder.** Both start when ours does (about 20 ms), but they reach full level in under 20 ms and peak by 50 ms. Ours takes about 50 ms to rise and peaks at 120 to 190 ms. Their sound is more "up front": C80 is 2.5 to 4 dB higher. Their start is also completely smooth from the first millisecond (grain 0). Ours is smooth too (0.033 s), just not instantly.
- **At long decays theirs is warmer and darker in tone, not just in decay.**
  - Bass: their wet carries about 2 dB more at 63 to 250 Hz, and Lithium's bass rings 21 s against our 17 s.
  - Highs: they hold 3 to 8 dB less at 8 kHz.
- **Theirs gets louder as the decay gets longer**, by up to +8 dB, enough to clip the Chrome take. Ours gets quieter as the decay gets shorter.
- **Width is the same.** Valhalla's short plates are a little narrower in the tail.

## MASSIVE vs Valhalla Supermassive

In each cell: **ours · Sagittarius · Andromeda**. The decay rows come from the impulse responses. The "2 s" Andromeda is its 0 % feedback take, which rings only about 0.5 s.

| | about 2 s | about 5 s | about 15 s |
|---|---|---|---|
| Decay 125 Hz, impulse (s) | 1.8 · 2.1 · 0.5 | 5.1 · 4.7 · 5.1 | 15.8 · 14.4 · 14.3 |
| Decay 1 kHz, impulse (s) | 2.5 · 2.0 · 0.5 | 5.3 · 4.9 · 4.7 | 14.3 · 14.5 · 14.6 |
| Decay 8 kHz, impulse (s) | **0.5** · 2.7 · 0.1 | **2.9** · 4.9 · 4.9 | **8.3** · 14.8 · 14.5 |
| Tail darkens 0.25 → 2 s (dB, music) | −4.5 · −8 · (–) | −5.9 · −4.2 · −1.1 | **−5.1** · −1.3 · +0.9 |
| First sound (ms) | 190 · 421 · 1118 | 181 · 38 · 731 | 184 · 3 · 649 |
| Tail peaks at (ms) | 1383 · 1290 · 1143 | 1731 · 1285 · 1553 | 1642 · 57 · 1286 |
| Rise, −20 → −3 dB (ms) | **588** · 24 · 12 | 685 · 928 · 278 | **785** · 4 · 438 |
| C80, sync click (dB) | **−26.4** · −6.4 · −6.3 | **−24.0** · −8.3 · −6.9 | **−26.7** · −9.0 · −20.8 |
| Grain, the start (s) | 0.060 · **1.300** · 0.159 | 0.043 · **0.836** · **0.725** | 0.004 · 0.037 · 0.075 |
| Tail IACC (music) | 0.23 · 0.13 · 0.18 | 0.28 · 0.13 · 0.08 | 0.23 · 0.08 · 0.08 |
| Wet balance 125 Hz re 1 kHz (dB) | +1.8 · 0.0 · 0.0 | +0.9 · 0.0 · 0.0 | +0.5 · −0.1 · +0.2 |
| Wet balance 8 kHz re 1 kHz (dB) | −0.2 · −0.1 · −0.2 | −1.1 · −0.2 · −0.2 | **−3.0** · −0.2 · −0.2 |
| Wet level re dry (dB) | **−11.5** · −3.0 · −2.9 | **−11.1** · −2.9 · −3.0 | −7.2 · −2.9 · −1.7 |

**In plain words.**
- **Supermassive's tail never darkens.** Its highs ring exactly as long as its lows: 14.8 s at 8 kHz at the 15 s setting, against our 8.3 s. Its wet is flat in tone, within 0.2 dB at every band. Ours carries 1 to 2 dB more low-mid and up to 3 dB less top. At 2 s, our 8 kHz dies in half a second; Sagittarius's lasts 2.7 s. This is the same fixed-share damping as the HALL. Note that the guide set Supermassive's High Cut to its maximum, so "no darkening" is its neutral, not necessarily how it's usually heard.
- **The attacks are shaped differently.** Both are loudest about 1.1 to 1.7 s after the note.
  - Ours fades in from about 0.2 s over 0.6 to 0.8 s: a slab of swell.
  - Supermassive mostly waits, then arrives fast. Sagittarius's first sound comes 0.4 s late at 2 s and its rise takes 24 ms; at 15 s it starts within 3 ms and peaks at 57 ms, no swell at all. Andromeda waits 0.65 to 1.1 s.
  - Because so little of ours arrives early, its C80 is 15 to 20 dB lower (6 dB against Andromeda at 15 s): we are much more "behind" the note.
  - Caveat: Supermassive's start time varies between takes far more than feedback should explain (see recording checks below).
- **Supermassive is grainy at low feedback** (Sagittarius at 2 and 5 s, Andromeda at 5 s): you hear separate echoes for most of a second (grain 0.7 to 1.3 s). Ours is a wash from the start at every decay. The "echoes → wash" journey is a Supermassive trait that our MASSIVE at SIZE 0.7 doesn't show.
- **Supermassive moves more in pitch at 15 s** (29 to 35 cents on the vibes, against our 6.6). It's the one wobble figure clear of the method's noise.
- **Supermassive holds its level at any feedback.** Ours is 4 to 9 dB quieter than it, most at short decays.

---

## Suggestions, ranked

Each could be heard on a page next to the Valhalla takes before anything is decided. None of them has been tried.

1. **Make the highs fade at a steady rate, like air, instead of a fixed share of the decay (HALL first).**
   - *Measured:* a 2 s HALL loses 23 dB/s extra at 8 kHz, against Room's 5 to 7, and gets 22 dB darker in its first 2 s, against 1 to 4. At 15 s the two already agree.
   - *Likely cause:* `core/dsp/Governor.h` `absorbent()` sets `highRate = lowRate / highRatio`, with `kDampingHighT60Ratio = 0.45f` (`core/params/SpaceVoicing.h`) at TONE noon. The highs' share is the same at every DECAY, so short tails lose their top fastest.
   - *Alternative:* a fixed extra loss, highRate = lowRate + about 5 dB/s at the top (Room measures 3 to 7), keeping TONE's ends as they are.
   - *Musical question:* "Should a short HALL stay bright, as a real room does, or go dull quickly, as now?"
   - *Page:* HALL at 2 s and 5 s, now against the fixed rate, beside Large Room and Dark Chamber.
2. **Let the HALL answer sooner.**
   - *Measured:* the first sound comes at 80 ms and the peak at 130 to 160 ms, against Room's 9 to 32 ms and 35 to 57 ms.
   - *Likely cause:* the HALL has no early path. `core/dsp/Engine.cpp` calls `fdn_.setEarlyTaps(massive ? 1.0f : 0.0f)`. So nothing comes out until the shortest line has played once: `kFdnRatios[0] = 0.60` × the HALL's mean line (`hallMeanSeconds`, about 117 ms at SIZE 0.6), about 70 ms, plus TIME's 10 ms. Turning MASSIVE's early taps on in the HALL (`kEarlyTapLow`, `kEarlyTapLevel`) is the smallest experiment.
   - *Musical question:* "Should the hall start right behind the note, or with a small gap?"
   - *Page:* HALL with and without early taps at TIME 0, beside Room.
3. **Let MASSIVE's top ring as long as its body.**
   - *Measured:* at 15 s our 8 kHz rings 8.3 s against Supermassive's 14.8; at 2 s, 0.5 s against 2.7. Their wet is tonally flat; ours tilts dark.
   - *Likely cause:* the same shared `kDampingHighT60Ratio` (suggestion 1), with no MASSIVE-specific value. MASSIVE could take its own share (closer to 1) or almost no extra loss. TONE CCW would still darken it on demand.
   - *Musical question:* "Should MASSIVE's tail stay airy and shimmering, or settle darker as now?"
4. **Keep the reverb's loudness when DECAY changes.**
   - *Measured:* from 15 s down to 2 s, our wet falls 10 dB (HALL −2.6 → −12.4 dB against the dry), 5 dB (PLATE) and 4 dB (MASSIVE). Room and Supermassive stay within 1 dB; Valhalla Plate gets *louder* with decay. On the module, turning DECAY down also turns the reverb down, so you'd have to chase it with MIX.
   - *Likely cause:* the tail's energy follows its T60 and nothing makes it up. `core/params/Mappings.h` `hallSizeLevelTrim` compensates SIZE only; the output gains are `kFdnOutputGain`, `kPlateOutputTrim` and `kMassiveOutputTrim`. A partial make-up, for example half of the 1/√T60 loss, would keep a short tail audible.
   - *Musical question:* "When you shorten the tail, should it also get quieter?"
   - *Page:* the page's level-match must be off for this one.
5. **Let the PLATE hit sooner.**
   - *Measured:* the peak comes at 120 to 190 ms against 45 to 52; the rise takes 50 ms against 16 to 23; C80 is 2.5 to 4 dB lower.
   - *Likely cause:* the four Dattorro input allpasses before the tank (`kDiffuserSeconds`, about 30 ms in all, with coefficients `kDiffuserCoeff` 0.75 / 0.625, in `SpaceVoicing.h`) smear the start. Then the output taps (`core/dsp/Plate.cpp`, `kLeft` / `kRight`) read mostly from the long delays.
   - *Experiment:* lower diffuser coefficients for the PLATE, or earlier taps.
   - *Musical question:* "Should the plate's attack be a crack or a bloom?"
   - Lower priority: the decays already match.
6. **Listen for modulation; don't measure it yet.**
   - *Measured:* the wobble method's noise (2 to 28 cents even with our modulation off) swamps most differences. Only MASSIVE at 15 s is clear: Supermassive 29 to 35 cents, ours 6.6.
   - *Page:* first a DRIFT page, our DRIFT at noon / 2 o'clock / 4 o'clock beside Supermassive and Room tails ("Tail only", +12 dB). That shows whether their shimmer is something you want at noon.
   - *Likely cause, if yes:* the modulation floor `kFloorDepthSeconds = 0.00046f` (about ±1.5 cents) in `SpaceVoicing.h`.

---

## Recording issues (not sound differences)

- **Clipping (re-export these with the output turned down 6 dB):**
  - `valhallaplate_chrome_long_music`: 15,187 samples at 0 dBFS. Valhalla Plate's wet rises with decay (+8 dB over the dry at 15 s).
  - `valhallaplate_lithium_long_music`: 298 samples.
  - `supermassive_andromeda_fb100`: 201 samples in the music take, 1,873 in the measure take.

  The 15 s Plate tables above include the clipped take; its level figures read low and its decays are barely affected.
- **Supermassive's start time changes between takes far more than feedback should explain:**
  - Sagittarius's first sound comes at 421 ms (fb 7 %), 38 ms (30 %) and 3 ms (73 %);
  - Andromeda's at 1118, 731 and 649 ms.

  Each take's measure and music files agree, so this is a setting, not misalignment: most likely the DELAY knob (or a preset recall) differed between takes. Please check. Until then, the MASSIVE attack comparison is between different delays.
- **Sound before the sync click** in `supermassive_andromeda_fb012_music` (−88 dBFS) and `_fb100_music` (−81 dBFS): the plugin was still ringing from earlier playback when the export started. It's harmless here, but bounce from silence next time.
- **No dry leak in any take.** At the dry click's instant, every take is 116 dB or more below its first 100 ms (where it has sound there).
- **`measure.wav`'s click comes only 3 s after its sync click**, so for long decays the sync's tail is still sounding under the click. The script ignores that tail when timing the click, and uses the music take's sync click (1 s of silence before it) for the timing and dry-leak figures.
- **The ingested pieces** (`test_audio/reference/<take>/`) are cut from the sync click's response, which removes each unit's predelay. Anything measuring onset should use the raw takes, as the script does.
- **Missing takes.** The guide asked for `measure` takes at every target. Room and Plate have music only, so their decays come from music tails. A `measure` take of each would let us check their numbers against an impulse.
