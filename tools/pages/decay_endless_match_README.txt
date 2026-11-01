Turning DECAY into and out of endless with no change in level (owner, on decay_blend: "the shifts in and out of endless shouldn't have these audible shifts and the transition between them should be imperceptible from an amplitude perspective"; picks: endless follows the playing, the crossing pace folded in). HALL, SIZE 0.9, every source played quietly and hot; the last two rows are PLATE and MASSIVE. Renders are 100 % wet; the MIX fader starts at noon (an even blend, as BLEND at noon on the module) and blends the dry in live. Not level-matched on purpose: the level is what this page is about. The trace under each card shows the wet's level for every version.

Every card follows the same timeline (the chapter bar):
  0 s    playing, DECAY just under 3 o'clock (0.78): the tail swells
  8 s    ↑ into endless: DECAY turned fully clockwise in 0.5 s
  16 s   ↓ out of endless: DECAY back to about 2:30 (0.75) in 0.5 s
  24 s   stop playing: the tail rings on at 0.75
  27 s   ↑ into endless (tail): the same turn up, on the tail alone
  35 s   ↓ out (tail): and back down; the tail then fades out

Why today steps: endless holds the tail at one fixed level (about −18 dBFS in HALL), whatever you play into it. Played hot, the tail sits above that level and gets pulled down; played quietly, it sits below and gets pushed up. That's the flip you heard.

How C and D work: past 3 o'clock, while you play, the tail behaves exactly like the longest finite tail (DECAY 0.80): same length, same level, same trims, so turning into endless can't change its level, quiet or hot. When the playing stops (the input falls well under the tail), the tail stops fading within about half a second and holds where it is, like a freeze with the input still open, so nothing builds up. Play again and it carries on as the longest tail. Turning back out, the tail just starts fading from where it was held. The old duck (endless pulling down under new playing) is gone: with endless at the long tail's level there's no build-up for it to hold back, and on its own it made 1-2 dB steps when you played hot. A safety ceiling (−6 dBFS) still catches a runaway.

A_today          Today (v1.0.60)
B_paced          Today, with the 3 o'clock zone crossed in 3 s (C on decay_cross)
C_follows        Endless follows the playing
D_follows_paced  Endless follows the playing, with the 3 s crossing

What to listen for: at each chapter mark, does the level move? Quiet and hot rows should both stay put on C and D. On D, turning into endless on a fading tail lets it fade for the 3 s crossing before it holds; on C it holds at once.
