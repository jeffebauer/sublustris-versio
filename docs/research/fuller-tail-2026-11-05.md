# A fuller, sweeter tail: why it reads "thin/reedy", and three fixes

*5 Nov 2026, branch `claude/fuller-tail-6dcc8d` (off `claude/proto-wear-taper-78e1da`, v1.0.70). Measurement plus one
renderer-only prototype hook (`IV_FULLER`, off by default; the firmware is byte-identical). Page: `renders/fuller_tail`.
Tools: `tools/fuller_tail/` (`ft_lib.py` has every definition; `measure.py` our cases; `valhalla.py` ours against the
owner's takes; `sweep_mod.py` the modulation sweep; `checks.py` the safety checks; `page.py` the page). The Valhalla
takes were read in memory from the main checkout's `test_audio/reference/incoming/` (the clips of the owner's Ableton
set, `Immersio Versio Testing.als`, extracted on 23-25 Oct; the set has no newer takes). Nothing of them is copied or
committed (ADR 0006).*

Tags: **[M]** measured here on the desktop renderer, **[L]** literature, **[E]** estimate or inference.

## What was said

- **lfad on Modwiggler (4 Nov):** "on the thin/reedy side of things, overall, compared to others… I personally wouldn't
  object to a little more body… it sounds thinner, wispier than [Desmodus and Pleroma]". PLATE and HALL "pretty similar".
- **The owner:** it doesn't sound reedy "unless I've got radio turned up", and wonders whether it comes from avoiding
  metallic tones: "fuller and sweeter without being muddy or metallic".

## The short answer

1. **A hole in the body (HALL and CANYON).** [M] Below 300 Hz the tail rings in the still-lows tank, above it in the main
   tank (v1.0.18, `docs/research/still-lows.md`). The split is a 2nd-order Butterworth pair, chosen because two
   *uncorrelated* tanks add in power. But the two tanks have the same lines, matrix and taps, and the low one isn't
   modulated, so for seconds they ring nearly in step, and a Butterworth low/high pair summed in step cancels at its
   corner (its two halves are 180° apart there). Pink noise, still lows on minus off: CANYON −7.5 dB at 250 Hz in the
   first 2 s, −5.9 over 2-6 s, −2.9 over 6-20 s, the hole spanning 125-500 Hz; HALL −1 to −3 dB at 100-200 Hz early
   (its deeper modulation decorrelates the tanks sooner). On music, CANYON 15 s carries −3.0 dB of low-mids (150-500 Hz,
   re 1 kHz, wet against dry) where Supermassive carries +0.4; HALL 5 s −1.4 where Room carries −0.3 / +0.3. This is
   the "body" the listener misses, and it is not a decay-time or EQ choice: it is a cancellation.
2. **Reedy lines in the upper mids (HALL and CANYON).** [M] After the playing stops, our tails hold the notes' upper
   partials (1.6-6.4 kHz) as narrow, steady spectral lines (bins over +10 dB against the third-octave-smoothed
   spectrum). HALL holds 3.2-6.8 ‰ where Room holds 1.0-2.4; CANYON 12.8-14.5 ‰ where Supermassive holds 0.9-1.8. They
   move with each piece's notes, so they are the notes ringing on, not fixed resonances of the tank. What smears them
   in the references is pitch movement: Supermassive moves 29-35 cents (valhalla-comparison.md), Room's partials spread
   13-20 cents. Ours is the modulation floor, a fixed 0.46 ms in seconds: on CANYON's 0.3-2 s lines that barely moves
   anything (a held 1 kHz wavers 0.7 cents p95). A narrow-peaked upper-mid spectrum is the "reedy, nasal" picture
   [L, see below].
3. **PLATE is not the problem.** [M] PLATE measures in line with Valhalla Plate: its low-mids sit within 1 dB of
   Chrome's and Lithium's, and it has fewer upper-mid lines (4.4 ‰ against their 12.4-12.6, which are plates being
   plates: "accept the metal and call it a plate", Costello). No version touches it.
