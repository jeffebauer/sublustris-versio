# Valley's Plateau against our PLATE (6 Nov 2026)

The owner asked how Valley built and tuned Plateau from Dattorro's 1997 plate, where it differs from ours and why, and
what we could use. Lead-ins from launch: lfad on Modwiggler found PLATE and HALL "pretty similar" and the sound
"thinner, wispier"; our PLATE builds slowly against Valhalla Plate (`valhalla-comparison.md`, item 5); and it is grainy at
big SIZE (`decay-curve-2026-11-06.md`, section 9, on branch `claude/proto-decay-curve-01af2c`).

Research only: nothing in `core/` changed, and there is no prototype or page. Nobody has listened to any of this yet.

**How each claim is marked:**
- **[M]** measured here, or in the note named.
- **[P]** Dattorro's paper.
- **[V]** stated by Valley in its manual or README.
- **[S]** read from Valley's source.
- **[I]** inferred: my reasoning, not anyone's statement.

## In short

**Plateau is closer to our PLATE than expected.** It has the same figure 8, the same delay lengths and the same two tank
allpasses in the same places. It differs in five ways that matter:

1. **Its output taps stay put when Size changes.** Ours stretch with SIZE, and that is most of why our big PLATE turns
   grainy. Holding them still costs 0 B and takes about 40 % off the graininess at full SIZE. [M]
2. **It has one extra output tap, right after the first allpass in each branch.** That makes it answer at once. An extra
   tap like it, with a fixed-tap setting, brings our "up front" figure to Valhalla's. [M]
3. **Its modulation is about ten times gentler than Dattorro's and ours**, and very slow by default (0.1-0.2 Hz). On our
   tank, slowing ours the same way lifts the tail's metallic lines from 5 to 11-13 ‰, right where Valhalla's plates sit
   (12.4-12.6 ‰). It also halves the held-note waver. It is a change of constants, so about 0 B. [M, flash I]
4. **Its second tank allpass runs at 0.7, not Dattorro's 0.5.** On our tank this adds metal and doesn't help the attack. [M]
5. **Its input is mono, and its decay isn't calibrated.** Ours is better on both, for this module.

**What did not work:** changing the input diffusion, in either direction, makes the start grainier. [M] That rules out two
of our own open ideas (section 5).

## 1. What I read

- **Valley's manual:** <https://valleyaudio.github.io/rack/plateau/>, and the README in `ValleyAudio/ValleyRackFree`,
  which states GPL-3.0, Copyright Dale Johnson. An older copy of the README in `miRackModular/ValleyRackFree` has the
  fuller control notes. Valley publishes **no tuning or design notes** that I could find, so almost every "why" below is
  inferred.
- **Valley's source, for understanding only (ADR 0006: concept only, no code):** `src/Plateau/Dattorro.cpp` and `.hpp`,
  `Plateau.cpp` and `.hpp`, `src/dsp/delays/AllpassFilter.hpp` and `InterpDelay.hpp`, and `src/dsp/modulation/LFO.hpp`,
  on `main`, 6 Nov 2026. No code was copied, and no code structure or tables are transcribed here. Numbers below are
  single facts or rounded ranges. **Anyone prototyping from this note should work from Dattorro's paper and this note,
  not from that source.**
- **Dattorro (1997), "Effect Design, Part 1"**, sections 1.3.1-1.3.7, Fig. 1 and Tables 1-2:
  <https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf>.
- **Campestria Versio** (a port of Plateau to the Versio, also GPL): the forum threads only. I did not open its source.
- **Ours:** `core/dsp/Plate.h` and `.cpp`, `Engine.cpp`, `core/params/SpaceVoicing.h` and `Mappings.h`, ADRs 0006, 0013
  and 0043, and the research notes named here.

## 2. Side by side

