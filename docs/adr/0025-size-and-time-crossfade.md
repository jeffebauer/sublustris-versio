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
- **Phase 2:** BLOOM's swell taps under TIME, and PLATE's SIZE, still glide. *(Both done: the swell in v1.0.54, below; PLATE's SIZE in v1.0.68, ADR 0043.)*
- **Tests:** `test_fdn` `sizeAndTimeCrossfade` (pitch, flicks, infinite hold, jumps between fades); soak regressions (small-HALL jumps, unfreeze after SIZE moved).

## Amendment, 26 Oct 2026 (v1.0.37): SIZE's hysteresis

The rest timer restarts whenever a line's target moves by more than half a sample in a block, and MASSIVE's lines are long enough that ±2·10⁻⁵ of knob noise (about one ADC step) does that: on the module SIZE could never take effect (the overnight review, section 5.1; HALL from ±2·10⁻⁴). SIZE now moves on in HALL and MASSIVE only once the mean line length would move by more than 0.1 % (`voicing::kSizeHysteresis`, 1.7 cents), and everything SIZE sets follows the held value (the lines' targets, MASSIVE's in-loop diffusion and pre-delay, the twin's SIZE). SIZE can settle up to 0.1 % from where the knob stops. The PLATE glides its scale and has no rest timer. `test_size_noise`. A slow LFO into SIZE's CV still holds the tank until it rests (by design; the owner may revisit).

## Amendment, 26 Oct 2026 (v1.0.39): the crossfade in two groups

On the module a SIZE crossfade in MASSIVE reached ~110 % of the CPU (M3 rounds 12-14: the tank's loop and outputs at ~2.4× while every line reads twice), and once the knob noise was fixed the owner's remaining drop-outs came right after moving SIZE. Building the crossfade's code for speed took only 1-2.5 points off (round 14, reverted): the cost is the memory. So the lines crossfade in two groups, 0, 2, 4, 6 then 1, 3, 5, 7, each over kSizeCrossfadeSeconds, back to back; the owner picked it on `renders/size_stagger` (B on five of six cards). A waiting line reads once on the fast path and keeps its own in-loop diffusion amount, loss-filter blend and first-trip level correction until its own fade (sharing them gave clicks at the second group's start, found on the page's renders). Where a small HALL's in-loop diffusion joins or leaves with the move, all lines fade together (waiting lines rejoined on stale allpass memory). The saving on the module is to be measured (M3 round 15); the desktop can't show it (compute-bound where the module is memory-bound).

## Amendment, 27 Oct 2026 (v1.0.45): four groups, and what the drop-outs were

The owner still heard drop-outs on v1.0.40 at the rack, "generally when changing size in MASSIVE mode", mostly while frozen, "very occasionally when unfrozen too". Measured (`b2233fb`, `e418b86`; the profile's SIZEHAND lines, SIZE turned by a scripted hand through release's knob path):
- **Playing:** a hand move never overlaps or restarts a crossfade. The peak is the crossfade's extra reads on top of the old cassette at a large SIZE (~106 %); round 15 had timed it only with the Neve at SIZE 0.3-0.8.
- **Frozen:** the pad holds its SIZE, so only the twin crossfades, but the twin was a second big tank.
- **Just unfrozen:** the pad's two tanks and the still-ringing twin crossfade at once (1.5× a playing move).

So the lines now crossfade in **four groups** (lines g and g + 4 together, a quarter of them reading twice at once; `IV_SIZE_GROUPS` 4, `Fdn::setStagger`). The owner picked it on `renders/size_moves` (level-matched, MIX 100 %): "C - four groups = winner. Hardly perceptible difference from 2 groups, whilst 8 groups introduces some slight staggering that's audible". 1, 2 and 8 groups stay for the desktop's `IV_SIZE_STAGGER` and the profile's SIZEHAND lines. Only `hall_sizemove` and `massive_sizemove` differ in `tools/same_render.py`; the goldens are unchanged (no SIZE moves in them).

On the module (the candidate lap, `docs/research/m3-candidate-lap1.log`, MASSIVE at SIZE 0.7, the old cassette, playing): a SIZE move peaks at 99.1 % with two groups, 88.8 % with four and 83.0 % with eight, with no late block in any. Frozen, any group count still overran with the old twin (111-122 %). The small room fixes that (ADR 0028): with it a frozen SIZE move peaks at 71-83 %.

**Amendment (27 Oct 2026, the owner's v1.0.48 rack test: amber and red on SIZE moves "even when there's nothing in the reverb tank"; SPEC v1.0.49):** the cost is the crossfade's own reads, the same with or without sound; no single block spikes. The crossfading lines now run on a register path and the old early taps read like still ones: the same output to the bit, every SIZE move ≤ 89.6 % on the module (was 94.6 %), no near-late blocks. HALL crossing noon (all eight lines fading together) is now the heaviest case and fits; letting its diffusion join and leave per group stays an option if it's ever needed.

## Amendment, 28 Oct 2026 (v1.0.54): phase 2 for BLOOM's swell

The owner on v1.0.52 at the rack, in BLOOM: "Hall, high decay, when moving the time knob, noise is introduced into the reverb which carries into the tail". In BLOOM, TIME sets the swell's attack, and `Swell::setAttackSeconds` glided it over 150 ms, so its 12 taps slid across seconds of their line at up to ~9x speed and the smear's allpass lengths stepped once a chunk: the same wash this ADR removed from the pre-delay, fed into the tank (+11-14 dB above 3 kHz while TIME moved, +15-24 dB under 100 Hz left in an endless tail). Not a regression: v1.0.49 measured the same. Now (`core/dsp/Swell.h`) once the attack has rested 50 ms the taps jump one after another, each crossfading from its old place to its new one over 25 ms (equal power), 0.3 s in all, and the smear's allpasses read from their old to their new lengths across the same 0.3 s. One tap reads twice at a time, so the cost is one extra read a side for 0.3 s (all 12 at once would double the swell's reads, ~12 % of the module). Measured on the desktop (HALL, BLOOM, DECAY 0.9-1): above 3 kHz +11.5 → +1.0 dB on a sweep, +13.9 → +0.7 on twists; the tail's lows after a twist match TIME held. Held settings are bit-identical; MASSIVE's built-in swell follows SIZE and DECAY, so it crossfades on SIZE moves too (the same or slightly cleaner). Branch `claude/time-noise` (5e38080).

## Amendment, 3 Nov 2026 (v1.0.68): phase 2 done for PLATE's SIZE

PLATE's SIZE now crossfades too (ADR 0043; the owner's pick B on all 12 cards of `renders/plate_size_xfade2`): once the knob rests 50 ms the plate jumps and reads old and new lengths across 0.3 s, with this ADR's agreement gain, loss-filter blend and one-trip decay correction. Unlike HALL and CANYON, moves of 4 % of the scale or less glide (about 5 cents; crossfaded, reads that close summed to a comb in the loop, the owner's round-1 drops), and so do moves while frozen and the glide after an unfreeze. Phase 2 is complete: no SIZE or TIME move bends the pitch.