4. **The radio.** [M] MOTION right of noon narrows the band on purpose (ADR 0036): at MOTION 0.75 HALL loses 2.6 dB of
   lows and 2.4 dB of highs (re 1 kHz) against noon, CANYON 2.6 / 2.5. It doesn't add peaks (the line counts are the
   same), so it is thinner but not reedier; the owner's "reedy with the radio up" is the narrow band itself.
5. **What it isn't.** [M] Not the bass decay: our bass ratio (Beranek's BR, T60 at 125 + 250 over 500 + 1k) is already
   at or above the references' (HALL 0.99-1.15 against Room 0.96-1.08; CANYON 1.09 against Supermassive 0.74-1.37;
   PLATE 1.12-1.19 against Valhalla Plate 1.20-1.37). Not a high-mid lift: our 2-4 kHz balance sits within 0.5 dB of
   Room's. And the literature says perceived bass follows low-frequency *level*, not low T60 [L: Bradley, Soulodre &
   Norcross 1997], so a longer bass T60 would be the wrong lever, and the riskier one (below).

## Research: metallic versus thin

**What prevents metallic ringing** [L]
- *Modal density.* An FDN has as many modes as samples of total delay, spread evenly in frequency. Schroeder & Logan:
  0.15 modes/Hz suffice at T60 = 1 s, rising in proportion to T60, because each mode's width is about 2.2/T60 Hz
  (J. O. Smith, *Physical Audio Signal Processing*, "Mode Density Requirement",
  https://ccrma.stanford.edu/~jos/pasp/Mode_Density_Requirement.html). Dattorro asks for eigentone densities "to
  approach 3 per hertz" (Dattorro, "Effect Design Part 1", JAES 45(9), 1997, p. 661). Modal overlap = density x 2.2/T60.
- *Delay lengths.* Mutually prime lengths keep the response from repeating (PASP, "Choice of Delay Lengths"); short
  lines carry the most energy and "produce strongly audible metallic-sounding comb peaks", and fewer lines colour more
  (Dal Santo, Prawda, Schlecht & Välimäki, "Optimizing tiny colorless feedback delay networks", EURASIP JASMP 2025,
  arXiv 2402.11216).
- *Even excitation.* Colouration is a few modes with large residues; Jot & Chaigne's delay-proportional loss keeps
  every mode decaying together, which "prevents single modes from ringing past the overall decay" (Heldmann &
  Schlecht, "The role of modal excitation in colorless reverberation", DAFx20in21 / Applied Sciences 2021).
- *Diffusion.* Allpasses longer than about 10 ms sound like combs (PASP, "Schroeder Reverberators"); short allpass
  chains ring on pulse-like input, so halls use fewer input allpasses or modulated ones (Costello, "Reverbs: diffusion,
  allpass delays, and metallic artifacts", valhalladsp.com, 2011).
- *Modulation.* ValhallaRoom's late section uses many randomised LFOs: around 0.5 Hz they "smooth out any artifacts in
  the decay", above 1 Hz they chorus (Valhalla's control notes, quoted by Splice); VintageVerb's "Random Space" reduces
  "metallic artifacts without the pitch change" of chorusing. For long T60s, modulation is the only practical way to
  reach Schroeder's overlap: at 15 s it would take ~20 s of total delay.
- *The decay ratio between bands.* My own working from the above [E]: modes are as dense at the lows as at the mids, so
  a band whose T60 is 1.5x the mids' has 1.5x narrower modes and 1.5x less overlap; a longer bass or low-mid T60 needs
  more delay memory or more movement to stay smooth, and raises that band's level by about 10 log10(1.5) = 1.8 dB
  unless corrected (Jot's tonal correction filter, PASP "Tonal Correction Filter").

**What makes a tail thin** [L]
- *Low-frequency level, early and late.* Bradley, Soulodre & Norcross (ASA 133rd, 1997): "low frequency reverberation
  time was not found to be important"; early and late low-frequency *levels* were, listeners being more sensitive to
  the late one (https://acoustics.org/pressroom/httpdocs/133rd/4aaa6.html). Beranek's bass ratio (best halls BR > 1;
  1.1-1.25 often quoted for long-RT halls) is the older measure, and his 2011 "bass index" moved to level (G125
  against mid-band G).
- *How units expose it.* Lexicon's PCM 80: low RT as a multiplier of the mid, "1.5X or less" for a natural hall, the
  crossover two octaves above the boosted band (User Guide p. 3-36). Eventide H9 Hall: a 3-band crossover network at
  300 Hz / 1.5 kHz with separate decay and *level* per band (Algorithm Guide p. 4). Eventide Blackhole: shelves on the
  tail at 350 Hz and 2 kHz. ValhallaRoom: Bass Mult and Bass Xover. Strymon BigSky's Low End: "more low frequencies
  reverberate". Mutable Instruments' Clouds and Rings: a Griesinger/Dattorro loop with one in-loop low-pass and no
  low-band damping at all (pichenettes/eurorack, `clouds/dsp/fx/reverb.h`, MIT). Jot and PASP separate the decay
  filters from a "spectral coloration equalizer" on the input or output, which sets level without touching mode widths.
- *Desmodus and Pleroma.* Desmodus Versio (Noise Engineering) puts a DJ filter in the feedback loop (low-pass left,
  high-pass right, off at noon), a limiter and a little distortion in the tank, and calls itself "more of a synthetic
  tail generator" (NE manual and blog). Pleroma is Recursive Field Instruments', not NE's: 16 lines as four 4x4 Hadamard
  FDNs, a 3-band decay plus wet EQ ("the center adds a gentle midrange bloom"), and "even subtle delay modulation
  suppresses static resonances" (recursivefieldinstruments.com/pleroma). Neither publishes its damping; there are no
  takes of either, so they are not measured here.
- *"Reedy" / "nasal".* No source ties "reedy" to a band. Nasality in speech is a weaker first formant and an extra
  resonance near 2 kHz (Fant), cues spread over 1-3 kHz; in reverb design, sparse narrow peaks are what is called
  metallic or ringing (PASP; Dal Santo 2025). [E] Read together with the listener's words: low-mids under the mids plus
  narrow peaks in the 1-3 kHz region, which is what was measured.

**The distinction, for this design.** The things that prevent metal (prime lengths, delay-proportional loss, the
modulation floor, a short loss shelf) are in place and work: no tank rings on its own. The thinness comes from two side
effects: the still-lows split cancelling the body it was meant to steady, and a modulation floor so gentle (to keep
notes still) that the notes' upper partials hang on as lines. Neither needs an EQ or a longer bass.

## Measurements

Method [M]: renders at MIX 100 %, WEAR 0, SIZE and DEPTH at their defaults, TONE noon. *Balance:* the wet's long-term
third-octave spectrum against the dry's, re 1 kHz, grouped (lows 50-125 Hz, low-mids 160-500, mids 630-1.6k, upper mids
2-4k, highs 5-10k). *Body:* in the tail (0.3-2.3 s after the last note; 3-6 s for a held endless tail), dB(150-500 Hz)
minus dB(1-4 kHz), against the same for the dry. *T60:* octave bands from a click, Schroeder backward integral, T20 x 3.
*Colour:* the fine spectrum against its 1/3-octave-smoothed self, 200 Hz-8 kHz: std (a Gaussian tail: 5.57 dB, 10/ln10
x pi/sqrt 6), per mille of bins over +10 dB (Gaussian ~0.05), and *upper peaks*, the same count over 1.6-6.4 kHz. White
noise with each tail's envelope reads std 5.6, 0.0-0.1 ‰.

### Our spaces (v1.0.70; pad, piano, vibes, drums)

Balance in dB re 1 kHz; body in dB re the dry; drums' tail colour; T60 at 63 Hz-8 kHz (DECAY noon only).

| Case (SPACE, DECAY, MOTION) | lows | low-mids | mids | upper | highs | body | drums std / ‰ > +10 dB | T60 63…8k (s), BR |
|---|---|---|---|---|---|---|---|---|
| PLATE noon, noon | +1.7 | +0.8 | +0.2 | −0.8 | −2.5 | +13.7 | 7.02 / 7.4 | 5.8 6.0 5.8 5.5 4.9 4.2 3.3 3.0, 1.12 |
| PLATE noon, MOTION 0.75 | −0.9 | −0.9 | 0.0 | −2.2 | −4.9 | +12.2 | 7.02 / 7.3 | (same), 1.10 |
| PLATE endless | +2.8 | +2.9 | +0.2 | −2.2 | −4.5 | +10.2 | 7.29 / 6.5 | |
| HALL noon, noon | +3.2 | −0.2 | +0.4 | −0.3 | −2.9 | +5.9 | 6.51 / 4.8 | 8.5 8.3 8.2 8.4 8.2 7.8 6.6 4.8, 0.99 |
| HALL noon, MOTION 0.75 | +0.6 | −1.8 | +0.2 | −1.7 | −5.3 | +4.5 | 6.50 / 4.8 | (same), 0.97 |
| HALL endless | +2.6 | +1.3 | +0.3 | −1.4 | −6.2 | +4.5 | 6.22 / 2.8 | |
| CANYON noon, noon | +0.6 | **−2.8** | +0.3 | +0.6 | −0.9 | +1.7 | 7.06 / **9.7** | 37 40 40 37 37 36 37 37, 1.09 |
| CANYON noon, MOTION 0.75 | −2.0 | −4.5 | +0.1 | −0.8 | −3.4 | +0.5 | 7.07 / 9.7 | (same), 1.11 |
| CANYON endless | −0.6 | **−3.7** | −1.6 | −1.2 | −2.3 | −1.6 | 6.94 / 8.8 | |

- CANYON's low-mids are the only band group well under the mids, in every case; its body (the tail's low-mid share
  against the dry's) is 4-12 dB under HALL's and PLATE's.
- MOTION at 0.75 (the radio half in) takes about 2.5 dB off the lows and the highs and changes no line count.
- The drum tails' colour is above a Gaussian wash's everywhere (5.6 dB, ~0.05 ‰), most in CANYON. The pad's: PLATE
  6.6 ‰, HALL 5.8, CANYON 13.7.

### Against the Valhalla takes (the review-2 matched settings; the kit's music)

The same measures on the kit's pieces (vibes, stabs, pad, drums, keys), ours rendered from the kit at review 2's bisected
1 kHz matches (WEAR 0, DEPTH 0.2, TONE and MOTION noon). T60 from the pieces' tails (median); upper peaks = ‰ of bins over
+10 dB at 1.6-6.4 kHz, the mean of the drums, pad, stabs and keys tails.

| | low-mids | body | T60 125 / 1k / 8k (s) | BR | drums ‰ > +10 dB | pad ‰ | upper peaks ‰ |
|---|---|---|---|---|---|---|---|
| **HALL 5 s** | **−1.4** | +5.9 | 6.4 / 5.5 / 3.7 | 1.15 | **7.0** | 7.3 | **6.8** |
| Room Large Room | −0.3 | +6.6 | 4.8 / 5.1 / 3.8 | 0.97 | 1.9 | 4.4 | 2.0 |
| Room Dark Chamber | +0.3 | +6.2 | 5.4 / 5.1 / 3.1 | 1.03 | 1.9 | 4.3 | 2.4 |
| **HALL 15 s** | +0.6 | +2.6 | 19.1 / 18.6 / 8.0 | 1.06 | **3.7** | 5.5 | **3.2** |
| Room Large Room | −0.2 | +2.7 | 18.7 / 15.0 / 7.9 | 1.08 | 1.0 | 3.8 | 1.4 |
| Room Dark Chamber | +0.7 | +3.5 | 13.7 / 14.5 / 5.6 | 0.96 | 1.3 | 2.8 | 1.0 |
| **PLATE 5 s** | +0.1 | +10.1 | 5.9 / 4.7 / 2.8 | 1.16 | 5.2 | 6.9 | (4.4 at 15 s) |
| Valhalla Plate Chrome | −0.8 | +13.8 | 5.6 / 4.6 / 1.8 | 1.20 | 12.7 | 16.0 | 12.4 |
| Valhalla Plate Lithium | +1.4 | +11.0 | 6.3 / 4.9 / — | 1.30 | 12.0 | 18.4 | 12.6 |
| **PLATE 15 s** | +0.8 | +8.8 | 19.1 / 13.4 / 4.5 | 1.19 | 4.4 | 6.4 | 4.4 |
| Valhalla Plate Chrome | +1.4 | +10.1 | 19.3 / 12.1 / 2.6 | 1.35 | 13.1 | 17.8 | 12.6 |
| Valhalla Plate Lithium | +1.5 | +7.1 | 24.2 / 13.9 / — | 1.37 | 11.0 | 20.3 | 12.6 |
| **CANYON 5 s** (SIZE 0.35) | −0.2 | +4.6 | 5.9 / 5.6 / 5.9 | 1.09 | **12.2** | **17.0** | **14.5** |
| Supermassive Sagittarius fb 30 | +0.3 | +6.3 | 7.6 / 5.7 / 6.9 | 1.37 | 1.2 | 3.6 | 1.5 |
| Supermassive Andromeda fb 12 | +0.3 | +7.2 | 4.7 / 6.2 / 6.4 | 1.00 | 1.9 | 4.0 | 1.8 |
| **CANYON 15 s** (SIZE 0.7) | **−3.0** | **−0.2** | 16.5 / 14.9 / 14.5 | 1.09 | **10.6** | **17.2** | **12.8** |
| Supermassive Sagittarius fb 73 | +0.4 | +2.2 | 13.2 / 14.5 / 14.7 | 0.94 | 1.1 | 3.2 | 1.0 |
| Supermassive Andromeda fb 54 | +0.4 | +3.5 | 7.1 / 14.5 / 14.7 | 0.74 | 0.9 | 2.7 | 0.9 |

Where the extra lines are (‰ of bins over +10 dB per octave, drums / pad / stabs tails, left channel):

| from | 100 | 200 | 400 | 800 | 1.6k | 3.2k |
|---|---|---|---|---|---|---|
| ours HALL 15 s | 16.7 | 19.2 | 11.2 | 8.1 | **6.0** | **2.1** |
| Room Large Room | 15.0 | 15.8 | 8.8 | 7.3 | 2.4 | 0.6 |
| ours CANYON 15 s | 18.3 | 16.7 | 22.5 | 16.7 | **14.4** | **11.0** |
| Supermassive Sagittarius | 13.3 | 13.3 | 10.0 | 7.1 | 1.7 | 0.8 |
| ours PLATE 15 s | 21.7 | 19.2 | 14.6 | 11.7 | 6.4 | 3.9 |
| Valhalla Plate Chrome | 13.3 | 18.3 | 20.4 | 18.1 | 15.0 | 14.7 |

Below 800 Hz every unit's tail holds the pieces' own notes about equally; the difference is above 1.6 kHz. The lines'
frequencies change from piece to piece (CANYON 15 s: 2.08 kHz in the drums, 1.86 in the stabs, 1.96 in the pad,
1.83 in the keys), so they are the playing's partials ringing on, not a resonance of the tank.

### The still-lows split

Pink noise, 2 s, then 20 s of tail; still lows on (as built) minus off (`IV_STILL_LOWS=0`), third-octave dB:

| | 125 | 160 | 200 | 250 | 315 | 400 | 500 |
|---|---|---|---|---|---|---|---|
| CANYON noon, 0-2 s | −1.6 | −2.5 | −4.1 | **−7.5** | −6.8 | −4.2 | −1.8 |
| CANYON noon, 2-6 s | −1.3 | −2.3 | −4.4 | −5.9 | −4.4 | −2.5 | −0.8 |
| CANYON noon, 6-20 s | −1.2 | −2.2 | −2.7 | −2.9 | −2.1 | −0.7 | −0.2 |
| HALL noon, 0-2 s | −2.9 | −2.0 | −1.9 | +0.1 | −0.1 | −0.6 | −0.2 |
| HALL noon, 2-6 s | −0.3 | −1.1 | −0.9 | −1.0 | +1.8 | −0.2 | −0.2 |

The music balance with still lows off: CANYON's low-mids come back from −2.8 to +0.3 dB at DECAY noon; HALL's from
−0.2 to +0.4; the band T60s don't move (CANYON 36-37 s, HALL 8.2-8.6 s). Why it cancels: the low tank and the main tank
share their lengths, matrix and taps, and the low one isn't modulated, so their outputs stay coherent for seconds
(CANYON's main tank moves 0.7 cents; HALL's, three to six times deeper, loses coherence sooner). A 2nd-order Butterworth
low-pass and high-pass are 180° apart at their corner, so in step they null there; they are flat only when the tanks are
uncorrelated (power complementary), which is what `still-lows.md` assumed. Flipping one band's sign makes the in-step
sum LP − HP = (k² − s²)/(s² + √2 k s + k²), whose magnitude peaks at +3 dB at the corner instead of nulling, and the
power sum is unchanged (still flat once the tanks decorrelate). Real tails sit between the two cases, so in practice it
comes out close to flat. Measured (B, the flip, minus the tank with still lows off, same pink noise):

| | 125 | 160 | 200 | 250 | 315 | 400 | 500 |
|---|---|---|---|---|---|---|---|
| CANYON, 0-2 s | +1.1 | +1.8 | +2.6 | +2.2 | +2.3 | +2.0 | +1.2 |
| CANYON, 2-6 s | +1.4 | +1.7 | +1.9 | +2.1 | +1.9 | +1.4 | +0.7 |
| CANYON, 6-20 s | +1.2 | +1.2 | +1.6 | +1.3 | +1.1 | +0.4 | +0.2 |
| HALL, 0-2 s | −1.8 | −0.2 | +0.1 | +1.2 | +0.5 | −0.2 | +0.1 |
| HALL, 2-6 s | −0.7 | −1.7 | −0.2 | −0.9 | +1.0 | −0.7 | +0.1 |

So B turns CANYON's −7.5 dB hole into a +2 dB rise (the in-step case, as the formula says) that settles to about +1 dB,
and leaves HALL within ±2 dB of the unsplit tank. On music that puts CANYON 15 s at +0.8 dB of low-mids against
Supermassive's +0.4. If the owner hears B as a little thick in CANYON, a split between the two (a 1st-order
complementary pair, or LR2) would land nearer flat; not tried.

## The prototypes (`IV_FULLER`, `Engine::setFullerPrototype`)

Desktop only, under `#ifndef IV_FIXED_VOICINGS` (Engine.h/.cpp, Fdn.h/.cpp, host/render/main.cpp). `IV_FULLER=B|C|D`, or
a custom `flip,hallFloor,hallRate,canyonFloor,canyonRate,hallDiff`. Each aims at one cause found above; none is an EQ.

| | What it changes | Aimed at |
|---|---|---|
| **B body** | The still-lows tank's input is inverted (its 300 Hz Butterworth band meets the main tank's in phase) | the low-mid hole |
| **C movement** | B, and the modulation floor: HALL x2 deep (same pace); CANYON x4 deep at x2 pace (smoothing 0.6 Hz, a new target every ~0.9-1.5 s) | the upper-partial lines, to Room's level |
| **D more** | B, and HALL x3 deep; CANYON x8 deep at x4 pace (1.2 Hz) | the lines, near Supermassive's |

