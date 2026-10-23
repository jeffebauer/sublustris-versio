# The M8 knob-feel audit (24 Oct 2026)

**Shipped as SPEC v1.0.21** (24 Oct 2026; owner: B on 22 of 23 cards of `renders/knob_feel`, the one A being ERA at 95 % on the skank). All five B curves are the defaults now; `IV_KNOB_FEEL` and its tuning hooks are gone, with `tools/pages/knob_feel.sh` and `knob_feel_measure.py`. On the way: MASSIVE's longer shortest tails made it up to 1.8 dB louder at full SIZE, so `map::massiveDecayLevelTrim` takes that off again (each setting keeps its loudness, as `hallSizeLevelTrim` does for HALL's SIZE). Release firmware 99,748 B.

**Status during the audit (before it shipped):** prototypes only, every one off by default; the owner's pick page is `renders/knob_feel` (built by `tools/pages/knob_feel.sh`). Nothing in the default sound changed. 27 default renders across SPACE × ATTACK × ERA / SIZE / DECAY corners are bit-identical to main's build (02af9fa), and `iv_test_golden` passes unchanged.

**The goal** (SPEC §2.3, `docs/TASKS.md` M8): every knob should change the sound evenly across its travel, with no stretch that does nothing (a **dead zone**) and no notch that jumps (a **cliff**). It was measured after today's changes: v1.0.16 Midiverb rebuilt, v1.0.17 HALL SIZE squeezed, v1.0.18 still lows, v1.0.19 ERA make-up, v1.0.20 DRIFT CW = Wander.

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## How it was measured

