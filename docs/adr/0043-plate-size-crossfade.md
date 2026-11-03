# 0043 — PLATE's SIZE by crossfade

**Status:** Accepted, 3 Nov 2026 (owner, B "crossfade" on all 12 cards of `renders/plate_size_xfade2`, pad and stabs, no notes). SPEC v1.0.68. Completes ADR 0025's phase 2 for PLATE; amends ADR 0012 (SIZE's glide) for PLATE.

**Context:**
- Until v1.0.67 PLATE's SIZE glided its scale at up to a semitone of bend (`kScaleSlewPerSecond`), so a move retuned everything in the tail, "like tape". HALL and CANYON have crossfaded since v1.0.34 (ADR 0025), which left PLATE as the one space whose SIZE bent the pitch.
- **The method** is ADR 0025's, from PASP ("Delay-line interpolation": crossfade between the two delay settings rather than glide) with Robert Bristow-Johnson's crossfade for any correlation (a gain that follows how much the two reads agree, so differing reads don't dip 3 dB a trip). Dattorro's plate (1997) keeps its topology: per branch the modulated allpass, d1, ap2 and d2, with its 14 output taps.
- **Round 1, `renders/plate_size_xfade`** (A the glide as built, B the crossfade, pad and stabs, five moves). The owner picked B on the slow turn, the flicks, the LFO into SIZE's CV and the endless moves; A on the frozen moves (pad and stabs); no pick on the stabs' slow turn. Their notes, verbatim:
  - "Stabs/B Crossfade: 5.5s there's a drop in amplitude that isn't present in A glide as built" (slow turn up).
  - "Pad/B Crossfade: There's a steep drop-off at 20s that isn't present in A glide as built" (frozen moves; the same on the stabs).
- **The drops' cause, measured:** both came from tiny moves (the last few hundredths of a turn, and a frozen pad's SIZE wobble) crossfading. Reads only a fraction of a percent apart sum to a comb, and in the loop it repeats each trip: 0.300 → 0.303 took −4.9 dB at 1.2-4 kHz. A glide that small bends only a few cents. A full grow also clicked (+18.8 dB over the median in the worst 1 ms above 5 kHz): the input's gain stepped with the output level and the modulated allpass rang it. A shrink at short DECAY swelled 1.4-1.6 dB: the one-trip decay correction over-corrected.
- **Round 2, `renders/plate_size_xfade2`** (the fixes below; six moves including a full shrink at DECAY 0.3): B on all 12 cards.

**Decision:**
- **PLATE's SIZE crossfades** once the knob has rested 50 ms: the plate jumps to its new size and reads its old and new lengths across a 0.3 s sin²/cos² fade, with the agreement gain (followers over about 5 ms, ramped each sample), the loss filters blended old to new and a one-trip decay correction (0.6 of it on a shrink). What was written before the jump is rescaled by its age so the level holds.
- **Small moves glide:** a move of 4 % of the scale or less (`kXfSmallStep`) glides at 0.0193 a second (`kXfSmallSlewPerSecond`, about 5 cents while it moves, under the plate's own chorus).
- **Frozen moves glide,** and so does the glide after an unfreeze, until the scale arrives (a latch, so a long unfreeze glide isn't cut into a crossfade). The twin (the layer over a frozen pad) always glides.
- **The input is eased** across the fade (from √(old/new) to 1), which removed the full-grow click.

**Rejected:**
- **A, the glide:** bends the pitch; the owner preferred B everywhere once the drops were fixed.
- **Crossfading every move:** the drops above.
- **An equal-power fade without the agreement gain:** an endless tail drained −14 dB over repeated moves.

**Consequences:**
- **Sound:** a SIZE move in PLATE no longer bends the pitch. Settings held still are bit-identical (every golden passes; `tools/same_render.py` SAME except the PLATE SIZE-move renders). Clicks within 1-3 dB of HALL's; a shrink's worst swell 0.79 dB; an endless hold +0.00 dB after moves; a SPACE switch mid-fade +7.7 to +9.9 dB (the glide +9.9 to +15.1).
- **Renders** equal the page's B: SAME 60/60 against the prototype with `IV_PLATE_XFADE=B` (branch `proto/plate-size-xfade`, facbf40), and all 12 page cards identical to the sample.
- **Flash:** release 111,828 B (812 B under the 112,640 budget; the crossfade's code about 3.4 KB), profile 130,956 of 131,072, m0test 83,556. Paid in part by sound-neutral savings on the branch (each SAME 60/60): the profile hook's flags as constants outside the SIZE-spike laps (−180 B profile), `prof::mark` out of line (−480 B profile), zero-initialised members left to the Engine's value-initialisation (`IV_ZI`; Plate −128/−136 B, the absorb memo −56/−48 B), a redundant `exp` (−48/−56 B), and the fade's taps from a byte table (−168 B).
- **CPU:** the fade reads each line twice for 0.3 s; measure at the rack with the profile build.
- **Tests:** `test_plate_xfade` (one jump per move through knob noise, an LFO into SIZE, a minute of moves on an endless tail, small moves glide, frozen and unfreeze moves glide).
