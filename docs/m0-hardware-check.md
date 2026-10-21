# M0 hardware check

Test firmware: **`firmware/build/immensio_versio_m0test.bin`** (`make -C firmware MODE=m0test`). It passes audio straight through and shows every control on the LEDs and over USB serial. Criteria: SPEC §7 M0.

**Shorter than Resilio's.** Your module already passed Resilio's full M0 check (29 Sep 2026): the pot wiring (P1 = K0, P2 = K4, P3 = K2, P4 = K1, P5 = K5, P6 = K3, P7 = K6), switch direction (left reads 0), CV at 0 / 5 V, the gate, and the analog path (polarity-inverted, +1.17 dB hot, undone by `kOutputTrim` in release). Those are hardware facts and carry over. This check confirms that Immensio's firmware loads and that every control lands where ADR 0017 puts it.

**Never connect rack power and USB at the same time** (NE manual). Session 1 is USB only; session 2 is rack power only.

## Session 1: USB only (rack power unplugged)

1. Flash `immensio_versio_m0test.bin` with NE Firmware Swap → **Select Custom File**.
2. **Boot pattern:** the four LEDs step through **blue, violet, blue, white** for about half a second. This is not Resilio's red/green/blue/white.
3. Serial monitor:
   ```bash
   screen /dev/tty.usbmodem* 115200
   ```
   (Quit: Ctrl-A, then K, then Y.) A line prints 10 times a second:
   `P  500  250 1000    0  500  500  200 | SW 1 0 | BTN 0 3 | GATE 0 7`
   The P values read 1000 on USB power. The knob circuit needs the rack's ±12 V, so that's expected.
4. **SPACE** (top toggle): left / centre / right → the first `SW` number shows 0 / 1 / 2 = PLATE / HALL / MASSIVE.
5. **ATTACK** (bottom toggle): left / centre / right → the second `SW` number shows 0 / 1 / 2 = NORMAL / BLOOM / REVERSE.
6. **Button:** each press adds exactly 1 to the second `BTN` number.

## Session 2: rack power (USB unplugged)

LED key (pots in panel order, ADR 0017):

| LED | Shows |
|---|---|
| LED_0 | R = P1 MIX, G = P2 DECAY, B = P3 TONE |
| LED_1 | R = P4 ERA, G = P5 SIZE, B = P6 DRIFT |
| LED_2 | R = P7 TIME, G = SPACE position, B = ATTACK position |
| LED_3 | White = button held. Red = gate high. Green = every knob fully off or fully on (within 2 %) |

7. **Knobs:** turn each pot from fully CCW to fully CW. Its colour channel should fade smoothly from off to full, and the right pot should light the right channel above.
8. **Switches** change LED_2's green (SPACE) and blue (ATTACK).
9. **Gate:** a clock into Gate in → LED_3 red on every pulse. **Button:** LED_3 white while held.
10. **Passthrough** (quick listen is enough; Resilio already measured the path): play `test_audio/stimulus/04_skank.wav` through. Both outputs should sound like the input, and In L alone should come out of both outputs.

Tell me the results and I'll tick the M0 boxes. After this, flash `immensio_versio.bin` (release) only when there's DSP to play. At M0 it's a passthrough with meters.
