# Balancing wet and dry: curves, levels and how a tail is heard

1 Nov 2026. The owner, while judging how a long tail drops in level as DECAY moves in or out of endless: "I'm also wondering how much the dry/wet balance and curve is impacting my perception as well." This builds on `docs/research/level-at-infinite.md` (27 Oct) and `rack-levels.md` (29 Oct, branch `claude/rack-levels`, not on main). Those already surveyed the mix laws of Valhalla, Strymon, Eventide, Chase Bliss and Make Noise, and the owner chose to keep the equal-power BLEND at noon (`rack_levels`, A on every card). This note adds Mutable's code, the perception research and how units handle infinite tails. Read-only: nothing was rendered.

## 1. Mix curves

- **Linear** (dry = 1 − x, wet = x). Each is −6 dB at noon. Linear suits a wet that is a copy of the dry (a chorus, a short delay), because the two add in amplitude. A reverb's wet is mostly uncorrelated with the dry, so their powers add instead, and a linear knob dips about 3 dB in the middle. ([Signalsmith on energy-preserving crossfades](https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/))
- **Equal power** (dry = cos, wet = sin). Each is −3 dB at noon, so the total stays level across the knob. It's the textbook choice for reverb. Valhalla uses it ("balanced in volume at all settings", cited in `rack-levels.md`). Mutable's Clouds uses it too, as a sin/cos table with a small dead zone at each end: `clouds/resources/lookup_tables.py`, [the eurorack repo](https://github.com/pichenettes/eurorack). The catch: noon is a true 50/50, which reads as a lot of reverb.
- **Dry at unity, the wet rising** (the send style). The dry stays at full over most of the knob, so noon means "the dry with reverb added". The Eventide H9000 and Chase Bliss Dark World work this way (sources in `rack-levels.md`). Strymon's even blend sits at about 3 o'clock. Eventide's Blackhole has a "special nonlinear taper" that puts most of the travel in the useful range ([Blackhole guide](https://www.bhphotovideo.com/lit_files/690187.pdf)).
- **Mutable Beads**, the most Eurorack-minded of these. It's a send law in code (`beads/dsp/fx/reverb.h:92-101`):
  - The dry is 1 − x², so −2.5 dB at noon.
  - The wet output is fixed, and the knob opens the *send* into the reverb instead (1 − (1 − x)⁴ ≈ 0.94 at noon).
  - Turning it down stops new sound going in, so a tail already ringing carries on instead of being cut.
  - Gain staging keeps the level inside the tank the same whatever the decay: less goes in as the decay rises, and the output makes it up.
- **Rings' and Clouds' reverbs** blend linearly, `x += (wet − x) · amount` (`rings/dsp/fx/reverb.h:120`). They are side effects there, not the main voice.
- **Eurorack.** A mixer downstream is common, so many modules make the far right fully wet. Most Versio manuals state no law. Fully wet plus a send/return is the reason ADR 0012 kept "no kill-dry".

## 2. Wet level against dry

- There's no standard. Units differ by more than 10 dB (the owner's takes, `rack-levels.md`):
  - ValhallaRoom: about +3 LU over the dry, flat with decay.
  - ValhallaPlate: −1 to +10 LU, louder as the tail gets longer.
  - Supermassive Sagittarius: about −0.4 LU.
  - Mix engineers, by contrast, put reverb far lower: one listening study summarised by Sage Audio found listeners preferred it about 9 LU under the dry (unverified, `rack-levels.md`).
