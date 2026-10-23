# DSP audit: the engines against the published sources

**Shipped (SPEC v1.0.22, 24 Oct 2026; owner: B on every card of `renders/dsp_audit`):** A3 (a frozen pad holds still), E4 (vintage reads, ADR 0019) and E5 (tape out, ADR 0019, now at 4×) are the defaults; their `IV_FREEZE_STILL` / `IV_VINTAGE_MOD` / `IV_TAPE_OUT` hooks and the page's scripts are gone. Every other item below is measured and closed.

*22 Oct 2026. Docs only: no DSP was changed. This compares the code in `core/dsp/` with the sources named in `IMMENSIO_BRIEF.md` §8, read at the source, not from memory. It answers the owner's question: were the reverbs built from the research, or from reasoning alone?*

**The short answer.**
- **PLATE** follows Dattorro closely and can be checked line by line against his paper.
- **HALL and MASSIVE** follow Jot's loss rule and a standard Hadamard FDN, and MASSIVE's in-loop allpasses match Zita-Rev1 and Gardner. HALL, though, leaves out the in-loop diffusion that every published hall design uses.
- **The swell stage (BLOOM and MASSIVE's mass), REVERSE, the floor governor and most of ERA** are our own inventions with no published basis. They are also where most of the listening problems have come from.

How to read it: each finding gives the source, what our code does, and what you'd likely hear. Findings are ranked by likely audible impact within each engine, and the experiments at the end are ranked across engines. "Likely" means it hasn't been measured or heard yet. Per the house rule (measure the cause against a control before fixing), each finding comes with an experiment rather than a fix.

**24 Oct 2026: every remaining item measured** (A3, P2–P5, E2–E6, R1–R2; MI #5 in `mutable-instruments-reverbs.md`). Each has a dated "Measured" line below. Three showed a real, audible difference and passed the energy check: A3 (a still freeze), E4 (noisy vintage reads) and E5 (tape saturating the wet). They're asked on one pick page, `renders/dsp_audit` (`tools/pages/dsp_audit.sh`). The rest were dropped or need nothing.
- **Cost of the three prototypes:** release firmware 99,180 → 101,308 B (+2.1 KB, under the 110 KB budget); +1–3 % of a desktop render each. Firmware CPU is unmeasured.
- **How it was measured:** the energy harness is `tools/pages/dsp_audit_energy.py`; the per-item measurements are in `tools/pages/dsp_audit_measure.py`, `dsp_audit_a3.py` and `dsp_audit_cpu.py`.
- **Side findings** (not fixed) are listed before the experiments at the end.

---

## Sources (read for this audit)

| Source | URL | Notes |
|---|---|---|
| Dattorro, "Effect Design Part 1: Reverberator and Other Filters", JAES 45(9), 1997 | https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf | Fig. 1, Tables 1–2, §1.3.7 (modulation) |
| Dattorro, "Effect Design Part 2: Delay-Line Modulation and Chorus", JAES 1997 | https://ccrma.stanford.edu/~dattorro/EffectDesignPart2.pdf | Allpass vs linear interpolation, coefficient transients |
| Jot, "Efficient models for reverberation and distance rendering…", ICMC 1997 (summarises Jot & Chaigne 1991 and Jot 1992) | https://freeverb3-vst.sourceforge.io/doc/10.1.1.48.8176.pdf | Jot & Chaigne 1991 (AES preprint 3030) is paywalled: https://aes2.org/publications/elibrary-page/?id=5663 |
| J. O. Smith, *Physical Audio Signal Processing* (PASP), reverb chapters | https://ccrma.stanford.edu/~jos/pasp/Artificial_Reverberation.html, plus the pages Mode_Density_Requirement, Choice_Delay_Lengths, Achieving_Desired_Reverberation_Times, First_Order_Delay_Filter_Design, Multiband_Delay_Filter_Design, Tonal_Correction_Filter, Hadamard_Matrix, Householder_Feedback_Matrix, Zita_Rev1, First_Order_Allpass_Interpolation, Minimizing_First_Order_Allpass_Transient (each at `…/jos/pasp/<Page>.html`) | |
| Schlecht, PhD thesis, *Feedback Delay Networks in Artificial Reverberation…* (includes the 2015 JASA time-varying-matrix work) | https://theses.eurasip.org/wp-content/uploads/sebastian-jiro-schlecht-feedback-delay-networks-in-artificial-reverberation-and-reverberation-enhancement.pdf | Modulation inside FDNs |
| Schlecht & Habets, "Scattering in Feedback Delay Networks", IEEE/ACM TASLP 28, 2020 | https://arxiv.org/pdf/1912.08888 | Filter feedback matrices, in-loop density |
| Välimäki et al., "Late reverberation synthesis using filtered velvet noise", Appl. Sci. 2017 | https://acris.aalto.fi/ws/portalfiles/portal/13412521/applsci_07_00483_v2.pdf | Sparse noise FIRs; density needed for smoothness |
| Costello, Getting Started With Reverb Design, parts 1–4 | https://valhalladsp.com/2021/09/20/getting-started-with-reverb-design-part-1-dev-environments/ · …/2021/09/22/…-part-2-the-foundations/ · …/2021/09/23/…-part-3-online-resources/ · …/2021/09/28/…-part-4-books/ | valhalladsp.com returns 403 to fetchers. Read through the Wayback Machine (`web.archive.org/web/2023/<url>`) and the built-in browser |
| Costello, Supermassive posts | https://valhalladsp.com/2020/05/06/introducing-valhallasupermassive/ · https://valhalladsp.com/2020/05/06/the-philosophy-of-valhallasupermassive/ · https://valhalladsp.com/2020/05/06/valhallasupermassive-the-controls/ · https://valhalladsp.com/2020/05/07/valhallasupermassive-the-modes/ · https://valhalladsp.com/2021/05/19/tips-and-tricks-infinite-supermassive/ | Plus the mode-update posts for Cirrus (2020/11/23), Orion/Cassiopeia (2021/11/22), Aquarius/Pisces (2022/06/01) and Scorpio/Libra (2022/11/21) |
| Costello, other posts on technique | https://valhalladsp.com/2009/07/30/modulation-in-reverbs-reality-and-unreality/ · https://valhalladsp.com/2011/01/21/reverbs-diffusion-allpass-delays-and-metallic-artifacts/ · https://valhalladsp.com/2011/07/07/algorithmic-reverbs-distortion-and-noise/ · https://valhalladsp.com/2023/02/10/valhallavintageverb-the-modes/ · https://valhalladsp.com/2017/05/06/valhallavintageverb-1-7-1-chaos-reigns/ · https://valhalladsp.com/2010/12/02/valhallashimmer-tips-and-tricks-bloom/ · https://valhalladsp.com/2010/12/03/valhallashimmer-tips-and-tricks-using-diffusion-instead-of-predelay/ · https://valhalladsp.com/2010/05/14/stability-through-time-variation-ursa-major-space-station/ | |
| Keith Barr / Spin Semiconductor, FV-1 knowledge base, reverberation | http://www.spinsemi.com/knowledge_base/effects.html | The allpass-loop recipe; Barr designed the Midiverb |
| emeb/MIDIVerb_RE (MIT) | https://github.com/emeb/MIDIVerb_RE (README, `docs/MV_Slides.pdf`, `code/emulator/midiverb.c`, `spice/*.cir`) | Midiverb I hardware |
| thement/midiverb_emulator (GPL-3.0, **read for structure only, no code taken**, ADR 0006) | https://github.com/thement/midiverb_emulator | Midiverb II / DASP structure |
| Chowdhury, ChowTape manual (for the DAFx 2019 tape paper, whose PDF host didn't resolve) | https://ccrma.stanford.edu/~jatin/chowdsp/Products/ChowTapeManual.pdf · https://dafx.de/paper-archive/details/Z5Ow_E5YHVxvndTAyeEuzA | Tape ingredients |

Secondary sources (search snippets only, treat as approximate): the Lexicon 224 (20 kHz, 12-bit gain-ranging converters) and EMT 250 (24 kHz, 12-bit) figures from vintagedigital.com.au, tapeop.com and preservationsound.com. The Midiverb II rate is 23.4 kHz per thement and 31.25 kHz per Temecula DSP (https://www.synthtopia.com/content/2026/03/30/free-reverb-for-mac-windows-recreates-vintage-alesis-midiverb-ii-sound); this is unresolved.

---

## Findings that apply to every engine

### A1. TONE at noon hardly darkens the upper mids (high impact, every space)
- **Source.** Jot and Smith (PASP, Multiband_Delay_Filter_Design) say a first-order loss filter is a minimum. Perceptual studies call for at least three bands with adjustable crossovers. Zita-Rev1 uses a low shelf times a lowpass with a set crossover, and Jot suggests a biquad for three bands.
- **What our code does.** `absorbent()` in `core/dsp/Governor.h:83` is a one-pole filter that matches the decay at DC and at Nyquist only. The shape in between is fixed: the extra loss follows (1 − cos ω), so it sits mostly above ~8 kHz. Computed for a 4 s HALL tail at TONE noon (high-end ratio 0.45), the same for any line length:

  | 500 Hz | 1 kHz | 3 kHz | 6 kHz | 12 kHz | 20 kHz |
  |---|---|---|---|---|---|
  | 3.99 s | 3.97 s | 3.78 s | 3.29 s | 2.39 s | 1.86 s |

  At TONE fully CCW (ratio 0.08) it swings hard the other way: 1 kHz 2.5 s, 3 kHz 0.96 s.
- **Likely heard.** At and around noon, the 2–6 kHz presence region (vocal edge, hi-hat, snare crack) rings almost as long as the body. A long tail gets a glassy sheen rather than the warm, woolly fade of a hall. Turning TONE down doesn't gradually take the edge off: the decay goes from bright to muffled with little in between. The output tilt added at v1.0.8 changes the level of the highs, not how long they ring.
- **Not covered at all:** a separate low-band decay. Lexicon's "bass multiply" and Supermassive's Libra, with its low EQ in the loop, let the lows ring longer or shorter than the mids.

### A2. Allpass interpolation: the right choice, but the docs say cubic (doc fix, low impact)
- **Source.** Dattorro (Parts 1 and 2) and Schlecht recommend allpass interpolation inside the loop because it keeps the gain exact. Smith (PASP, First_Order_Allpass_Interpolation) says to keep the fractional part away from 0. Our `readAllpass` keeps it in [0.5, 1.5), which satisfies that. The weakness, from Dattorro Part 2 and PASP, is a small transient each time the read crosses a whole sample. It limits clean use to about ±1 semitone of movement.
- **Gap.** SPEC §4.3 and §5.1 still say "cubic interpolation", but changelog v1.0.2 replaced it with allpass. Fix the stale lines.
- **Likely heard.** Nothing at the modulation floor's depth. At DRIFT's far end (±35 cents, plus the cassette's 6–8 Hz flutter), the read crosses whole samples often, which may leave a faint high-frequency grit. Worth one measurement (experiment E9).

### A3. Freeze keeps modulating (medium, held pads)
- **Source.** Costello ("Tips and Tricks: Infinite Supermassive"): for a freeze, set the modulation depth to 0. Modulation in a lossless loop turns a held note into "a cluster of sidebands" with no clear pitch, and a lowpass in the loop dulls it over minutes.
- **What our code does.** While frozen, PLATE drops Dattorro's chorus to 0.15 of its depth (`kFrozenPlateModDepth`): same conclusion, reached by ear (M5). But both tanks keep the modulation floor running, and DRIFT too (ADR 0009 keeps "only the modulation floor and Drift"). The FDN's floor never drops.
- **Likely heard.** Over minutes, a frozen chord's pitch smears into a shimmer of beating partials. That may be the life we want, or it may be the "seasick" note coming back slowly on FDN pads. The owner should decide by ear.

- **Measured (24 Oct 2026):** shows, most in HALL. A six-note chord frozen for 5 minutes (DRIFT noon, ERA clean; `tools/pages/dsp_audit_a3.py`); "focus" is the share of the 150–900 Hz energy within ±1.5 Hz of the chord's notes. HALL 0.88 at 10 s → 0.81 at 1 min → 0.66 at 5 min, smearing steadily; MASSIVE 0.96 → 0.89; PLATE wanders between 0.48 and 0.62 (its 0.15 chorus and its floor). With the tanks' own modulation off while frozen, it stays put for the whole 5 minutes: HALL 0.93, MASSIVE 0.97, PLATE 0.53. **Prototyped:** `IV_FREEZE_STILL`: the FDN's floor and the PLATE's chorus and floor glide to 0 with the freeze and back on unfreeze; DRIFT is untouched (ADR 0009), and the low tank was already still. Source: Costello, "Tips and Tricks: Infinite Supermassive" (mod depth to zero for a freeze, or the pitch dissolves into "a cluster of sidebands"); I found no manual that says whether other units' freezes keep modulating. **Energy:** freeze holds as in `padHolds`, 3/3 pass (5 min within 0.12 dB in each SPACE, as today); unfrozen renders are bit-identical (md5), so the sweep doesn't apply. Without its movement a frozen MASSIVE beats a little more in 5 s windows (2.4 against 1.7 dB of swing, same mean). Cost: nothing measurable. **On the page** (`renders/dsp_audit`, the freeze cards).

### A4. Modulating the lows (medium, dub bass in the tail)
- **Source.** Schlecht (thesis, chapter 6): keep the band below about 400 Hz unmodulated (split each line with a one-pole), because a modulated low end sounds detuned or drifting. Spread the lines' rates by ±50 % so they don't beat together.
- **What our code does.** The whole signal is modulated, lows included. The floor's step times are already spread (`kFloorStepSeconds × (1 + 0.12 i)`). DRIFT's sine rates are spread per line.
- **Likely heard.** With DRIFT turned up, sub-bass in the tail wavers in pitch. For dub that may be a Cathedra-like feature or a muddy bass, so it is a listening question.

- **Measured and prototyped (23 Oct 2026):** a held 55 Hz into the tail, peak-to-peak pitch wander (crude FFT peak tracking): HALL DRIFT CW 397 → 23 cents with the lows read unmodulated (`IV_STILL_LOWS`, a second read per line crossed over at 400 Hz); MASSIVE CW 120 → 27; MASSIVE CCW 35 → 17; HALL CCW 84 → 109 (estimator noise). FDN only. Page: `renders/still_lows`. **Owner (23 Oct):** B on every MASSIVE card and HALL Cathedra; A on HALL tape ("waves of built-up distortion"). **Not safe:** the first-order crossover between the still and moving reads isn't energy-preserving; where the reads drift out of phase near 400 Hz the sum reaches +3 dB a pass, so HALL builds (up to +12.7 dB at SIZE 0, either DRIFT side) and, made MASSIVE's default, the 10-minute infinite hold failed (−46 … −23 dB) and the 1 kHz T60 fell 22 → 16.6 s (the split leaks into the mids). Reverted. Next: a stable method (Schlecht & Habets: time-varying unitary feedback matrices are stable where delay modulation isn't; a steeper, delay-matched split). **Rebuilt (23 Oct 2026):** two tanks, the lows in their own unmodulated tank (`IV_STILL_LOWS=4`); every split-in-the-loop variant measured unsafe or draining. Details and measurements: `docs/research/still-lows.md`. Page: `renders/still_lows` (round 2). **Shipped, v1.0.18** (B on all 25 cards).

---

## PLATE: `core/dsp/Plate.*` against Dattorro (1997)

**What matches the paper** (checked against Fig. 1 and Tables 1–2): the four tank lengths per branch and the two input-diffuser pairs (142/107 at 0.75, 379/277 at 0.625). Decay diffusion 1 is 0.70 with its inverted sign, and decay diffusion 2 is 0.50. All 14 output taps match, with their offsets and signs, as does the ×0.6 output gain. Only the first allpass of each branch is modulated, as in the paper, and the reads use allpass interpolation, which Dattorro himself recommends.

### P1. Left goes in, the plate answers on the right (high, panned sources) — measure first
- **Source.** Dattorro sums the input to mono and feeds both halves, so his output taps were never designed to keep a stereo image.
- **What our code does.** True stereo in (ADR 0013): L enters branch 0 and R enters branch 1 (`Plate.cpp`, `in[b]`). But the left output takes 4 of its 7 taps from branch 1, including both strong +1 taps on the 4217 line, and the right output takes 4 of its 7 from branch 0. So a hard-left source's first plate energy comes out mostly on the right. Only after half a lap of the figure 8 does it cross back.
- **Likely heard.** A hi-hat panned left blooms on the right, as if the image were swapped or ping-ponging. This is a reading of the code. Measure it before deciding anything (experiment E3).

- **Measured (23 Oct 2026):** confirmed. A hard-left burst's first 50 ms of PLATE is entirely on the right (L−R −127 dB at SIZE ½), then balanced. Feeding L into branch 1 and R into branch 0 (`IV_PLATE_SWAP`) puts it on the left first, at every SIZE. Page: `renders/plate_stereo`.

### P2. Decay diffusion 2 doesn't follow DECAY (low)
- **Source.** Dattorro, Table 1: decay diffusion 2 = clamp(decay + 0.15, 0.25, 0.50).
- **What our code does.** It is fixed at 0.50.
- **Likely heard.** Short plate decays smear a little more than Dattorro's would. Hardly audible.

- **Measured (24 Oct 2026):** confirmed in the paper (Table 1: "decay diffusion 2 = decay + 0.15, floor = 0.25, ceiling = 0.50"), and tiny in the sound. With Dattorro's rule (`IV_PLATE_DD2`, his "decay" taken as the branch's low-end gain over ap2 + d2), only PLATE tails under ~1.7 s change at all: B − A sits 28.5 to 57 dB below A, the band T60s are at most ~10 % shorter, and most settings render bit-identically. **Dropped**, not paged: too small for a listen to pick out. The hook comes out after commit 5ea65e8, which still has it.

### P3. Modulation depth and rate (low)
- **Source.** Dattorro: an excursion of 16 samples at most and about 8 typical (at 29.8 kHz), at about 1 Hz, ideally on every tank delay at different rates.
- **What our code does.** ±11 samples (×1.61 for 48 kHz) round a +16 offset, at 0.71 and 0.93 Hz, plus the slow random floor, on the first allpass only.
- **Likely heard.** Fine. This is within his range.

- **Measured (24 Oct 2026):** checked against the code; no experiment needed. The chorus uses his EXCURSION of 16 (paper samples, scaled to 48 kHz): ±11.2 of sine at 0.71 and 0.93 Hz plus ±4.8 of floor, so it peaks at his maximum of 16. Since v1.0.12, d1 and d2 wander too, each at its own rate, which is his "ideally on every tank delay". Nothing to fix.

### P4. Mode density at small SIZE (low; Costello: "accept the metal and call it a plate")
- **Source.** Jot: the total delay in the loop should be at least a quarter of T60 (Σ ≥ T60/4).
- **What our code does.** The plate's total is 0.73 s × SIZE scale, which is 0.36 s at fully CCW. That covers T60 up to about 1.5 s, but PLATE's DECAY goes to 12 s and to infinite.
- **Likely heard.** A small plate with a long decay rings with a few audible tones. That is part of a plate's character. The modulation masks some of it.

- **Measured (24 Oct 2026):** doesn't show as a problem. The renderer's ringing score on the vibes (`dsp_audit_measure.py p4`), at DECAY 0.75: PLATE SIZE 0 (T60 7.9 s) 33 dB, PLATE SIZE noon 30 dB, HALL SIZE 0 42 dB, HALL SIZE 1 51 dB. The vibes' own notes count towards the score, so compare rows, not absolute values. The small plate rings no more than the medium one, and its T60 holds at every SIZE (P5). No prototype: Costello's "accept the metal and call it a plate", and the owner called the PLATE "excellent" on M5.

### P5. Beyond Dattorro, deliberately
These are ours, and none of them is in the paper: a Jot loss on both elements of each branch (Dattorro has one fixed damping filter plus a decay gain), the SIZE scale with its glide, ERA and DRIFT inside the loop, the floor governor, and the twin.

- **Measured (24 Oct 2026):** the one checkable claim holds: the Jot loss gives DECAY's T60 at every SIZE. Target (DECAY's 0.4 → 12 s scale) against measured, at 250–500 Hz: DECAY 0.2, 0.94 s → 0.96–1.24 s; 0.5, 3.36 s → 3.14–3.54 s; 0.75, 9.7 s → 8.9–9.9 s, at SIZE 0, noon and full alike. The rest is ours by design; no experiment.

---

## HALL and MASSIVE: `core/dsp/Fdn.*` against Jot, Smith, Schlecht and Costello

**What matches the sources:**
- The Hadamard feedback matrix through a fast transform, scaled by 1/√8. Smith (PASP) calls the Hadamard matrix the optimal mix.
- Distinct prime lengths.
- Jot's per-line loss, set from each line's own length, so DECAY doesn't retune the tail.
- Slow random modulation of the reads. Stautner and Puckette started it, and Costello says every Valhalla reverb does it.
- MASSIVE's in-loop diffusion: two allpasses per line at 0.62. This matches Zita-Rev1's allpass in every line (±0.6) and Gardner's allpass loop.
- MASSIVE's line lengths of 0.35–2 s match Supermassive's "up to 2 s each".
- MASSIVE's built-in attack gets faster as DECAY goes up, which is what Costello describes for Sagittarius ("higher feedback fades in faster").

### F1. HALL has no in-loop diffusion (high, percussion and skanks)
- **Source.** Zita-Rev1 (PASP) puts an allpass in every FDN line. Schlecht and Habets (2020) show that in-line allpasses raise the short-term echo density, and that allpasses after the delays double the network's effective size. Costello ("Reverbs, Diffusion, Allpass Delays…") lists using few allpasses as the "hall" approach, at the cost of low early density. He fixes that with longer, modulated allpasses, because short ones only make chorus. Barr's loop uses one to three allpasses per block.
- **What our code does.** `setInLoopDiffusion` is 0 in HALL (`Engine.cpp:300`). The only smear is the 30 ms input diffuser. At SIZE fully CW the lines are 156–367 ms, so each pass returns 8 echoes, each just 30 ms wide.
- **Likely heard.** At large HALL sizes, a snare or a dub skank comes back as a grainy flutter of distinct repeats for the first second or so before it fills in. It sounds "pointillistic" (Costello's word for a sparse Supermassive) rather than smooth.

### F2. Small HALL has too few modes for its decay; the buzz in the M1 notes (high)
- **Source.** Smith (PASP, Mode_Density_Requirement): the total delay should be at least 0.15 × T60 × fs. Jot: the total delay should be at least a quarter of T60. Barr: a loop shorter than about 200 ms flutters, and the ear is most sensitive to flutter at 4–8 Hz. Below that, short loops are heard as a pitch.
- **What our code does.** At SIZE fully CCW the mean line is 20 ms, so the lines are 12–28 ms. They recirculate 35–80 times a second, and their total is 0.16 s. That covers T60 up to about 0.65–1 s. HALL's finite T60 at SIZE fully CCW is 0.32–10 s (× 0.4), and it can be infinite.
- **Likely heard.** This is the "buzzy resonance build-up" of the M1 notes (vibes ringing score 43 dB). v1.0.8 shortened small-HALL tails, which treats the symptom. The cause is 8 short combs. In-loop allpasses (F1) or more lines at small SIZE would add modes without making the room bigger.

### F3. No Density control: sparse to full mixing (medium; a design option, not a fault)
- **Source.** Supermassive's DENSITY crossfades how much the delays mix, from parallel and unmixed up to a full unitary matrix. That is its main echo-to-reverb control. WARP sets the delays relative to the longest one.
- **What our code does.** MASSIVE uses spread (Warp-like) and diffusion instead. The matrix is always the full Hadamard.
- **Likely heard.** MASSIVE can't do Supermassive's echoes that fade in and out, or "harmonic delays". It was a deliberate choice (ADR 0007, v1.0.6: no rhythmic echoes at low SIZE), and the note is here for completeness.

### F4. No tonal correction filter (low)
- **Source.** Jot, and PASP Tonal_Correction_Filter: when the highs decay faster, the reverb's total energy leans toward the lows. A first-order corrector, E(z) = (1 − b z⁻¹)/(1 − b), with b = (1 − α)/(1 + α), flattens it.
- **What our code does.** It has none. TONE's output tilt (v1.0.8) plays a similar role by ear.
- **Likely heard.** The wet's overall balance moves with TONE more than its decay shape alone would suggest. It is already being tuned by ear, so this is low priority.

### F5. MASSIVE's early taps and level compensation (ours, no basis needed)
The three early taps per line are our own invention. The closest published idea is Puckette's feedforward early network ahead of an FDN, cited by Costello. Costello's VintageVerb "smooth" modes also shape their output taps for an exponential decay. The √(length) level compensation is ours.

---

## The swell stage (BLOOM and MASSIVE's mass): `core/dsp/Swell.h`

**There is no published basis for a 24-tap rising FIR as a swell.** What the sources describe instead:
- **Bloom and slow attacks from diffusion.** Barr's Midiverb II "Bloom" programs and Valhalla Shimmer's Bloom use allpasses with a low coefficient (0.5–0.618) inside a loop with high feedback. Costello: 0.9 or more attacks fast, while 0.5–0.618 "fades in very slowly". Barr: 0.5 "builds", with an audible delay before the density arrives.
- **Attack tied to feedback.** Costello's Sagittarius and Centaurus tie the attack to feedback, so higher feedback brings a faster attack.
- **Sparse FIRs as noise.** Välimäki 2017: sparse velvet-noise FIRs only sound smooth above about 600 pulses a second (lowpassed) and about 2000 at full bandwidth.

### S1. The swell's taps are far too sparse, so long BLOOMs likely pulse (high, drums and skanks) — measure first
- **What our code does.** 24 taps over 1.25 × A. Tap spacing is 52 ms at A = 1 s, 104 ms at 2 s and 208 ms at 4 s, or 312 ms with MASSIVE's mass. Each tap repeats the input as smeared by the input diffuser, about 30 ms wide. (The diffuser and the taps are both linear, so their order doesn't matter.) That gives 5–10 taps a second against the 600+ that velvet-noise research needs.
- **Likely heard.** On sustained pads it's fine: the tank fills the gaps. On percussion with a BLOOM above about 1 s, the swell is a train of ghost hits rising in level. This is worst in HALL and PLATE, which have no in-loop diffusion to blur it, and gentlest in MASSIVE. The rule "below 40 ms the taps would bunch into a comb" shows the same structure was already a problem at the other end.

- **Measured (23 Oct 2026):** confirmed. A single hit through BLOOM flickers 3–7.5 dB (10 ms RMS around its smooth rise; HALL 7.5, MASSIVE 6.4, PLATE 4.6) against ~2 dB in a NORMAL tail. Smearing each tap with three allpasses sized to the tap gap (`IV_SWELL_SMEAR` 0.5) takes HALL to 5.7 dB, and with HALL in-loop diffusion 0.5 to 4.0 dB. Page: `renders/swell_smear`.

### S2. The attack isn't coupled to the tank (low; deliberate)
Supermassive's slow modes build *inside* the network: the energy grows as the diffusion recirculates. Our swell is feedforward, so it can be steered to an exact attack time (SPEC v1.0.1 chose this over "4 long modulated delays and allpasses" for that reason). Keep it. The fix for S1 is density, not topology.

---

## ERA: `core/dsp/Era.h` against the Midiverb, vintage converters and tape

### E1. Our "Midiverb" is much grainier than a Midiverb, and brighter in the wrong way (high, ERA's last stretch)
- **Source** (emeb/MIDIVerb_RE slides and emulator, MIT):
  - The Midiverb I runs at 23,437.5 Hz, with a 16-bit word and 13 bits of audio: a 12-bit ADC plus a faked lowest bit on the DAC. The converters are linear, with no companding. The input is mono.
  - The only coefficient is ½. Each halving is a shift that drops a bit, which gives grainy, stepped decays and a noise floor around −78 dB. It has 0.7 s of delay memory and no modulation in its reverbs.
  - Its typical program is six input allpasses at 0.5, then a single loop of three allpass-plus-delay blocks with a one-pole lowpass at 0.5 (about −3 dB at 2.7 kHz per pass).
  - The input filter is a 6th-order lowpass with a small presence lift before it falls off steeply above ~11 kHz (calculated from the SPICE netlist, approximate).
- **What our code does.** It reduces the word to 8 bits (12 → 8 across the stage) inside every one of the 8 FDN lines (or both plate branches), on every pass, through a 48 → 24 → 48 kHz box.
- **Likely heard.**
  - 8 bits is 30 dB coarser than the real unit's 13. Applied per line and per pass, our Midiverb stage sounds like a bitcrusher on the tail, not like a Midiverb.
  - What makes the real sound is mostly elsewhere: a dark loop (a heavy one-pole lowpass every pass), short loops, no modulation (so a slightly metallic ring), the presence-lifted 10 kHz input filter, and fine grain at the 13-bit level as the tail fades.
- **Docs.** SPEC §4.7 and ADR 0004 still say "TPDF dither", which changelog v1.0.3 removed. Fix the stale lines.

- **Measured and prototyped (23 Oct 2026):** at ERA fully CW, a HALL tail's share above 4 kHz is −6.6 dB today (the 8-bit grain is broadband) and −35.7 dB with 13 bits and the unit's ½ one-pole every pass (`IV_MIDIVERB_REAL`). Page: `renders/midiverb`. The stale "TPDF dither" lines in SPEC §4.7 and ADR 0004 are fixed.

### E2. The box is a filter in the loop (medium, measure)
- **What our code does.** The Midiverb box's halfband pair runs inside every line, on every pass. v1.0.3 moved ERA's darkening out of the loop for exactly this reason: a filter in the loop takes its loss on every pass. A brickwall ceiling is the same at any line length. But any passband ripple or droop below 10 kHz compounds: at 0.1 dB, 50 passes a second makes 5 dB a second.
- **Likely heard.** Short HALL lines may lose their upper mids faster than long MASSIVE lines under ERA's last stretch. Measure the box's passband (E7).

- **Measured (24 Oct 2026):** the passband doesn't show; two other things do.
  1. **The halfbands are flat.** The 48 → 24 → 48 kHz halfband pair, simulated sample by sample from `Oversampler.h`, is flat within 0.0002 dB up to 11 kHz (−3 dB at 12 kHz). No droop compounds.
  2. **The dark loop compounds with line length.** Since v1.0.16 the box also has the Midiverb's ½ one-pole on every pass, and that does compound with line length, as it does in the real unit (one low-pass per loop; the loop must fit the unit's 0.7 s of memory, per emeb/MIDIVerb_RE). With truncation off (`IV_MIDI_PART=1`), ERA 1 shortens the 4 kHz T60 to 0.23× in HALL SIZE 0, 0.28× at noon and 0.42× in MASSIVE SIZE 1.
  3. **The truncation is level-dependent.** On a quiet click the 13-bit truncation shortens every band (to 0.2–0.3× in the FDN, 0.55× in the PLATE). On music at page level, the tails at ERA 1 fall at DECAY's rate until about −55 to −75 dBFS, then truncate to silence: 40 dB down 0–2 s sooner than without truncation. That is the design ("grainier as it fades, true silence").
  4. **The prototype changed nothing.** One that rounds while above 16 steps (`IV_MIDI_KEEP`) made no difference at music level.
  **Dropped**, not paged; the owner chose this Midiverb on `renders/midiverb`.

### E3. μ-law is telephony; the vintage reverbs used gain-ranging (low; naming and flavour)
- **Source.**
  - The Lexicon 224 ran at 20 kHz with 12-bit gain-ranging converters (about 24 dB of ranging). The EMT 250 ran at 24 kHz with 12 bits, filtered at ~11 kHz.
  - Costello used Chamberlin's "floating point converters" to dial in VintageVerb's early-80s modes.
  - μ-law (G.711) is telephone companding.
- **What our code does.** It uses μ-law at 24 kHz, 12 → 10 bits, on the wet output. It's the same family: the noise follows the signal level. But gain-ranging steps its noise in 6 dB bands, while μ-law's noise floor slides smoothly with the level.
- **Likely heard.** The difference is subtle. It's worth a listen if ERA's μ-law stretch sounds like "telephone" rather than "1980s rack".

- **Measured (24 Oct 2026):** subtle, as predicted.
  - **Sources.** The 224X service manual gives "14-bit floating point" converters: a 12-bit mantissa and a 2-bit exponent. The original 224's 20 kHz, 12 bits and ~24 dB of ranging come only from secondary sources.
  - **Prototype.** `IV_MU_MODE=2`: a gain-ranging converter with four 6 dB ranges, at the μ-law layer's own bits and rate.
  - **Noise.** Measured against the layer's band limit alone, HALL ERA 0.8, on music. μ-law sits at −52.6 dB re the signal, steady. Gain-ranging sits at −59 to −61 dB while the music is loud, rising to −31 dB as the tail fades past −55 dBFS.
  - **What that means.** While the music is loud, both are 50–60 dB under it. In the fading tail, the difference sits below −80 dBFS.
  - **Dropped**, not paged: the owner hasn't heard "telephone" in M6. The hook comes out after commit 5ea65e8.

### E4. The vintage feel Costello says mattered most is missing: noisy, quantised modulation (medium, idea)
- **Source.** Costello ("Algorithmic Reverbs, Distortion, and Noise", 2013 edit) says the noisy modulation source mattered most. The 224's multiplier quantised linear interpolation to about 32–64 steps, which put a "halo" of noise round the tail. Linear interpolation gives a less "glassy" top. VintageVerb's 1970s and 1980s modes rely on this.
- **What our code does.** It has none of it: ERA changes bits, rate and saturation, not the modulation or the interpolation.
- **Likely heard.** ERA's middle (μ-law) could gain that "old rack" halo cheaply with coarse interpolation steps plus noise on the modulation. This would be new work, so it needs an ADR.

- **Measured (24 Oct 2026):** shows: today has almost no halo. Through HALL, a held 1 kHz tone's energy off the tone (0.2–10 kHz, re the tone) is −39 dB at ERA 0.65 and 0.8, and −29 dB at ERA 1 (the Midiverb's grain).
  - **Sources.** Costello, "Algorithmic Reverbs, Distortion, and Noise", read through the Wayback Machine:
    - The 224's multiplier quantised its linear interpolation: "I think it was 32 or 64 chunks per sample" (his guess). That put a halo of noise round the tail.
    - His 2013 edit says the noisy modulation source mattered more ("The modulation in my PCM70 is noisy as all get out").
    - VintageVerb's 1970s and 1980s colour modes use it.
  - **Prototyped.** `IV_VINTAGE_MOD`, FDN only, weighted by the μ-law layer, so it runs from ERA ~0.65 to the end of the knob.
    - The reads crossfade to linear interpolation with the fraction quantised to 32 steps.
    - Each line's read gets a fresh random offset every control block (0.15 samples, smoothed once).
    - The 32 steps follow Costello. The jitter's size is ours, tuned on this measurement: 0.5 samples gave −20 dB (too much), and the quantised steps alone barely register (−37 dB).
  - **Result at ERA 0.8.** Halo: HALL −39 → −30 dB; MASSIVE −42 → −39 dB (long lines, so fewer passes). Band T60s stay within 3 %, except 8 kHz in HALL at −14 %: linear interpolation's dulling, which Costello also describes.
  - **Energy.** Sweep at ERA 0.8 (135 settings):
    - Build-up: none, except a small HALL at infinite DECAY, up to +5 dB re today in its first seconds, before both settle within 1.6 dB of each other.
    - Finite decay slopes: within −0.9 … +3 % (MASSIVE lows −4 %).
    - Holds: 6/6 pass in the FDN at ERA 0.8 and 1. The PLATE isn't touched (see the side findings).
  - **Cost.** +1.8–2.6 % of a desktop render. It glides out while frozen, with the rest of the loop's colour.
  - **On the page** (the vintage cards). If picked, it would need an ADR (new ERA content).

### E5. Cassette: memoryless saturation, and taped on every pass (medium)
- **Source.** ChowTape and Chowdhury (DAFx 2019) credit hysteresis for most of tape's warmth. Hysteresis is a saturation with level-dependent memory. The other ingredients are a head bump, speed-dependent high-frequency loss, wow below ~4 Hz, flutter at 4–20 Hz, and hiss. Costello's VintageVerb "chaotic" modes do put tape-like saturation and wow inside the loop, and they found that it *reduced* ringing. So in-loop tape has a precedent.
- **What our code does.** A memoryless rational soft clip, drive ×1.4, inside the loop. Darkening through the T60 share, not a filter. Wow at 0.3–0.8 Hz, flutter at 6–8 Hz. Hiss into the tank. No head bump, because a boost in the loop breaks rule 2.
- **Likely heard.** Clean and controlled, but closer to a soft limiter than to tape's thick, smeared warmth. The head bump could live on the wet output, outside the loop, where rule 2 doesn't apply.

- **Measured (24 Oct 2026):** stronger than the audit guessed: the in-loop drive does nothing on music.
  - **The soft clip never engages.** Today minus the same render with the soft clip bypassed (`IV_SAT_BYPASS`) is bit-identical on the pad, the drums and the full mix: every SPACE, ERA 0 and 0.55, DECAY 0.4, 0.75 and 0.95. Only a 1 s burst of −10 dBFS RMS noise at DECAY 0.75 reaches the knee (a −39 dB share). So the tape stage is darkening, wow and flutter, hiss and compression, with no saturation.
  - **Head bump: not built.** Chowdhury's DAFx-19 paper and the ChowTape manual give no frequency. Every tape loss scales with speed (k = 2πf/v), so a bump near 50–100 Hz at 7.5–15 ips moves two to three octaves down at cassette speed, to the bottom of the audio band or below it [inference].
  - **Prototyped instead:** `IV_TAPE_OUT`, the cassette saturating the wet on its way out, outside the loop, so there is no energy risk.
    - An asymmetric tanh (even and odd harmonics, unity small-signal gain), its drive rising 1 → 3 with the cassette layer.
    - Run at 2× oversampling, then a 5 Hz DC blocker.
    - Precedent: dub's tape send and return, and ChowTape used as an insert. The curve and the amounts are ours.
  - **Result at ERA 0.55.** Distortion share, measured against the same 2× path left linear: −24 to −26 dB re the wet on the pad, bass and drums, −35 dB on the quieter full-mix send. Level within 0.2 dB.
  - **Cost.** +1.2 % of a desktop render.
  - **On the page** (the tape cards).

### E6. Neve in the loop: ours
Even harmonics inside a reverb loop have no published reverb basis. It's the dub desk's send/return, recirculated. Its DC and runaway handling (v1.0.3) is sound engineering. There is nothing to compare it with.

- **Measured (24 Oct 2026):** no experiment: it's ours, with no reference to compare it against, and the tests already cover its stability (`test_era`: infinite holds at every ERA). One note from E5's measurement: Neve's even term goes in ahead of the soft clip and is proportional to the level, so unlike the clip it does act at normal levels. It fades out at infinite DECAY by design.

---

## REVERSE: `core/dsp/Reverse.h`

**There is no published basis for reversing the tank's output in segments, which is how prototypes A and B work.** It's the obvious real-time idea, but it has two built-in problems. It cuts the sound at every segment boundary, and it plays a decaying tail backwards, so each segment starts quiet and noisy and rises to a cut. A rise and a cut every TIME interval is the "choppy and disjointed, chunky" the owner heard in both rounds.

**What the sources and hardware describe instead.**
- **A rising envelope, then a gate.** Classic "reverse" reverbs (AMS RMX-16 Reverse, Lexicon "inverse" programs, VintageVerb's Nonlin, which morphs from truncated through gated to "huge reverse" with Size setting the length) are feedforward: dense reverberation with a *rising* envelope, then a gate. Nothing is played backwards. This rests on Costello's description of Nonlin plus general knowledge of those units. I didn't find a primary paper on any of their internals. **[inference]**
- **Velvet-noise FIRs.** Välimäki 2017 shows how to build that kind of dense, smooth FIR cheaply with velvet noise, at about 600–2000 pulses a second.

### R1. Prototype C (swell to a cut) has the right topology, built from the too-sparse swell (high)
- **What our code does.** C reuses the 24-tap swell (S1): the tank's tail rises over TIME and then lets go with a 0.3 s T60. The structure matches the hardware. Its density doesn't: at TIME 2 s the taps are 104 ms apart.
- **Likely heard.** A reverse swell that steps up in pulses rather than sucking in smoothly. Fixing S1 should fix C as well.

- **Measured (24 Oct 2026):** resolved by the rebuild. REVERSE is now the hardware recipe (32 jittered rising taps per side into a short reverb of its own; ADR 0015 amended), not the 24-tap swell. Measured on a single hit: the 10 ms envelope's deviation around its own 150 ms smoothing, from onset to peak. REVERSE gives 0.6 dB at TIME 0.6 and 1.4–1.5 dB at TIME 1 (2 s), in every SPACE. For comparison, a NORMAL tail gives 0.4–0.7 dB and the smeared BLOOM 0.3–0.4 dB. No pulses to fix.

### R2. Prototype D (the input reversed into the tank) is like reverse tape (medium)
- **What our code does.** D reverses the input and sends it into the tank, which is closer to a reverse-tape effect than to a reverse reverb.
- **Likely heard.** It still chops the input at TIME boundaries, so phrases get cut mid-note. That makes it more of an effect than a "reverse swell".

- **Measured (24 Oct 2026):** obsolete: prototype D is gone from the code (REVERSE is the rebuilt hardware recipe, R1). Nothing to measure.

---

## Our own inventions (no published basis; judged by ear and by test only)
- The floor governor and its hold, the twin and paint, and ADR 0008's rule-2 exception for held tails.
- The swell FIR (BLOOM, MASSIVE's mass, REVERSE C).
- MASSIVE's early taps.
- SIZE gliding as a pitch bend.
- True stereo into the Plate (and its image, P1).
- Neve and cassette inside the loop. (VintageVerb's chaotic modes are the nearest precedent for the cassette.)
- The Midiverb box per line.
- μ-law on the wet output.
- REVERSE prototypes A, B and D.

None of this is wrong because it's unpublished. But these are the parts with no outside reference to check them against, and so far they're where most listening notes have landed.

---

## Side findings from the 24 Oct 2026 measurements (not fixed; worth a look)

- **A hot feed can make an infinite PLATE sag, then climb back.**
  - **What happened.** Fed 20 s of Gaussian noise at −10.5 dBFS RMS (peaks past full scale) at DECAY 1, today's PLATE dropped to between −34 and −48 dBFS by 60–70 s. The hold makeup then lifted it back to about −25 dBFS over the 10 minutes (+8 to +24 dB). HALL at SIZE 0 and noon did the same, by +2 to +4 dB.
  - **With the tests' feed** (uniform noise, 0.3 peak), every SPACE passes.
  - **Why it matters.** A Versio input driven hard could give a tail that fades, then swells back over minutes.
  - **Next.** Measure before fixing; the likely cause is the duck or the tail capture ratcheting during the loud feed.
- **PLATE SIZE 1 at ERA 1 fails the 10-minute hold**, even with the tests' own feed: −29.8 dBFS at 60–70 s, then up to −27.1 (+2.7 dB; the test allows +1). `test_era` only checks the default SPACE (HALL), so the test doesn't catch it.
- **The loop's soft clip never engages on music** (E5 above): the cassette's drive is inaudible. That also removes MI #5's reason (`mutable-instruments-reverbs.md`).

---

## Suggested experiments, ranked

All of these are desktop first (Renderer, then Plugin), one variable at a time, rendered 100 % wet with `--wet-only --dry` on a `make_review.py` page. They need no ParamSpec changes unless noted.

1. **E1, swell density (S1, R1).** Measure first. Render a dry snare and a skank through BLOOM at TIME 1, 2 and 4 s in HALL, PLATE and MASSIVE. Plot the envelope and count the pulses, and check whether the owner hears "ghost hits". Then compare: (a) 24 taps now; (b) 96 taps with velvet-style jitter; (c) 24 taps, each fed through a long low-coefficient allpass chain (100–300 ms total at 0.5–0.6, per Barr and Costello's Bloom), so each tap becomes a cloud as wide as the gap. Cost: (c) is cheap in memory; (b) costs CPU, so check `firmware profile`. Then use the winner for REVERSE C.
2. **E2, TONE shape (A1).** Replace the one-pole loss with Zita's low shelf × lowpass, or a biquad, with a high crossover around 3–5 kHz. Keep the DC gain and the high-end ratio the same. Measure the T60 per octave band, then do a blind A/B on a long HALL tail at TONE noon and at the quarter points. Optionally add a low band (a bass multiply). It would need an ADR if it reaches the panel's meaning.
3. **E3, plate image (P1).** Pan a click hard left and measure the left/right energy in the first 50 ms of the PLATE wet. If it lands mostly right, render it as it is, with L and R swapped into the branches, and with L+R summed (Dattorro) for the owner to hear on a panned hi-hat.
4. **E4, HALL in-loop diffusion (F1, F2).** Turn on HALL's two in-loop allpasses per line at 0.3–0.6, scaled with SIZE. Measure the echo density over time and the vibes ringing score (the M1 metric) at SIZE fully CCW with a long DECAY, with `kHallSmallT60Scale` at 1.0. Does the buzz go without shortening the tail?
5. **E5, Midiverb fidelity (E1).** A variant of the last ERA stretch with 13-bit truncation instead of 8, a one-pole loop darkening that ends near a ½ coefficient at 23.4 kHz (applied as a T60 share, as v1.0.3 does), the presence-lifted input filter on the tank's input only, and the modulation floor off. Compare it with the current box in a demo. The owner's ears decide whether "authentic" or "crunchy" is the goal.
6. **E6, freeze modulation (A3).** Freeze a chord in HALL and MASSIVE for 5 minutes with the floor at ×1, ×0.3 and ×0, with DRIFT at noon. Track the spectral peak spread over time, then hold a showcase listen.
7. **E7, box passband (E2).** Measure the 48 → 24 → 48 box's magnitude response below 10 kHz, then the T60 per band in HALL fully CCW against MASSIVE fully CW with ERA at 100 %. If the short lines drain faster, flatten the passband or compensate through the T60 share.
8. **E8, low-band modulation (A4).** Split each FDN line at about 400 Hz and modulate only the upper band. Listen with DRIFT at both ends on a dub bassline. It costs one one-pole per line.
9. **E9, interpolation grit (A2).** With DRIFT fully CCW and cassette at full, measure the noise above 10 kHz on a held sine in the tail, comparing allpass reads with cubic reads. Add Dattorro's warped allpass only if it shows.
10. **E10, vintage modulation (E4).** A quantised-interpolation (32–64 steps) plus noisy-modulation layer in ERA's middle stretch, compared with the present μ-law-only layer. This would be new ERA content, so it needs an ADR first.

**Doc fixes, no listening needed:** SPEC §4.3 and §5.1 say "cubic" (it's allpass since v1.0.2). SPEC §4.7 and ADR 0004 say "TPDF dither" (removed in v1.0.3).
