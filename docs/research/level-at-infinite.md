# The level at infinite DECAY, the limiter and the duck

27 Oct 2026, session 9, measuring the owner's v1.0.49 rack notes ("the limiter is keeping the level a bit too low ... sweeping the mix from 50:50 dry wet to 100% wet, the reverb gets noticeably louder"; "ducking ... reduce it from 4:1 to 2:1 or even 1.5:1"). Core v1.0.49 (`claude/combined` 219abb4), BS.1770 loudness in 400 ms windows, hot input (dry peaks at −1 dBFS) unless marked.

**The limiter isn't it.** On music it takes 0.00-0.13 dB on average (MASSIVE at DECAY 0.75-0.8 catches single peaks 1-4 % of the time); at infinite DECAY nothing reaches it (wet peaks −8 to −12 dBFS). A higher ceiling or another release changes loudness by ≤ 0.1 dB.

**At infinite DECAY (≥ 0.85) the wet sits 7-12 dB under the dry while playing** (HALL −9.9, MASSIVE −12.3 dB; finite DECAY 0.75-0.8: within ±1.5 dB). From DECAY 0.8 to infinite the wet drops 9.8 dB (HALL) / 12.8 dB (MASSIVE): the governor's −24 dBFS floor gives −4.3 / −7.5 dB, the 4:1 duck the other ~5.4 dB. The infinite wet doesn't follow the input (playing 12 dB harder leaves it ~−23 LUFS; with the duck it gets quieter). At 50:50 the reverb is buried; turning to fully wet removes the dry and adds the equal-power law's +3 dB, so the reverb suddenly stands out (the total drops 5-9 dB).

**The duck at infinite** (mean / p95 / max reduction while playing): 4:1 5.4 / 8.4 / 10.1 dB (a held washy chord 6.8 / 8.7 / 9.3); 2:1 3.6 / 5.6 / 6.7 (wet +1.7 dB); 1.5:1 2.4 / 3.7 / 4.5 (wet +2.9 dB). At the material's own level 4:1 is 2.4-3.6 dB mean, matching SPEC's "about 3 dB".

**Levers:** the floor (−21: +2.5 dB, −18: +5 dB at infinite; needs hold, freeze and paint checks); a gentler duck; a BLEND law with the wet at unity from noon (0 dB rise from 50 to 100 %, works everywhere); a make-up through DECAY's crossover. The duck and the limiter don't interact. Unknown: the module's jack-to-dBFS calibration (the hot tier is the upper bound).
