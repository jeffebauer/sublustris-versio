# review2 / litfdn: FDN, plate, colouration and classic hardware against our design

*28 Oct 2026. Branch `review2/litfdn` (worktree `.claude/worktrees/r2-litfdn`). Read-only on the repo apart from this file and `tools/review2/litfdn_*.py`. Renders came from the pinned reference renderer of main (`.claude/worktrees/ref-main/build/iv_render`, run read-only; renders went to a scratch directory and were deleted after each measurement). Tags: **[measured]** by a script in `tools/review2/`; **[derived]** arithmetic from the code's constants; **[literature]** read at the source (URL and section given); **[unverified]** from memory or a search snippet only.*

This builds on `docs/research/dsp-audit.md` (22-24 Oct) and `review-2026-10-26.md` section 6 and does not redo them. Most of what those found still stands; the points below are new, corrected, or re-sourced.

---

## 0. Findings, ranked by value

| # | Finding | Evidence | Sound change | CPU | Flash | Needs |
|---|---|---|---|---|---|---|
| 1 | **No DC or sub-sonic filter in front of or inside the tanks, and the tanks amplify what gets in.** The loop's DC gain is 1 at infinite DECAY and >1 at finite: a constant 10 mV (-40 dBFS) input offset becomes a wet DC of 35 mV in PLATE at DECAY 0.5 (x3.5), x1.9 in HALL, and at infinite DECAY x10 (PLATE), x12 (HALL), x-8 (MASSIVE); at WEAR 0 the infinite wet is then a pure DC offset at the governor's floor (-18 to -21 dBFS RMS = 0.09-0.13) that *stays after the input stops*. From WEAR ~0.05 up the wear stage's own 5 Hz blocker hides the DC at the output, **but it is still in the loop**: it still counts as level in the governor and as "input present" in the quiet detector (`tankQuiet`, Engine.cpp:829, -70 dBFS = 0.3 mV), and it moves the audible tail of an infinite hold: at WEAR noon a 1 mV DC (-60 dBFS) shifts the AC tail by +3.6 to +4.9 dB at 5-8 s, and 3-10 mV by -17 to -26 dB at 30 s in PLATE. On real programme (the skank loop, input mean -0.23 mV) an infinite PLATE tail drifts to -13 mV of DC, HALL -5 mV, in 10 s and keeps climbing; pink noise gives wanders of +-20 mV. A one-pole 5-10 Hz high-pass *in front of the tank* (outside the loop: no rule-2 risk, and no draining of the lows, which is what the code comment at Era.h:74 rightly avoids with an in-loop blocker) takes all of that to <0.3 mV and the AC-tail shift to 0 / -2 dB. The `DcBlocker` in `Filters.h` is dead code. No open design checked (Dattorro, DaisySP ReverbSc, MI Clouds/Rings) has one either; they have finite or AC-coupled loops, and we have an infinite hold. | [measured] `litfdn_measure.py dc`, `litfdn_dcfix.py`, `litfdn_dcimpact.py` | none-provable above ~30 Hz (a one-pole at 10 Hz is -0.26 dB at 40 Hz; at 5 Hz -0.07 dB) | ~0.1 % (2 channels x ~5 cycles) [estimated] | ~60-120 B [estimated; 76 B spare] | module question first: how much DC the Versio's input carries (is it AC-coupled?); desktop: a null test with the hook off |
| 2 | **In-loop allpasses lump their loss on the line; the literature has an exact cure, but the gain looks small.** The loss is set for (line + allpass) seconds (Fdn.cpp:541), so the *mean* is right, but a mode at an allpass group-delay peak meets the loss fewer times a second (single-line bound: loop time 0.88-1.41 x the mean in a small HALL, 0.91-1.26 x in MASSIVE). Blesser warns of exactly this (Dattorro 1997 p. 665); Schlecht 2021 SV.A and Jot and Chaigne give the cure: loss proportional to the delay each mode meets, i.e. scale each allpass's delay output by gamma^M inside it (z -> z/gamma). In a stand-alone model of the code's topology the energy decay curve is straight either way (late/early 0.93-1.01, lumped and cured alike); the renderer's own interrupted-noise decays are straight too (late/early 0.95-1.05 from 500 Hz up in every SPACE, section 4.4) and the cure only stops the tail's spectral roughness creeping up (+0.65 dB HALL-like, +0.76 dB MASSIVE-like std from 2 s to 21 s, flat with the cure). | [derived] `litfdn_theory.py` s3; [measured, model] `litfdn_apdecay.py`; [literature] | tiny (late tail) | +16 multiplies per 24 kHz tank sample + 16 `exp` per control block: ~0.3-0.4 % [estimated] | ~100-200 B [estimated] | drop (no measurable effect in the renderer) |
| 3 | **PLATE's echo density builds slowly at large SIZE; the literature says a plate is dense at once.** Normalized echo density (Abel and Huang) first reaches 0.8 at 0.12 / 0.27 / 0.42 s for PLATE SIZE 0 / 0.5 / 1 (dips below 0.8 until 0.12 / 0.30 / 0.58 s), against 14-36 ms in HALL. Costello (EMT 140: "instant onset of high echo density"), Griesinger 1989 ("very rapid build-up of impulse density" for plates), Valimaki 2012 (plate "very quickly dense", whip-like onset), Lexicon 480L manual (SHAPE at minimum = small chamber and plate). Dattorro's own topology does this (his plate is not dense at once either), so ours *matches the paper* and departs from the hardware. | [measured] `litfdn_measure.py ned`; [literature] | audible (needs the owner) | an onset cloud of ~100 sparse taps/s per side: 1-3 % [estimated], or more input-diffuser stages: ~0.5 % | 100-300 B | owner's ears; it changes the plate's character (the owner calls it "excellent") |
| 4 | **Late tails are uncorrelated in every SPACE, as Griesinger asks; the first 50 ms are not.** L/R correlation 0.5-2 s: |rho| <= 0.07 in all octave bands (one 0.23 at 250 Hz, MASSIVE SIZE 0.8). Mono sum power 0.0 to +0.2 dB against L+R (no cancellation). The first 50 ms of HALL at SIZE 0.8: rho -0.5 at 125 Hz, mono sum -4.7 dB below 300 Hz. Cause: while a single line's first echo is the only thing sounding, L and R carry it with the product of two Walsh signs (+-1): fully in phase or in antiphase line by line. | [measured] `litfdn_measure.py corr` | tiny (first 50 ms, bass, in mono) | 0 if the early taps' signs are changed | 0 | ears optional |
| 5 | **Our Hadamard matrix and sign vectors are on the good side of the colouration literature.** Frequency-response roughness (std of dB, Rayleigh ideal 5.57): code 5.77, random orthogonal 6.00, Householder 7.12, Hadamard with b = c = ones 6.31. 24 random +-1 b/c pairs: 5.83 mean, 42 % smoother than the code's rows; the 99th-percentile resonance 0.75 dB lower on average. Nothing to gain from re-picking the signs. | [measured] `litfdn_excitation.py` | none | none | none | none; closes the question |
| 6 | **PLATE's highs decay far longer than an EMT 140's.** T60 at 10 kHz is 2.65 s for a 4 s plate at TONE noon (1.6 s at 2 s), 3.97 s at 8 s; Costello: an EMT 140 is under 1 s at 10 kHz at every damper setting, and a real room never beats ~1.25 s there. Ours is the owner's calibration (air 8 dB/s, v1.0.25, against Valhalla Room's measured 3-7 dB/s) so it is a choice, but it is the biggest character gap to the hardware the PLATE is named for. TONE CCW already gets there. | [derived] `litfdn_theory.py` s2; [literature] Costello plate post | audible | 0 | 0 | owner's ears (a PLATE-only `air`) |
| 7 | **The DECAY loudness make-up (v1.0.28) is Jot's power law.** Jot 1997 S2.4: the loop's power gain is k/(1-k) with k = mean loop gain, which for small loss is proportional to T60/(mean line length): wet *power* grows as T60 (amplitude as T60^0.5) and falls as 1/length (the SIZE correction is sqrt(length): matches). The owner-tuned exponents are 0.47 (HALL) and 0.445 (MASSIVE), against the theory's 0.5; PLATE's 0.18 is lower because Dattorro's output taps sample the loop early. | [literature] + [derived] | none (a cross-check, not a change) | | | |
| 8 | **Modal density against the four published yardsticks.** Small HALL (SIZE 0, 0.43 s of loop incl. allpasses) supports T60 <= 1.7 s (Jot), 2.9 s (Smith/Schroeder-Logan), ~5 s (Dal Santo's >6000 modes): DECAY noon there is 10 s. PLATE SIZE 0 (0.36 s): 1.5 / 2.4 / 4.2 s against 10 s. MASSIVE and big HALL are far inside. Already known (audit F2, P4) and handled by modulation (Karjalainen and Jarvelainen via Schlecht thesis S6.1: time variation lets modal density drop; Lexicon RHall manual agrees) and by the owner's small-HALL pick; no new action. | [derived] `litfdn_theory.py` s1 | | | | |
| 9 | **Correction to the last review: "nothing states 'don't modulate the lows' as a rule" is wrong.** Schlecht's thesis S6.3.1 (p. 100): informal pre-tests found modulating the lows undesirable ("easily perceived as detuned", "unnatural sense of drifting"), so all listening items were split at ~400 Hz with a one-pole and only the upper part modulated. Our 300 Hz still-lows tank is a direct relative. | [literature] | none | | | |

Everything else checked came out as already known or fine: the allpass interpolator (Schlecht thesis S6.1: Lagrange reads change the in-loop magnitude response and so the T60; allpass reads do not), the late-tail echo density (NED 0.96-1.01 +- 0.04-0.08 from 0.5 s on in every SPACE and SIZE), the Hadamard choice, the delay-length spread.

---

## 1. Sources (all fetched and read unless marked)

| Source | URL | What was taken |
|---|---|---|
| Jot, "Efficient models for reverberation and distance rendering", ICMC 1997 (summarises Jot and Chaigne 1991) | https://freeverb3-vst.sourceforge.io/doc/10.1.1.48.8176.pdf | S2.1: absorptive filters, 20 log|g_i| = -60 tau_i / T_r(w); sum of delays >= T60/4 for modal overlap; S2.2: Hadamard needs N log N adds, minimal crest factor, Householder high crest; S2.4: power gain k/(1-k), correcting filter |c|^2 = E(w)(1/k - 1), biquad absorptive filters, 3 bands |
| Jot and Chaigne 1991, AES 90th Conv. | https://aes2.org/publications/elibrary-page/?id=5663 (paywalled; read through Costello's summary and Jot 1997) | one damping filter per line proportional to its length |
| Schlecht and Habets, "Accurate reverberation time control in FDNs", DAFx-17 | https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_11.pdf | S2.2 Eq. 6-7: errors in the attenuation filter propagate non-linearly (reciprocally) into T60; worst where the per-pass loss is small (long T60, long-ringing bands); JND 3-12 % |
| Prawda, Schlecht, Valimaki, "Improved reverberation time control for FDNs", DAFx-19 | https://www.dafx.de/paper-archive/2019/DAFx2019_paper_46.pdf | octave graphic EQ as attenuation filter, more accurate than earlier methods (abstract only skimmed) |
| Schlecht, "Allpass feedback delay networks", IEEE TSP 69, 2021 | https://arxiv.org/pdf/2007.07337 | S V.A: homogeneous decay (all modes alike) by delay-proportional absorption with a unilossless matrix (Jot and Chaigne), and its extension to allpass FDNs (series Schroeder, nested Gardner) |
| Schlecht, PhD thesis 2017 | https://theses.eurasip.org/wp-content/uploads/sebastian-jiro-schlecht-feedback-delay-networks-in-artificial-reverberation-and-reverberation-enhancement.pdf | S6.1 (delay modulation by Lagrange reads spoils the magnitude response; allpass reads do not; matrix modulation; Karjalainen and Jarvelainen: time variation allows lower modal density), S6.3.1 p. 100 (lows not modulated, 400 Hz one-pole split, 17 listeners, 8-FDN with modulation rated above 8-FDN without) |
| Dal Santo, Prawda, Schlecht, Valimaki, "Differentiable FDN for colorless reverberation", DAFx-23 | https://www.dafx.de/paper-archive/2023/DAFx23_paper_32.pdf | S2.4: colouration follows the width of the modal-excitation distribution; > 6000 modes needed for an IR to sound "rather colorless"; delays co-prime, log-spaced; avoid low-order dependencies |
| Dal Santo et al., "Efficient optimization of FDNs for smooth reverberation" (arXiv 2402.11216v2) | https://arxiv.org/html/2402.11216v2 | S5 and S7: as few as four lines suffice once matrix, b and c are optimised; Householder is a restricted parametrisation |
| Schlecht and Habets, "On lossless FDNs", TSP 2017 (arXiv 1606.07729); "Modal decomposition of FDNs", TSP 2019 (arXiv 1901.08865); "Scattering in FDNs", TASLP 2020 (arXiv 1912.08888) | https://arxiv.org/pdf/1606.07729, https://arxiv.org/pdf/1901.08865, https://arxiv.org/pdf/1912.08888 | losslessness classes; few modes carry much of the energy; filter matrices raise density (last review used the third) |
| Schlecht and Habets, "Feedback delay networks: echo density and mixing time", TASLP 25, 2017 | https://cris.fau.de/publications/212159278 (abstract only; thesis ch. 5 read) | echo density is a polynomial in time set by the delays; mixing time from mean delay |
| Abel and Huang, "A simple, robust measure of reverberation echo density", AES 121, 2006 | AES e-library (paywalled) | not read; the normalized echo density (share of samples more than one std from the mean, over 0.3173) as described in Schlecht's thesis and in the SDN paper (arXiv 1502.05751) **[unverified at the source]** |
| Dattorro, "Effect Design Part 1", JAES 45(9), 1997 | https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf | S1.3.5 first-order filters limit-cycle only at dc; S1.3.6 mono-summed input, "synthetic stereo image" from the output taps; S1.3.7 modulation (excursion 8 samples at 29.8 kHz, ~1 Hz, ideally on every tank delay, quadrature pair, piano vibrato); **Appendix 1 (p. 665), Blesser's letter**: Schroeder's 3 eigentones/Hz, EMT 250 at 32 kHz with 8K words emulating the eigentone density "entirely by randomizing delay lines", an allpass inside a larger loop "must be used with great care" (group-delay ripple, "very coloured") |
| Griesinger (Lexicon), "Practical processors and programs for digital reverberation", AES 7th Int. Conf., 1989 | https://freeverb3-vst.sourceforge.io/doc/10.1.1.72.5367.pdf | tests: echo count within 20 dB per 20 ms, energy vs time, autocorrelation after exponential gain, sonogram, T60 of each mode vs frequency, steady-state response; 1000 echoes/s (Schroeder) up to 10,000/s for percussion, higher at high bandwidth; first 15 dB of decay sets the perceived reverb time; "very important" that the two outputs be uncorrelated; chambers and plates: "very rapid build-up of impulse density, then smooth exponential decay"; modal density ~ total memory (100 ms = 10 Hz spacing); halls need 0.5 s memory, up to 2 s |
| Lexicon 480L Owner's Manual (Banks 1-4 philosophy; Banks 11-12 Random Hall) | https://freeverb3-vst.sourceforge.io/doc/Lexicon/lexicon_480l.pdf | SHAPE at minimum: fast build-up, "characteristic of small reverberation chambers and plates"; "the early reflection myth"; BASS MULTIPLY: ideal hall 1.2, physical spaces rarely > 1.5, good recording rooms <= 1.0, crossover 400-500 Hz; RT HF CUT = air absorption; RHall: random delay elements "reduce long-lived modes", less metallic, apparent T60 close to the displayed one, steeper treble-decay filter |
| EMT 250: Blesser and Bader, US 4,181,820, "Electric reverberation apparatus" | https://patents.google.com/patent/US4181820A/en | short all-pass loops plus long loops with taps; tap delays, and their sums, differences and quotients, "non-harmonic"; tap amplitudes follow an e-function; delayed *unreverberated* input fed to the output; random length variation of the loops; frequency-dependent feedback in the long loops; each output "randomly damped" for naturalness |
| US 4,955,057 (the number in the brief) | https://patents.google.com/patent/US4955057A/en | is Dynavector's "Reverb generator" (Tominari, phase-shifting circuits in a feedback path), not a Lexicon patent; not relevant |
| Costello, "The Physics and Psychophysics of Plates" | https://valhalladsp.com/?p=74060 | EMT 140: instant onset, instant echo density, dispersion ("PEW", tuned: matters for stereo imaging, LF onset L/R difference > 10 ms, HF < 1 ms), decay reference 2-3.5 kHz, < 1 s at 10 kHz at every setting, physical room < ~1.25 s at 10 kHz, "not necessarily metallic" |
| Costello, "Getting started... part 2"; "Modulation in reverbs, reality and unreality" (2009) | https://valhalladsp.com/2021/09/22/getting-started-with-reverb-design-part-2-the-foundations/ , https://valhalladsp.com/2009/07/30/modulation-in-reverbs-reality-and-unreality/ | Jot and Chaigne's same-decay-for-every-line rule; the EMT 250 "used an enormous amount of modulation, to the point where it sounded like a chorus"; the old units' depth "goes far beyond what can be heard in any real world space" |
| Bilbao, "Electromechanical effects" (DAFX 2nd ed. chapter) | https://ccrma.stanford.edu/~bilbao/dafx10/electro.pdf | S1.2: plate dispersion w = kappa beta^2, v_group = 2 sqrt(kappa w): highs outrun lows, the pulse "rapidly loses coherence"; mode density constant in a plate, rising as f^2 in a room |
| Valimaki, Parker, Savioja, Smith, Abel, "Fifty years of artificial reverberation", TASLP 20(5), 2012 | https://www.academia.edu/82609908 (copy) | S II.A: EMT 140 "very quickly dense with arrivals", "whip-like onset"; S VI.3: "plate" in digital units means no early reflections and a fast onset of a diffuse tail; Greenblatt et al.: EMT 140 emulation as a short measured onset + FDN, onset independent of the damper |
| Smith, PASP: Desired Qualities, Perception of Echo Density and Mode Density, Mode Density Requirement | https://ccrma.stanford.edu/~jos/pasp/Mode_Density_Requirement.html and neighbours | Schroeder and Logan: 0.15 modes/Hz per second of T60; echo density 1000/s, 10,000/s for impulsive sound |
| Alary, Politis, Valimaki, "Velvet-noise decorrelator", DAFx-17 | https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_96.pdf | skimmed (decorrelation by sparse FIR); not needed for any finding |
| DaisySP-LGPL ReverbSc (structure only, LGPL), MI Clouds and Rings `reverb.h` (MIT) | raw.githubusercontent.com/electro-smith/DaisySP-LGPL ..., pichenettes/eurorack ... | grep for dc / high-pass: none in any of them |
| Eventide SP2016 Reverb User Guide | https://downloads.eventide.com/audio/manuals/plug-ins/SP2016+Reverb+User+Guide.pdf | fetched, marketing level only (Room, Stereo Room, Hi-Density Plate; vintage vs modern bit depth); nothing structural |
| Kuttruff, Room Acoustics (air absorption exponent) | search only, not read | the 1.25 s ceiling at 10 kHz is Costello's, not Kuttruff's; **[unverified]** against a text |
| Arcas and Chaigne, "On the quality of plate reverberation", Appl. Acoust. 71(2), 2010 | https://hal-ensta.archives-ouvertes.fr/hal-00626968 (record only) | not read (full text restricted) |
| Karjalainen and Jarvelainen, AES 111, 2001; Gerzon 1971-76; Stautner and Puckette 1982 | not fetched | cited through Schlecht's thesis, Dal Santo et al., and Costello's list **[unverified at the source]** |

---

## 2. What the literature says, and where our code stands

(Lines marked OK need no action.)

### 2.1 The FDN core
- **Unitary matrix, per-line delay-proportional loss** (Jot 1997 S2.1; Schlecht 2021 S V.A). OK: Fdn.cpp sets each line's loss from its own length *plus its allpass lengths* (line 541).
- **Hadamard** (Jot S2.2: equal-magnitude coefficients, N log N adds; Householder has a high crest factor at large N). OK, and measured (finding 5): Householder is the roughest of the matrices tried here.
- **Input / output vectors** matter as much as the matrix (Dal Santo 2023 S2.4; Schlecht and Habets 2019). The code feeds L and R with two Walsh rows and takes the outputs with two others. Measured: typical-to-good. OK.
- **Delay lengths**: distinct primes, log-spread, avoid low-order dependencies (Dal Santo S3.4). Ours are primes at the ratios 0.60-1.41 (linear, not log; the spread is 2.35:1). OK; the review's order-2 coincidence picker stays a free refinement.
- **Modal density**: sum of delays >= T60/4 (Jot), 0.15 T60 (Schroeder and Logan via Smith), > 6000 modes (Dal Santo). Table in section 4.1. Small HALL and small PLATE sit below the first two at long DECAY; the first mitigation in every source is time variation (we have it), the second more delay (in-loop allpasses count: the owner picked scale = mean/0.2 on `renders/small_hall`).
- **Mode T60 homogeneity**: Jot and Chaigne's rule is exact only when no delay element can be traversed a mode-dependent number of times. Our in-loop allpasses can (finding 2).

### 2.2 The loss filter
- Jot S2.4: a first-order or biquad absorptive filter per line; three bands with adjustable crossovers from a biquad. Schlecht and Habets 2017: what matters is the error *in T60*, which blows up where the per-pass loss is small. Ours is a first-order shelf with exact gains at DC and Nyquist (Governor.h, owner's pick on `renders/tone_shelf`), so the two ends are exact by construction and the middle follows the shelf. T60 against frequency in section 4.2.
- Lexicon 480L: BASS MULTIPLY 1.2 is "an ideal concert hall", <= 1.0 for good recording rooms. Ours is 1.0 (no extra bass decay), inside the norm. RT HF CUT "simulating air absorption": ours is `air` in dB/s.
- Jot's correcting filter c(w): the wet's own spectrum follows T60(w) (power). TONE's output tilt is an independent aesthetic control; no change.

### 2.3 Modulation
- Dattorro S1.3.7: ~8 samples at 29.8 kHz, ~1 Hz, ideally on every tank delay, a quadrature pair for the first two. Plate: OK (audit P3). Blesser: the EMT 250's eigentone density was emulated "entirely by randomizing delay lines". Lexicon RHall: random delays "reduce long-lived modes ... less metallic". Costello 2009: the old depths exceed anything physical; ours is deliberately small (+-1-2 cents). OK.
- Allpass reads: right (Schlecht thesis S6.1).
- Lows: the thesis supports an unmodulated low band (finding 9). OK.
- EMT 250 patent: "each output randomly damped": slow random *gain* on each line's output (a time-varying c) is the cheap relative of modulation, pole-neutral by construction (Schlecht thesis S6.2: modulating b or c is only FIR modulation). We have fixed +-1 outputs. Option 6 in section 5.

### 2.4 Echo density and the start of the tail
- Griesinger: >= 1000 echoes/s, up to 10,000 for percussion; the first 15 dB of the decay sets the perceived time; the density should keep building. HALL: NED 0.8 within 14-36 ms of the click (section 4.3): excellent. PLATE: slow at large SIZE (finding 3). MASSIVE: its swell hides the early part (its first 0.3 s is 40-100 dB under the tail's body).
- Lexicon 480L: SHAPE minimum = "builds very quickly to a maximum amplitude" = chamber and plate; the manual's "early reflection myth" backs our choice to have only 2-4 early taps per line.

### 2.5 Plates
- EMT 140: dispersion (highs first: Bilbao S1.2 Eq. 8-9), dense at once, T60 reference at 2-3.5 kHz, < 1 s at 10 kHz, mode density flat in frequency, L/R pickups at different distances (Costello's theory: LF onset difference > 10 ms, HF < 1 ms, gives depth).
- Dattorro's plate "in the style of Griesinger" has none of the dispersion, builds density over its first lap, and gets its stereo from tap positions on a mono-summed input (S1.3.6). Ours is true stereo in (ADR 0013) on the same taps, and the audit's P1 found the first 50 ms of a hard-left source lands on the right. Not re-measured.
- Dispersion in a few cheap stages (an allpass chirp) is the standard emulation (Valimaki 2012 on spring/plate; Abel et al.), but it is hundreds of stages for a real chirp: not within a 10,000-cycle budget alongside everything else. Costello himself found the "PEW" unimportant and the L/R time structure important.

### 2.6 DC and sub-sonics
- Dattorro S1.3.5: his first-order filters can only limit-cycle at dc; he has no DC blocker. DaisySP ReverbSc, MI Clouds and Rings: none (grep). Their inputs are AC-coupled by the codec and none has our loop gain of exactly 1 at DC with an infinite hold. So this is not "best practice violated", it is a gap that our infinite mode opens (finding 1).

---

## 3. Where ours departs, in one list

1. No sub-sonic filter; DC amplified x1.9-3.5 at finite DECAY, x10-12 at infinite; at WEAR >= 0.06 it is hidden at the output but still steers the governor and the quiet detector (finding 1).
2. In-loop allpasses lump their loss on the line (finding 2; small effect in a model).
3. PLATE onset density builds over 0.12-0.58 s (finding 3).
4. First 50 ms of HALL: L/R in or out of phase line by line (finding 4).
5. PLATE highs outlast an EMT 140's by 2.5-4x (finding 6).
6. Not departures but worth knowing: no Lexicon-style SHAPE / SPREAD build-up control beyond the swell (a new mode, out of scope); no dispersion (out of scope).

---

## 4. Measurements

(`tools/review2/`; the reference renderer is read-only; all at WEAR 0, ONSET NORMAL, DEPTH 0, BLEND 100 %.)

### 4.1 Modal density (`litfdn_theory.py`, [derived])
Sum of line delays and of the in-loop allpasses, in seconds (= modes per Hz); the longest T60 each criterion supports; the T60 DECAY gives at 0.4 / 0.6 / 0.8 (HALL's finite T60 is multiplied by 0.49^(1-SIZE)).

| space | SIZE | sum lines | sum + allpass | T60 max Jot (x4) | Smith (/0.15) | 6000-modes (/0.087, rough) | T60 at DECAY .4 / .6 / .8 |
|---|---|---|---|---|---|---|---|
| HALL | 0 | 0.273 | 0.430 | 1.7 | 2.9 | 4.9 | 3.4 / 10.0 / 29.4 |
| HALL | 0.25 | 0.451 | 0.710 | 2.8 | 4.7 | 8.2 | 4.1 / 11.9 / 35.1 |
| HALL | 0.5 | 0.744 | 0.744 | 3.0 | 5.0 | 8.6 | 4.8 / 14.3 / 42.0 |
| HALL | 1 | 2.028 | 2.028 | 8.1 | 13.5 | 23.3 | 6.9 / 20.4 / 60 |
| MASSIVE | 0 | 2.77 | 3.67 | 14.7 | 24.5 | 42 | 15.9 / 43.6 / 120 |
| MASSIVE | 0.5 | 6.58 | 7.48 | 29.9 | 49.8 | 86 | 24.5 / 54.3 / 120 |
| MASSIVE | 1 | 15.6 | 16.5 | 66 | 110 | 190 | 37.9 / 67.5 / 120 |
| PLATE | 0 | 0.363 | | 1.5 | 2.4 | 4.2 | 3.5 / 10.2 / 30 |
| PLATE | 0.5 | 0.647 | | 2.6 | 4.3 | 7.4 | 3.5 / 10.2 / 30 |
| PLATE | 1 | 1.153 | | 4.6 | 7.7 | 13.3 | 3.5 / 10.2 / 30 |

The "6000-modes" column converts Dal Santo's >6000 modes at fs 48 kHz (0.125 modes/Hz) assuming their test T60 of ~1.4 s (the 2023 paper uses 1.4-2.5 s); it is a rough yardstick. The low tank of HALL / MASSIVE has the same sums in seconds (modal density per Hz is independent of the sample rate). The PLATE's tank is one figure-8 loop of 21,589 samples at 29,761 Hz (Dattorro Table 1) x the SIZE scale (0.5-1.59).

### 4.2 T60 against frequency (`litfdn_theory.py`, [derived] from Governor.h)
HALL (24 kHz tank, air 5.5 dB/s) at TONE noon, T60 at the low end 1 / 2 / 4 / 8 / 16 s -> T60 at 1 kHz | 4 kHz | 10 kHz:
1 s: 0.99 | 0.94 | 0.92; 2 s: 1.96 | 1.76 | 1.69; 4 s: 3.83 | 3.15 | 2.93; 8 s: 7.33 | 5.21 | 4.63; 16 s: 13.5 | 7.7 | 6.5.
PLATE (48 kHz, air 8 dB/s): 4 s: 3.75 | 2.91 | 2.65; 8 s: 7.05 | 4.56 | 3.97; 16 s: 12.6 | 6.4 | 5.3.
TONE quarter (ratio 0.265) PLATE 4 s: 3.61 | 2.46 | 2.15; three-quarter (0.975): 3.87 | 3.40 | 3.24.
Reading: the highs ride well above any physical room's (Costello: <= ~1.25 s at 10 kHz) once the low-end T60 passes ~2 s, by design (the owner calibrated to Valhalla Room's measured 3-7 dB/s). Only PLATE is named after hardware that is known to be dark.

### 4.3 Normalized echo density (`litfdn_measure.py ned`, [measured])
Click in both channels, 100 % wet, DECAY 0.5. NED = share of samples beyond one windowed (20 ms Hann) standard deviation over 0.3173, 1 ms stride.

| space | SIZE | first reaches 0.5 | first reaches 0.8 (held 20 ms) | last time under 0.8 (L [R]) | NED 0.5-1.5 s |
|---|---|---|---|---|---|
| PLATE | 0 | 49 ms | 119 ms | 119 [137] ms | 1.00 +- 0.04 |
| PLATE | 0.5 | 82 | 273 | 303 [321] | 1.01 +- 0.04 |
| PLATE | 1 | 175 | 423 | 582 [589] | 0.96 +- 0.08 |
| HALL | 0 | 4 | 14 | 14 [20] | 1.00 +- 0.05 |
| HALL | 0.5 | 8 | 19 | 65 [57] | 1.00 +- 0.05 |
| HALL | 1 | 12 | 36 | 168 [164] | 1.01 +- 0.05 |
| MASSIVE | 0 | 25 | 106 | 744 [732] | 0.97 +- 0.06 |
| MASSIVE | 0.5 | 52 | 64 | 634 [64] | 1.00 +- 0.05 |
| MASSIVE | 1 | 143 | 161 | 202 [199] | 0.97 +- 0.04 |

MASSIVE's early part is its swell rising from 40-100 dB under the tail (envelope -94 dB at 20 ms, -62 dB at 0.4 s, SIZE 0): the dips below 0.8 there are at levels nobody hears; not a finding. The late tail is Gaussian-dense everywhere (NED 1.0 +- 0.05).

### 4.4 In-loop allpass decay (`litfdn_apdecay.py`, [measured on a stand-alone model]; `litfdn_measure.py edc`, [measured on the renderer])
A scaled model of the code's topology (8 lines at the code's ratios and spread, two Schroeder allpasses per line at the code's lengths, Hadamard, a flat loss for a 12 s T60; 6 kHz, its own code). A: the loss lumped on the line for (line + allpass) seconds, as Fdn.cpp does. B: delay-proportional loss inside each allpass (gamma^M on its delay output), the line carrying gamma^m only.

| model | variant | T60 from EDC stretches -5..-25 / -25..-45 / -45..-65 dB | late/early | tail log-spectrum std (dB), t 1-3 s -> t 20-22 s |
|---|---|---|---|---|
| MASSIVE SIZE 0.5-like (mean 0.84 s, g 0.45) | A | 12.44 / 12.01 / 11.64 | 0.94 | |
| | B | 12.38 / 11.90 / 11.46 | 0.93 | |
| MASSIVE SIZE 1-like (mean 2 s, g 0.62) | A | 11.86 / 11.99 / 11.71 | 0.99 | |
| | B | 11.78 / 11.93 / 11.60 | 0.99 | |
| HALL SIZE 0-like (35 ms, g 0.31, allpasses x0.175) | A | 12.10 / 12.34 / 12.21 | 1.01 | 16.80 -> 17.45 |
| | B | 12.00 / 12.01 / 11.66 | 0.97 | 16.81 -> 16.80 |
| MASSIVE SIZE 0-like (mean 0.35 s, spread 0.4, g 0.28) | A | | | 5.45 -> 6.21 |
| | B | | | 5.46 -> 5.39 |

Reading: the single-line loop-time bound in `litfdn_theory.py` s3 (up to +-20-40 %) does not show as a curved decay: the eight-line mixing averages each mode over the lines. What does show is a small drift of the tail's spectral roughness with time in A (+0.65 to +0.76 dB std over 20 s) that B removes. A small HALL at T60 12 s with no modulation has a very peaky tail (16.8 dB std against 5.57 for noise): that is audit F2 again, in numbers (the real HALL has its floor, x3, which this model lacks).

Renderer, interrupted noise (Griesinger test 5, Schroeder's method; 4 s of white noise, then 70 s of silence; Gaussian bands of ~0.7 octave; T60 in seconds from the running-minimum band envelope in stretches below its start level; DECAY 0.3, WEAR 0; `litfdn_measure.py edc`). A tail with modes that outlive the rest would show a later stretch slower than the first (late/early > 1):

| space, SIZE | band | -5..-25 | -25..-45 | -45..-65 | -65..-85 dB | late/early |
|---|---|---|---|---|---|---|
| HALL 0 (allpasses) | 250 Hz | 1.87 | 2.10 | 2.12 | 1.90 | 1.13 |
| | 500 Hz | 1.89 | 1.88 | 2.04 | 1.97 | 1.08 |
| | 1 kHz | 1.94 | 1.92 | 1.95 | 2.00 | 1.00 |
| | 2 kHz / 4 kHz | 1.88 / 1.67 | 1.79 / 1.71 | 1.83 / 1.72 | 1.92 / 1.68 | 0.97 / 1.03 |
| HALL 0.7 (no allpass) | 250 Hz | 2.83 | 2.96 | 3.57 | 3.17 | 1.26 |
| | 500 Hz-4 kHz | 3.38-2.53 | 3.03-2.52 | 3.42-2.51 | 3.00-2.55 | 1.01 / 1.00 / 1.05 / 0.99 |
| MASSIVE 0.5 (allpasses) | 250 Hz | 18.41 | 18.07 | 17.66 | 15.93 | 0.96 |
| | 500 Hz-4 kHz | 16.67-16.32 | 17.59-16.49 | 16.29-16.77 | 16.95-16.69 | 0.98 / 1.01 / 1.00 / 1.03 |
| MASSIVE 1.0 (allpasses) | 250 Hz | 32.67 | 26.80 | 26.49 | 31.33 | 0.81 |
| | 500 Hz-4 kHz | 28.49-28.72 | 29.17-28.56 | 27.02-28.15 | 30.46-29.12 | 0.95 / 0.98 / 0.97 / 0.98 |
| PLATE 0.5 | 250 Hz | 2.05 | 1.95 | 2.34 | 2.35 | 1.14 |
| | 500 Hz-4 kHz | 1.85-1.63 | 2.15-1.61 | 2.03-1.60 | 2.48-1.63 | 1.10 / 1.05 / 1.06 / 0.98 |

Reading: the real tails are straight from 500 Hz up in every SPACE (late/early 0.95-1.05), with or without in-loop allpasses. Only the 250 Hz band of the short loops (HALL, PLATE: 1.13-1.26) curves up, which is the sparse-modes effect of section 4.1 (few modes under 300 Hz in a 0.4-0.7 s loop; HALL's low tank is the one with 35 ms lines), not the allpasses. MASSIVE's allpass lines show no curvature at all (0.81-1.03). Finding 2 is therefore closed as "no measurable effect".

### 4.5 L/R correlation (`litfdn_measure.py corr`, [measured])
L/R correlation coefficient per octave band 125 Hz ... 8 kHz, a centred click, 100 % wet, DECAY 0.5:

| space | SIZE | 0-50 ms | 50-200 ms | 0.5-2 s |
|---|---|---|---|---|
| PLATE | 0.2 | +.01 +.11 +.05 +.08 +.05 +.05 -.07 | .00 .03 .05 .00 -.07 .01 .00 | -.04 .03 -.02 -.03 -.02 .00 -.03 |
| PLATE | 0.8 | -.13 -.39 -.12 -.01 -.04 +.11 +.06 | +.15 +.20 +.01 +.13 -.03 .00 -.02 | +.05 +.04 -.03 -.01 .00 -.02 +.01 |
| HALL | 0.2 | -.55 -.50 +.03 -.24 +.05 +.04 -.06 | +.17 +.02 +.03 +.03 +.04 +.06 +.03 | +.05 -.03 -.05 +.01 -.03 +.03 +.02 |
| HALL | 0.8 | -.53 -.17 -.24 -.23 -.14 -.11 -.39 | +.04 -.17 -.01 +.06 +.08 +.10 +.20 | +.05 +.04 -.03 .00 +.04 +.03 -.01 |
| MASSIVE | 0.2 | -.75 -.92 -.91 -.94 -.87 -.84 -.63 | +.35 -.16 -.33 -.07 -.17 -.13 +.03 | +.02 .00 +.03 -.05 -.03 +.01 .00 |
| MASSIVE | 0.8 | +1.00 (all bands) | +.04 -.52 -.55 -.38 -.40 -.34 -.44 | -.07 +.23 +.08 +.03 +.05 +.05 +.05 |

Mono-sum power against the channels' summed power (0 dB = no cancellation), whole band (below 300 Hz): 0.5-2 s: -0.0 to +0.2 dB (+0.1 to +0.4) in every case. 0-50 ms: PLATE 0.0 / +0.1 (+1.6 / -0.6), HALL 0.2 -0.1 (-1.3), HALL 0.8 -1.2 (-4.7), MASSIVE 0.2 -7.3 (-14.6), MASSIVE 0.8 +3.0 (+3.0): MASSIVE's first 50 ms is the swell's own first taps at < -90 dB, not audible.

### 4.6 DC (`litfdn_measure.py dc`, `litfdn_dcfix.py`, [measured])
10 mV (-40 dBFS) DC on both inputs for 30 s, then silence; wet mean in units of the input DC at 5 / 15 / 29 s, and after the input stops (31 / 35 / 45 / 59 s):

| space | DECAY | 5 s | 15 s | 29 s | 31 s | 35 s | 45 s | 59 s |
|---|---|---|---|---|---|---|---|---|
| PLATE | 0.50 | +3.47 | +3.49 | +3.49 | +1.00 | +0.02 | 0 | 0 |
| PLATE | 0.78 | +7.6 | +10.9 | +11.4 | +8.6 | +3.8 | +0.5 | 0 |
| PLATE | 1.00 | +11.1 | +9.9 | +9.9 | +9.1 | +9.1 | +9.1 | +9.2 |
| HALL | 0.50 | +1.86 | +1.88 | +1.88 | +0.67 | +0.02 | 0 | 0 |
| HALL | 0.78 | +4.0 | +6.1 | +1.9 | +0.9 | +0.1 | 0 | 0 |
| HALL | 1.00 | +6.7 | +12.7 | +12.8 | +11.9 | +12.2 | +12.2 | +12.2 |
| MASSIVE | 0.50 | -1.4 | -2.5 | -2.8 | -2.8 | -1.4 | -0.3 | 0 |
| MASSIVE | 0.78 | -1.5 | -3.9 | -5.7 | -5.8 | -4.6 | -2.8 | -1.4 |
| MASSIVE | 1.00 | -1.8 | -5.7 | -8.6 | -8.7 | -8.5 | -8.1 | -8.4 |

At infinite DECAY the wet's RMS is -18 to -21 dBFS and it is all DC, 30 s after the input stopped (WEAR 0). **Scope in WEAR:** the DC at the *output* disappears from WEAR ~0.06 up (HALL infinite: +11.5 x the input DC at WEAR 0.01, +4.6 at 0.03, 0.00 from 0.06; PLATE the same), consistent with the 5 Hz blocker of the wear stage (EraTexture.h:92, 261) on the wet; but the loop still holds it. At WEAR noon, infinite, a noise burst at t = 2 s with DC on the input for the first 20 s, AC level of the tail against the same run without DC (dB, at 5-8 s / 30-33 s; `litfdn_dcimpact.py`):

| space | DC 0.3 mV | 1 mV | 3 mV | 10 mV |
|---|---|---|---|---|
| PLATE | +0.1 / -4.4 | +4.9 / +1.5 | +4.8 / -16.6 | -11.9 / -26.2 |
| HALL | +0.1 / -3.1 | +4.3 / +3.1 | +4.3 / -5.5 | +4.2 / -5.8 |
| MASSIVE | +3.6 / +6.6 | +3.6 / +6.2 | +3.6 / +3.3 | +3.6 / -3.2 |

(The input's power, DC included, feeds the quiet detector that decides whether an infinite tail is "held" or "fed", Engine.cpp:829 against -70 dBFS = 0.3 mV, and the governor judges the loop's DC as level: both change the tail.) With a 10 Hz one-pole high-pass in front the same runs differ from their no-DC references by 0.0 / -0.0 to -2.0 dB (`litfdn_dcimpact.py hp 10`; PLATE 0.0/-2.0, HALL 0.0/-1.7, MASSIVE 0.0/-0.7 at 3-10 mV).
Real programme, 3 s windows late in an infinite hold (SIZE 0.5): skank loop (input mean -0.23 mV): wet mean PLATE -12.8 mV, HALL -4.7 mV; with a 10 Hz one-pole in front: -0.10 / +0.04 mV (and 20 Hz: -0.05 / +0.04). Pink noise: +-8-30 mV wanders without, <= 0.3 mV with. Wet energy below 40 Hz in the skank: 4.1 % PLATE / 2.3 % HALL without, 0.4 % / 0.5 % with the 10 Hz high-pass, 0.5 % / 0.6 % with 5 Hz (skank mean at 5 Hz: -0.13 / +0.03 mV; the 10 mV DC test: <= 0.28 mV, wet at -67 to -75 dBFS).

### 4.7 Frequency-response roughness of the sign vectors (`litfdn_excitation.py`, [measured])
Std of 20 log10 |H| over 262,144 frequencies and the 99th percentile over the median (dB, lower = smoother), lossy FDN with the code's delays at HALL SIZE 0.5 (24 kHz), T60 2 s:

| matrix, b, c | std | p99 - median |
|---|---|---|
| code: Hadamard, b = kInL, c = kOutL | 5.77 | 10.44 |
| code, R in -> L out | 5.92 | 10.97 |
| Hadamard, b = c = ones | 6.31 | 11.99 |
| Hadamard, random +-1 | 5.53 | 8.19 |
| Hadamard, Gaussian | 6.41 | 10.51 |
| Householder, code b, c | 7.12 | 10.39 |
| random orthogonal, code b, c | 6.00 | 9.56 |

At SIZE 0.2 and at T60 6 s the order is the same (Householder worst: 8.05 and 9.28 against 6.21 and 6.71 for the code). 24 random +-1 pairs: std 5.83 (5.46-6.24), p99 9.71 (8.67-10.81); 42 % smoother than the code by std, 92 % by p99: the rows are typical in smoothness and ~0.75 dB peakier at the top end. Not worth a change.

---

## 5. Options

(Sound change: none-provable / tiny / audible. CPU is a share of the module's 10,000 cycles per output sample; assumptions given. Nothing here is built; each would be a prototype behind an `IV_*` hook.)

### 5.1 Input high-pass (finding 1) -- recommended
- **What:** a one-pole high-pass at 5-10 Hz on each channel of the tank input (before HALL / MASSIVE / PLATE and the twin), outside every loop.
- **Evidence:** section 4.6; DC and rumble are integrated by a loop whose DC gain is 1 at infinite DECAY and >= 1.9 at finite; the governor then judges DC as level.
- **Sound:** none-provable. One-pole 10 Hz: -0.97 dB at 20 Hz, -0.26 dB at 40 Hz; at 5 Hz -0.26 dB at 20 Hz, -0.07 dB at 40 Hz. Renderer-bit-identical with the hook off.
- **CPU:** 2 channels x (1 multiply, 3 adds) ~ 10 cycles = 0.1 % [estimated]. **Flash:** ~60-120 B [estimated; the release is 76 B under budget; the dead `DcBlocker` struct could be revived, or EraTexture's 5 Hz subtractor pattern reused]. **Risk:** none for rule 2 (outside the loop); check the freeze and the twin paths. **Needs:** the module's input/output coupling decides how much it matters on the rack; on the desktop and in a plugin host it is a null test.

### 5.2 Delay-proportional loss inside the in-loop allpasses (finding 2) -- not recommended on this evidence
- **What:** in `Fdn` (MASSIVE, small HALL) scale each allpass delay output by gamma^M (gamma = the per-sample low-end gain) inside the allpass, and give the line only gamma^m; keep the HF ratio on the line. All modes then decay at exactly the DECAY rate.
- **Evidence:** Blesser (Dattorro p. 665), Schlecht 2021 SV.A, Jot and Chaigne; the stand-alone model (4.4).
- **Sound:** tiny, on the late tail (the model shows a straight decay either way and a spectral-roughness drift of < 1 dB std that the cure removes). Needs ears if ever pursued. **CPU:** 16 multiplies per 24 kHz sample plus 16 `exp` per control block ~0.3-0.4 % [estimated]. **Flash:** 100-200 B [estimated]. **Risk:** touches loop gain: a TONE-bright setting (high rate < low rate) would make the line's compensation exceed 1; clamp it. The energy loop (infinite holds, SIZE sweeps) must be re-run.

### 5.3 PLATE onset density (finding 3) -- owner's ears
- **What:** a short onset cloud (a velvet-noise FIR of ~30-60 ms at 2000-4000 pulses/s, Valimaki 2017 / Greenblatt) or two to three more short input-diffuser stages for large SIZE.
- **Sound:** audible; makes a large PLATE dense at once and less like Dattorro. **CPU:** 100-200 taps/s per side as a sparse FIR: 1-3 %; extra allpass stages ~0.5 % each [estimated]. **Flash:** 100-300 B. The owner called the PLATE "excellent": a page decides.

### 5.4 Early-window stereo (finding 4)
- Change the signs the early taps go out with (Fdn.cpp:782 `(i + 3) & 7`) so that no tap pair is a +-1 mirror at low frequencies, or give L and R slightly different tap positions. **Sound:** tiny; **CPU/flash:** 0; ears optional.

### 5.5 PLATE highs (finding 6)
- A PLATE-only larger `air` (60 dB/s would be EMT-like; 15-25 dB/s is a milder step), or a slope TONE already reaches. **Sound:** audible; CPU 0; flash 0. The owner's calibration and the "excellent" verdict argue for leaving it unless a "more EMT" page is wanted.

### 5.6 Random output-gain wander (EMT 250 patent)
- Slow random gains (+-1 dB, 1-3 s steps) on each line's taps: pole-neutral, "alive" stereo image. **Sound:** tiny to audible. **CPU:** 8 multiplies per sample per channel ~0.1-0.2 % [estimated]. Not recommended without a listening request.

### 5.7 Not recommended, with reasons
- **Matrix modulation instead of the floor** (Schlecht thesis S6): no pitch drift, but the delay reads remain for DRIFT, so no worst-case saving.
- **Jot's tonal correction filter:** the wet's spectrum already follows T60(w); TONE's tilt is a choice.
- **Bass multiply:** Lexicon's norm is 1.0-1.2; ours is 1.0.
- **Re-picking the sign vectors:** section 4.7.
- **Dispersion chain for the plate:** hundreds of stages.

---

## 6. What was not checked

- The module: whether the Versio's audio input and output are AC-coupled (decides how much finding 1 matters on the rack); any CPU or flash number here is an estimate.
- Abel and Huang 2006, Jot and Chaigne 1991, Arcas and Chaigne 2010, Karjalainen and Jarvelainen 2001, Gerzon 1971-76: not read at the source.
- Lexicon 224 internals and the EMT 250's exact sample rate: Blesser's 32 kHz and 8K words come from his letter in Dattorro 1997 (the audit quotes 24 kHz from a secondary source: the two disagree).
- The shelf's accuracy against a physical T60(f) curve (no reference curve exists for our spaces).
- NED was measured on a click; programme-dependent echo density (Griesinger's count within 20 dB) was not.