| | Dattorro 1997 [P] | Plateau [S unless marked] | Our PLATE |
|---|---|---|---|
| **Topology** | Figure 8: per side, modulated allpass, delay, damping, decay, allpass, delay, decay | Same | Same, with a loss filter in place of each decay gain, and the loop chain (the Neve, soft saturation) after the second delay |
| **Tank lengths** | Table 1 | **Dattorro's**, scaled to the sample rate | **Dattorro's** |
| **Input** | L + R summed to mono; the paper calls the stereo synthetic | Summed to mono, as in the paper; both sides of the tank get the same feed | **True stereo:** L and R have their own diffusers and enter different branches (ADR 0013) |
| **Input diffusers** | Four, 0.75 / 0.625 | Same coefficients; can be bypassed, to sharpen the input [V]; first length one sample short of the paper's | Same coefficients, with a pair per side (right 7 % longer); the first stage wanders slightly (v1.0.16) |
| **Tank allpass coefficients** | 0.70 and 0.50 (the second follows decay, floor 0.25) | **Both 0.7** at full Diffusion, and a knob scales both down to 0 | 0.70 and 0.50, fixed |
| **Modulated delays** | The first allpass of each side; ideally all of them | **The two allpasses of each side**; neither long delay | First allpass and both long delays; **the second allpass isn't moved** |
| **Modulation shape, rate** | Quadrature sine, about 1 Hz | **Triangle**, its shape a knob; **0.10-0.18 Hz** by default, each its own rate; a knob goes up to ×100 | Sine at 0.71 / 0.93 Hz plus a slow random floor; long delays 0.19-0.37 Hz |
| **Modulation depth** | Up to ±16 samples, typically ±8 (at 29.8 kHz) | **±8 by default**, and a knob goes far past the paper's limit | ±11 sine + ±5 random on the allpass; ±0.36 ms on the long delays |
| **Pitch movement per pass** (worked out from the above) | 2.9 cents | **0.2-0.3 cents** at default | **2.9-3.8 cents**, plus 0.7-1.5 on the long delays |
| **Interpolation** | Linear, or better all-pass (linear dulls the tank) | Linear | All-pass |
| **Damping** | One-pole low-pass per side, after the first delay | One-pole low-pass **and low-cut** per side, once a side; knobs 14 Hz-14 kHz, so a one-pole always sits near 14 kHz in the loop | Jot shelf **twice** per side, set from DECAY, TONE and Air |
| **Decay** | A plain gain, 0.5 by default | A plain gain, by knob (squared curve) | A **real T60**, from the delay lengths, so SIZE doesn't change the tail's length |
| **Size** | Fixed | Every tank length together: **0.01× to 4×**, 1× at noon. Moves are direct, so they bend pitch | **0.5× to 1.59×**, by crossfade (ADR 0043); T60 held |
| **Output taps** | Seven a side, from both branches, signs mixed, × 0.6 | The paper's seven left offsets used on **both** sides; its signs; **fixed whatever Size**; plain whole-sample reads; **plus an eighth tap on the first allpass's output**; × 0.5 | The paper's fourteen, **scaled with SIZE**, lerped; the cross-branch signs flipped to + (v1.0.25); × 0.6 × trim |
| **Which branch a side reads first** | The other side's | **Its own** (no effect on image with a mono feed [I]) | The other side's: a hard-left source answers on the right first [M]. The owner chose this (below) |
| **Freeze / hold** | Decay near 1, damping off | Decay 1, filters faded out, modulation left running, **input left open** so sound piles up; Clear empties it | Input closed, loop gain 1, colour bypassed, chorus × 0.15, long wander 0; what you play goes into the twin |
| **Other** | | 0-500 ms predelay; tuned mode (1 V/oct on Size, down to 0.0025×); tanh output option; 20 Hz DC blocks in and out | The floor governor and held endless tail; the wear chain; 5 Hz DC block on the input |

## 3. Each deviation, and why

No "why" here is stated by Valley unless marked [V].

### Plateau from Dattorro

- **Taps that ignore Size.** [I] The cheap way to stop taps falling off shortened lines, and it keeps the moment the reverb
  first speaks the same at every Size. Whether it was aimed at the sound or was a convenience, I can't tell.
- **The eighth tap on the first allpass.** [I] Without it the first output waits for the first tap. With it, an immediate
  answer. It suits the module's uses: gated sounds and tuned, played plates.
- **Second allpass at 0.7, and one knob for both.** [I] A single Diffusion knob from plain echoes to smeared is simpler
  to patch [V for what the knob does]. Dattorro warns that values toward 1 buzz and says the paper's values came from trial
  and error [P].
- **Slow triangle modulation, on both allpasses.**
  - Valley repeats Dattorro's reason: modulation suppresses ringing and keeps the reflections varying, and more of it is audibly
    pitched [V].
  - [I] A triangle's pitch holds steady, then flips, and it is ten times gentler at default, which avoids the vibrato on
    piano that Dattorro warns about [P]. The knobs then reach well past the paper as an effect.
