# Still lows, done safely (23 Oct 2026)

**Shipped as SPEC v1.0.18** (owner: B on all 25 cards of `renders/still_lows` round 2): mode 4, two tanks, now the default; the other modes are gone from the code. `IV_STILL_LOWS=0` (desktop) brings back one tank for demos.

The owner picked "still lows" on every MASSIVE card and on HALL Cathedra (23 Oct, B: "the frequency response feels more even and balanced"). The bass in a moving tail stays put; above it, the tail moves as before. The first prototype wasn't energy-safe (dsp-audit.md, A4). This note covers how established designs handle the problem, why the first prototype failed, and the candidates, each measured.

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## Sources

- **Schlecht, thesis (FAU Erlangen, 2018), ch. 6, pp. 98–103** ([PDF](https://theses.eurasip.org/wp-content/uploads/sebastian-jiro-schlecht-feedback-delay-networks-in-artificial-reverberation-and-reverberation-enhancement.pdf)); the JASA paper (Schlecht & Habets 2015, 138(3):1389–1398) is essentially this chapter.
  - [S] In informal listening, modulating the lows sounded wrong: low tones pick up sidebands and sound detuned, and the bass seems to "drift".
  - [S] So he splits each line's output at ~400 Hz with a one-pole lowpass, and the high band is the signal minus that lowpass. Only the high band goes through the moving part. He modulates the feedback matrix, not the delays.
  - [S] He proves stability only for unitary matrices without the split, and lists "which frequencies to modulate" as an open question.
  - [S] Matrix modulation spreads a tone into narrow noise around its pitch (shimmer and image motion, no pitch glide). It is smooth up to ~4 Hz, and the rates are spread ±50 %.
- **Two-tank designs.** Each band is its own reverb: [S] Barr II, a dual-band reverb after Keith Barr's design, splits the input by filters into a low reverb and a high reverb ([patchstorage](https://patchstorage.com/barr-ii-a-dual-band-reverb-based-on-keith-barrs-design/)). [S] Eventide's H90 Hall is a "3-band crossover reverb network" (crossovers at 300 Hz and 1.5 kHz, a decay per band) ([manual](https://cdn.eventideaudio.com/manuals/h90/1.9.4/content/algorithms/reverb.html)). [S] Logic's PlatinumVerb splits its input into two bands "for separate processing" ([Apple](https://support.apple.com/en-mz/guide/logicpro/lgcef2969436/mac)). Neither manual says whether the bands are modulated differently.
- **Valhalla** (valhalladsp.com blocks fetchers; search snippets only): [S] VintageVerb's "Random Space" randomises delays "without the pitch change" of its chorused modes, and UberMod's history cites the Roland Dimension D for keeping the bass intact.

## Why the first prototype built up

[S/M] It used Schlecht's split, a one-pole lowpass L plus its complement H = 1 − L. The two bands' *magnitudes* add to more than 1: at the crossover |L| + |H| = 1.41. When the still and moving reads drift apart in phase, the bands stop cancelling, and a pass gains up to +3 dB. The research agent worked out +3.01 dB for this filter; we measured +3 dB a pass. In our sweep it built up to +83 dB in a small HALL before the limiter, and passed only 2 of 18 infinite holds.

## The condition that makes a split safe

[I, checked numerically] Call the still read S and the moving read M. Both are delays of the same line, so their spectra have the same magnitude |X|. Split them with filters whose magnitudes add to exactly 1 at every frequency. Then a pass can't gain, however far apart the reads drift:

|L·S + H·M| ≤ |L|·|X| + |H|·|X| = |X|.

Linkwitz-Riley crossovers do this, because they are Butterworth filters squared and Butterworth pairs are power complementary: |L| + |H| = |B_L|² + |B_H|² = 1. That covers LR2 (high band inverted) and LR4. A merely power-complementary pair (|L|² + |H|² = 1) still allows √2, so it isn't enough.

**The cost:** where the reads are out of phase near the crossover, the two bands cancel instead of adding. That's a moving loss in the crossover band, so it decays faster there.

## Candidates

1. **Two reads with a Linkwitz-Riley split** (out = LR_L(still) + LR_H(moving)): our own variant of Schlecht's split. One extra read per line, plus 4 biquads per line for LR4; no extra memory. Measured below.
2. **Two tanks** (the Barr II / H90 pattern): an unmodulated low tank, possibly at a decimated rate, beside today's moving tank fed the highs. Stable by construction. But freeze, the twin, the governor, ERA and SIZE would all need a second copy, so it costs a lot more code and flash (~12 KB left).
3. **Matrix modulation on the highs** (Schlecht proper, with an LR split): it shimmers rather than wows, so it can't deliver DRIFT's tape side without redesigning DRIFT.

## Measurements (23 Oct 2026)

Desktop, `IV_STILL_LOWS=<mode>` (`IV_STILL_HZ` sets the crossover). The energy harness rendered a 1 s noise burst for every SPACE × SIZE 0/0.6/1 × DECAY 0.5/0.75/0.95 × DRIFT 0/0.25/0.5/0.75/1, and compared each mode's level with today's (full band, < 250 Hz, 600 Hz–2 kHz) and with its own earlier windows (build-up). Holds: fed hard for 20 s at infinite DECAY, then 10 minutes, judged as in `test_massive` (within 3 dB, no growth). Band T60 is the analyzer's band decay on a click (DECAY 0.6). Wobble: a held 55 Hz into SIZE 0.6 / DECAY 0.7, tracked by `tools/pitch_track.py` (p95 deviation).

| Mode | Build-up (worst, vs today) | Infinite holds | Decay per band | 55 Hz wobble, DRIFT CW (HALL / MASSIVE) |
|---|---|---|---|---|
| today | — | 18/18 pass | — | 191 / 51 cents |
| 1: one-pole split (first prototype) | **+83 dB** (small HALL) | 2/18 pass | — | 17 / 21 |
| 2: two reads, LR2 at 400 Hz | none over today (≤ +0.1 dB full band) | **0/18**: they drain (down to −17 dB) | 250 Hz–2 kHz up to **−79 %** | 3 / 16 |
| 3: two reads, LR4 at 400 Hz | none | 14/36 (they drain) | 500 Hz up to −57 % | — |
| 3: LR4 at 200 / 150 Hz | none | — | with DRIFT turned: 125–250 Hz up to −57 / −63 % | — |
| **4: two tanks, 300 Hz** | none (≤ +0.3 dB full band) | **30/30 pass** | every band within 7 % | **7 / 14** (the same at either DRIFT end) |

What the table shows:
- [M] Modes 2 and 3 are safe from build-up, as the magnitude rule predicts. But the reads drift apart by up to DRIFT's 70 ms, so near the crossover they cancel on every pass, and that band dies early. A lower crossover only moves the hole down into the bass. Infinite holds drain; the hold makeup can't keep up.
- [M] Two tanks behave like today in every band. The bass no longer depends on DRIFT at all.
- [M] Two-tank details that matter:
  - The tanks' outputs are uncorrelated, so they add in *power*. The split therefore has to be power complementary (a Butterworth pair), not an LR4: with LR4, chords came out 2–3 dB light around 250 Hz.
  - With a Butterworth pair, every golden is within ~1 dB of loudness at ERA clean.
  - A small MASSIVE at high DECAY can sit up to 3 dB louder below 250 Hz at moments. That is a level difference, not growth.
- [M] ERA: the Midiverb box (48 → 24 → 48 kHz plus its ½ one-pole) would darken the low tank's lows every pass at 12 kHz, so the low tank keeps only the 13 bits (the 10-minute hold at ERA 1 then passes).
- [M] Two issues remain if this ships:
  - At finite DECAY the Midiverb's truncation eats a quieter (highs-only) main tank sooner: `hall_midiverb`'s tail is 10–20 % shorter in every band.
  - ERA's loudness spread across the knob widens from 3.9 to 4.4 dB (`test_era` allows 4).
  - The page uses ERA clean.
- Cost [M/I]:
  - Release firmware 97,692 → 101,132 B, with every prototype mode still compiled in. `std::tan` cost ~3 KB, so it was replaced by the series the loss shelf already uses (`bilinearK`).
  - Delay memory: + one tank at ¼ rate in SDRAM.
  - CPU [I]: a quarter of an FDN plus 8 biquads a sample. M3 would measure it.
- Not covered by the prototype: the freeze twin still moves its lows (mode 4 changes only the main tank).

[I] Our own invention within the two-tank pattern:
- decimating the low tank ÷4;
- making it follow the main tank's SIZE, DECAY and governor (its level is fed into the main tank's governor, and the main tank's governor loss is applied to it);
- the Butterworth power split.
