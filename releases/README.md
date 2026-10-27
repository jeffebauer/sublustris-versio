IMMENSIO VERSIO {{VERSION}} — a huge, warm, lush reverb
By Jesse Bauer (Ombra Audio) — https://jessebauer.xyz

Immensio Versio is firmware for the Noise Engineering Versio. Not affiliated with or endorsed by Noise Engineering.

Immensio is a big reverb that runs as firmware on the Versio Eurorack module. The plugin is its desktop twin: the exact same sound engine as the module, laid out like its panel. It's a work in progress, so honest ears are very welcome.

Made for dub techno, ambient and dark ambient: long tails that drift and swell, sustain almost forever without running away, and go from sweet to gritty. Warm, never metallic.
{{NOTES}}


INSTALL (Mac, macOS 12 or newer, Apple Silicon or Intel)

1. Unzip. The plugin is two files: "{{NAME}}.vst3" (VST3) and "{{NAME}}.component" (Audio Unit, for Logic or Ableton). Install whichever your DAW uses, or both. (LICENSE, NOTICE and the LICENSES folder are the licence texts; see LICENCE below.)
2. In Finder, Go > Go to Folder... and paste:
     ~/Library/Audio/Plug-Ins/
   Drag "{{NAME}}.vst3" into the VST3 folder and "{{NAME}}.component" into the Components folder (create the folder if it's missing).
3. It isn't signed by Apple, so macOS will block it at first. Open Terminal and paste these two lines (each one is a single line):
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"{{NAME}}.vst3"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"{{NAME}}.component"
4. Restart your DAW and rescan plug-ins. In Ableton: Settings > Plug-Ins, turn on the VST3 / Audio Units system folders, then hold Option and click Rescan. In Logic it appears under Audio Units > Ombra Audio. It shows up as "{{NAME}}".

Use it on a return track (MIX fully up) or as an insert (MIX to taste).


THE CONTROLS (same places as on the module)

MIX — dry / wet. Fully up = reverb only (best on a send).
DECAY — how long the tail rings. Past about 3 o'clock it never ends: it settles into a warm bed that steps back while you play and comes forward when you stop.
TONE — noon is neutral. Left darkens the tail a little more on every pass, right brightens it.
ERA — the reverb's age, from sweet to gritty, in five stretches: a Neve desk's warmth, an old Midiverb (grainy, lo-fi digital), a bucket-brigade delay chip, a cassette (hiss, wow, softer highs), then an AM radio, fading and crackling. Turning it blends from one to the next.
SIZE — how big the space is. In HALL and MASSIVE a move crossfades to the new size; in PLATE it bends the pitch like tape. While frozen, it plays the layer you hear over the pad (see FREEZE).
DRIFT — noon is still. Left = tape wow and flutter that never repeats. Right = a quicker, restless wander. It moves the pitch inside the tail, not the dry sound.
TIME — depends on ATTACK: in NORMAL it's predelay (up to a quarter second), in BLOOM how long the swell takes, in SHIMMER how much shimmer.

SPACE (PLATE / HALL / MASSIVE) — a dense, quick plate; a big hall; or MASSIVE: enormous, slow-swelling, almost endless.
ATTACK (NORMAL / BLOOM / SHIMMER) — how the sound enters: at once; swelling in over TIME; or with an octave up blended into the reverb.
FREEZE — tap it to freeze: the tail becomes a still pad, and what you play now goes into a quieter, fresh reverb on top of it. Tap again to let the pad fade. While frozen, hold it to paint: a little of what you're playing soaks into the pad over a few seconds. Held MIDI notes into the plugin act like the button (a short note taps, a long one paints), so a clip can play it.

The four lights at the top are level meters: input left / right, output left / right. Green to amber as it gets hot; red means the input is close to clipping, or the output is being limited. The output pair turns blue while frozen, and pulses while you paint. ZOOM (1x / 1.5x / 2x) sets the window size.


THINGS TO TRY

- Chord stabs on a send: HALL, NORMAL, DECAY noon, TONE a little left, DRIFT a little left, ERA into the cassette.
- A pad or a drone: MASSIVE, BLOOM, TIME noon, DECAY past 3 o'clock. Turn SIZE slowly.
- Play a chord, tap FREEZE, then play over the pad. Hold FREEZE now and then to paint new notes into it.
- SHIMMER on sparse piano or guitar, TIME around 10 o'clock, ERA up into the radio for a broken, distant feel.


KNOWN ISSUES

- Still in development: the sound may keep changing between versions.


FEEDBACK

Send it straight to Jesse. Especially welcome:
- How it sounds next to reverbs you know (plugins or hardware).
- Any knob that feels dead, too touchy, or doesn't do what its name suggests.
- Anything that clicks, glitches, or sounds broken, with the settings if you can.


LICENCE

Version {{VERSION}}, built from commit {{COMMIT}}. The source isn't public yet; ask Jesse for the source of this build.
Immensio's own code is MIT (LICENSE). The plugin is built with JUCE, used under the GNU AGPLv3, so the plugin binaries in this download are distributed under AGPLv3 (LICENSES/AGPL-3.0.txt). Notices: NOTICE.

Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.

Thanks!