- **Same offsets on both sides, plain taps, linear interpolation.** [I] Simplification. Dattorro notes linear
  interpolation dulls the tank a little [P], and Plateau's one-pole already darkens it.
- **Size up to 4×, tuned mode, low-cut in the loop, open-input Hold, Clear.** The stated aim is to go beyond a plain
  reverberator, with many control combinations [V]. These are the features that do it.
- **Plain, uncalibrated decay.** [I] A straight reading of the paper. On a Rack module with a Size knob and CV, a tail that
  grows with Size is acceptable, and possibly wanted.

### Ours from Dattorro (all our own choices, on record)

- **True stereo in:** ADR 0013. Stereo placement survives.
- **Cross-branch taps flipped to +:** v1.0.25, owner's pick. The paper's signs made the lows near mirror images with a stereo feed.
- **Jot loss, SIZE, Air, governor, loop chain:** ADRs 0006 and 0008; so a tail's length is what DECAY says, at every SIZE.
- **The long delays wander too:** v1.0.12, owner's pick, the paper's "ideally"; it took plate ping down.
- **SIZE by crossfade:** ADR 0043, owner's pick.

### Ours from Plateau, and what each side gets

- **Mono against true stereo:** ours keeps placement. Plateau gets a fast, even wet from a panned source. A mono feed in our
  tank measured about 2.7 dB quieter (my half-sum) and changed little else. **Keep ours.** [M]
- **The answer on the other side.** The owner listened on `renders/plate_stereo` and chose it: A on all 8, "more cohesive";
  answering on the same side "creates a sonic gap" (SPEC changelog). Plateau's own-side reading gives the opposite. **Keep
  ours, and any new tap must respect this** (section 6, item 2).
- **All-pass interpolation, lerped taps:** ours is cleaner, with no modulation-dependent dulling. **Keep ours.** [P]
- **Crossfaded SIZE against bent Size:** the owner picked ours. **Keep ours.**

## 4. Measured: which of these move our PLATE

**Method.** I tested each idea on our own Plate, not on Plateau; no Plateau code was compiled or run. The renderer was a
throwaway copy of `main` in my scratch folder, with each idea behind a switch. With the switches off it is bit-identical
to stock. [M] The scripts, raw results and the switches' diff are in this session group's shared notes folder
(`plateau-research/`), not in the repo. Everything below is 100 % wet, WEAR 0, MOTION noon, TONE noon, NATURAL, at 48 kHz.

- **Valhalla comparison:** the kit's sync click and music, SIZE 0.6, at the matched 5 s and 15 s settings, against the
  owner's Chrome and Lithium takes, read in memory and never saved.
- **SIZE comparison:** a single click, DECAY noon, DEPTH 0, SIZE 0 / noon / full, as in `decay-curve` section 9. The base
  row matches that note's figures.

**The measures:**
- **C80** is how "up front" it is: higher means more energy in the first 80 ms.
- **Peak** and **rise** are when the wet is loudest and how long it takes to get there.
- **NED** is echo density: 1 is smooth, well under 1 is separate echoes.
- **Kurt** is spikiness: 0 is smooth, high is grainy.
- **Lines** are `upper_peaks` (fuller-tail note): narrow lines at 1.6-6.4 kHz in the tail, in ‰. More lines means more
  metal.

### 4.1 The attack, against Valhalla Plate

| Version | 5 s: C80 | peak | rise | NED | kurt | 15 s: C80 | peak | lines 5 / 15 s |
|---|---|---|---|---|---|---|---|---|
| **Today** | −8.0 | 143 ms | 26 | 0.71 | 3.0 | −10.9 | 271 ms | 5.2 / 4.4 |
| Taps held at 1× | −7.9 | 144 | 27 | 0.70 | 3.1 | −10.8 | 144 | 5.2 / 4.1 |
| Taps held at 0.7× | −6.6 | 123 | 28 | 0.73 | 2.6 | −9.6 | 123 | 5.3 / 4.1 |
| Taps held at 0.5× | −5.2 | 81 | 31 | 0.74 | 2.4 | −8.4 | 81 | 5.5 / 4.6 |
| Extra tap, full weight, other side | −6.0 | 52 | 13 | 0.74 | 2.7 | −8.9 | 52 | 5.0 / 4.0 |
| **Extra tap + taps at 0.5×** | **−4.2** | **76** | **19** | **0.79** | **2.0** | **−7.3** | **76** | 5.6 / 4.6 |
| Valhalla Chrome | −3.5 | 35 | 7 | 0.96 | 0.1 | −7.2 | 61 | 12.4 / 12.6 |
| Valhalla Lithium | −2.8 | 41 | 7 | 1.00 | −0.1 | −6.6 | 41 | 12.6 / 12.6 |

