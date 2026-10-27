# The level at infinite DECAY, the limiter and the duck

27 Oct 2026, session 9, measuring the owner's v1.0.49 rack notes ("the limiter is keeping the level a bit too low ... sweeping the mix from 50:50 dry wet to 100% wet, the reverb gets noticeably louder"; "ducking ... reduce it from 4:1 to 2:1 or even 1.5:1"). Core v1.0.49 (`claude/combined` 219abb4), BS.1770 loudness in 400 ms windows, hot input (dry peaks at −1 dBFS) unless marked.

**The limiter isn't it.** On music it takes 0.00-0.13 dB on average (MASSIVE at DECAY 0.75-0.8 catches single peaks 1-4 % of the time); at infinite DECAY nothing reaches it (wet peaks −8 to −12 dBFS). A higher ceiling or another release changes loudness by ≤ 0.1 dB.

**At infinite DECAY (≥ 0.85) the wet sits 7-12 dB under the dry while playing** (HALL −9.9, MASSIVE −12.3 dB; finite DECAY 0.75-0.8: within ±1.5 dB). From DECAY 0.8 to infinite the wet drops 9.8 dB (HALL) / 12.8 dB (MASSIVE): the governor's −24 dBFS floor gives −4.3 / −7.5 dB, the 4:1 duck the other ~5.4 dB. The infinite wet doesn't follow the input (playing 12 dB harder leaves it ~−23 LUFS; with the duck it gets quieter). At 50:50 the reverb is buried; turning to fully wet removes the dry and adds the equal-power law's +3 dB, so the reverb suddenly stands out (the total drops 5-9 dB).

**The duck at infinite** (mean / p95 / max reduction while playing): 4:1 5.4 / 8.4 / 10.1 dB (a held washy chord 6.8 / 8.7 / 9.3); 2:1 3.6 / 5.6 / 6.7 (wet +1.7 dB); 1.5:1 2.4 / 3.7 / 4.5 (wet +2.9 dB). At the material's own level 4:1 is 2.4-3.6 dB mean, matching SPEC's "about 3 dB".

**Levers:** the floor (−21: +2.5 dB, −18: +5 dB at infinite; needs hold, freeze and paint checks); a gentler duck; a BLEND law with the wet at unity from noon (0 dB rise from 50 to 100 %, works everywhere); a make-up through DECAY's crossover. The duck and the limiter don't interact. Unknown: the module's jack-to-dBFS calibration (the hot tier is the upper bound).

## What was built (v1.0.51) and the sources behind it

The owner picked D on `renders/level_duck` (duck 2:1, floor −21 dBFS: ADR 0033) and E on `renders/level_glue` (held playing ducked less and let go faster: ADR 0014's v1.0.51 amendment). The sources the `level_duck` / `level_glue` designs followed (collected 27 Oct 2026 by the page agent, written up 28 Oct; only the Eventide thread was re-read then, the rest are cited from the agent's notes and not re-checked):

- **D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis", J. Audio Eng. Soc. 60(6), 2012.** The duck's structure: a level detector in dB, a static curve (threshold, ratio), and the gain smoothed with separate attack and release.
- **D. Giannoulis, M. Massberg, J. D. Reiss, "Parameter Automation in a Dynamic Range Compressor", J. Audio Eng. Soc. 61(10), 2013.** Attack and release set automatically from the signal's crest factor (peak against average): short for steady, low-crest material, long for transient material. E's "held" test (the 10 ms power within 3 dB of its 0.5 s power) is a crest-factor test, and its release moves from 0.4 s (transients) to 0.08 s (held) by it.
- **TC Electronic 2290 (1985), the dynamic delay:** the classic ducked delay. The repeats are pushed down while the input is above a threshold and come back when it stops: the effect gets out of the way of the playing and fills the gaps.
- **Eventide TimeFactor, DuckedDelay:** ratio (how far the effect is attenuated above the threshold; 2:1 gentle, 7:1 strong), threshold (−36 to −66 dB) and release (10-500 ms: short brings the effect back sooner after you stop, long keeps it down between notes). Eventide's staff note that ducking needs tuning to the player's technique, the reason E adapts. <https://www.eventideaudio.com/?p=106574>
- **Valhalla DSP, ValhallaDelay's ducking:** the same idea in a plugin: the wet ducked by the dry, with the amount the main control.

How E maps onto them: a 2:1 static curve above −24 dBFS (Giannoulis 2012), a crest-factor switch between two releases and between full and 40 % reduction (Giannoulis 2013), the hardware units' short release for held playing so the tail arrives as the dry stops. Measured on the build: renders of v1.0.51 with no hooks equal `renders/level_glue`'s `*_E_both.wav` to the bit (all 10).
