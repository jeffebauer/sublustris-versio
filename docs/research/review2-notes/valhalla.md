# Overnight review 2: ours against the Valhalla takes (v1.0.55)

*28-29 Oct 2026, branch `review2/valhalla`. Measurement, plus one desktop-only prototype hook
(`IV_LOSS_SHELF_HZ`, off by default: renderer SAME 60/60 against the pinned main, ctest 100 % of 21,
release firmware 112,564 B unchanged). Builds on `docs/research/valhalla-comparison.md` (v1.0.24)
and `valhalla-notes.md`; it does not redo their per-band music-tail decays, wet balance or loudness.
The recordings were read in memory from `test_audio/reference/incoming/` only; nothing of them is
copied or committed. Numbers: `valhalla-data/*.json`. Tools: `tools/review2/valhalla_metrics.py`
(measure), `tools/review2/valhalla_table.py` (print).*

Tags: **[M]** measured tonight on the desktop renderer, **[E]** estimated, **[L]** literature.

## What was measured, and how

- **Same stimulus, same cuts.** Every take is cut on the kit's timeline (sync click at 1.0 s,
  `cues.json`), 0.5 s before the stimulus: the click (Room and Plate have only the music take's sync
  click, 2.95 s long; Supermassive's measure take has a click with 25 s after it), and the drums, pad
  and vibes pieces with their tails. Ours renders the same kit cuts from an empty tank. ERA 0, TIME 0.2,
  TONE and DRIFT noon, MIX 1, NATURAL (as the first comparison).
- **Matched by tail length, not knob.** Our DECAY was bisected so the 1 kHz T60 (EDC, T30) meets the
  references' own 1 kHz T60 from the same method: HALL SIZE 0.6 DECAY 0.2232 / 0.4089 / 0.6401 (2.0 /
  5.0 / 14.7 s); PLATE SIZE 0.6 DECAY 0.2874 / 0.4807 / 0.7111 (1.8 / 4.7 / 13.3 s); MASSIVE 3.0 s
  (SIZE 0.2, DECAY 0: the shortest a MASSIVE can be now is 2.1 s at SIZE 0, 7.2 s at SIZE 0.7 since
  v1.0.32's six passes), 5.4 s (SIZE 0.35, 0.076), 14.5 s (SIZE 0.7, 0.2004).
- **There is no VintageVerb take** in `test_audio/reference/` (20 folders: Supermassive Sagittarius
  and Andromeda, ValhallaPlate Chrome and Lithium, ValhallaRoom Large Room and Dark Chamber). HALL is
  compared with Room only.
- **Metrics** (definitions in the script's docstring):
  - T60 per octave: Schroeder backward integration, noise subtracted and truncated at the floor;
    T30 where the click has 45 dB of range, else the drums tail [L: ISO 3382-1; Schroeder 1965].
  - Normalised echo density (NED): Abel & Huang 2006 (AES 121st, paper 6985), 20 ms Hann, 100 Hz-10 kHz
    band-limit on both sides; 1 = a Gaussian wash; t90 = time from the first sound to NED 0.9.
  - Colouration: the fine power spectrum against its 1/3-octave-smoothed self, 200 Hz-8 kHz. A tail
    whose modes overlap is complex Gaussian, so the dB deviation has std 5.57 dB, 99th percentile
    +6.6 dB and almost nothing over +10 dB [L: Schroeder 1962, "Frequency-correlation functions of
    frequency responses in rooms"; Schroeder & Kuttruff 1962]. Excess over that = audible ringing modes.
    Windows: click IR 0.3-1.3 s after the first sound; drums tail 0.5-2.5 s after the last hit.
  - Modal density: peaks per Hz (3 dB prominence, 200-2000 Hz) in a 4 s late window, against white
    noise with the same envelope.
  - Stereo: zero-lag L/R correlation, IACC (±1 ms) and the mono fold-down: energy of (L+R)/2 against
    the mean of L and R (0 dB identical, −3 dB uncorrelated, below −3 dB cancels in mono).
  - Modulation: the strongest partial (150-2000 Hz) of the pad and vibes tails, 0.3-3.3 s after the
    last note: its spectral spread (cents, energy-weighted, ±50 cents) and instantaneous-frequency std;
    ours also with the tank modulation off (`IV_MOD_SCALE=0`) as the method's floor. Plus
    `tools/pitch_track.py` on a held 1 kHz (stimulus 08, 3-8 s) for ours.
  - Onset: first sound (−40 dB), the 50 ms envelope's peak, rise −20→−3 dB, the share of the first
    second's energy in the first 20/50/100 ms after the first sound, centre time.

## HALL vs Valhalla Room

Cells: **ours · Large Room · Dark Chamber**.

| | 2 s | 5 s | 15 s |
|---|---|---|---|
| T60 1 kHz (s) [M] | 2.0 · 2.0 · 2.0 | 5.0 · 5.1 · 5.0 | 14.7 · 14.9 · 14.4 |
| T60 125 / 500 Hz re 1 kHz | 1.10/1.00 · 1.00/1.05 · 1.05/0.95 | 1.08/1.06 · 0.96/0.94 · 1.02/1.06 | **1.28/1.20** · 0.94/0.99 · 1.07/1.02 |
| T60 2k / 4k / 8k re 1 kHz | 0.90/0.85/0.80 · 1.0/0.95/0.90 · 1.0/0.90/0.80 | 0.88/0.72/0.66 · 0.96/0.88/0.69 · 0.94/0.76/0.60 | **0.72/0.48**/0.39 · 0.95/0.77/0.58 · 0.90/0.65/0.43 |
| NED at 10 / 50 ms; t90 (ms) | 0.59/0.90; 28 · 0.07/0.32; 218 · 0.13/0.13; 361 | same | same |
| First sound / peak / rise (ms) | 12 / **126** / 72 · 32 / 57 / 18 · 9 / 34 / 5 | 12 / 128 / 71 | 12 / 128 / 70 |
| First 50 ms, dB of the first second | **−17.1** · −5.0 · −3.3 | −20.8 · −7.7 · −6.0 | −23.3 · −10.4 · −8.8 |
| Centre time (ms) | 217 · 167 · 108 | 385 · 349 · 263 | 735 · 754 · 637 |
| Colour, click: std dB / p99 / ‰ over +10 dB | 5.90/7.2/1.1 · 5.64/6.6/0.0 · 5.66/6.7/0.1 | 5.92/7.3/1.1 · 5.57/6.6/0.0 · 5.63/6.8/0.0 | 5.91/7.4/0.8 · 5.58/6.6/0.0 · 5.63/6.8/0.1 |
| Colour, drums tail: std / ‰ over +10 dB | 6.79/**6.2** · 6.18/2.9 · 6.12/1.7 | 6.49/**5.0** · 6.06/1.6 · 6.05/1.7 | 6.40/**4.5** · 6.00/1.3 · 5.91/1.2 |
| L/R corr 0-50 ms (mono dB) | **−0.22 (−4.1)** · 0.01 (−3.0) · 0.00 (−3.0) | same | same |
| L/R corr 0.2-1 s; drums tail | 0.05; −0.09 · −0.01; 0.01 · 0.01; 0.06 | 0.03; 0.03 · 0.00; 0.32 · −0.02; 0.13 | 0.02; 0.05 · 0.01; 0.06 · 0.00; −0.01 |
| Pad / vibes partial spread (cents) | 15.7/5.7 · 13.0/9.9 · 15.1/10.4 | 9.5/5.8 · 11.2/9.1 · 17.4/14.7 | **4.7/6.1** · 12.8/15.7 · 17.6/20.2 |
| ours with modulation off | 7.9/11.0 | 6.3/3.1 | 2.5/1.6 |

## PLATE vs ValhallaPlate

Cells: **ours · Chrome · Lithium**.

| | 2 s | 5 s | 15 s |
|---|---|---|---|
| T60 1 kHz (s) | 1.8 · 1.8 · 1.9 | 4.7 · 4.5 · 4.9 | 13.3 · 12.4 · 14.0 |
| T60 125 Hz / 8 kHz (s) | 2.1/1.5 · 1.8/1.2 · 2.1/1.5 | 5.4/2.8 · 5.6/1.8 · 5.4/3.5 | 19.1/4.6 · 17.8/2.7 · 18.8/5.8 |
| NED at 10 / 20 / 50 ms | **0.26/0.48/0.63** · 0.50/0.73/0.97 · 0.96/0.96/1.05 | same | same |
| NED t90 (ms) | **176** · 34 · 17 | **256** · 34 · 17 | **251** · 34 · 17 |
| First sound / peak / rise (ms) | 19 / **134** / 41 · 22 / 50 / 19 · 18 / 45 / 16 | 19 / **144** / 42 · 22 / 52 / 20 · 18 / 45 / 17 | 19 / **166** / 43 · 21 / 52 / 23 · 18 / 50 / 18 |
| First 50 ms, dB of the first second | −9.0 · −4.3 · −3.9 | −11.1 · −6.7 · −6.1 | −12.9 · −8.8 · −8.0 |
| Colour, click: std / ‰ over +10 dB | 6.58/3.5 · 5.98/1.3 · 5.87/0.8 | 6.42/2.9 · 5.98/0.9 · 5.89/0.8 | 6.35/2.3 · 5.93/0.6 · 5.95/0.7 |
| Colour, drums tail: std / ‰ | 7.21/7.4 · 7.94/12.6 · 7.90/12.6 | 6.74/5.3 · 8.36/12.8 · 7.82/11.8 | 6.58/4.3 · 8.46/13.6 · 7.55/10.9 |
| L/R corr 0-50 ms; drums 63 / 125 Hz | 0.04; 0.38/−0.16 · −0.02; −0.25/−0.13 · 0.05; −0.13/0.65 | 0.04; 0.35/0.26 · −0.03; −0.06/−0.31 · 0.05; 0.06/−0.02 | 0.04; 0.00/0.15 · −0.01; 0.30/0.02 · 0.05; 0.15/0.06 |
| Pad / vibes spread (cents) | 11.9/6.5 · 13.8/6.7 · 12.2/10.7 | 11.3/5.1 · 11.1/3.8 · 9.9/1.7 | 7.4/4.7 · 9.7/2.6 · 7.3/2.5 |

## MASSIVE vs Supermassive

Cells: **ours · Sagittarius · Andromeda** (feedback takes fb007 / fb030 / fb073 and fb000 / fb012 / fb054).
Supermassive's onset figures are between different DELAY settings (valhalla-comparison.md, "Recording
issues"), so they are left out; its NED is from the music take's sync click.

| | ~3 s | ~5 s | ~15 s |
|---|---|---|---|
| T60 1 kHz (s) | 3.0 (SIZE 0.2) · 2.9 · 0.4 | 5.4 (SIZE 0.35) · 4.8 · 6.0 | 14.5 · 14.5 · 14.5 |
| T60 125 Hz / 8 kHz | 3.0/3.0 · 3.3/3.3 · – | 5.5/5.4 · 4.5/4.7 · 6.2/6.0 | 14.8/14.5 · 14.9/14.6 · 14.4/14.5 |
| NED at 50 ms; late (0.5-2.5 s) | 0.80; 0.97 · 0.18; 0.34 · 0.55; 1.35 | 0.84; 0.99 · 0.52; 0.73 · 0.26; 0.64 | 0.90; 0.99 · 0.80; 0.97 · 0.57; 0.99 |
| Colour, click: std / p99 / ‰ | 6.19/7.8/1.2 · 5.60/6.4/0.0 · 5.46/6.4/0.0 | 6.20/7.8/1.1 · 5.58/6.6/0.0 · 5.39/6.3/0.0 | 6.16/7.6/0.7 · 5.59/6.5/0.1 · 5.59/6.4/0.0 |
| Colour, drums tail ‰ (see the caveat) | 10.8 · 1.9 · 2.1 | 11.4 · 1.4 · 1.5 | 10.1 · 0.9 · 1.0 |
| L/R corr 0-50 / 50-200 ms (mono dB) | −0.13/−0.07 (−3.6/−3.3) · ~0 (−3.0) | −0.14/−0.13 (−3.6/−3.6) · ~0 | **−0.48/−0.21 (−5.8/−4.0)** · −0.03/0.02 · 0.03/0.00 |
| Pad / vibes spread (cents) | 10.0/10.8 · 14.8/10.6 · 12.4/11.7 | 7.0/6.4 · 14.0/15.0 · 14.6/11.6 | **8.0/1.5** · 10.1/15.5 · 18.0/16.9 |
| ours, modulation off | 8.7/7.4 | 7.0/4.8 | 7.7/1.5 |

Caveat: MASSIVE's built-in mass (the swell stage) still feeds the last drum hits into the tank for
~1 s after they stop, so its drums-tail window holds late, pitched drum sound; its click figures are
the clean ones. 24 swell taps instead of 12 (`IV_SWELL_TAPS=24`) changed nothing (drums ‰ 10.9,
click std 6.24), so the swell's tap count is not the cause.

## Modal density (inconclusive)

Peak counts sit at 0.86-1.0 of the shaped-noise baseline for every take, ours and theirs: at 4 s
windows the method is resolution-limited and can't tell the units apart [M]. Estimated instead [E]:
HALL at SIZE 0.6 has a mean line of 117 ms (`map::hallMeanSeconds`, 35 → 260 ms exponential), so
8 lines × ratios summing to 7.80 = 0.91 s of loop delay ≈ 0.9 modes per Hz (Jot: modal density =
total delay). At 15 s a mode's bandwidth is 2.2/T60 = 0.15 Hz, so the modal overlap is ~0.14, far
under Schroeder's 3 for a Gaussian (mode-free) response [L: Jot 1997 §2; Schroeder 1962]. Only time
variation can hide the modes then, which is what the modulation rows show.

## Where ours is weaker, ranked

### 1. A long HALL's upper mids die early; its bass rings long [M]
At 15 s (matched at 1 kHz), our 125-500 Hz ring 1.20-1.28 × the 1 kHz T60 and 2 / 4 kHz only 0.72 /
0.48 ×. Room is flat to 2 kHz (0.94-1.07 below, 0.90-0.95 at 2 kHz) and falls above. So our long HALL
is boomier-then-duller in the band where voices and keys live; at 5 s the gap is small, at 2 s none
(the v1.0.25 air fix holds: 8 kHz 0.80 × vs 0.80-0.90).
- **Cause:** the loss is a *first-order* shelf crossing at 2.5 kHz (`core/params/SpaceVoicing.h:112`
  `kLossShelfHz`; `core/dsp/Governor.h:108-131` `absorbent`). A first-order shelf's transition spans
  about three octaves, so with the high end at 0.45 × the low T60 (`SpaceVoicing.h:101`) plus air
  5.5 dB/s (`SpaceVoicing.h:62`) it already takes ~20 % off the 1 kHz T60 and ~30 % off 2 kHz. Air in
  a real room rises with roughly f² and leaves 1-2 kHz almost alone [L: ISO 9613-1 absorption tables].
- **Prototype `IV_LOSS_SHELF_HZ`** (desktop only; `Fdn::setShelfHzPrototype`, `Engine::setLossShelfPrototype`,
  `host/render/main.cpp`): the HALL / MASSIVE tank's shelf crossing moved. Ratios re 1 kHz, 125 / 250 /
  500 / 2k / 4k / 8k, at 15 s:
  - built 2.5 kHz: 1.28 / 1.26 / 1.20 / 0.72 / 0.48 / 0.39
  - 4 kHz: 1.11 / 1.09 / 1.09 / 0.82 / 0.56 / 0.37
  - **6 kHz: 1.04 / 1.02 / 1.04 / 0.91 / 0.71 / 0.44** (Dark Chamber 1.07 / 1.02 / 1.02 / 0.90 / 0.65 / 0.43;
    Large Room 0.94 / 1.01 / 0.99 / 0.95 / 0.77 / 0.58)
  - at 5 s, 6 kHz gives 1.00 / 1.00 / 1.00 / 0.96 / 0.87 / 0.70 (Large Room 0.96 / 1.00 / 0.94 / 0.96 / 0.88 / 0.69).
  The knob's T60 is the lows' (the mapping's), so the mids get longer at the same DECAY (1 kHz 14.7 → 18.1 s).
- **Cost:** CPU 0 (a coefficient; the memo already recomputes on change). Flash 0 B as a constant
  change for both FDN spaces; HALL-only needs a per-space shelfK (est. 20-50 B: tight against the 76 B).
  The Taylor `bilinearK` is 1.4 % low at 6 kHz on the 24 kHz tank (`Governor.h:122-126`, its comment
  still says "from 44.1 kHz up"; at 2.5 kHz / 24 kHz it is 3e-4 low, fine).
- **Sound:** audible: a brighter, less bass-heavy long HALL; TONE's darkening then acts higher up, and
  the level make-up (`kDecayLevelPower`, `SpaceVoicing.h:75-76`) would need refitting (a brighter tail
  is louder). It partly reverses the owner's `tone_shelf` pick (v1.0.15, "upper mids fade with the highs").
  **Needs the owner's ears** ("Should a long hall keep its voice-range ringing as long as its body?").

### 2. A long HALL holds still: ringing modes, little pitch movement [M]
The click's late spectrum is 0.3 dB further from Gaussian than Room's (std 5.90 vs 5.57-5.66; values
over +10 dB 0.8-1.1 ‰ vs 0.0-0.1), and on the drums tail ours has 3-4 × Room's share of +10 dB spikes
(4.5-6.2 ‰ vs 1.2-2.9). At 15 s the pad and vibes partials spread 4.7 / 6.1 cents against Room's
12.8-17.6 / 15.7-20.2 (ours with modulation off: 2.5 / 1.6, so the method floor is low enough there).
- **Cause:** the modulation floor (`SpaceVoicing.h:53` `kFloorDepthSeconds` 0.46 ms × `kHallFloorScale` 3,
  `:57`; `core/dsp/Fdn.cpp:350, 519-530`) is set for the short tails; with ~0.9 modes/Hz (above) a long
  tail's modes are resolved unless they move. The v1.0.25 ×3 was judged on a drums 95th-percentile metric
  that this one agrees with but still sees a gap.
- **Measured with the existing hook `IV_MOD_SCALE`** (HALL 15 s; ×2 = the floor at 6 × the base):
  - drums ‰ over +10 dB 4.5 → 1.9 (×2) → 0.9 (×4) (Room 1.2-1.3); click std 5.91 → 5.85 → 5.65 (Room 5.58-5.63);
  - partial spread pad / vibes 4.7 / 6.1 → 8.2 / 11.8 → 7.8 / 17.2 cents (Room 12.8-17.6 / 15.7-20.2);
  - held 1 kHz under the tone (pitch_track p95): 10.2 → 11.1 (×2) → 18.4 (×3) → 23.4 cents (×4).
  - MOTION right (Wander 0.7 / 0.9) instead: drums ‰ 3.4 / 2.2, vibes 9.3 / 12.3 cents.
- **Option:** HALL's floor depth rising with T60 (×3 up to ~5 s, ×6 at 15 s and over), so short tails
  stay as picked. CPU ~0 (control rate, once per block); flash est. 20-40 B. Sound: subtle but audible
  (smoother long tails, a little more chorus on held notes: +1 cent p95 at ×2). Owner's ears; ×2 is the
  measured sweet spot (drums spikes at Room's level, held-tone wobble almost unchanged).

### 3. PLATE starts sparse and peaks late [M]
NED reaches 0.9 at 176-256 ms after the first sound against 17 (Lithium) and 34 ms (Chrome); at 10 /
20 ms ours is 0.26 / 0.48 (separate echoes) where Lithium is already 0.96. The envelope peaks at 134-166
ms against 45-52 ms, and the first 50 ms carry 4-5 dB less of the first second. (The 24 Oct figures
said the same about the peak; the echo density is new.) Its colouration on the click is also above
Valhalla's (std 6.35-6.58 vs 5.87-5.98), though on the drums tail ours is *less* coloured than Valhalla's
plates (6.6-7.2 vs 7.6-8.5): mixed, not a clear weakness.
- **Cause (plausible):** Dattorro's four input allpasses (`SpaceVoicing.h:119-120`, 0.75 / 0.625, ~30 ms)
  smear the hit only lightly before the tank, and the output taps (`core/dsp/Plate.cpp:33-36`, his Table 2)
  read mostly long delays, so the density only builds after a trip or two round the figure-8.
- **Options (not prototyped):** (a) PLATE-only higher input diffusion (0.8 / 0.7): CPU 0, flash ~0-20 B,
  audible (softer, denser crack); (b) a few early taps on the input-diffusion chain into the output,
  as HALL's early taps do: CPU est. 0.2-0.5 % of the budget (2-4 reads × 2 sides at 48 kHz), flash est.
  60-150 B (over the headroom without a saving elsewhere). Owner's ears ("crack or bloom?", raised on
  24 Oct and still open). Lower priority than 1-2: the decays already match.

### 4. HALL's onset is quiet, then peaks late [M]
The first sound now comes at 12 ms (v1.0.25's early taps), but the envelope peaks at 126 ms and the
first 50 ms carry −17 dB of the first second against −3 to −5 dB in Room; centre time 217 vs 108-167 ms at
2 s. Room's start is sparse (NED 0.07-0.13 at 10 ms) but loud; ours is smooth (NED 0.59 at 10 ms, t90
28 ms) but quiet.
- **Cause:** HALL uses 2 early taps a line at `kEarlyTapLevel` 0.355 with gains rising from
  `kEarlyTapLow` 0.3 (`SpaceVoicing.h:142-147`, v1.0.36's CPU pick); the bulk arrives with the lines'
  first pass (mean 117 ms at SIZE 0.6).
- **Option:** HALL's early-tap level up (+3 to +6 dB) or its low end raised: CPU 0, flash ~0-16 B,
  audible ("more immediate"). The owner kept the smoother early profile on `early_hits` (v1.0.36), so
  this is a level, not a density, question. Owner's ears.

### 5. HALL's and MASSIVE's first 200 ms lean anti-phase between L and R [M]
L/R correlation over the first 50 ms is −0.22 in HALL (every decay) and −0.13 / −0.14 / −0.48 in MASSIVE
(3 / 5 / 15 s), then −0.21 over 50-200 ms at 15 s. Summed to mono, that start loses 1.1 dB (HALL) and up
to 2.8 dB (MASSIVE) more than a decorrelated field. Room, Plate and Supermassive all sit at 0.0 ± 0.05.
Late tails are fine everywhere (−2.7 to −3.4 dB).
- **Cause (plausible, not isolated):** a centred input reaches lines 0, 2, 5, 7 (`kInL + kInR`,
  `core/dsp/Fdn.cpp:33-34`); the early taps go out with line (i+3)'s signs (`Fdn.cpp:782-785`), which
  for those four lines are L/R opposite for two and equal for two, so the balance rests on the input
  diffusers' small L/R differences; MASSIVE's sign-scrambled swell adds to it.
- **Option:** a tap-sign map whose excited lines balance to ≥ 0 (or alternate the cross-sign per tap):
  CPU 0, flash ~0. Tiny to audible (the onset's width and its mono sum). Measurable before the owner
  hears it; a golden refresh.

### 6. MASSIVE barely moves in pitch [M]
At 15 s the vibes partial spreads 1.5 cents (the same as with modulation off) against Supermassive's
15.5-16.9; a held 1 kHz wavers 1.1 cents p95 (×4 floor: 2.4). MOTION Wander at 0.9 reaches only 5.7.
The 24 Oct comparison (29-35 vs 6.6 cents) said the same with a noisier method.
- **Cause:** the floor's depth is absolute (0.46 ms, `SpaceVoicing.h:53`); pitch spread builds per pass,
  and MASSIVE's lines are ~10 × HALL's, so a tail makes ~10 × fewer passes a second [E].
- **Option:** scale MASSIVE's floor with its mean line (keeping the per-second spread of a HALL), CPU ~0,
  flash est. 20 B. Audible and a character change (Supermassive's default MOD is part of its sound;
  ours was picked "pitch-accurate"): already listed as a DRIFT comparison for the owner
  (`review-2026-10-26.md`); this is the number to hand them.

### 7. MASSIVE's late spectrum is a little coloured [M]
Click std 6.16-6.20 dB against 5.39-5.60 (p99 7.6-7.8 vs 6.3-6.6). ×4 modulation brings it to 6.09 / 5.85
(late window): the same static-mode cause as 2 and 6, smaller in effect.

## Where ours matches or is better [M]
- MASSIVE's band decays are flat like Supermassive's (v1.0.31 holds): 125 Hz-8 kHz within 4 %.
- PLATE's band decays sit between Chrome and Lithium at every length.
- HALL's short tails no longer go dull: 8 kHz at 0.80 × the 1 kHz T60 at 2 s (Room 0.80-0.90).
- Late echo density is a wash (0.97-1.01) everywhere; Supermassive's low-feedback takes are grainier.
- Late width and mono fold-down: −2.7 to −3.4 dB everywhere, like the references.
- PLATE's pitch spread matches ValhallaPlate's.

## What I could not check
- VintageVerb: no take exists.
- The module: all of this is the desktop renderer (fused multiply-adds possible); colouration and
  modulation are not expected to differ, the level details might.
- Supermassive's onset: its takes used different DELAY settings.
- Room / Plate long-tail EDCs come from the drums tail (their only long silence), ours from the click;
  ours agree within ~5 % between the two (v1.0.24 check).
- Modal density by peak counting: resolution-limited (above).
- Listening: none of the options was heard. Pages would be: HALL 15 s built vs `IV_LOSS_SHELF_HZ=6000`
  (level-matched) beside Room; HALL 15 s floor ×1 / ×2 beside Room (pad, vibes, drums).
