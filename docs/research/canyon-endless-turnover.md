# CANYON endless: old notes that never give way (12 Nov 2026)

Research and measurement only; no code changed. Code under review: v1.0.81 (`.claude/worktrees/port-fable`, renderer `build-port/iv_render`).

**The owner's note:** "canyon with infinite tails. If I play in notes with a higher frequency, they build up and never go until I reduce the decay. I wonder if we can hold frequencies in the tank but as new notes are played in, they slowly replace the existing built up frequencies." Asked, they chose: old notes **slowly give way** as new ones arrive (like overdubbing a tape loop), and with nothing new played the tail **holds forever**, as now.

## Summary

- **What is heard is real.** In CANYON at endless DECAY, a note played a minute ago is still only 17 dB down after a full minute of new, sparse playing (0.28 dB a second). HALL loses the same old highs three times faster (0.85 dB/s) and PLATE faster still. Once the playing stops, whatever is left stays for good.
- **It isn't a runaway.** The level settles within ~40 s, about 4 dB above a single note's (-21 dBFS against HALL's -24). The "build-up" is notes piling on top of each other because none leave, not a level that keeps climbing.
- **The cause is three choices that are each right alone:** while played, endless is the longest finite tail, which in CANYON is 120 s; CANYON's highs ring as long as its body (ADR 0022); and the hold switches the loss off between notes, so in sparse playing even those 120 s only count about half the time.
- **Recommendation:** give endless its own, much shorter "while you play" tail in CANYON (a **turnover time**, 20-30 s to start), still held the moment the playing stops. It is the looper's "feedback while overdubbing". No per-sample work, about 20-40 B of flash.

## 1. Measurement

Stimuli (my own, 48 kHz, 100 % wet, defaults otherwise: SIZE, TONE, MOTION noon, WEAR 0): plucked notes (three harmonics, ~0.45 s) and sines, peak -12 dBFS and 12 dB quieter.

- **high / low / high**: four high notes (1047-2093 Hz) in rotation, then four low notes (131-262 Hz), then four *different* high notes (1175-2349 Hz), then silence. Dense (a note every 0.5 s, 40 s a phase) and sparse (a note every 2 s, 60 s a phase).
- **highs only** for 3 minutes; **10 s of highs then lows** for 110 s; a **melody changing key** (C, then F sharp, then D minor).

Each note group's level is its narrow band (±3 %) summed, read from a 16k FFT. The method's own floor is about 28 dB under the loudest group (plucks and the tank's movement spread a little energy between neighbouring notes).

### How long an old note survives new playing

Old high notes' level, in dB against where they stood when their phase ended:

| Render (sparse, a note every 2 s) | after 30 s of new notes | after 60 s | rate |
|---|---|---|---|
| **CANYON endless** | **-8.6** | **-16.9** | **0.28 dB/s** |
| CANYON, DECAY just below endless (the 120 s tail) | -15.2 | -30.3 | 0.50 dB/s |
| HALL endless | -26.1 | -49.9 | 0.85 dB/s |
| PLATE endless | -44 | (at the method's floor) | 1.4+ dB/s |

| Render (dense, a note every 0.5 s) | after 20 s | after 40 s | rate |
|---|---|---|---|
| **CANYON endless** | **-14.2** | **-29.8** | 0.7 dB/s |
| CANYON, the 120 s tail | -17.0 | -37.4 | 0.9 dB/s |
| HALL endless | -32.5 | -57.7 | 1.5 dB/s |
| PLATE endless | -58 | | 3+ dB/s |

- 12 dB quieter, CANYON endless sparse gives the same 0.28 dB/s (dense: 0.48): the loss does not depend on how loud the playing is.
- **Endless is stickier than the 120 s tail next to it on the knob** (0.28 against 0.50 dB/s, sparse). That is the hold: between notes the loss is switched off.
- Old **low** notes go at the same pace as old high ones in CANYON (-15.6 dB in 60 s of new highs). In HALL the lows last and the highs go (lows -40 dB, highs -50 dB in the same test), as in a room. So in CANYON the highs are the ones that stand out: nothing takes them away first.
- The key change: 40 s after leaving C for F sharp, C's notes are 18 dB down in CANYON, at the method's floor (-20) in HALL after 20 s. With the third key 40 s in, the second key is still 16 dB under it in CANYON (HALL: at the method's floor within 20 s), so two keys sound at once for most of a minute; the first key has by then reached the method's floor in both.
- **After the playing stops** every CANYON endless render holds its level within 0.3 dB for the remaining 1-2 minutes (the spec's promise). The old notes left over hold with it; over two minutes their bands even read 5-7 dB *higher*, because CANYON's moving tank slowly smears the held notes into their neighbours (PLATE and HALL calm their movement on a held tail, v1.0.58; CANYON doesn't).

### Does the level build?

Highs only, sparse, 3 minutes: CANYON endless -24.3 dBFS after 10 s, -21.3 after 40 s, -19.8 after 3 minutes; HALL endless stays at -24 ±0.6 throughout; CANYON's 120 s tail settles at -24.5. So endless CANYON carries about 4.5 dB of piled-up notes that HALL and the finite tail don't, reached in the first minute, and it stops there. 12 dB quieter the picture is the same 12 dB lower: this is not the floor governor at work (the tail sits under its -21 dBFS floor), just very slow loss.

### The cause, in the code

1. **While played, endless is the longest finite tail** (v1.0.61, "endless follows the playing"): `core/dsp/Engine.cpp:586-587`, `decayFin = min(decay, kDecayFiniteTop)` and `rate = decayRate(decayFin, size)`. CANYON's longest tail is 120 s (`core/params/Mappings.h:37`, `kT60Massive{2, 120}`; `:264`), against HALL's 60 s and PLATE's 30 s. 120 s is 0.5 dB a second, and that is the fastest an old note can ever leave.
2. **CANYON's highs ring with the body** (ADR 0022): `Engine.cpp:590` passes `decayFin` to `map::massiveHighRatio` (`Mappings.h:252-256`), which at TONE noon gives a ratio of 1.0: the highs' T60 is the lows', 120 s. HALL's and PLATE's highs get 0.315 of the body's T60 (19 s and 9.5 s) plus HALL's air. ADR 0022 said endless would glide back to the shared ratio; since v1.0.61 `decayFin` never passes 0.80, so it doesn't (the SPEC's v1.0.61 line lists "MASSIVE's high ratio" as kept, so this is as decided).
3. **The hold switches the loss off between notes**: `Engine.cpp:588-589` (`rate *= 1 - endlessHold`), driven by `Engine.cpp:1092-1094`: once the tank's input is 30 dB under the tail for 0.25 s the loss glides out over 0.5 s (`core/params/SpaceVoicing.h:129-131`). A plucked note keeps the loss on for well under a second, so a note every 2 s leaves it on about half the time: 0.5 dB/s becomes the measured 0.28.
4. **Nothing else takes the highs.** The held tail's mellowing is PLATE and HALL only (`Engine.cpp:743`, `setEndlessTop(massive ? 0 : endlessTop)`). The fade of an endless tail's highs (`kInfiniteHighT60Seconds`, `SpaceVoicing.h:172`; `core/dsp/Governor.h` `absorbShared`) is `(1/highRatio - 1) / 30 s`, which is zero at CANYON's ratio of 1. The floor governor only acts above -21 dBFS and at 0.30 of its weight while played (`Engine.cpp:599-601`, `map::floorWeight(0.80, 2)` = 0.296), and its loss is flat across the spectrum (v1.0.58), so it trims the whole wash, old and new alike; it accounts for the extra 0.2 dB/s in the hot, dense render and nothing in the quieter ones. The soft saturation and the tank's resonances play no part at these levels: every old note falls at the same rate, none sticks out.

In short: CANYON endless is a loop that loses half a dB a second, in every register, and only while a note is actually sounding.

## 2. How established designs do it

Sourced survey (12 Nov 2026). "Read" = the page was read; "summary" = from a search summary, the primary page not opened.

**Most infinite reverbs simply pile up, as ours does.**
- Eventide Blackhole (H90 manual): Feedback at "Infinite" holds while still letting the input in; "Freeze" shuts the input. No displacement. (read) <https://cdn.eventideaudio.com/manuals/h90/1.9.4/content/algorithms/reverb.html>
- Strymon BigSky: Infinite adds each new note to the tail, Freeze holds without adding. (summary) <https://www.tubesoundbcn.com/img/manual/BigSky_UserManual_RevD.pdf>
- Meris Mercury7: holding Swell maximises the decay. (summary) <https://www.analoguehaven.com/meris/mercury7/manual.pdf>
- Lexicon PCM 80 "Infinite": the input ramps off and the tail holds, a freeze. (summary) <https://www.manualsdir.com/manuals/164814/lexicon-pcm-80.html?page=65>
- Image-Line LuxeVerb: Sustain leaves the input open "so the sound will continue to build". (read) <https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/LuxeVerb.htm>
- Mutable Instruments Clouds keeps build-up in check with a loop gain that never reaches 1 (reverb time tops out at 0.98), a low-pass in each half of the tank, a high-pass and a soft limiter on the feedback; nothing is displaced. (read) <https://raw.githubusercontent.com/pichenettes/eurorack/master/clouds/dsp/granular_processor.cc>, <https://raw.githubusercontent.com/pichenettes/eurorack/master/clouds/dsp/fx/reverb.h>

**(a) The looper's way: a fixed feedback below 1 that only acts while you add.**
- Echoplex Digital Pro, as its co-designer Kim Flint describes it: in "Delay" mode the input is always open and the feedback setting applies only while audio is being added; with nothing added the loop holds at 100 %. The amount is a fixed setting, not tied to how loud you play. (read) <https://loopersdelight.com/LDarchive/200108/msg00969.html>, <https://loopersdelight.com/tools/echoplex/echopedals.html>
- Chase Bliss Blooper: Repeats makes the old layers fade only while overdubbing; in playback the loop doesn't fade. (summary) <https://blooper.chasebliss.com/resources/docs/manual.pdf>
- A players' rule of thumb is about 95 % a pass while overdubbing. (summary) <https://www.sweetwater.com/insync/loopy-gibson-4/>

**(b) Input-keyed ducking of the loop's gain, all frequencies together.**
- ValhallaDelay's duck modes apply "feedback and output ducking" from one knob, on purpose without threshold or timing controls; the advice for PitchDuck is to set the feedback for the sustain wanted after you stop, then raise the ducking for clarity while playing. No time constants published. (read via archive / summary) <https://valhalladsp.com/2019/11/26/valhalladelay-update-three-new-modes-and-ducking/>
- LuxeVerb: an envelope follower on the dry input can drive the Decay time (negative scale shortens it while you play), with threshold, a fast attack, release and smoothing. (read, link above)

**(c) Per-band or spectral replacement: no documented reverb does it.**
- The nearest are SuperCollider's PV_MagSmooth, a fixed leaky average per frequency bin (read) <https://doc.sccode.org/Classes/PV_MagSmooth.html>, and Zynaptiq Adaptiverb's filter that takes out tail components clashing with the input, a filter on the reverb's output, not a loop gain (summary) <https://www.zynaptiq.com/adaptiverb/adaptiverb-in-depth>. Both are FFT designs, out of reach on the module.

**(d) A new note replaces the held one outright.**
- EHX Superego (Auto: each new note or chord is caught and the previous one fades at the Speed/Layer rate) (read, fragments) <https://www.ehx.com/wp-content/uploads/2021/01/superego-manual.pdf>; Gamechanger Plus (each press adds a layer with its own fade, the oldest replaced). (read) <https://www.gamechangeraudio.com/pluspedal/>. These are sample-and-hold sustainers, not reverbs.

**(e) Highs that always decay faster than the body.**
- Jot and Chaigne's absorbent filters, one per delay line (AES 90th Convention, 1991; summary) <https://aes2.org/publications/elibrary-page/?id=5663>; J. O. Smith's treatment <https://ccrma.stanford.edu/~jos/pasp/Delay_Line_Damping_Filter_Design.html>; Clouds' in-loop low-pass (above). This is what our PLATE and HALL do, and what ADR 0022 took out of CANYON on purpose.

Not found: any DAFx or AES paper on an FDN whose decay is keyed to its input (two searches), any Costello note on Supermassive's build-up at 100 % feedback beyond what `docs/research/valhalla-notes.md` already has, or a named "constant-energy overdub". Not researched: Lexicon 224/480L, NightSky, Microcosm beyond its manual summary, Beads' source.

**What this means for us.** Our endless already *is* the Echoplex's Delay mode in structure: a loss that acts while you play and a hold when you stop (v1.0.61). It is just set to the equivalent of 99 % feedback a pass (0.4 dB on CANYON's 0.84 s mean line at SIZE noon), where loopers are played at about 95 %.

## 3. Candidate designs

### A. A turnover time (recommended)

- **Musical effect:** each new note wears the whole held wash down a little, so the wash is always made of roughly your last handful of notes; stop, and it holds as now.
- **How it works:** at endless, CANYON's "while you play" tail stops being the 120 s finite top and becomes its own shorter time, the turnover time, coming in across DECAY's crossover (0.80-0.85) on `map::governorWeight(decay)`, which `Engine.cpp:588` already works out. The hold is untouched: 0.25 s after the playing drops 30 dB under the tail, the loss glides out and the tail holds. One line near `Engine.cpp:587` and one voicing constant.
- **Sources:** Echoplex Delay mode and Blooper's Repeats (a); ValhallaDelay's feedback duck and LuxeVerb's envelope-to-decay (b).
- **What to expect, measured on today's finite CANYON tails with the same stimuli** (the played behaviour of this design, without the hold between notes):

  | Turnover time | old highs after 10 s of new dense notes | sparse | one sparse note wears the wash by about |
  |---|---|---|---|
  | 30 s (2 dB/s) | -16 dB | -20 dB | 2.5 dB |
  | 20 s (3 dB/s) | -23 dB | -29 dB | 4 dB |
  | 10 s (6 dB/s) | -47 dB | gone | 7.5 dB |

  With the hold cutting in between sparse notes, expect roughly half those rates in sparse playing: at 20 s, an old note about 15 dB down after 10 s and inaudible after 20-30 s, against today's 3 dB after 10 s. The last column assumes the loss stays on about 1.25 s a note, as now.
- **Cost on the M7:** nothing per sample. A multiply-add per control chunk on values already there (well under 0.01 % CPU). Flash: an estimate of 20-40 B (one blend and a constant); to be measured with `make -C firmware all-variants`, since 60 B is all there is. No libm.
- **Risks:**
  - **Level across the knob.** v1.0.61 made the turn into endless level-neutral. With a shorter played tail, endless while played sits lower than the 120 s tail beside it: about 4 dB in the dense pluck test at 20 s (-24.6 against -20.7 dBFS), up to 10·log(20/120) = -7.8 dB on a sustained source if nothing makes it up. `kCanyonWetTrimEndless` (`SpaceVoicing.h:52`) already crosses over on the same part of the knob and can carry the make-up for free; it needs a level-matched page (quiet and hot, sparse and sustained) as `renders/decay_endless_match` had. Today's endless is itself 4.6 dB *louder* than the 120 s tail in sparse playing, so this may land closer than now.
  - **A step in the loop's gain at each note.** The hold lets go at once when a note arrives (`Engine.cpp:1093-1094`), and the lines' loss coefficients are set per chunk without a ramp (`core/dsp/Fdn.cpp:666-672`). Today that step is 0.4 dB a pass at SIZE noon; at a 20 s turnover it is 2.5 dB (6 dB at full SIZE, 2 s lines). It lands under the new note's attack, but must be click-checked on a quiet sine entering a loud held pad at full SIZE. If it shows, the hold releases over ~50 ms instead (another ~20-30 B).
  - **Pumping.** The loss follows the playing with a 0.5 s glide out. On a held drone with a quiet, gappy input it could breathe. Check with a tremolo source and with BLOOM and SHIMMER, which keep feeding the tank after the note.
  - **Endless stops meaning "longest".** While played, endless is shorter than 3 o'clock on the knob. That is the point, but it changes the SPEC's P2 row ("while played the longest tail") and ADR 0008's v1.0.61 amendment: an ADR and a changelog line, CANYON only.
  - Energy: loss is only ever added (loop gain ≤ 1 holds); FREEZE is untouched (`rate` is already scaled out by `freezeMix_`); deterministic. Goldens for CANYON endless change.
- **Listening page:** today against turnover 30 / 20 / 10 s, level-matched, pick buttons. Cards: sparse high plucks then low then new highs; the key-changing melody; a sustained pad swell into a held wash; one loud note into a long-held quiet pad (the click and "does my pad survive one note" check). Each card ends with 30 s of nothing played, to hear the hold.

### B. The wash keeps its level (runner-up)

- **Musical effect:** the wash never gets louder than where it settled, and new playing pushes old notes out exactly as fast as it adds itself: play softly over it and it barely changes, play as loud as it and it is replaced within seconds.
- **How it works:** a held tail already has a ceiling at its own level (the governor's `holdDb`, `Governor.h`; `tailCeilingDb_`, `Engine.cpp:1080-1082`), dropped the moment you play (`Engine.cpp:1066-1068`). Keep it while playing at endless: each dB the new notes lift the wash above it adds flat loss (`kGovernorRatePerDb`), so energy in equals energy out.
- **Sources:** the floor governor we have (ADR 0008); the recursive-average form of PV_MagSmooth; no reverb or looper documents it keyed to level, so part of it would be our own (the survey's finding), which this project's rule counts against it.
- **Cost:** nothing per sample; control-rate logic for when the ceiling may rise (a first note into an empty tank, playing louder later). Estimate 60-150 B: over budget unless something else is given up.
- **Risks:** the governor's 1.5 s level follower sets the pace, so the wash dips after each loud note and recovers nowhere (a slow pump); a quiet first note pins the wash quiet until a rule lets it grow; the hold's make-up (up to 0.3 dB/s) could work against it. More to tune, more to get wrong.
- **Listening page:** as A's, plus soft-over-loud and loud-over-soft cards, since those are where it differs.

### C. The highs go first (third; could ride along with A)

- **Musical effect:** CANYON's endless wash keeps its body and lets the top end turn over, like HALL's; old high notes thin out, low ones stay.
- **How it works:** while played at endless, CANYON's highs take the shared T60 share instead of ADR 0022's equal one: `massiveHighRatio(toneRatio, decay)` instead of `decayFin` at `Engine.cpp:590`, as ADR 0022 first described. No new code.
- **Sources:** Jot and Chaigne's frequency-dependent decay; Clouds' in-loop low-pass (e).
- **Cost:** zero flash, zero CPU.
- **Risks:** it barely reaches the owner's notes. The loss shelf is first-order at 2.5 kHz (`kLossShelfHz`), so 1-2 kHz fundamentals get a fraction of it, and the highs' T60 would still be 38 s. And it switches on the endless tail's own high fade (`kInfiniteHighT60Seconds`) for a held tail above -24 dBFS, so a loud held wash would darken: against "hold forever". Undoes part of an owner's pick (ADR 0022's flat tail).
- **Listening page:** today against C alone and A + C, on the high / low / high card.

## 4. Recommendation

**A, the turnover time, CANYON only, starting from 20 s and 30 s on a listening page.** It is the one design here with a direct precedent (the looper's feedback-while-overdubbing; Valhalla's and LuxeVerb's keyed decay), it reuses the played/held switch v1.0.61 built, it costs no CPU and the least flash, and its one real cost, the level against the finite top, has a trim already sitting on the right part of the knob. B is closer to "replace" in the strict sense but is partly our own invention and doesn't fit the flash. C is free but doesn't solve what was heard.

Before a page: prototype on a `proto/` branch, sweep input level and run 10-minute holds (the energy check), click-check the note-into-held-pad case at SIZE 0 / noon / full, and confirm each voicing really rendered.

Not proposed here, but seen on the way: a held CANYON tail's notes smear into their neighbours over minutes (5-7 dB into adjacent bands in 2 minutes), because CANYON has none of the calm PLATE and HALL got in v1.0.58. That is part of the "static wash" feeling and is its own question.

## 5. Questions for the owner (as musical choices)

1. **How many notes back should the wash remember?** A handful (each note takes a clear bite: turnover ~10 s), a phrase (~20 s), or a long passage (~30 s)? The page answers this by ear.
2. **PLATE and HALL too, or CANYON only?** They turn over 3-5 times faster already; the same idea would work there but wasn't asked for.
3. **If you hold one long note or drone into endless, should the wash settle at a steady level behind it (a 20-30 s reverb while you hold, held when you let go), or keep growing as it does now?** The turnover makes it the first.
4. **Should a single stray note be allowed to thin a pad you've been holding for minutes?** With the turnover it takes about 2-4 dB off; FREEZE stays the way to protect a pad completely.
5. **Is it all right for endless, while you play, to be a shorter tail than 3 o'clock on DECAY?** That is what "old notes give way" means on the knob.

## Files

Stimuli and analysis scripts (not in git): `/private/tmp/claude-501/-Users-jesse-Documents-Sites-sublustris-versio/93d6ad72-0922-40f4-ba03-04d1e7fe1605/scratchpad/endless/` (`stim.py`, `stim2.py`, `an.py`). The renders were deleted after measuring; each is one `iv_render <stimulus> <out> --set mix=1 --set space=<PLATE|HALL|CANYON> --set decay=<1 | 0.92 | 0.442 | 0.333 | 0.158>` (endless, the 120 s finite top, and CANYON at about 30 / 20 / 10 s).
