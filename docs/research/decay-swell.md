# DECAY turned down: the swell (28 Oct 2026)

**Status: a prototype behind a renderer hook (`IV_DECAY_RISE`, `Engine::setDecayRiseCapPrototype`), on `renders/decay_swell` for the owner's pick. Nothing has changed in the build.**

The owner, on v1.0.54 at the rack: "with massive/hall with long decay, moving the decay from large to small can create a swell in amplitude".

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## What happens

- [M] **Reproduced.** HALL and MASSIVE (SIZE 0.5 and 0.8), DECAY from 0.78, 0.83 or 0.92 down to 0.5, 0.3 or 0.2, turned in 0.2, 1 or 4 s, with the pad playing and on a tail: 216 moves. We took the wet's 50 ms RMS and asked how far the moved render rises above both unmoved renders (DECAY held where it started, and held where it ends). Today it rises by up to **+8.8 dB in MASSIVE while playing and +6.5 dB on a tail**, and by **+4.6 / +4.4 dB in HALL**. The swell peaks 0.3-1.3 s after the knob and fades over 2-3 s (`renders/decay_swell/_verify.txt`).
- [M] **The swell is old.** MASSIVE's numbers are the same at v1.0.49, v1.0.52 and v1.0.54. v1.0.49 was worse while playing (+9.2 MASSIVE, +5.5 HALL), because v1.0.51's ducking changes took some of it out. v1.0.53 (ADR 0034, HALL's longer range) made HALL's worse: on a tail it went from +2.8 to +4.4 dB, and while playing from +3.5 to +4.6 dB. The swell has been there since v1.0.28 (ADR 0020).
- [M] **The cause is DECAY's level make-up.** With the make-up off (`IV_DECAY_MAKEUP=0`), the tail swell is gone: −0.0 dB in both SPACEs. While playing what's left is +1.4 HALL and +3.2 MASSIVE, and that has another cause (below). The other suspects move the level by little or in the other direction:
  - `massiveDecayLevelTrim` turns MASSIVE down at short DECAYs.
  - `hallSizeLevelTrim` rises by +0.6 to +1.6 dB when HALL leaves endless. It glides over the SIZE crossfade and measured nothing on its own.
  - The ducking letting go: with `IV_DUCK_RATIO=1` the residual barely changed.
  - MASSIVE's swell attack shapes only new sound.
  - The v1.0.53 long-tail trim lowers the level as DECAY leaves endless, and lets go only further down, while the make-up rises much more.

### The mechanism, file:line (v1.0.54)

- `core/params/Mappings.h:239` `decayLevelMakeup`: a shorter tail gets more gain, `(15 s / T60)^p` below 15 s and `(top / T60)^p_long` above (`core/params/SpaceVoicing.h:74-80`). From MASSIVE's 120 s down to a ~16 s tail, that's about +9.5 dB.
- `core/dsp/Engine.cpp:445` sets `decayGain_` from it every block. `core/dsp/Engine.cpp:1085` glides `decayGainNow_` to it in ~40 ms (0.0005 a sample), and `:1089-1099` multiplies the tank's whole output and the twin by it.
- [I] So the gain is a gain **after the loop** (ADR 0020 says so: "the loop and its holds are untouched"). When DECAY drops, the tank is still full of the long tail's energy. The make-up meets that energy within 40 ms and turns it up by up to 9.5 dB. The new, shorter tail needs about one new T60/4 before it has replaced the old energy (2-4 s in MASSIVE), so for those seconds the old tail is heard boosted. While playing, the level then settles where it was before (the make-up is matched in the steady state). On a tail it then dies away at the new rate, from the boosted level.

### What's left: MASSIVE's loudness bump near 0.78 (not the make-up)

- [M] With the pad playing, held MASSIVE is about **4 dB louder at DECAY 0.75-0.80 than at endless or at short DECAYs**. At SIZE 0.8: −20.1 dBFS at 0.78, against −24.2 at 0.92 and −24.7 at 0.2. HALL has no bump: it only gets quieter from endless down.
- [I] At endless the ducking (ADR 0014) and the floor governor (ADR 0008, 0033) hold MASSIVE down while it's played. Both fade out across 0.85 → 0.80. The make-up matches the finite settings to the 120 s top's level, and that level is louder than ducked endless.
- [M] So a slow turn from endless passes the bump on the way down: +3 to +3.5 dB for a 4 s turn, whatever we do to the make-up. With the make-up off, B, or the make-up moved to the tank's input, it's +3.2 / +3.5 / +3.4. Taking the bump out means voicing the held settings themselves (endless's level while playing, or the make-up near the top). That's the owner's call, on a page of its own, and isn't part of this fix.

## How others handle it

- [S] **Jot's FDN** (Jot & Chaigne 1991, *Digital delay networks for designing artificial reverberators*, AES 90th Convention): the decay time is set by attenuation inside the loop. A tail's energy then grows with its T60, and nothing in the classic design makes up the level on the output.
- [S] **Normalisation goes on the input.** Magic Leap's "Reverberation gain normalization" (US 10,810,992 B2, Audfray, **Jot**, Dicker; filed 2019) corrects the reverb's power with factors applied in series **before the reverberator** ("the reverberation effects are introduced after the RIP correction factor is applied", claim 14). The patent says that adjusting the decay time at runtime then corrects automatically, because those factors stay the same. Only sound that enters after a change gets the new gain, and what is already ringing keeps its level.
- [S] **Time-varying FDNs** (Schlecht & Habets 2015, *Time-varying feedback matrices in feedback delay networks and their application in artificial reverberation*, JASA 138(3)) change the loop while it runs. The decay is again a property of the loop's gains, so a change affects how the energy already inside decays, not its level at that moment.
- [I] **What this means here:** a make-up gain after the loop turns up the tail that's already ringing. The established place for such a gain is the reverb's input, or a gain that never rises faster than the old tail fades.
- [M] **Input-side reference.** We measured it with a renderer mode, since removed (`Engine::applySpace`'s gain moved onto the tank's input). It removes the swell (tail −0.0 dB; playing HALL +1.3, MASSIVE +3.4, the bump). It isn't bit-identical on held settings, though: the null is −44 dB in MASSIVE and −53 dB in HALL, because ERA, the governor and the limiter in and after the loop see other levels. So it's a reference, not the fix.

## The prototype (B)

- The make-up may rise only as fast as k × the tank's own new decay (60 / T60 dB/s), with k = 0.3. Falls stay as built. It applies at finite DECAYs only: across the crossover the v1.0.53 trim releases as DECAY goes *up*, while the tank barely loses, and a cap there held HALL's endless level 2 dB down.
- `core/dsp/Engine.cpp` `applySpace` (after `decayGain_ = ...`): `decayGain_ = fmin(decayGain_, gainWas * (1 + k * 6.9078 * rate * n / fs))`. The state is the previous block's `decayGain_`, so there's no new member on the module.
- [M] **On a tail** the level only falls: +0.1 dB at most over the grid in MASSIVE, −0.0 in HALL. It falls at 70 % of the new rate while the make-up catches up (up to ~8 s for MASSIVE's +9.5 dB), then at the full rate.
- [M] **While playing** it matches the input-side reference within ~0.3 dB in every grid cell: HALL +0.4, MASSIVE +3.5 (the bump). PLATE, which shares the make-up, goes from +2.1 to +0.6 dB playing and from +1.4 to +0.0 dB on a tail. k = 0.5 lets 0.5-1 dB more through in MASSIVE. A rate lag of one mean line gained under 0.3 dB, so we dropped it.
- [M] **Gates:**
  - Hook off: `same_render` SAME 60/60 against 48b49c1, and ctest 100 % passed out of 21.
  - Hook on: SAME 60/60 too (no case there turns DECAY down), so held settings, freezes, flips and SIZE moves are bit-identical.
  - On the page's cards, B is bit-identical to A until the move.
  - Clicks: B/A level ratio steps ≤ 0.09 dB per 1 ms frame.
  - Bursts: >3 kHz against the unmoved render is never above A's.
  - Fed holds: 117 s with DECAY wiggled 0.95 ↔ 0.25 every 4 s stay bounded in all three SPACEs. B is steadier than A (MASSIVE peaks −8 to −10 dBFS against A's −3 to −5).
- **Flash.** With the hook off, the firmware is byte-identical to 48b49c1 (112,588 B). Built in (`make -C firmware MODE=release DECAY_RISE=0.3f`): **112,660 B, +72 B**, which is 20 B over the 112,640 B budget. With `DECAY_NO_POW=1` the make-up's `std::pow(x, decayMakeup_)` goes too (the module never sets `decayMakeup_`; desktop-only demo hook): **112,644 B, 4 B over**. The last bytes have to come from elsewhere, or the budget moves.
