SUBLUSTRIS VERSIO {{VERSION}}: a huge, warm, lush reverb
By Jesse Bauer (Ombra Audio), https://jessebauer.xyz

Sublustris Versio is firmware for the Noise Engineering Versio. Not affiliated with or endorsed by Noise Engineering.

Sublustris is a big reverb for the Versio Eurorack module. The plugin is its desktop twin: the same sound engine as the module, laid out like its panel. Made for dub techno, ambient and dark ambient: long tails that drift and swell, an endless setting that holds what you played without running away, and a freeze you can play over and paint into. Warm, never metallic.
{{NOTES}}


INSTALL THE PLUGIN (Mac, macOS 12 or newer, Apple Silicon or Intel)

1. Unzip. The plugin is two files: "{{NAME}}.vst3" (VST3) and "{{NAME}}.component" (Audio Unit, for Logic or Ableton). Install whichever your DAW uses, or both. (LICENSE, NOTICE and the LICENSES folder are the licence texts; see LICENCE below.)
2. In Finder, Go > Go to Folder... and paste:
     ~/Library/Audio/Plug-Ins/
   Drag "{{NAME}}.vst3" into the VST3 folder and "{{NAME}}.component" into the Components folder (create the folder if it's missing).
   If you had an earlier build called Immensio Versio, delete "Immensio Versio.vst3" and "Immensio Versio.component" from those folders first: it's the same plugin under its old name, and two copies confuse your DAW.
3. It isn't signed by Apple, so macOS will block it at first. Open Terminal and paste these two lines (each one is a single line):
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"{{NAME}}.vst3"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"{{NAME}}.component"
4. Restart your DAW and rescan plug-ins. In Ableton: Settings > Plug-Ins, turn on the VST3 / Audio Units system folders, then hold Option and click Rescan. In Logic it appears under Audio Units > Ombra Audio. It shows up as "{{NAME}}".

Use it on a return track (BLEND fully right) or as an insert (BLEND to taste).

{{FIRMWARE_START}}

FIRMWARE FOR THE VERSIO (only if you have a Noise Engineering Versio)

The same sound runs on the module. File: {{FIRMWARE}}
Sublustris replaces the module's firmware entirely, and you can go back just as easily.

!! NEVER CONNECT USB AND EURORACK POWER AT THE SAME TIME.

1. Power off your case. Take the Versio out and unplug the Eurorack power cable from the module.
2. Plug a micro-USB cable that carries data into the Daisy Seed on the back of the module, and the other end into your computer.
3. In Chrome, open Noise Engineering's Firmware Swap app (https://noiseengineering.us/portal/firmware) and choose your Versio.
4. Choose "Select Custom File" and pick {{FIRMWARE}}. Click "Connect", then "Change Firmware", and wait until it reports that it's done.
5. Unplug USB, put the module back in the case, reconnect the Eurorack power and power on. The four LEDs play a short sweep (blue, violet, white) to show Sublustris has loaded.
To go back, use the same app, on USB only, and pick any Noise Engineering firmware from its list.

The Versio's printed labels don't match Sublustris. Knobs counted top to bottom, left to right:
  Top left knob BLEND · top right DECAY · centre upper TONE · left middle WEAR · right middle SIZE · centre lower MOTION · right lowest DEPTH
  Top switch SPACE · bottom switch ONSET · button FREEZE / PAINT
  Jacks: CV for BLEND, TONE, DECAY, DEPTH (top row); WEAR, MOTION, SIZE and the FREEZE gate (second row); In L, In R, Out L, Out R (bottom row)
  Four LEDs, left to right: input L, input R, output L, output R
The CV inputs take 0 to 5 V, added to the knob. The FREEZE gate needs about 2 V. For mono, patch In L only.
{{FIRMWARE_END}}


THE CONTROLS (the same on the module and in the plugin)

Clock positions are approximate: fully left is 7 o'clock, noon is the middle, fully right is 5 o'clock.

BLEND: dry to wet, with no dip in the middle. Fully right is reverb only (best on a send). Your dry signal is never touched.
DECAY: how long the tail rings: PLATE about 0.4 to 30 s, HALL 0.8 to 60 s, CANYON 2 to 120 s. Past about 3 o'clock it's endless: while you play it follows you at the level of the longest tail just below, so crossing the line doesn't jump; when you stop, it holds the chord without running away, its notes staying notes instead of dissolving into haze.
TONE: flat at noon. Left darkens the tail as it rings, with a warm bump in the low mids; right brightens it, with a gentle sheen on top. Never harsh.
WEAR: how worn the reverb sounds. Fully left is clean. Turning up brings in tape saturation, a bucket-brigade delay's breathing, bit grit, and a soft hiss and crackle that follow your playing. It doesn't darken the tail (that's TONE) or move its pitch (that's MOTION).
SIZE: the size of the space. In HALL and CANYON, once the knob comes to rest, the space crossfades to its new size; in PLATE it bends the pitch a little, like tape. While frozen, it sets the size of the layer you play over the pad.
MOTION: both ways from noon. Noon is a faint, steady movement that keeps the tail from ringing metallic. Left is a cassette on a tired deck: wow and flutter, crinkle from about 9 o'clock, a few dropouts near the end. Right is an old AM radio: a narrow band, with a slow notch wandering through the tail; the pitch doesn't move on this side.
DEPTH: depends on ONSET. In NATURAL it's the predelay (up to 250 ms), in BLOOM the swell time (0.1 to 4 s), in SHIMMER how much octave-up shimmer you hear.

SPACE (PLATE / HALL / CANYON): a dense plate with a quick attack; a big room, starting right behind the note; or CANYON, long lines and mass like a space with no roof, swelling in with each sound and growing into a boundless wash. Switching crossfades. (CANYON was called MASSIVE until v1.0.62; sets saved with MASSIVE open on CANYON.)
ONSET (NATURAL / BLOOM / SHIMMER): how each sound enters: at once; swelling in over DEPTH's time; or with one steady octave up blended in.
FREEZE / PAINT: tap to freeze: the tail holds as a still pad, and what you play goes into a quieter layer on top that rings and fades within about eight seconds. Hold the button while frozen (longer than about a third of a second) to paint: a little of that layer blends into the pad over about three seconds. Tap again to let the pad fade. The gate jack does the same; in the plugin, held MIDI notes do (a short note taps, a long one paints), so a clip can play it.

The four LEDs are level meters: input left / right, output left / right. Green to amber as it gets hot; red means the input is close to clipping, or the output limiter is catching peaks. The output pair turns cool blue while frozen and pulses while you paint. In the plugin, ZOOM (1x / 1.5x / 2x) sets the window size.


THINGS TO TRY

- The endless pad: CANYON, DECAY fully up, BLEND at 3 o'clock. Play slow chords and let each one join the last.
- Freeze and play over it: HALL, DECAY at 2 o'clock. Freeze a chord, then play a melody over the pad. Hold the button through the best phrase to paint it in.
- Dub techno chords: HALL, DECAY just into endless, BLEND fully right on a return. The tail fills the gaps between the stabs and holds when they stop.
- Swell: BLOOM, DEPTH at 2 o'clock, on single piano notes or plucks.
- Worn tape: WEAR at 2 o'clock, MOTION at 8 o'clock, TONE at 10 o'clock.


KNOWN ISSUES

- A crackle can come after you unfreeze in CANYON, if you moved SIZE while frozen with SHIMMER, WEAR and MOTION all up. A fix is on the way.


FEEDBACK

Bugs and sound ideas are welcome on GitHub Issues: https://github.com/jeffebauer/sublustris-versio/issues
- How it sounds next to reverbs you know (plugins or hardware).
- Any knob that feels dead, too touchy, or doesn't do what its name suggests.
- Anything that clicks, glitches or sounds broken, with the settings in panel names, and a short recording if you can.


LICENCE

Version {{VERSION}}, built from commit {{COMMIT}}. Source: {{SOURCE}}
Sublustris's own code is MIT (LICENSE). The plugin is built with JUCE, used under the GNU AGPLv3, so the plugin binaries in this download are distributed under AGPLv3 (LICENSES/AGPL-3.0.txt); the link above is their source. The firmware uses libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0), and no JUCE. Notices: NOTICE.

Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.

Thanks!
