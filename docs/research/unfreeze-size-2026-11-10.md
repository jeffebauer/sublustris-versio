# SIZE right after an unfreeze (the "second crackle"), 10 Nov 2026

Research before designing. Nothing was built, rendered or lapped here: read-only on the beta's Core (`.claude/worktrees/beta`, `beta/player-feedback` at `e9bc141`), plus web sources.

## The problem

The owner, on round 2: "A crackle when I turn SIZE during the second the pad fades out after I let go of FREEZE, in HALL and CANYON." It is a module overload, not a click: the 29 Oct audit (`docs/research/m3-review2-audit-212b68b.log`, a v1.0.45+ build, so it already has the small room and the four-group stagger) measured its UNFRZ lines (frozen at SIZE 0.3, the hand moves under the freeze, a tap unfreezes at 0.2 s) at up to **107.5 % in CANYON** (thousands of late blocks) and **97.5 % in HALL**. The reverb's usual worst is 97.6-98.6 %, and audio drops near 99 %.

One correction to the framing in the brief: in HALL and CANYON the SIZE crossfade is **not** a second tank. It happens inside the one FDN (each line reads its old and its new length for 0.3 s), in the high tank and in the still-lows tank alike. The "twin" is the **small room** (`dsp/TwinRoom.h`), the layer you play over the pad. The overload is the small room still ringing after the unfreeze **plus** the FDNs' double reads.

The audit's split shows the shares: on the UNFRZ lines the twin costs **10.3-13.0 %** of the block (0.6 % when it isn't running), on top of a tank that is already near the edge.

## How the Core sequences it now (beta, file:line)

**At the freeze** (`core/dsp/Engine.cpp:380-398`): the big tank keeps the SIZE it had (`heldSize_`, 383-384), its input closes over 20 ms, and the twin starts (`startTwin`, 397, 421-439), unless a SPACE change is running, in which case the twin waits (394-396).

**While frozen**, SIZE plays only the twin:
- The tank reads `heldSize_` (`applySpace`, `Engine.cpp:476`).
- The room takes SIZE as it comes and **glides** its short lines, with no crossfade (`Engine.cpp:816-850`; `TwinRoom.h:13`).

**At the unfreeze** (`Engine.cpp:399-403`):
- The tank's input reopens.
- The twin's input closes over `kTwinFadeSeconds` = 20 ms (`SpaceVoicing.h:293`).
- The pad's colour glides back over `kUnfreezeSeconds` = 1 s (`freezeMix_`, `Engine.cpp:942-943`; `SpaceVoicing.h:265`). That is the "second the pad fades out".

**The twin keeps running after the unfreeze** until its own 50 ms power is under -100 dB (`Engine.cpp:1228-1247`; the stop is at 1247). Its loss is at most `kTwinMaxT60` = 8 s (`SpaceVoicing.h:279`). So at a long DECAY it rings, at full cost, for several seconds after the unfreeze. *Inferred:* about 9 s for a room 70 dB above the stop level. With its input closed, its input diffusers still run every sample on zeros (`Engine.cpp:1169-1172`), and so do the room's own input stages (inside `twinHalf_.process`, 1230).

**SIZE lands on the tank as soon as the unfreeze happens:**
- From the next block, `applySpace` reads the knob through the hysteresis instead of `heldSize_` (`Engine.cpp:476`), and `fdn_` and `lowFdn_` get new target lengths (`Engine.cpp:623-630` -> `Fdn::setMeanSeconds`, `Fdn.cpp:309-319`).
- `Fdn::control` (`Fdn.cpp:468-505`) waits until the targets have rested `kSizeRestSeconds` = 50 ms (475, 485; `SpaceVoicing.h:51`).
- Then it starts one crossfade of 0.3 s (`kSizeCrossfadeSeconds`, `SpaceVoicing.h:50`; 505) on one group of lines: a quarter of them, so 2 of 8 lines read twice (490-501; `Fdn.h:143-154`).
- The next group follows at once. A whole move is about 1.2 s of crossfades, in both FDNs at the same time.
- Only one crossfade runs at a time per tank (`xfLeft_ <= 0`, 485). A move made during a fade waits, then goes again. So a knob turning through the unfreeze second keeps a crossfade running for its whole length plus about 1.2 s.

So any SIZE that differs from the frozen one starts the tank's crossfades 50 ms after the unfreeze, while the room is still at full cost. That happens if SIZE moved while frozen (which is the twin's normal use) or moves just after.

