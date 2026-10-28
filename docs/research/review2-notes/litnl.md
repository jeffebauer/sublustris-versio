# review2 / litnl: saturation, tape, BBD, AM radio, aliasing, modulation (the literature against our design)

28 Oct 2026, overnight review 2, branch `review2/litnl` (from `review/overnight-2`, code at v1.0.55). Read-only on the Core:
everything written is reading plus a measurement harness (`tools/review2/`). Nothing was changed in `core/`, `host/` or
`firmware/`; no hook was added, so the shipped sound is untouched.

Tags: **[M]** measured here, **[E]** estimated (operation counts, arithmetic), **[L]** literature (source and section given),
**[U]** from memory, unverified.

## 0. What matters (ranked)

1. **[M] The wear's tape does alias, but only on bright, broadband, loud wets, near the top of the knob.** On steady chords
   (the skank) through PLATE / HALL / MASSIVE the tape's alias is -67 dB or lower re the output at every level tried (-30 to
   -12 dBFS wet RMS) at WEAR 1. On broadband bursts (hits, noise bursts) at WEAR 1 it is **-32 to -37 dB (HALL and MASSIVE, wet at -24 dBFS RMS; MASSIVE noise bursts -42)**,
   -26 to -31 at -18 and -20 to -23 at -12; PLATE is 5-8 dB worse (its wet is brighter); at WEAR 0.5 it is 4-15 dB better (most at low levels). The
   in-code claim ("the wet of a 24 kHz tank has little to fold") is right for tonal and tilted material and wrong for hats and
   noise bursts: the tank is band-limited at ~11 kHz but its wet is not empty between 4 and 11 kHz, and the tape's +6 dB emphasis puts
   exactly that band into the clip (§2).
2. **[M, L] Antiderivative antialiasing is the wrong tool here; 2x oversampling is the one that works.** First-order ADAA of the
   clamped cubic moved the alias by -7 to +4 dB on those wets (better only at the loud end, 3-4 dB; worse at -30 dBFS); the divide-free
   polynomial form -7 to +6 dB (3-6 dB better at -18 / -12 dBFS, worse at -30); 2x oversampling by the Core's own halfbands,
   **14 to 50 dB lower** (25-33 dB at -24 dBFS, 19-28 at -18; HALL hits at -18 dBFS: -28 -> -53). Literature agrees: ADAA has sinc zeros at
   multiples of fs, so it kills low-frequency aliases and does little for those near Nyquist (Vicanek, §3); it is "worth ~3x
   oversampling" only for a hard clipper at high gain on a low-pass-limited tone (Parker et al. 2016, Table 1). Cost of 2x: +107
   instructions per stereo sample (**about +1.5 to +1.8 points of the module's budget, +0.4 KB of flash**), which the release
   (76 B spare) cannot take without paying elsewhere. **Recommendation: no change** unless the owner hears fizz on hats at
   WEAR > 0.8; that is a question for ears, and the remedy is known.
3. **[M, E] Do not run ERA's chain at 24 kHz to save CPU** (the era-rethink note's "~3.4 points" idea) without hearing it:
   the same shaper at 24 kHz aliases **7-19 dB more** on the same wets (HALL hits at -24 dBFS: -35.5 -> -24.0 dB; MASSIVE noise bursts -43.7 -> -28.0; chords stay under -56 dB).
   A cubic is alias-free into the band only while the content is under fs/4 (12 kHz at 48 kHz, 6 kHz at 24 kHz).
4. **[M] The tank loop's saturator (Era.h) is not an aliasing problem at 24 kHz.** Its x^2 term folds only content above 6 kHz
   (a line at f > 6 kHz makes a line at 24 - 2f of relative size ev*A/2: -44 dB at A = 0.1, i.e. -20 dBFS internal); on a
   tail-like spectrum the one-pass alias is -54 to -47 dB at -3 dB/oct (sigma 0.1 to 0.2) and -80 to -73 at -6 dB/oct. The soft clip
   is exactly linear below its knee. Per-pass alias compounds, but lands in the band where the loop's own damping eats it (ADR 0008).
5. **[M] The AM band's over-modulation clamp aliases -41 to -52 dB re the band's output at normal levels** (after its own band-pass), so
   it is already inaudible; a soft knee would take another 7-14 dB (cost about nothing on the module, +~70 B of flash, a visibly
   different trough). Not worth the flash.
6. **[L, M] Our wow and flutter are inside what real machines do, at the heavy end of consumer gear**: transport wow +-12 c at
   0.5-1.2 Hz = 0.69 % peak (AES6/IEC-weighted ~0.35 %, ~0.58 % with the variance), against Lindos's figures of 0.03 % (pro), 0.08 %
   (best cassette), >= 0.2 % (average cassette deck or car player). Flutter +-3 c at 6-10 Hz = 0.17 % peak, about an average
   cassette's whole reading. The tank's Wander (20 -> 28 c per line, 1 -> 2.7 Hz) is ~10x Dattorro's +-3 c and 4-6x Clouds/Rings, by
   the owner's request ("stronger ends"); the incoherent lines make it a chorus, not a vibrato.