- **C80 reaches Valhalla's** at 15 s and nearly at 5 s, and the long tail's peak stops running away with DECAY. The tail is
  unchanged: lines and decay stay put.
- **It doesn't reach Valhalla's smoothness.** NED stays 0.79 against 0.96-1.00. That looks like the limit of this topology
  and its taps, and Plateau has the same taps. So I'd expect Plateau's start isn't smooth either, though I haven't
  measured it. [I]
- **A cost:** the lows' share falls about 0.8 dB at 5 s (+1.3 to +0.5), and the 125 Hz T60 reading drops (5.9 to 4.7 s at
  5 s; 19.1 to 18.1 s at 15 s). The loop is untouched, so this is early energy changing the slope being read, not a shorter
  tail. [I] Overall level is +0.4 dB. Worth listening for, given "thin".

### 4.2 Graininess across SIZE (the big-SIZE problem)

Each cell is SIZE 0 / noon / full.

| Version | NED | kurt | C80 | peak (ms) |
|---|---|---|---|---|
| **Today** | 0.78 / 0.62 / 0.51 | 1.4 / 6.9 / **14.2** | −7.2 / −8.8 / −7.8 | 107 / 129 / 125 |
| Taps held at 1× (Plateau's way) | 0.74 / 0.64 / 0.56 | 2.2 / 4.6 / 8.1 | −9.2 / −7.5 / −6.4 | 145 / 140 / 133 |
| Taps held at 0.5× (our SIZE floor's positions) | 0.78 / 0.68 / 0.57 | 1.4 / 3.7 / 8.5 | −7.2 / −5.6 / −4.3 | 107 / 62 / 71 |
| Extra tap, other side | 0.80 / 0.64 / 0.54 | 1.4 / 6.1 / 9.4 | −5.7 / −6.1 / −5.0 | 91 / 43 / 26 |
| **Both** | 0.80 / 0.71 / 0.62 | 1.4 / 3.2 / **6.5** | −5.7 / −4.0 / −2.8 | 91 / 61 / 61 |
| Input diffusion × 0.7 | 0.73 / 0.54 / 0.41 | 2.7 / 9.4 / 16.1 | −7.4 / −8.8 / −7.9 | 89 / 119 / 44 |
| Input diffusion × 1.2 | 0.63 / 0.44 / 0.34 | 4.4 / 23 / 36 | −6.7 / −8.7 / −7.7 | 105 / 212 / 17 |

- **Holding the taps takes big-SIZE kurtosis from 14 to 8**, and with the extra tap to 6.5. So the grain grew because the
  taps spread out with the tank, not because the diffusers were overstretched.
- **Taps at 0.5× leave SIZE 0 exactly as it is.** They sit where they already are there, so only the larger sizes change.
- **The wet's peak also stops jumping with SIZE**, which is the unevenness `tools/sweetspot.py` found at 1 and 4 o'clock.
  [I from these peak figures; sweetspot not re-run]
- **Input diffusion is worse in both directions.** Dattorro's values hold up.

### 4.3 The tail: metal and waver

Lines at 5 / 15 s. Held-note waver is `tools/pitch_track.py` on a held 1 kHz, SIZE 0.6, the 15 s setting. Click ring is
the share of bins over +10 dB at DECAY noon, SIZE noon.

| Version | Lines ‰ | waver p95 / peak (cents) | click ring ‰ |
|---|---|---|---|
| **Today** | 5.2 / 4.4 | 10.4 / 41.7 | 1.9 |
| Allpass sine ÷ 5 (constants only) | 10.6 / 8.5 | 5.0 / 14.9 | 3.0 |
| Sine ÷ 5, long wander and floor × ½ | 11.7 / 10.3 | 4.7 / 19.6 | 5.0 |
| Sine ÷ 5, no long wander, no floor | 13.5 / 12.5 | 4.1 / 9.0 | 8.3 |
| Plateau's shape: triangles on both allpasses, 0.1-0.2 Hz, ±8, no long wander, no floor | 11.5 / 10.9 | 5.4 / 25.3 | 3.1 |
| Second allpass at 0.6 | 6.2 / 4.9 | – | 3.6 |
| Second allpass at 0.7 | 8.0 / 5.5 | – | 6.9 |
| Second allpass moves too (±8, ~1 Hz) | 3.0 / 2.2 | 14.9 / 27.9 | 0.8 |
| Valhalla Chrome, Lithium | 12.4-12.6 | not measured | – |

- **Slowing the modulation is the strong lever.** It doesn't touch the attack, and it moves lines to where Valhalla's plates
  sit. It also halves the waver. Plates are metallic; "accept the metal and call it a plate" (Costello, in the fuller-tail
  note).
- **It pulls apart from HALL.** HALL holds 6.8 ‰ today and about 4.2 ‰ with the fuller_tail pick, by moving more. So PLATE
  still and metallic, HALL moving and washed: two characters out of "pretty similar". [M for the numbers, I for the
  listening]
- **The second allpass at 0.7 adds metal but nothing in the attack** (C80 slightly lower, NED unchanged). It lifts the 1 kHz
  T60 about 4 % and the level 0.6-0.8 dB, so DECAY's calibration would need redoing.
- **Moving the second allpass goes the wrong way for a plate**: fewer lines and more waver.
- **Nothing here adds body.** Holding taps and the extra tap slightly reduce the lows' share, and the modulation versions
  shift them by about 1 dB or less. By reading, Plateau's subtracted cross taps partly cancel its lows with a mono feed [I], so
  Plateau isn't where to look for a fuller tail. Body belongs to the wet-loudness and fuller_tail work.

### 4.4 Flash

Measured with the release build, `firmware` on `main`: base 111,772 B of 112,640 (868 B free on `main`; less once the
queued picks land).

| Change | Flash | CPU |
|---|---|---|
| Second allpass coefficient | **0 B** [M] | none |
| Taps held (normal audio path) | **0 B** [M] | none |
| Extra tap + the two above, normal path | **+16 B** [M] | two additions a sample |
| Extra tap in the crossfade path too | a further ~16-40 B [I] | |
| Slower modulation | ~0 B, constants only [I; not built] | none |
| Second allpass moving (not recommended) | ~50-100 B [I] | a divide per branch per sample |

- **Possible saving:** held taps no longer need their old-and-new fade during a SIZE crossfade (`processXf`), which could
  free more than the extra tap costs. Not built. [I]
- **The profile build has only 44 B free** (handoff), so even +16 B needs care there.

## 5. What this rules out

- **More input diffusion as SIZE grows** (`decay-curve` section 9.3's page idea): 1.2× makes it much spikier. Drop it.
  [M, at the one step measured]
- **Lower input diffusion for a sooner hit** (`valhalla-comparison.md`, item 5's first experiment): 0.7× is grainier, and C80
  doesn't move. Drop it. The same item's second suggestion, earlier taps, is the one that works. [M]
- **Swapping the branches, or a mono feed:** the owner's pick and ADR 0013 stand.
- **Plateau's open-input Hold:** our closed input plus the twin is the designed behaviour (ADR 0009).
- **Linear interpolation, whole-sample taps, the plain decay:** savings we don't need, with worse sound or control.
- **A low-cut in the loop:** nobody is asking for less low end.

## 6. Enhancement opportunities, ranked

1. **Hold the output taps still across SIZE, at about SIZE's floor positions.**
   - *Musically:* a big PLATE starts as a plate, not as a spatter of separate echoes, and hits sooner at every SIZE above the
     floor. The floor itself doesn't change.
   - *Support:* strong [M]. Kurt 14.2 → 8.5 at full SIZE; C80 +2.5 to +2.8 dB at the matched settings, +3.2 to +3.5 dB on the click at noon and full SIZE.
   - *Cost:* 0 B on the normal path, perhaps a saving in the crossfade. CPU none.
   - *To check:* SIZE moves and flicks, now the taps don't fade. Plateau holds them at 1×; 0.5× is my extension, and 0.7× is
     a milder step.
2. **Add a tap on the first allpass's output, on the other side's output (so the owner's "cohesive" pick stands).**
   - *Musically:* the wet speaks at once and peaks early, closer to a real plate's crack. With item 1 it reaches Valhalla's
     C80, the "up front" gap we've had since October.
   - *Support:* strong [M]. Peak 143 → 52 ms alone; with item 1, C80 −8.0 → −4.2 (Valhalla −3.5, −2.8).
   - *Cost:* +16 B on the normal path, ~16-40 B more in the crossfade. Two additions a sample.
   - *To check:* at DEPTH 0 the tap arrives with the dry, so check it for colouring at middle BLEND. A hard-left source stays
     on the right for the first 20 ms; with item 1 the left comes in from 20-50 ms (L −8 dB against R), where today it waits
     until 50 ms.
   - *The musical question:* "should the plate's attack be a crack or a bloom?" Items 1 and 2 are the crack.
3. **Slow PLATE's modulation.** Start at allpass sine ÷ 5, with the long wander and floor at about half.
   - *Musically:* a tail that rings like metal instead of drifting; held notes stay truer; PLATE and HALL are less alike.
   - *Support:* strong on the numbers [M]: lines 5.2 → 11.7 ‰ (Valhalla 12.4-12.6), waver p95 10.4 → 4.7 cents. But it
     **cuts against two earlier choices**: the long-delay wander (owner's pick, v1.0.12) and the modulation floor (SPEC §2.3
     rule 1). Hence half, not off.
   - *Cost:* ~0 B. CPU none.
   - *To check:* the metal could turn "reedy", what the fuller-tail work found and reduced in HALL and CANYON. Long tails and small SIZEs
     first, plus `test_plate_freeze` and a held endless tail. Needs a listen before anything else.
   - *The musical question:* "should PLATE's tail sit still and ring, or keep drifting?"
4. **Second tank allpass at 0.6.** Only if item 3 isn't enough, or as a gentler route to the same thing.
   - *Musically:* a little more metal and sheen. It does nothing for the attack, despite what the name "diffusion" suggests.
   - *Support:* weak [M]: 5.2 → 6.2 ‰ at 0.6. 0.7 reaches 8.0 ‰ but rings more and needs DECAY recalibrated.
   - *Cost:* 0 B.
5. **A smaller SIZE floor, for tight, metallic little plates.** Not measured. Plateau reaches 0.01×; ours stops at 0.5×.
   - *Musically:* boxy, springy, resonant short spaces, and at the extreme tuned, playable tones.
   - *Support:* none yet. `decay-curve` section 9 found nothing wrong with today's floor, and small plates have few modes
     (`dsp-audit.md`, P4). This is a new sound, not a fix: it would change what SIZE's whole travel means.
   - *Cost:* probably ~0 B, as a mapping change. [I]

**Items 1 and 2 go together, item 3 stands apart.** They touch different things, the start and the tail, so they can be
judged on separate pages. Order against the queue: PLATE is unchanged by fuller_tail, so nothing here blocks that port. The
wet-loudness change will move PLATE's level, so level-match against it.

**A note on "wispy":** this work mainly offers a harder, sooner, more metallic PLATE. That may answer "wispy" and "pretty
similar", but it adds no low end. Only a listen can say how much of "thin" is the slow attack. [I]

## 7. Not checked

- **Plateau was never rendered or measured.** No Plateau audio appears anywhere above; statements about how it sounds are
  inferences from its source.
- **Nothing has been heard**, by anyone. All of section 4 is numbers.
- **Not run on these versions:** energy sweeps and 10-minute holds, metallic checks, SIZE-move click checks, the golden
  tests, the twin, or anything on the module. Taps and the extra tap only read the loop, so they can't change its stability
  [I]; slower modulation can change how a long or endless tail behaves.
- **Module CPU** wasn't profiled. Flash is the release build only, with the normal path patched and no crossfade path.
- **Valley's reasons** are mostly inferred. It publishes almost none.
- **Combination effects:** only the combinations in section 4 were tried, and none with fuller_tail, decay_curve or
  wet_loudness.

## Sources

- [Valley Plateau manual](https://valleyaudio.github.io/rack/plateau/)
- [ValleyRackFree source and README](https://github.com/ValleyAudio/ValleyRackFree) (GPL-3.0)
- [miRack copy of the README, older control notes](https://github.com/miRackModular/ValleyRackFree)
- [Dattorro, Effect Design Part 1 (1997)](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)
- [Campestria Versio, Daisy forum](https://forum.electro-smith.com/t/campestria-versio-vcv-racks-plateau-for-ne-versio-hardware-eurorack/5092)