- **`tools/sweetspot.py`** (refreshed `docs/sweetspot.md`): every knob in every SPACE in 10 steps. The tool gained `--bin`, `--only`, `--json` (each step's raw measures, so a jump can be traced to the measure behind it) and `--jobs`. A step's size is its biggest change in just-audible units: 1 dB loudness, 10 % T60, 1 semitone brightness, 15 % arrival or peak time, 0.1 width.
- **`tools/pages/knob_feel_measure.py`**: the same measures in 20 steps (figures below are scaled back to per 10 % of travel), at settings the table doesn't cover (MASSIVE DECAY at SIZE 0 / ½ / 1, say). It prints A (today) and B (`IV_KNOB_FEEL=<name>`). "Worst ÷ median" is how many times harder the biggest notch moves than a typical one; the sweet-spot cliff line is 3×.
- **`tools/pages/hits_envelope.py`** and a build-dip measure (below), for the owner's Hits note.
- **`tools/pages/energy_check.py`**: the energy check for a prototype that reaches inside a loop.

## Re-checked findings

| Finding (before today's changes) | Now | Verdict |
|---|---|---|
| ERA: the first 20 % barely changes anything | 0.0 / 0.0–0.5 per 10 % in every SPACE (0–10 % is Clean by design; Neve then fades in on a smoothstep, flat at its start) | **confirmed** |
| ERA: the Midiverb stretch jumped | Still a cliff, now of a different shape: the box switches fully on between 85 % and 87 % (`kMidiEngageSpan` 0.02) and a click's T60 falls ×4.9 (HALL 3.9 → 0.8 s; MASSIVE 20 → 4.6 s; PLATE 2.5 → 1.55 s). The last 13 % then does nothing (bits are 13 → 13 since v1.0.16). Worst ÷ median: 27× PLATE, 53× HALL, 126× MASSIVE | **confirmed** (new shape: cliff then dead) |
| PLATE: TONE's first step from fully CCW jumps | 0.7 then 0.7, 0.7, 1.1… (20 steps: worst 1.6× median) | **gone** (the v1.0.15 loss shelf) |
| PLATE: SIZE's last step jumps | 0.6 (20 steps: worst 2.3× median, at 85 %) | **gone** |
| MASSIVE: DECAY's first step jumps | 13.6 (10 steps at SIZE noon); 19× at SIZE ½ and 27–30× at SIZE 1 in 20 steps. It's the T60: 0.79 s at DECAY 0 for a mapped 2 s | **confirmed**, worse at full SIZE |
| MASSIVE: DRIFT nearly flat from 20–50 % | 1.2 0.8 1.8 1.1 0.7 0.3 1.7 … (no dead zone). Wander moved the CW side; the measures don't see pitch, so this is level and width only | **gone** (by these measures) |
| MASSIVE: TIME (NORMAL) does nothing for its first 30 % | dead 0–20 % at SIZE ½, 0–40 % at SIZE 1 (20 steps) | **confirmed** |
| Owner: "MASSIVE medium and large a little uneven on Hits" | The swell after a hit dips 2.8 dB (medium) and 5.8 dB (large) before its peak, against 1.6 dB for MASSIVE small, 1.0 for a large PLATE and 0.4 for a large HALL | **confirmed** (level, not grain) |

**New findings:**
- **BLOOM's first notch jumps** in PLATE (16.6 per 10 %, 6.2× median) and HALL (10.0, 3.1×). The wet's arrival goes from ~20 ms to 76 ms (PLATE). [M] The swell stage hands over to the direct path below 0.12 s (`Swell.h`, `kDirectBelowSeconds` 0.04 → `kTapsFromSeconds` 0.12), and BLOOM's range starts at 0.1 s, so TIME fully CCW still lets a quarter of the dry attack straight through.
- **MASSIVE ERA:** dead 0–50 % and 60–80 % by these measures. Neve and cassette barely move MASSIVE's loudness, T60 or brightness, and the cassette's wow and the μ-law grit are things these measures can't see. Every SPACE's μ-law stretch (65–85 %) reads near 0 for the same reason. **For the owner's ears**: do Neve, cassette and μ-law do enough in MASSIVE?
- **PLATE DRIFT CW** reads small (0.2–0.5 per 10 %): Wander's pitch isn't measured, so this isn't a dead zone. **PLATE DECAY's** 80 → 90 % step is the crossover to infinite, by design.
- **Side finding, not knob feel** (for the main session): the energy check's 10-minute hold, judged like `infiniteHoldsTheFloor`, shows an infinite **PLATE with ERA in the Midiverb** (0.88–1) creeping up 1.3–1.4 dB above its 60–70 s level over 10 minutes. That fails the test's "never above the first window + 1 dB", though it stays within 3 dB and under the floor. The test only checks HALL and MASSIVE at ERA 0, so no gate catches it. Fed 5 dB hotter (Gaussian noise, std 0.3), infinite PLATE and HALL tails at ERA 0 sagged to −40 dBFS and took minutes to climb back to the floor (−25.5). Both are today's behaviour.

## What established units do (sources)

The research was done by a sub-agent. valhalladsp.com and KVR block fetchers, so their lines come from search extracts.

- **DECAY: equal ratios per notch.**
  - [S] The just-noticeable change of reverberation time is ~5 % (ISO 3382-1, after Seraphim 1958; studies range from 3.3 % to 26 %) (https://digitalcommons.unl.edu/archengfacpub/63).
  - [S] Below ~0.6 s the threshold is a fixed amount instead, 0.042 ± 0.015 s (Niaounakis & Davies, JAES 2002; https://salford-repository.worktribe.com/output/1466111/perception-of-reverberation-time-in-small-listening-rooms).
  - [I] So an even DECAY is exponential in T60, as ours is (SPEC §3).
  - [S] Valhalla warps its DECAY control "exponentially" toward short times (Costello on KVR, https://www.kvraudio.com/forum/viewtopic.php?p=4783225).
  - [S] Strymon's BigSky shows decay as RT60 per machine (Hall 0.5–20 s, Cloud 1–50 s; https://www.tubesoundbcn.com/img/manual/BigSky_UserManual_RevD.pdf).
  - [S] ValhallaPlate says the "exact curve is dependent on the MODE" (https://valhalladsp.com/?p=74068).
  - [I] A maker shapes each mode's range to what that structure can ring. MASSIVE's 2 s floor is shorter than its long lines can ring: with a 0.84 s mean line, a 2 s T60 loses 25 dB a pass, so the tail is the build and one echo. Our own PLATE comment already says "a big plate can't ring for only 0.4 s".
- **Predelay: the first tens of ms are fused.**
  - [S] Precedence: a reflection fuses with the direct sound within ~1–5 ms for clicks and up to ~40 ms for speech or piano (Wallach; https://en.wikipedia.org/wiki/Precedence_effect).
  - [S] Griesinger: with sharp attacks the first 50 ms "can be separately distinguished" (https://www.ioa.org.uk/system/files/proceedings/d_griesinger_further_investigation_into_the_loudness_of_running_reverberation.pdf).
  - [I] That is why NORMAL's x² curve suits PLATE and HALL, whose wet arrives within ~20–60 ms.
  - [I] In MASSIVE the wet already arrives 0.3–0.6 s after a hit (its swell and SIZE predelay). A change is heard as a share of that arrival (sweetspot's 15 % unit), so a few ms of predelay is nothing, and equal steps are even.
- **The build envelope after a hit.**
  - [S] Lexicon's Random Hall and Concert Hall shape the build with **Shape** (the contour of the reverb envelope) and **Spread** (how long the build lasts). The build is a designed contour, not a by-product. (480L / PCM manuals as cited by the research agent; the PDFs weren't read first-hand.)
  - [I] MASSIVE's early taps are its build. Today's three taps per line sit in three bunches, and the bunches leave a gap before the lines' first echoes. Making the build continuous follows the Lexicon idea without adding a control.
- **Multi-stage "character" knobs.**
  - [S] Strymon's Riverside DRIVE is "continuously variable circuit tuning" across one knob (https://www.manualslib.com/manual/1438544/Strymon-Riverside.html).
  - [S] A DAFx 2026 single-control renderer derives five cues from one control, kept "mutually coherent by construction" (https://www.emergentmind.com/papers/2606.08286).
  - [I] Each stage should come in gradually across its own stretch. None should switch fully on within a sliver of the travel.
- **Audio-taper convention** (https://www.amplifiedparts.com/tech-corner/potentiometer-types): the taper puts the travel where the ear needs it. The researchers found no source stating "the first notch must do something audible" as a rule; that's our SPEC §2.3 goal.

## Prototypes (all behind `IV_KNOB_FEEL`, off by default)

**Code layout:**
- The renderer reads `IV_KNOB_FEEL=decay,era,time,bloom,hits` (or `all`) in `host/render/main.cpp` and calls `Engine::setKnobFeel(mask)`.
- The curves live in `core/params/Mappings.h`, in the section marked PROTOTYPES (`map::feel`): `massiveDecayRateFeel`, `eraKnobFeel` / `midiEngageFeel`, `massiveNormalPredelayFeel`, `bloomSecondsFeel`. Their tunings sit in `map::feel::tuning()`, set by `IV_FEEL_PASSES`, `_CLEAN`, `_MIDIPOW`, `_BLOOMMIN`, `_TAPS`, `_TAPCOUNT`, `_TAPPOW` and `_TAPEVEN`; the defaults are the page's B.
- `hits` changes the FDN's early-tap layout (`Fdn::setEarlyLayout`). Its default call reproduces today's seeded layout exactly.

| Hook | What B changes | Before → after (worst notch ÷ median; per 10 %) |
|---|---|---|
| `decay` | MASSIVE's shortest T60 = max(2 s, 3 × mean line): 2 s at SIZE 0 (unchanged), 2.5 s at noon, 6 s at full SIZE; still exponential to 120 s; crossover unchanged. Reaches only T60s that already exist (pure mapping) | SIZE ½: 4.9× → 2.2× (19.0 → 5.2 first step); SIZE 1: 7.0× → 3.0× (27.2 → 2.6 first step, 29.6 → 9.7 worst); SIZE 0 unchanged (1.9×) |
| `era` | Clean only 0–3 % (was 0–10 %), Neve's share grows in straight steps from there (the knob read through the inverse of the layer smoothstep); the Midiverb box engages across 85–100 % as (share)² instead of within 85–87 % | HALL 53× → 30× (one 33.3 step → 9.8 / 16.2 / 7.4 over the last 15 %); MASSIVE 126× → 56×; PLATE 27× → 18×. First 10 %: 0.0 0.0 → 0.2 0.4 (HALL) |
| `time` | MASSIVE NORMAL predelay 0 → 250 ms in straight steps (was ∝ x²). PLATE / HALL keep x² | dead 25 % → 0 (SIZE ½), 40 % → 5 % (SIZE 1), 15 % → 5 % (SIZE 0); steps an even 0.3–0.5 |
| `bloom` | BLOOM's swell 0.12 → 4 s (was 0.1 → 4 s), so fully CCW is already all taps | PLATE 6.2× → 1.8× (16.6 → 2.3 first step); HALL 3.1× → 1.9× (10.0 → 1.6); MASSIVE unchanged |
| `hits` | MASSIVE's early taps: 6 per line instead of 3, each line's taps offset by its own eighth of a slot so the 8 lines interleave (shorter lines first), × 0.71 so their total level is unchanged. Output only: the loop is untouched | build dip, mean of 18 isolated hits: SIZE 0 1.6 → 1.1 dB, ½ 2.8 → 2.7 (DECAY 0.4) and 2.7 → 1.8 (DECAY 0.7), ¾ 3.7 → 2.6, 1 5.8 → 3.5 (DECAY 0.4) and 7.1 → 3.2 (DECAY 0.7) |

**What didn't work (hits):**
- More tap level, quieter early taps (gains rising to a power of 2 or 3) and 3 interleaved taps were each no better or worse at some size.
- 3 interleaved taps cost nothing, but they made SIZE 0 grainier (2.6 dB) and large DECAY 0.7 6.1 dB.
- With the taps off the build has no dip (1.3 dB at large), but nothing comes out for 1.3 s, which the owner rejected at M4.

**The build-dip measure:**
- An isolated snare or rim, each at three block offsets.
- The 50 ms level envelope (10 ms hop) from 30 dB under its peak up to the peak.
- The biggest fall below its own running maximum: how far the swell falters before it peaks.
- The scratch tool is the method; `tools/pages/hits_envelope.py` is the per-hit version, with lumps listed.

**Costs and limits:**
- `hits` doubles the early-tap reads: 24 more interpolated reads per sample in the main tank (and the low tank at 12 kHz). That needs an M3-style CPU check on hardware before it ships.
- The other four cost nothing at run time.
- **Firmware flash with the hooks compiled in:** release 102,076 B (main 99,180: +2,896 B, mostly `asin`/`sin`/`pow` pulled in by ERA's and the taps' prototypes), m0test unchanged at 82,356 B, profile 103,032 B (main 100,184). That's still under the 110 KB release limit. Whatever is picked should land as plain constants, and the unpicked hooks should come out (or be compiled only on the desktop).
- ERA B can't make the Midiverb stretch as even as the rest of ERA. Its total change (a click's tail ×4.9 shorter: the 13-bit truncation toward zero eats a fading tail at every frequency, 250 Hz too) is ~10× larger than all the other stages together, and B only spreads it across the 15 % it owns. Making it truly even would mean giving the Midiverb more of the knob (the layer boundaries, ADR 0003) or a gentler box. **That's the owner's call.**

## Energy check (ERA's B: a share of the Midiverb box inside the loop)

- [I] Partial engage is safe in principle: |a·H + (1 − a)| ≤ a|H| + (1 − a) ≤ 1 when the box's |H| ≤ 1.
- [M] `tools/pages/energy_check.py --feel era --set era=0.865 --set era=0.9 --set era=0.95 --ref era=0.85` measured the levels, B against the louder of today at the same ERA and today at 85 % (box out):
  - 405 settings: every SPACE × SIZE 0 / 0.6 / 1 × DECAY 0.5 / 0.75 / 0.95 × DRIFT 0 / ¼ / ½ / ¾ / 1 × 3 ERAs.
  - B was never more than **+0.07 dB** above A in any window (pass).
- [M] 10-minute infinite holds, fed and judged like `infiniteHoldsTheFloor`:
  - HALL and MASSIVE passed at all three ERAs, within 0.3–0.8 dB.
  - PLATE passed at 0.865, but at 0.9 and 0.95 it rose 1.5 and 2.4 dB above its 60–70 s level.
  - Today's PLATE at ERA 0.9 / 0.95 / 1 rises 1.3–1.4 dB the same way (the side finding above).
  - B starts lower (−29.9 against −28.5 dBFS at 0.95) and ends at today's level (−27.5 against −27.1), so it's a recovery to the same ceiling, not a build-up past today's. It still fails the strict criterion, as today does.
- Not energy-checked, because they only reach settings that already exist or leave the loop alone: `decay` (T60s inside today's range), `time`, `bloom` (mappings) and `hits` (output taps only).

## The page

`renders/knob_feel/index.html`, built by `tools/pages/knob_feel.sh` (copying the setup and `r()` of `tools/render_review_pages.sh`):
- Renders are 100 % wet with `--wet-only --dry`, level-matched, and the page asks for picks.
- Five columns, with rows p1–p3 for positions along the uneven stretch; every card has A (today) and B (the hook):
  - **1_decay:** MASSIVE at full SIZE, DECAY 0, 0.1, 0.2.
  - **2_era:** HALL, ERA 5 %, 87.5 %, 95 %.
  - **3_time:** MASSIVE NORMAL, TIME 0.1, 0.2, 0.3.
  - **4_bloom:** PLATE BLOOM, TIME 0, 0.1, 0.2.
  - **5_hits:** MASSIVE SIZE ½ and 1, DECAY 0.4, on hits and drums.
- The material is skank and drums throughout, apart from 5_hits.
- The script refuses to build the page if any B is bit-identical to its A (md5).
- The README asks each question in plain words, e.g. "Does DECAY now grow evenly from the first notch?"

## What needs the owner

1. The five picks on the page.
2. **DECAY B lengthens MASSIVE's fully CCW tail** at big SIZEs (6 s at full SIZE instead of a 2 s tail that sounds like one echo). Is that still short enough?
3. **The Midiverb's share of ERA:** B spreads its change over its 15 %, but it still moves the tail far more per notch than the rest of ERA. Give it more travel?
4. **MASSIVE's Neve, cassette and μ-law stretches** barely register on these measures. Do they do enough by ear?