7. **[L] The BBD/compander model is the literature's, with two deliberate departures**: the clock stage is an unfiltered sample-and-hold
   on a 48 kHz grid (images at -23 dB re a 1 kHz tone, -9 dB at 5 kHz: the owner's "fizz", renders/wear_fizz F), and the expander's
   envelope is slower than the compressor's (12-32 ms against 10 ms; a real NE570 pair uses one RC for both), which exaggerates
   breathing. Both are voicing choices already picked by ear.

Nothing here is a bug. Options with costs are in §8.

## 1. Sources read

| Source | URL | Sections used | Took |
|---|---|---|---|
| Chowdhury, "Real-time physical modelling for analog tape machines", DAFx-19 | https://ccrma.stanford.edu/~jatin/420/tape/TapeModel_DAFx.pdf | 1.2 (Jiles-Atherton), 2.2 (RK4 + trapezoidal derivative, Langevin continued fraction), 3.3 (bias 55 kHz, gain 5, 24 kHz recovery low-pass), 3.4 (wow and flutter from a pulse train through a TC-260, a periodic function driving a modulated delay), 4.1 (**16x** oversampling: the bias and the hysteresis's own aliasing; unstable near Nyquist without it) | Why our memoryless clip needs no hysteresis solver; his 16x is for a 55 kHz bias we do not have |
| ChowTape repository (licence check) | https://github.com/jatinchowdhury18/AnalogTapeModel | README, licence | **GPL-3.0**: cite only, nothing used (ADR 0006) |
| Parker, Zavalishin, Le Bivic, "Reducing the aliasing of nonlinear waveshaping using continuous-time convolution", DAFx-16 | https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf | §2 (linear interpolation, rectangular kernel = ADAA1), §3-4 (half-sample delay; low-pass below Nyquist: y = (x_n + x_{n-1})/2 for a transparent shaper; ill-conditioning when x_n ~ x_{n-1}), §5 Table 1 (hard clipper, gain 10: same alias level needs 12x with no method, 4x with the rectangular kernel, 3x with the triangular) | ADAA's real worth and its price (a divide, a fallback, a low-pass) |
| Bilbao, Esqueda, Parker, Valimaki, "Antiderivative antialiasing for memoryless nonlinearities", IEEE SPL 24(7), 2017 | (not open; read through Holters 2020 and Chowdhury's notebook, which restate it) | second-order ADAA: one more divide and more fall-backs [L via Chowdhury notebook] | Orders above 1 are not cheaper |
| Holters, "Antiderivative antialiasing for stateful systems", DAFx-19 / Appl. Sci. 10(1), 2020 | https://www.hsu-hh.de/ant/wp-content/uploads/sites/699/2020/10/applsci-10-00020-v2.pdf | §1-2 (y = (F(u_n) - F(u_{n-1}))/(u_n - u_{n-1}), the half-sample low-pass "can be countered by a modest amount of oversampling (e.g. a factor of two)"), §4 (cost: two-fold oversampling doubles everything else) | The usual pairing is ADAA plus 2x, not ADAA instead of 2x |
| Chowdhury, "Practical considerations for antiderivative anti-aliasing" (notebook) | https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html | ADAA1 / ADAA2 formulas, tolerance and fallback | Divide count and fallbacks |
| Vicanek, "Note on alias suppression in digital distortion", 2023 (rev. 2024) | https://www.vicanek.de/articles/AADistortion.pdf | §2 (one-pole pre/post compensation of ADAA's low-pass; 3-tap variant, 6 dB down at Nyquist), §4 (ADAA reduces low-frequency aliases, "less efficient in the high-frequency range"; 1/f spectra: 2x OS only -6 dB, 4x -12 dB; ADAA + OS together best) | Why ADAA gives us nothing: our aliases come from 5-10 kHz content |
| Esqueda, Valimaki, Bilbao, "Aliasing reduction in soft-clipping algorithms", EUSIPCO 2015; Esqueda, Bilbao, Valimaki, "Aliasing reduction in clipped signals", IEEE TSP 64(20), 2016 | https://new.eurasip.org/Proceedings/Eusipco/Eusipco2015/papers/1570104119.pdf ; https://www.pure.ed.ac.uk/ws/files/26997332/07499828.pdf | abstracts, conclusions (polyBLAMP 2-point and 4-point corrections: +12 and +20 dB SNR on average, cheaper and lower-latency than oversampling, no overshoot; needs the fractional clip point and slope, fails at high fundamentals) | A third route for a clamp: not usable on dense wets |
| Kahles, Esqueda, Valimaki, "Oversampling for nonlinear waveshaping: choosing the right filters", JAES 67(6), 2019 | https://acris.aalto.fi/ws/portalfiles/portal/34665796/ELEC_Kahles2019_Oversampling_JAES.pdf | abstract, Table 1 (operation counts of IFIR / elliptic IIR / CIC), conclusion ("with less distorting nonlinearities, M = 2 or 4 may be acceptable"; audible-alias threshold NMR -10 dB) | A lower oversampling factor is accepted for mild shapers |
| Raffel, Smith, "Practical modeling of bucket-brigade device circuits", DAFx-10 | https://dafx.de/paper-archive/2010/DAFx10/RaffelSmith_DAFx10_P42.pdf | 3 (compander: NE570/571, full-wave rectifier + one-pole, tau = 10 kOhm * C_rect; feedback compressor f(x) = x/avg(\|f(x)\|), feedforward expander f(x) = avg(\|x\|)*x), 4.2 (BBD nonlinearity: THD ~ 1.01^(N/1024) - 1, about 1 % per 1024 stages, level-independent, harmonics falling off linearly in magnitude), 5 (aliasing: clock sample-and-hold between filters) | Our breath stage is this model; THD law for sizing a2, a3 (the scan's text layer is garbled; numbers checked against the formulae) |
| Holters, Parker, "A combined model for a bucket brigade device and its input and output filters", DAFx-18 | https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf | abstract, §1-2 (the BBD as a fixed-length variable-rate delay; filters before and after do the resampling, so no interpolation), conclusion (the audio rate must leave the filters enough attenuation at Nyquist "otherwise additional aliasing distortion may be introduced"; the cure is oversampling) | Our unfiltered hold is outside this model on purpose |
| ON Semiconductor NE570 data sheet (Farnell mirror) | https://www.farnell.com/datasheets/81871.pdf | rectifier/gain-cell text: gain G(t) = (G_init - G_final) e^(-t/tau) + G_final, tau = 10 kOhm * C_RECT; "ripple ... will modulate the signal ... third harmonic distortion, so there is a trade-off between fast attack and decay times and distortion" | A real compander's distortion is gain ripple (3rd), not a polynomial; ours is a polynomial plus a 10 ms follower |
| Lindos Electronics, "Wow and flutter measurement" | https://lindos.co.uk/articles/wow-and-flutter-measurement | professional machines 0.03 % weighted, best cassette decks ~0.08 %, average cassette decks and car players >= 0.2 %; wow < 4 Hz, flutter > 4 Hz; 1 kHz tone with 0.1 % flutter fine in a dead room, clearly heard in a reverberant one (tone beating with its own echo); scrape flutter above 100 Hz | Real-machine depths; the beating-with-echo mechanism |
| Virtins, "Wow and flutter measurement using Multi-Instrument" | https://www.virtins.com/doc/Wow-and-Flutter-Measurement-using-Multi-Instrument.pdf | §1-4 (drift < 0.5 Hz, wow 0.5-6 Hz, flutter 6-100 Hz; weighting peaks at 4 Hz; AES6 weighting factor 0.5 at 0.8 Hz; peak = 2-sigma value) | Rates and weighting |
| Wikipedia, "Wow and flutter measurement" (as quoted by Lindos); Sound & Vision's Nakamichi Dragon test via search | https://en.wikipedia.org/wiki/Wow_and_flutter_measurement | equipment figures; Dragon 0.019 % WRMS spec (a quartz-PLL transport), DR-2 0.035 % | The floor of consumer gear |
| Fallows, "Understanding synchronous detection"; "Legacy AM receivers: how envelope detection works" | https://play.fallows.ca/wp/radio/shortwave-radio/understanding-synchronous-detection ; https://ergo.fallows.ca/wp/radio/shortwave-radio/legacy-am-receivers-how-envelope-detection-works/ | selective fading drops the carrier 10-20 dB under the sidebands: "effective over-modulation and distortion"; diode envelope detectors distort weak and strong signals | The band's trough clamp is the right picture of over-modulation |
| Already in the repo (not redone) | `docs/research/radio-artifacts.md`, `new-eras.md`, `era-rethink.md`, `drift-rethink.md`, `drift-cathedra-side.md`, `review-2026-10-26.md` | Dattorro 1997 (+-8 samples, 1 Hz at 29.8 kHz), Clouds/Rings, Valhalla (above 1 Hz chorusing), Lexicon Spin 1-2 Hz and Wander in ms (PCM Native manual), ITU-R F.1487 / Watterson | Cited below where used |

Not found or not read: the original SPL 2017 letter (paywalled; read through three restatements), any Costello post on modulated delay lines
(searched; **[U]** what I remember of his advice, that long lines tolerate less depth and that rate sets chorus against smear, agrees with the
drift notes but is not used as evidence), Lexicon 224/480L chorus figures (as drift-rethink.md also found), a source for the NE570's attack and
release in a BBD pedal (the data sheet gives only tau = 10 kOhm * C_RECT; Raffel's capacitor range is unreadable in the scan).
Hysteresis models (Jiles-Atherton) are covered by `tape-out.md`; nothing new here.

## 2. Our nonlinear stages: where they run and what they fold

Where (read from `Engine.cpp`): PLATE's loop (with its LoopEra) runs at 48 kHz; HALL's and MASSIVE's FDN and the twin's room at **24 kHz**
(`fdn_.prepare(large_, 0.5f * sampleRate ...)`, halfbands either side, flat to ~10.5 kHz, 85 dB down from 13.4 kHz); the still-lows tank at sr/8.
ERA's wear (`EraTexture`), the Transport and the Shimmer run at 48 kHz on the wet, after the tank. The wet is **not** empty above the tank's
band: the attack/early taps and PLATE keep the full band, and drum material has 8-12 kHz energy.

| Stage | Curve | Order / corner | Alias risk (analytic) | Measured here |
|---|---|---|---|---|
| Loop: `softClip(x + ev(x^2 - mean))` at 24 kHz | linear to 0.84, rational to 1.2; x^2 term, ev = 0.12 | x^2 folds when f1 + f2 > 12 kHz; clip corner only above the knee | alias line = ev*A/2 at 24k - 2f for f in (6, 12) kHz | §2.1 |
| Tape (wet, 48 kHz): emphasis +6 dB at 2 kHz, drive 3.65x, bias, clamp +-1 cubic x - x^3/3, exact de-emphasis | cubic, C1 at the clamp (2nd derivative jumps by 2) | cubic alone can't alias into [0, B] while B < fs/4 = 12 kHz; the clamp's harmonics fall as ~k^-3 | h3 = A^2/12 below the clamp; above it 5th, 7th, ... (table below) | §2.2 |
| Breath: u - a2 u^2 - a3 u^3, knee beyond +-2 | 3rd order | alias-free into band for B < 12 kHz; knee only on onsets | none expected | not measured (its noise generator dominates a sine test); analytic |
| Clock: S&H at 28-32 kHz on the 48 kHz grid, no filters | zero-order hold, ticks every 1 or 2 samples | images at k fclk +- f, jittered by the grid | intended | §2.3 |
| Grain: level-relative truncation | quantiser | non-harmonic | intended | not measured |
| Band: `max(v*m, -1)/m`, m = 2.7-4.2, then a 2-pole band-pass | one-sided hard clamp (C0 slope step) | harmonics ~k^-2 | folds, then the band-pass removes most of the top | §2.2 |
| Transport: Catmull-Rom read, 48 kHz | cubic interpolation | droop, images when the delay moves | negligible | droop at fr = 0.5: -0.04 dB at 5 kHz, -0.23 at 8, -0.53 at 10, -1.07 at 12 kHz [M, arithmetic] |
| Shimmer: grains read at 2x speed, linear interpolation at a fixed fraction per grain | decimation by 2 | folds above 12 kHz | the 8th-order Butterworth at 6 kHz before the shift is -48 dB at 12 kHz | by design fine (fixed-fraction 2-tap low-pass: -0.7 dB at 6 kHz at worst) |

Harmonics of the tape's clip for a sine of peak A at the clip input (u = drive * emphasised wet + bias), dB re the fundamental [M, arithmetic on
the shipped curve]:

| A | h3 | h5 | h7 | h9 | h11 | h13 |
|---|---|---|---|---|---|---|
| 0.5 | -33.1 | none | | | | |
| 1.0 | -19.1 | none | | | | |
| 1.5 | -13.1 | -26.3 | -52.8 | -44.4 | -80.7 | -54.5 |
| 2.0 | -11.4 | -20.0 | -30.9 | none | -43.8 | -48.3 |
| 3.65 (drive at WEAR 1, no emphasis, a full-scale wet) | -10.1 | -15.6 | -20.3 | -24.9 | -30.2 | -36.8 |

A wet peaking -12 dBFS (0.25) reaches u = 0.9 at low frequencies and 1.8 above 2 kHz (emphasis x2): so at WEAR 1 the clamp is engaged on the
high-frequency peaks of any wet louder than about -12 dBFS. For a 7 kHz tone the 3rd (21 kHz) does not fold; the 5th (35 -> 13 kHz), 7th (49 -> 1 kHz,
the one `test_era` looks for), 9th (63 -> 15), 11th (77 -> 19) and 13th (91 -> 5 kHz) do. At WEAR 0.5 (drive 1.9x, emphasis 1.41x) the clamp needs a wet
peak of about -8.5 dBFS at high frequencies, so most of the knob is polynomial-only and alias-free in band.

### 2.1 The loop [M]

`loop_alias.py` drives the real `LoopEra` (probe.cpp) at 24 kHz.
- Sine, even = 0.12: no alias for f below 6 kHz; for f = 7.1, 9.1, 10.3 kHz the alias is **20 log10(ev*A/2)** re the line: -50 / -44 / -38 / -32 / -26 dB for A = 0.05 / 0.1 / 0.2 / 0.4 / 0.8; the same code at 8x: none. With even = 0 the soft clip alone is clean below its knee (0.84) and -34 dB at A = 1.0 (the corner).
- Noise (white to 10.5 kHz, tank-internal RMS sigma), one pass: sigma 0.03 / 0.1 / 0.2 / 0.4: white -53 / -43 / -37 / -29 dB; -3 dB/oct -64 / -54 / -48 / -37; -6 dB/oct -90 / -80 / -73 / -59 dB. A held tail's peaks sit under the knee (crest 4-5, `EraVoicing.h`), so sigma is about 0.1-0.2.
- Reading: negligible per pass on anything tail-like; only a hot tank with a flat, bright spectrum folds audibly. HALL and MASSIVE only (PLATE's loop is at 48 kHz).

### 2.2 The wear's tape and band on real wets [M]

Method: real wets rendered from the pinned main renderer (`ref-main/build/iv_render`, ERA 0, MIX 1, DECAY 0.5, SIZE 0.5, PLATE / HALL / MASSIVE) from the
repo's synthetic stimuli (`test_audio/stimulus`: 04_skank = chords, 02_hits = drum-like bursts, 06_noise_bursts), the 2^18-sample window with most energy,
scaled to wet RMS -30 / -24 / -18 / -12 dBFS (the repo's own estimate of a tail: -30 to -15). The shaper is isolated (memoryless) with the shipped
constants (`memoryless_alias.py`); alias = naive output minus the same shaper at 8x brick-wall decimated, both through the stage's own linear tail
(the exact de-emphasis; for the band a gentle band-pass), dB re the ideal output. Cross-check: the real `EraTexture` (probe.cpp) agrees with the model
to within 1 dB wherever alias is above -35 dB (white noise to 10.5 kHz at -24 / -18 / -12 dBFS: probe -32.5 / -23.4 / -17.2, model -34.8 / -23.8 / -17.2);
below that the probe's linear-mismatch floor takes over, which is why the memoryless numbers are the ones quoted.

Tape at WEAR 1 (t = 1.0), alias dB re output at wet RMS -30 / -24 / -18 / -12 dBFS:

| Wet | -30 | -24 | -18 | -12 |
|---|---|---|---|---|
| skank PLATE | -105 | -92 | -80 | -71 |
| skank HALL | -130 | -115 | -89 | -73 |
| skank MASSIVE | -125 | -104 | -82 | -67 |
| hits PLATE | -34 | -26 | -22 | -19 |
| hits HALL | -43 | -32 | -26 | -23 |
| hits MASSIVE | -50 | -36 | -27 | -20 |
| noise bursts PLATE | -35 | -27 | -22 | -19 |
| noise bursts HALL | -47 | -34 | -27 | -23 |
| noise bursts MASSIVE | -57 | -42 | -31 | -23 |

(Run on a different 2^17 window the HALL hits read -49 / -37 / -28 / -23, so the cells are good to a few dB.) At WEAR 0.5 every cell is 4-15 dB lower, the most at low levels
(hits HALL -58 / -44 / -34 / -27). Synthetic spectra, same shaper: white to 10.5 kHz at -24 dBFS -35 dB; -3 dB/oct -53 dB; -6 dB/oct -94 dB; so the figure
is a steep function of how much 4-11 kHz energy the wet carries, as the harmonic table predicts.

The band's clamp (WEAR 1), after its band-pass, dB re the band's output at -24 / -18 / -12 dBFS: hits HALL -52 / -44 / -41, hits PLATE -43 / -38 / -36,
noise bursts MASSIVE -63 / -49 / -44; chords: no clipping below -24 dBFS (error exactly zero), -75 to -66 dB at -18 / -12. Inaudible.

### 2.3 The clock [M]

The unfiltered S&H alone (t = 0, 32 kHz; t = 1, 28 kHz), a -20 dBFS sine, non-harmonic energy re the fundamental: 1.1 kHz -23 dB, 2.1 kHz -18 dB,
3.1 kHz -14 dB, 5.3 kHz -9 dB. That is the "fizz" the owner picked (wear_fizz F); Holters & Parker's model would have the input and output filters
remove most of it. Nothing to change; recorded because the era-rethink note called it "unfiltered clock images turn metallic below ~16 kHz".

## 3. ADAA, polynomial shapers and oversampling: what the literature says and what it costs here

**The literature in four lines.** (1) ADAA1 = continuous-time convolution of the nonlinearity of the linearly interpolated input with a one-sample
rectangle; output y_n = (F(x_n) - F(x_{n-1}))/(x_n - x_{n-1}) (Parker et al. §2); it adds half a sample of delay and a low-pass (a zero at Nyquist, -1.25 dB
at 8 kHz, -3 dB at 12 kHz), and needs a fallback to f((x_n + x_{n-1})/2) when the inputs are close (Parker §4; Holters §2; Chowdhury notebook). (2) Its
zeros sit at multiples of fs, so it removes aliases that fold to low frequencies and is "less efficient in the high-frequency range"; for a 1/f spectrum 2x
oversampling alone gives only -6 dB, but ADAA with 2x or 4x is "practically a non-issue even for a hard clipper" (Vicanek §4). (3) Parker's Table 1 (hard
clipper, input gain 10): same alias level at 12x with no method, 4x with ADAA1, 3x with ADAA2 (the triangular kernel). (4) Esqueda et al.'s polyBLAMP
corrections buy +12 / +20 dB at 2 / 4 points but need the fractional clip time and slope, and fail on dense material (EUSIPCO 2015, conclusions).

**Polynomial (band-limited) shapers.** A shaper that is an odd polynomial of order N makes harmonics only up to N*f. Fed a signal band-limited to B at
rate fs it can alias into [0, B] only if N*B > fs - B, i.e. B > fs/(N+1): a **cubic is alias-free in band while B < fs/4 = 12 kHz (48 kHz) or 6 kHz (24 kHz)**,
the x^2 of the Neve while B < fs/3 (16 kHz at 48, 8 kHz at 24). [E, arithmetic.] So everything the stages do inside the polynomial range is already clean at
48 kHz for a wet limited to the tank's 10.5 kHz; the aliasing in §2.2 is the **clamp** (a non-polynomial corner) engaged by the drive. A smoother joint
does not fix it: a quintic with a C2 joint (p = u - 2u^3/3 + u^5/5, same slope at 0) read -1 to -3 dB better in alias on the same wets and has a stronger 3rd
harmonic (-27 vs -33 dB at A = 0.5) - not an improvement. The alias is a property of overdriving a flat-topping curve, not of the corner's smoothness.

**Costs on the Cortex-M7** [E]. Each variant compiled alone for the firmware's target (`arm-none-eabi-g++ -O3 -mcpu=cortex-m7 -mfpu=fpv5-d16 -ffp-contract=off`;
`tools/review2/cost/count.sh`), the per-sample loop counted for both channels together; cycles at the project's measured 1.35 cycles/instruction (round 15,
the BBD stage; 1.7 for the oversampler's memory-heavy variant), points = % of 10,000 cycles (480 MHz / 48 kHz). In the real Engine the compiler's register
allocation will differ a little; flash numbers are the functions' sizes in isolation.

| Variant (tape stage unless named) | Instr. per stereo sample | Extra over shipped | Points (extra) | Flash (extra) | Alias effect on §2.2's wets |
|---|---|---|---|---|---|
| Shipped | 58 | | (0.78 total) | 356 B | |
| ADAA1, divide-free polynomial form (a+b)/2 * (1 - (a^2+b^2)/6) on the clamped inputs | 68 | +10 | +0.14 | +76 B | -7 to +6 dB (3-6 dB better at -18 / -12 dBFS, worse at -30); the clamp's corner stays; adds half a sample and a -1.25 dB-at-8 kHz low-pass |
| ADAA1 exact (piecewise F1, one divide a channel, fallback) | 88 | +30, 2 VDIV (~14 cycles each, partly hidden) | +0.6 to +0.7 | +236 B | -7 to +4 dB (3-4 dB better only at the loud end) |
| 2x oversampled with the Core's halfbands (emphasis and clip at 2x) | 165 | +107 | +1.45 (+1.8 at 1.7 c/i) | +388 B | **14-50 dB lower** (25-33 at -24 dBFS, 19-28 at -18) |
| Band clamp as shipped | 12 | | | 58 B | |
| Band clamp, soft knee (compander's) | 31 | +19 on the engaged path only | ~+0.05 | +68 B | -7 to -14 dB (already inaudible) |
| Band clamp, ADAA1 exact | 66 | +54 (common path ~+20) | +0.3 | +242 B | none |
| Loop saturate(), per line-sample at 24 kHz (shipped) | 27 | | | 196 B | (no change needed) |

Divide-free ADAA1 works because the cubic's antiderivative F1 = x^2/2 - x^4/12 is a polynomial, so the divided difference reduces to (a+b)/2 * (1 -
(a^2+b^2)/6): no divide, no fallback, no branch. It is exact only while both samples are inside the clamp; across the clamp it is an approximation, and
the alias comes from the clamp, which is why it gains little.

The release has 76 B spare (112,564 of 112,640): the cheapest honest option costs about as much as the whole margin, and 2x costs five times it.

## 4. BBD and compander against our breath and clock

- **Compander structure** [L, Raffel §3]: compressor 2:1 with its rectifier on its own output (feedback), expander 1:2 on the BBD's output (feedforward), a full-wave
  rectifier into a one-pole, tau = 10 kOhm * C_RECT. Our `breath()` is that: `y = x*gComp` with `envY` on `|y|`, `out = z*gExp` with `envZ` on `|z|`, gains from
  the envelopes once a block and ramped. The NE570 data sheet adds the real chip's weakness: with a fast tau the gain follows the rectified ripple at 2f and
  puts a **third** harmonic on low notes; ours is a polynomial (2nd and 3rd) on a smooth gain. Owner-visible effect of adding ripple: a little more 3rd on
  bass notes. Not recommended (v1.0.49 halved a2, a3 on the owner's pick).
- **Time constants** [L]: one RC for both halves on a real chip; ours 10 ms (compressor) and 12 -> 32 ms (expander, with WEAR). A longer expander tau than compressor
  tau makes the level overshoot after a transient (more breathing). A choice, not an error.
- **Harmonic distortion** [L, Raffel §4.2]: THD ~ 1.01^(N/1024) - 1 (about 1 % per 1024 stages), nearly level-independent (because the compander holds the level
  constant), harmonics falling off linearly in magnitude. Ours at WEAR 1: a2 = 0.07, a3 = 0.035 on u of about 0.5-0.7 peak: 2nd about 1.8-2.5 % (a 2000-2800-stage
  chip), 3rd about 0.2-0.4 % (about a sixth of the 2nd; Raffel's measured fall-off would put it nearer two thirds). The level-independence is right (the compressor does it).
  The 3rd is lower than a real chip's; the owner picked it lower (v1.0.49, wear_saturation E).
- **Aliasing** [L, Holters & Parker]: a BBD's aliasing is the input fold at the clock's Nyquist and the output hold's images, both normally filtered. Our `clock` stage does the
  hold without either filter and on the 48 kHz grid (§2.3). Their remedy for unwanted extra aliasing is oversampling; we want it.

## 5. Tape against ours

- Chowdhury's chain (record head, hysteresis, play-head loss, bias, wow and flutter) needs 16x because of a 55 kHz bias and the hysteresis solver; his tape sound
  is "warm, tape-like distortion" with the loss filters and head bump doing the rest (§4.2). We have a memoryless clip between pre- and de-emphasis (Strymon's and
  Chowdhury's structure per `era-rethink.md`) and no head bump, on purpose (no stage low-passes or boosts the wet; TONE owns that). Nothing in the paper argues for more.
- Hysteresis was tried and dropped on the owner's ears (`tape-out.md`); ChowTape is GPL-3.0 (cite only).
- The clamp-engaged regime is where tape sounds like tape (flat-topped, compressed highs); the alias in §2.2 is the price of running that at 48 kHz without oversampling.

## 6. AM radio against ours

- [L, Fallows] A fading carrier 10-20 dB under the sidebands makes an envelope detector over-modulate: the troughs clip. Our band's `max(v*m, -1)` is exactly a trough
  clip, with m = (0.9 + 0.5t) * 3 over a -12 dBFS carrier. Right picture.
- [L, repo] selective fading as a two-path comb with 0.1-1.5 Hz spread (ITU-R F.1487 / Watterson): our one wandering notch at 0.1-0.5 Hz, 4-18 dB deep, is the cheap
  version, already decided (`radio-artifacts.md`, `era-rethink.md`).
- Aliasing of the clamp: §2.2, inaudible. Nothing to add from the literature within the single-knob design.

## 7. Wow, flutter and modulation: real machines against ours

Conversions: 1 cent = 0.0578 % speed; 3 c = 0.173 %, 4.5 c = 0.26 %, 12 c = 0.696 %, 20 c = 1.16 %, 28 c = 1.63 % (peak); a sine's RMS = peak / 1.414.
Weighted readings (IEC 386 / DIN 45507 / AES6) multiply by a factor that peaks at 4 Hz (1.0) and is 0.5 at 0.8 Hz [L, Virtins]; wow is 0.5-6 Hz, flutter 6-100 Hz,
scrape flutter above 100 Hz [L, Virtins; Lindos].

| What | Rate | Depth (ours) | As a W&F reading | Real machines [L, Lindos] |
|---|---|---|---|---|
| Transport wow (MOTION left, full) | 0.5-1.2 Hz, random + a wandering sine | +-12 c (0.69 % peak); with variance +-20 c (1.16 %), a swing 1.5x now and then | weighted peak ~0.35 % (0.58 % with variance) at 0.8 Hz | pro 0.03 %, best cassette 0.08 %, average cassette or car >= 0.2 %: ours is **1.7x (3x with variance) an average cassette**, 12-20x a pro machine |
| Transport flutter | 6-10 Hz, two sines | +-3 c (0.17 %); +-4.5 c with variance (0.26 %) | ~0.17-0.26 % (weight near 1) | about an average cassette's total reading |
| Crinkle bursts | ~45 Hz jitter, 0.1-0.6 s, 0.6 /s | up to +-12 c; scrape bed +-10 c at 100-200 Hz | scrape flutter, which the weighting suppresses | exaggerated on purpose (Strymon Magneto, Kinotone Ribbons as references by the owner) |
| Tank line wow (MOTION left, 1/3 share) | 0.2-1 Hz | 15 c x 0.33 = 5 c per line | below the transport's | |
| Tank Wander (MOTION right) | 1 -> 2.25 Hz x 1.2 at the end, rates fanned +-50 % | 20 -> 28 c per line | each line independent | Dattorro 1997: +-8 samples at 1 Hz at 29.8 kHz = **2.9 c**; Clouds / Rings ~5 c [`drift-rethink.md`] |
| Tank floor (always) | random, ~1.7 s steps | 0.46 ms swing: ~0.75 c at 0.15 Hz [E] | | |

Reading: (1) **the transport is a worn consumer cassette, not a good one**: depths 1.7-3x an average cassette's (12-20x a pro deck's), at rates (0.5-1.2 Hz) where the ear is
half as sensitive as at 4 Hz, so cents for cents it is less obvious than a 4 Hz wobble; consistent with the owner's "make the ends stronger to be noticeable in a mix".
(2) Raising the rate toward 2-4 Hz would make the same depth read louder (the weighting says so) - the drift note's "rate before depth" - at no CPU cost; a sound change for the owner's ears,
not recommended unprompted. (3) The Wander depth is 7-10x the classic reverb figures per line; the tail's measured p95 pitch (17-27 c on a held 1 kHz) is what the owner heard and picked.
(4) **Smear against chorus**: Lindos's mechanism (a steady tone beating with its own slightly different-pitched earlier echo, heard as amplitude wobble in a reverberant room) is why
modulation *inside* the tank sounds like chorus (every component beats with copies of itself at a slightly different pitch) and modulation *after* it (the transport: every
component moves by the same ratio at once) sounds like vibrato or tape speed, with no beating. That is the structure the left and right halves of MOTION already have, which the literature
supports. Interpolation is not a source of trouble: the transport's Catmull-Rom droops -0.5 dB at 10 kHz at worst (table in §2) and the tank uses allpass interpolation.

## 8. Options

| # | Option | Sound change | CPU (points of 10,000 cycles) | Flash | Risk | Needs |
|---|---|---|---|---|---|---|
| 0 | Leave aliasing as is (**recommended**) | none | 0 | 0 | none | nothing |
| 1 | Tape at 2x with the Core's halfbands, only while the tape stage is in | audible only on bright broadband transients at WEAR > 0.8: alias 14-50 dB lower; the tape's own harmonics unchanged | +1.45 to +1.8 (while WEAR is up) | +~390 B (the release has 76) | needs an offset elsewhere in flash; latency 2.5 samples on the wet (a level-hold input to move too) | owner's ears first (a hook-only prototype `IV_TAPE_OS`), then the module |
| 2 | ADAA1 divide-free on the tape's cubic | alias 3-6 dB better at loud levels, 7 dB worse at -30 dBFS; a -1.25 dB low-pass at 8 kHz that would need Vicanek's one-pole pair to cancel (+2 one-poles) | +0.14 (+0.3 with the pair) | +76 B (+ ~60) | the flash margin | not recommended: tiny gain for a tone change |
| 3 | ADAA1 exact (divide, fallback) | none | +0.6 to +0.7 | +236 B | divides | not recommended (no gain measured) |
| 4 | Band clamp soft knee | rounds the troughs: alias -7 to -14 dB lower (already -41 to -52) | ~+0.05 | +~70 B | flash | not recommended |
| 5 | Run the whole wear chain at 24 kHz for CPU (era-rethink's idea) | alias +7 to +19 dB on bright transients; high-frequency content of the chain (band, clock, grain) changes | -3.4 (their estimate) | ? | sound | only by ear, and only with the alias cost known |
| 6 | Transport wow rate toward 2-4 Hz at the same cents | the same depth reads stronger; less "seasick" per cent | 0 | 0 | the owner's picks | ears (render a page) |

None of these is a bug fix; options 1 and 6 are the only ones that would be heard.

## 9. What I could not check

- **No hearing.** Every alias figure is a level; whether -26 dB of fold-back on a loud hat at WEAR 1 reads as "fizz" or as part of the tape sound is the owner's.
  Kahles et al. put the audible threshold of alias on sustained tones at NMR -10 dB; transients and dense wets mask more; I did not run a masking model.
- **The stimuli are synthetic** (the repo's `test_audio/stimulus`), not the owner's music; the owner's reference recordings were not used. Real drum bus material will sit between the
  skank (nothing) and the bursts (most). The wet levels (-30 to -12 dBFS RMS) are the repo's estimate; I did not measure the wet's level inside the Engine at MIX 1.
- **The loop's alias per pass only**, not accumulated over a tail's thousands of passes (bounded by damping, ADR 0008; not simulated).
- **The breath stage's alias** (analytic only: third order, alias-free in band) and the grain's.
- **The module**: operation counts are the isolated functions' loops; cycles use the project's 1.35-1.7 cycles per instruction; no hardware run.
- The ADAA numbers rest on a coherence-based measure (a linear difference such as the half-sample low-pass is removed); a time-domain comparison against the delayed ideal might
  read a few dB differently, but not enough to turn "no gain" into "worthwhile".

## 10. Reproduce

`probe.cpp` (drives the Core's real `EraTexture` and `LoopEra` on float32 files; build `clang++ -O2 -std=c++17 -I core tools/review2/probe.cpp`),
`alias_probe.py` (sine and noise, 1x against 8x), `loop_alias.py`, `memoryless_alias.py`, `shaper_options.py`, `band_options.py`, `real_wet_alias.py`,
`cost/tape_variants.cpp` + `cost/count.sh`. Python needs numpy, scipy, soundfile. Wets: `build/iv_render in.wav out.wav --set mix=1 --set era=0 --set space=HALL --set decay=0.5 --set size=0.5`.