**The PLATE already avoids this:** frozen, and through the 1 s unfreeze glide, its SIZE glides instead of crossfading (`Engine.cpp:607-609`, and the hysteresis exception at 455; ADR 0043, the owner's pick A, "a crossfade there lost the tail's upper mids").

**House precedents for "one heavy transition at a time":**
- A freeze during a SPACE change: the twin waits (`Engine.cpp:394-397`).
- "Finish, then go": the SPACE switch isn't read while a morph runs (`Engine.cpp:1014-1017`).
- FREEZE early in a morph keeps the old space, and the change waits for the unfreeze (`Engine.cpp:1018-1025`, `morphHoldOld` 1514-1537; ADR 0055).
- When a tank must leave quickly, it goes in 0.3 s (`morphFast_`, 1572).

**Hooks that exist already:** `twinActive()` (`Engine.h:230`) and `Fdn::xfIdle()` (`Fdn.h:163`).

**Flash:** release 111,236 B, 1,404 B under budget (ADR 0055). Any fix must be a few compares, not a new path.

## What established units and designs do

| Approach | Who | Source |
|---|---|---|
| **Fixed lengths: size isn't a live length change at all** | Mutable Instruments' Clouds, Rings, Elements and Beads: the delay lengths are compile-time constants (`E::Reserve<113, 162, ... 4782>`). The setters are time, amount, diffusion, lp and input gain, with no size. Elements freezes the way we do. | [clouds reverb.h](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/fx/reverb.h), [elements reverb.h](https://raw.githubusercontent.com/pichenettes/eurorack/master/elements/dsp/fx/reverb.h), `docs/research/mutable-instruments-reverbs.md` |
| | Dattorro's plate: fixed lengths, only a small "excursion" modulation on two allpasses. | [Dattorro, Effect Design Part 1](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf) (from the paper as known; the PDF didn't parse here) |
| | Freeverb: fixed comb and allpass lengths in `tuning.h`. *From the source as distributed, not this page:* its "roomsize" is the comb feedback, not a length. | [JOS, PASP: Freeverb](https://ccrma.stanford.edu/~jos/pasp/Freeverb.html) |
| **Glide the lengths (a pitch bend that settles)** | Eventide H90 / Blackhole and Hall. Eventide-forum reply on Size: toward smaller there's "an apparent pitch shift upwards" and toward larger it goes down; slow turns bend briefly and settle, fast turns bend further and recover more slowly. | [H90 thread](https://www.eventideaudio.com/?p=175832), [reply](https://www.eventideaudio.com/?p=175875) |
| | Strymon NightSky makes the glide the instrument: a "variable rate process to expand and contract the reverb core size", which moves size and pitch together, with a Glide control. | [strymon.net/nightsky](https://www.strymon.net/nightsky) |
| **Mute while size changes** | Lexicon 480L (UAD's manual, per its search summary; the page refused the fetch): adjusting Size with signal present momentarily mutes the reverb. | [UAD 480L manual](https://help.uaudio.com/hc/en-us/articles/33194625601044-Lexicon-480L-Digital-Reverb-and-Effects-Manual) (not read directly) |
| **Live size is accepted as imperfect** | Acon Digital's developer: "virtually impossible to automate the size parameter ... without any artifacts". | [KVR, Verberate](https://www.kvraudio.com/forum/viewtopic.php?p=7241162) |
| | A KVR user on Valhalla: Size is "the one parameter on Valhalla reverbs that can make audible noises". A user post, not Sean Costello's. | [KVR, VintageVerb](https://www.kvraudio.com/forum/viewtopic.php?p=9142799) |
| **Crossfade two delay settings** | PASP recommends crossfading between the two delay settings rather than gliding when retuning is unwanted. This is our ADR 0025. | [JOS, PASP: Delay-Line Interpolation](https://ccrma.stanford.edu/~jos/pasp/Delay_Line_Interpolation.html) |

**What units do with size *during* a freeze or right after one:** I found no published behaviour for Eventide, Strymon BigSky or NightSky, Meris or Valhalla. The Mercury7 has no size control at all (its controls are Space Decay, Modulate, Mix, Lo/Hi Frequency and Pitch Vector: [Mercury7 manual](https://www.analoguehaven.com/meris/mercury7/manual.pdf)). Blackhole's freeze sits on the Feedback parameter. *Inferred:* hardware that sells a freeze doesn't advertise a clean size change across it, which is consistent with nobody having a cheap answer.

**Budgeting the CPU:** Ross Bencina's rule for real-time audio is to design for the worst case, not the average, and to "amortize computation across many audio samples ... rather than using 'bursty' algorithms" ([Real-time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)). Two transitions that overlap make a burst. The house fix pattern (ADR 0054/0055) is to **serialise** them: one heavy thing at a time, and the cheaper one yields.

## The options

"Cost in the twin's fade" means the extra per-sample work while the room still rings after an unfreeze.

| Option | Cost in the twin's fade | What you'd hear |
|---|---|---|
| **A. Glide SIZE after an unfreeze** (as the PLATE does, ADR 0043) | About zero: the lines already read between samples, so only their read position moves. No double reads. | **A pitch bend.** The tail slides up when SIZE goes smaller, down when it goes bigger, like tape. The owner rejected exactly this for HALL and CANYON (ADR 0025: "slow pitch shifts that make it sound really out of tune"). Kept to SIZE's semitone limit (`kSizeSlewPerSecond`), a big move would take tens of seconds to arrive (*inferred:* x4 in lines is about 24 s). Fast enough to arrive means bends of several semitones. Only good for small moves. |
| **B. Hold SIZE until the twin has stopped on its own** | Zero. | **The knob seems dead** for as long as the room rings: up to several seconds at long DECAYs (*inferred*, from the 8 s T60 and the -100 dB stop). Then the whole move lands at once, as a normal SIZE crossfade. |
| **C. The twin makes way** (new): a SIZE move pending after an unfreeze fades the room out quickly (0.3 s, as a morph's fast drop), and the tank's crossfade waits for it to stop. | Zero once the room has gone. During its 0.3 s fade-out the room still runs, but the tank only waits (it is pending, held at its lengths). | **SIZE answers about 0.3 s later than usual** (0.35 s after the hand rests instead of 0.05 s). **The quiet echo of what you played over the pad is cut short** when you touch SIZE after letting go. By then its input is shut and it sits about 8 dB under the pad, so it's the end of a tail fading away. If SIZE moved while frozen, the unfreeze itself triggers it: the twin layer goes about 0.3 s after you let go. |
| **D. A capped wait:** hold SIZE until the room is quiet (for example 30-40 dB below its level at the unfreeze), or at most about 1.5 s, then as C. | Zero. | Between B and C: the twin's tail is kept longer and SIZE lags by up to the cap. A longer, rubberier lag than C. |
| **E. A cheaper crossfade:** 8 groups, an eighth of the lines reading twice, after an unfreeze. | About half the double reads. *Inferred:* not enough at 107.5 %, since the room alone is 10-13 %. | The owner heard "some slight staggering" at 8 groups (`Fdn.h:148-149`). A move takes 2.4 s. |
| **F. Trim the twin's idle work** (alongside C or D): once its input has been shut about 100 ms, stop running its input diffusers and the room's input stages on zeros. | Removes work (*inferred:* a few % at most). | Nothing audible, if the diffusers' few-ms ring has died first. On its own it's not enough. |

Retargeting an in-progress crossfade isn't the problem: there is already only one fade at a time per tank. The overlap is between the room and the fade.

## Recommendation

**C, "the twin makes way",** as the house rule: in HALL and CANYON, a SIZE change and the small room don't run together.
- When the tank has a SIZE move pending (`!xfIdle()`, or a target off the held lengths) and the twin is running unfrozen, the room fades out in 0.3 s and stops.
- The tank's crossfade starts only once `twinActive()` is false.

It removes work instead of adding it, which is the brief's rule. It follows ADR 0055's pattern (the lighter thing yields, a 0.3 s exit), costs a few compares of flash, and changes nothing outside the unfreeze window. F can ride along if a lap shows C alone near 99 %.

**Two voicings for the listening page** (HALL and CANYON; frozen chord, play over it, let go, turn SIZE within 0.5 s; and the same with SIZE moved while frozen):
1. **"Twin makes way" (C):** the room fades in 0.3 s, then SIZE lands.
2. **"Twin rings, SIZE waits" (D, 1.5 s cap):** the room keeps its tail up to 1.5 s, or until it's 30 dB down, then goes, then SIZE lands.

Control: today's build. On the desktop it renders without the crackle, so the page judges only how the fix sounds. CPU is judged by a module lap with lines that turn SIZE within 0.5 s of an unfreeze, plus the audit's UNFRZ lines.

I'd leave A (glide) off the page: the owner already turned it down for these spaces. If they want it, it fits small moves only, as the PLATE's "4 % or less" does.

## Open questions

- **Is 0.3 s of SIZE lag acceptable by hand?** It only happens in the second or so after an unfreeze, but it's in exactly the gesture the owner makes.
- **Should touching SIZE kill the twin, or would the owner rather keep the layer and wait?** That is C against D.
- **Is the room plus *one* group's crossfade under 99 % in CANYON?** If it is, the twin could stay and only the stagger change (E). That needs a module lap, not reasoning. The audit split doesn't separate the high tank's double reads from the low tank's.
- **The CANYON -> PLATE 101.5 % left over from ADR 0055** may share a cause. Not checked.
