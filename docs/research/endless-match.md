# Endless at the tail's level (2 Nov 2026)

The owner, on renders/decay_blend: the level shifts at DECAY's 3 o'clock crossing are in the reverb, and they flip
with the input: quiet playing jumps up into endless, hot playing drops. The pick (docs/research/wet-dry-balance.md,
option 3): endless follows the playing. It starts at exactly the tail's level, tracks how loud you play like a long
tail, and never fades.

## How established units do infinite

- **Feedback at exactly 1.** Eventide's Infinite and Valhalla's reverbs at 100 % feedback keep every pass. A tail
  played into keeps growing until the unit's own saturation or limiter stops it; Valhalla's Supermassive notes
  advise turning the input down or using the freeze for an endless pad
  ([Valhalla DSP, Supermassive tips](https://valhalladsp.com/?p=567197)).
- **Freeze.** Others (Strymon BigSky's freeze, Mutable Instruments' Clouds) hold the tank and
  mute its input, so nothing builds and the level is wherever the tail was.
- **A soft limiter in the loop.** DIY Daisy reverbs keep a feedback-1 loop bounded with a soft clipper or a
  level-following gain ([Electrosmith forum, preventing reverb clipping](https://forum.electro-smith.com/t/preventing-reverb-clipping/3318);
  [Puckette, the theory of reverberation](https://msp.ucsd.edu/techniques/latest/book-html/node112.html), on
  why a lossless recirculating network has to be bounded from outside).

None of them governs endless to a fixed dBFS floor, as v1.0.60 does (ADR 0008). That floor is why the step flips with
the input: a hot tail sits above it and is pulled down; a quiet one sits under it and is pushed up.

## What the prototype does (IV_ENDLESS_MATCH, voicing::kEndlessMatch)

- **Played:** past the finite top, the tail is the finite top's tail. It keeps DECAY 0.80's loss, level make-up,
  long-tail trims, MASSIVE's high ratio and its governor (PLATE and MASSIVE ease that one in from 0.70, at today's
  floor). Turning across 3 o'clock can't step the level, and a hot input is bounded exactly as the longest finite tail is.
- **Quiet:** once the tank's input is 30 dB under the tail for 0.25 s, its loss glides out over 0.5 s and the
  tail holds where it is: a freeze with the input still open. The governor goes to full weight with the hold, its
  floor rising to a −6 dBFS safety ceiling, and the existing hold ceiling (the tail's own settled level) stops
  any creep. Playing again lets go at once.
- **The duck goes.** With endless at the finite top's level there's no build-up to hold back, and kept on its own
  it made 1.2-1.9 dB steps when played hot (value 2 of the hook keeps it, for comparison).
- FREEZE is untouched; held notes calm as before (PLATE, HALL).
