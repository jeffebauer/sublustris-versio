# 0025 — HALL's and MASSIVE's SIZE, and TIME's predelay, move by crossfade

**Status:** Accepted, 25 Oct 2026 (owner: "Option C: Crossfade slow is the winner" on `renders/knob_moves`). SPEC v1.0.34. Amends ADR 0012 (SIZE's glide).

**Context:**
- In the plugin, the owner heard two problems with knob moves:
  - "changing size moves too slowly causing slow pitch shifts that make it sound really out of tune" (MASSIVE);
  - fast TIME moves made "a large wash of noise", and fast HALL SIZE moves "a brief moment of aliasing-like sound".
- A glide in a loop retunes everything circulating by the length ratio, for the rest of its tail. PASP ("Delay-line interpolation") gives the alternative: crossfade between the two delay settings rather than glide.
- The prototypes were glide, fast glide and crossfades of three speeds; the owner picked the slow crossfade.

**Decision:**
- **When:** once SIZE (or TIME) has rested `kSizeRestSeconds` (50 ms). Back-to-back crossfades each left their small dip in the loop: an infinite MASSIVE drained 45 dB in a minute of sweeping.
- **What:** the lines, early taps and in-loop allpasses jump to their new lengths, and the old reads fade out over `kSizeCrossfadeSeconds` (0.3 s).
- **The old read** carries on as it was, with its own allpass-interpolation state. The new read's state starts from the cubic read a sample before.
- **Gains inside the loop:**
  - The fade is sin² / cos², summing to one. Equal power summed the two reads to 1.41× wherever they agree (the lows); in the loop that grew every trip, to a DC offset of −0.4 on HALL SIZE flicks.
  - Each line's mix is scaled by a gain that tracks, over about 5 ms, how much its two reads agree (Robert Bristow-Johnson's crossfade for any correlation): the powers' own fade over the mix's, capped at 1.42. Summing to one alone, reads that differ dipped 3 dB mid-fade, and in the loop that dip repeated every trip.
- **A small HALL's in-loop diffusion:**
  - Joining or leaving, it runs as a whole allpass on its own read: the new read when joining, the old when leaving. Blended inside the allpass between straight-through and delayed, mid-fade it passed 14 % of the signal, every trip, and a small HALL fell 13 dB in 0.25 s.
  - Present on both sides, its length and coefficient crossfade.
  - Its joining or leaving counts as a pending move. Joined on its own after the lines' fade, it sat at no length and full coefficient, which drains the loop.
- **The loss filter** (b1, gain, pole) fades from the old length's to the new one's with the fade. Switched at the start, the old read, then all of the mix, stepped to the new trip's loss: a click.
- **For one trip,** the new read is corrected by the tail's own decay over the length difference, eased out over the fade.
- **The level that follows SIZE** (√ of the mean line, the HALL and MASSIVE trims) moved from the tank's output to its input (ERA's in-loop stages are scaled back to the level they had). A longer loop holds the same energy over more samples, so the gain belongs to what goes in. On the output it stepped every held tail at a jump (an infinite MASSIVE +6 dB for SIZE 0 → 0.7, then pulled back by the governor over seconds).
- **TIME's predelay** crossfades the same way, equal power (it is outside the loop), stepped by a rotation, out of line.

**Considered:**
- Faster glides: they still retune, only faster.
- A twin tank per move: it costs memory and CPU, and the old tail wouldn't take the new size.
- Equal-power and sum-to-one fades alone: see Decision.

**Consequences:**
- **Measured:**
  - A held note keeps its pitch through a MASSIVE SIZE move (1 cent; was the length ratio).
  - HALL SIZE flicked on a held note: off the note −17 dB while moving (−24 still; the glide gave −1). The two reads of one held sine dip where they are out of phase.
  - TIME flicks −22 dB.
  - No clicks on instant or ramped flicks, or on small-HALL jumps across the diffusion point.
  - Single jumps stay within 1.2 dB of the old and new rooms' fades.
  - An infinite MASSIVE holds through a minute of moves (0.2 dB drift between them, back at the start +0.2 dB).
- **Sound change:**
  - What's in the tank no longer jumps in level when SIZE or DECAY's trims change; only new input arrives at the new level.
  - ERA's in-loop stages, which aren't linear, still hear the loop at the output-referred level. Moved with the gain, they shortened a Midiverb HALL's T60 by 9 %.
  - Settings held still sound exactly as before: every golden passes unchanged.
- **Flash:** release 112,060 → 110,764 B. The crossfade code costs about 1.4 KB, and `Fdn::control`, `Plate::control` and `Engine::applySpace` are built for size (control rate). That leaves 1,876 B under the budget.
- **CPU:** the extra reads run only during a fade (0.3 s). `applySpace` at -Os runs once a block; measure at M3 with the profile build.
- **Phase 2:** BLOOM's swell taps under TIME, and PLATE's SIZE, still glide.
- **Tests:** `test_fdn` `sizeAndTimeCrossfade` (pitch, flicks, infinite hold, jumps between fades); soak regressions (small-HALL jumps, unfreeze after SIZE moved).

## Amendment, 26 Oct 2026 (v1.0.37): SIZE's hysteresis

The rest timer restarts whenever a line's target moves by more than half a sample in a block, and MASSIVE's lines are long enough that ±2·10⁻⁵ of knob noise (about one ADC step) does that: on the module SIZE could never take effect (the overnight review, section 5.1; HALL from ±2·10⁻⁴). SIZE now moves on in HALL and MASSIVE only once the mean line length would move by more than 0.1 % (`voicing::kSizeHysteresis`, 1.7 cents), and everything SIZE sets follows the held value (the lines' targets, MASSIVE's in-loop diffusion and pre-delay, the twin's SIZE). SIZE can settle up to 0.1 % from where the knob stops. The PLATE glides its scale and has no rest timer. `test_size_noise`. A slow LFO into SIZE's CV still holds the tank until it rests (by design; the owner may revisit).