The floor is what PLATE/HALL glide out of a held endless tail (still notes, v1.0.58), so at endless HALL's C and D
come back to B once the playing stops; CANYON keeps its movement there, as today.

**Measured effect** [M] (the Valhalla-matched settings; low-mids and body from `valhalla.py`, upper peaks from
`sweep_mod.py`, wobble: `tools/pitch_track.py` p95 on a held 1 kHz, 3-8 s, HALL SIZE 0.6 DECAY 0.6401, CANYON SIZE 0.7
DECAY 0.2004):

| | HALL 5 s low-mids / body | CANYON 15 s low-mids / body | HALL upper peaks 5 s / 15 s ‰ | CANYON upper peaks 5 s / 15 s ‰ | held 1 kHz wobble HALL / CANYON (cents) |
|---|---|---|---|---|---|
| A now | −1.4 / +5.9 | −3.0 / −0.2 | 6.8 / 3.2 | 14.5 / 12.8 | 15.7 / 0.7 |
| B body | 0.0 / +6.0 | +0.8 / +2.0 | 6.7 / 3.2 | 14.5 / 12.8 | (as A) |
| C movement | (as B) | (as B) | 4.2 / 1.2 | 7.1 / 5.9 | 19.7 / 7.4 |
| D more | (as B) | (as B) | 2.8 / 0.7 | 2.4 / 2.3 | 30.3 / 15.3 |
| Room / Supermassive | −0.3 … +0.3 / +6.2 … 6.6 | +0.4 / +2.2 … 3.5 | 2.0-2.4 / 1.0-1.4 | 1.5-1.8 / 0.9-1.0 | (Supermassive's vibes 29-35) |

- B changes no loss in any loop and no line count. The music-tail T60 at 125 Hz reads shorter in CANYON 15 s (13.9 s
  against A's 16.5; 1 kHz 14.4 against 14.9; HALL 5 s 6.1 against 6.4): the in-step boost at the split fades as the two
  tanks drift apart, so the band starts a little higher and settles where the unsplit tank sits (the pink-noise table
  above), which the fit reads as a slightly faster decay. Its bass ratio comes out 0.90 (Supermassive 0.74-0.94).
- The sweep behind C and D (upper peaks, ‰, 5 s / 15 s): HALL x1.5 5.1 / 1.9, x2 4.2 / 1.2, x3 2.8 / 0.7, x2 at x2
  pace 1.6 / 0.3; CANYON x2 12.3 / 11.0, x4 at x2 pace 7.1 / 5.9, x8 7.7 / 6.9, x6 at x3 4.6 / 4.0, x4 at x4 5.5 / 4.4,
  x8 at x4 2.4 / 2.3. Slower and deeper (x2 at half pace) did *not* help HALL: what smooths the lines is the pitch
  movement (depth x pace), so there is no free lunch, only a choice of how much a held note may waver.
- HALL's in-loop diffusion kept at 0.35 at every SIZE (`hallDiff`) changed the lines by less than the method's spread
  (6.9 -> 6.7 ‰ at 5 s) and was dropped; nothing pointed at a longer low or low-mid T60 (the bass ratio already
  matches the references, and perceived bass follows level), at a loop shelf, or at less high-mid in the loop.

**Safety** [M] (the gates are under "Gates" below):
- `tools/pages/energy_check.py --hook IV_FULLER=<v> --set era=0` (every SPACE x SIZE 0 / 0.6 / 1 x DECAY 0.5 / 0.75 /
  0.95 x MOTION 0-1, then the 10-minute holds at DECAY 1): D passes outright (never more than +0.39 dB over A). B and C
  each flag two settings, neither the prototype's doing: a +0.65 dB window in a small HALL (SIZE 0, DECAY 0.5) that sits
  at −180 dBFS, and big endless CANYONs (SIZE 1, DECAY 0.95) rising +1.00 to +1.07 dB after the noise, where A rises
  +0.82 to +1.16 at the same settings (B +0.68-1.01, C +0.94-1.07, D +0.73-0.80). The 10-minute holds pass in all three
  spaces for every version (PLATE −26.6 … −26.5, HALL −21.7 … −21.4, CANYON −18.3 … −17.8 dBFS).
- `tools/fuller_tail/checks.py` (clicks on SIZE and DECAY sweeps and flicks, 5-minute frozen pads, endless held at
  MOTION noon / full left / full right, every SPACE): every render finite; the frozen pads and endless holds stay
  within A's own figures in every space (A itself trips the script's tight limits: CANYON frozen spread 1.54 dB, HALL
  endless at full left +1.11 dB); PLATE identical in every version. The 1 ms click detector (> 3 kHz, dB over its
  neighbours) reads 9-13 dB everywhere, against 15-20 for the music's own transients: HALL SIZE sweep A 10.1, B 10.2,
  C 13.0, D 10.8; HALL SIZE flick A 9.9, C 12.3; CANYON SIZE sweep A 9.0, C 11.3, D 9.6; CANYON flick A 9.1, C 10.5,
  D 10.5; DECAY sweeps 9-10 for all. C's 13.0 falls at 24.8 s, after the last SIZE move (22 s), at −51 dB; C with SIZE
  held reads 9.2 there, so it is the floor's movement in the pad's fading tail rather than a SIZE click, but it was not
  pinned down further. The page's own renders read 6-15 (`renders/fuller_tail/_verify.txt`).

## Gates

- ctest: "100% tests passed out of 23" (`build-ft`, no plugin target).
- SAME 60/60: the hook off renders bit-identically to v1.0.70 (4b95b8a) on the golden set.
- `make -C firmware all-variants` byte-identical to 4b95b8a: release 111,844 B, m0test 83,556 B, profile 131,028 B.
- The page: every pair of versions differs on every card and source; each version level-matched to A per source
  (gains −3.6 … +0.6 dB); 1.2 GB.

**Porting, flash** [E] (release 111,844 B, ~800 B left in its budget; profile 131,028 B, 44 B left):
- B: the low band's three numerators negated in `Engine::prepare`'s `butter` (or the HP's): 0-16 B. Fits both.
- C, D: the floor depths are constants (HALL `kHallFloorScale`, CANYON's `floorScale = 1`): 0 B. CANYON's pace needs a
  per-tank factor in `Fdn::control` (the floor's smoothing coefficient, out of its memo, and the step time): a member,
  a multiply and a divide, and the Engine setting it per SPACE, ~40-100 B. Fits the release; probably not the profile
  without a trim. Doing CANYON's movement by depth alone (x8 at today's pace: 7.7 / 6.9 ‰, close to C) costs 0 B.
- CPU: none for B; C and D the same work per sample (only the floor's targets move further).

## Open questions

- **For the owner, by ear:** does B's body read as warmth or as mud in CANYON (it overshoots the unsplit tank by about
  2 dB at 200-400 Hz early)? If thick, a 1st-order complementary or LR2 split would sit nearer flat (not tried).
- **How much may a held note waver?** C and D trade still notes for smoother upper partials; the owner chose still notes
  for held endless tails (v1.0.58), and C/D keep that (the floor still glides out of a held HALL). Played tails and
  CANYON are where the choice bites.
- **PLATE:** measured fine against Valhalla Plate, but its build is slow (review 2: NED t90 176-256 ms against 17-34;
  peak at 134-166 ms against 45-52), which may be the "wispy" in "thinner, wispier". Not prototyped here; a page on the
  plate's attack (input diffusers, output taps) would test it.
- **Desmodus and Pleroma** were not measured (no takes). A take of each at matched decays, through
  `docs/reference-recording-guide.md`, would show whether the listener's comparison is about body, movement or level.
- **Level:** the wet's loudness against the dry is the `claude/wet-loudness-8e55d6` session's; a quieter wet also reads
  thinner, so the two pages may interact. Untouched here.
- **On the module:** nothing here was heard or profiled on hardware; C/D's CANYON pace needs ~40-100 B and the profile
  build has 44 B.