- **Which measure.** Peak level says little about a tail, because peaks come from the first reflections. RMS is closer to what you hear. Loudness (BS.1770 LUFS, which is what we measure) is better still. Lee, Cabrera and Martens found that how reverberant a sound seems depends on the listening level. Measures built on how fast the *loudness* decays predicted it better than the usual T60 and EDT ([JASA 131, 2012](https://opus.cloud1.lib.uts.edu.au/handle/10453/115233)). So a level change in the wet also changes how long the reverb seems.
- **How long tails are kept from swamping the dry:**
  - make-up gain that falls as decay rises (Beads);
  - a ducker (the TC 2290, Eventide, Valhalla; our duck);
  - a level hold for endless (our floor governor).

## 3. Perception: what the blend does to a tail

- **Masking.** Griesinger treats reverberance as the loudness of the reverb "stream" against the notes' "stream". You hear reverb mostly where the notes stop, and less while they play ([Griesinger, "Further investigation into the loudness of running reverberation", IOA 1995](https://www.ioa.org.uk/system/files/proceedings/d_griesinger_further_investigation_into_the_loudness_of_running_reverberation.pdf)). With dry in the mix, the wet is judged against the dry. Moving BLEND changes that comparison, even when the wet itself hasn't changed.
- **A quieter wet sounds shorter.** A tail is heard until it sinks under the noise or the next notes. Start it x dB lower and it reaches that floor x/60 of a T60 sooner. At endless that has nothing to do with decay: only the level differs. At an 8 s T60, a tail 6 dB quieter disappears 0.8 s earlier. This is why a level drop at the endless crossover can sound like the tail "getting shorter".
- **How small a change is heard.** Listeners notice a change of 2-3 dB in the direct-to-reverberant ratio when dry and wet are about equal, near 0 to +10 dB. They are much less sensitive when one side dominates, at −10 or +20 dB ([Larsen et al., JASA 124, 2008](https://pmc.ncbi.nlm.nih.gov/articles/PMC2677334/)). Equal power at noon puts the owner where a level step in the wet is *easiest* to hear. At fully wet, or mostly dry, the same step is harder to notice. A JND of about 5 % in decay time is the usual working figure (ISO 3382 practice).
- **Dry leaving.** When playing stops, the total drops by the dry's share. At noon that drop is large, and it is heard as a dip in the reverb. That's the `input_stop` finding: ~3.5 dB on the PLATE pad.

## 4. Infinite and freeze

- **Eventide Blackhole:** Infinite keeps taking input, and Freeze mutes it (guide above). Strymon's Infinite and Freeze are the same split ([NightSky manual](https://manualmachine.com/strymon/nightsky/18984332-user-manual)).
- **Valhalla's Supermassive tips:** feedback at full and Mod Depth at 0. The loop's filters still dull a freeze over minutes ([Valhalla](https://valhalladsp.com/?p=567197)).
- **Elements:** freezes at time 1 with input 0. **Clouds:** its freeze raises the reverb amount (`granular_processor.cc:263-267`).
- **What the units have in common:** a frozen level is whatever was in the tank. An infinite one grows with what you play and needs saturation or a limiter to stay bounded. None we found publishes a level-matching scheme between finite and infinite. Ours is unusual in holding endless at a fixed level in dBFS, which doesn't follow the input. `review-2026-10-29.md` measured the result: endless is louder than the longest finite tail when you play softly, and quieter when you play hard.

## What this means for Sublustris

**What we do now.**
- BLEND is equal-power: −3 / −3 dB at noon, smoothed over 5 ms (`core/params/Mappings.h:25`, `mixGains`).
- DECAY's make-up keeps the wet's loudness roughly the same up to the finite top. Long PLATE and HALL tails get a trim (`SpaceVoicing.h:84-103`).
- Endless is held at a floor in dBFS with a 2:1 duck. The pending v1.0.60 level match moves that floor (HALL −2, MASSIVE +2 dB).

**How it compares.**
- The law matches Valhalla and Clouds. It's the most even-sounding law, but the drier side of practice (Strymon, Eventide's H9000, Beads) puts more knob before the even blend.
- The owner already kept equal power for the *playing* balance.
- The new point is perception. At noon the owner sits exactly where a level step in the tail is easiest to hear. The dry also masks the tail while playing and exposes it when playing stops. So part of a "drop" may be the blend rather than the reverb.

**Options, as musical choices:**

1. **"Is the drop I hear in the reverb, or in the blend?"** (a diagnosis, no change). A page of DECAY moves into and out of endless, and endless → 0.7, at BLEND 100 %, 75 %, 50 % and 30 %. Pad, keys, and quiet against hot playing. A level trace sits under each card. Existing pages can already do part of this with their live MIX fader.
2. **"When you turn BLEND down, should the tail already ringing fade with it, or ring on while new sound stops going in?"** (Beads' send law, on the wet side only; the dry law unchanged). The page compares as built against send-closing on BLEND moves made during a long tail, and on BLEND at 0 → 100 % with the tail held.
3. **"Should endless hold the level of what you were just playing, or a fixed level?"** (an endless floor that follows the recent playing loudness instead of a fixed dBFS). The page compares as built against input-following, on slow DECAY turns across the crossover at quiet, normal and hot playing, at BLEND 100 % and 50 %.

**Recommendation: option 1 first.** It costs no code. It answers the owner's question directly. It also tells us whether 2 or 3 are needed: if the drop disappears at 100 % wet, the issue is masking and the blend, and the reverb is fine. If it stays at 100 %, option 3 targets the real cause, endless's fixed level. The bump research already showed that the step changes with how hard you play.
