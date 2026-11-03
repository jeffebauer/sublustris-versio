# Modwiggler: launch post

Draft for the owner, 3 Nov 2026. Modelled on the Resilio Versio post. Not posted.

## Where and how

- **Forum:** Noise Engineering subforum (Eurorack Modules → Noise Engineering), new thread.
- **Format:** Modwiggler runs phpBB, so the body below is BBCode (`[b]`, `[list]`, `[url=]`). Markdown links like `[text](url)` show up as plain text there. Paste it into the full editor, then use Preview before you submit.
- **Image:** attach `docs/minisite/assets/renders/front-on-wide.jpg` through the Attachments tab, then click "Place inline" to put it just under the first paragraph. `docs/minisite/assets/renders/patched.jpg` works as a second image.

## Title

Sublustris Versio, a free deep reverb firmware for the Versio (and a macOS plugin)

## Body

```
Hi all,

Some of you may have tried Resilio Versio, my dub spring reverb for the Versio. Its sibling is out today. Sublustris Versio is a big, dark reverb for ambient, dub techno and dark ambient, the reverb I wanted to sit inside for a whole track. Like Resilio, it's free alternative firmware for any Versio module, and the same sound runs as an AU/VST3 plugin on macOS, so you can try it without reflashing anything.

[b]What it does[/b]

It's built for long tails and for playing over them.

[list]
[*]Past about 3 o'clock, DECAY goes endless. While you play, the tail follows you at the level of the longest tail below it, so nothing jumps as you cross over. When you stop, it holds the chord, and in PLATE and HALL the notes stay notes instead of smearing into haze.
[*]Tap the button or send a gate and the tail freezes into a still pad. Whatever you play on top rings in a quieter layer of its own, and holding the button while frozen paints a little of that layer into the pad, where it stays. In the plugin, MIDI notes play the freeze, so a clip can freeze and paint in time.
[*]SPACE picks the room. PLATE is a dense plate with a quick attack, HALL is a big room that starts right behind the note, and CANYON is long lines and mass, a space with no roof that swells in with each sound.
[*]ONSET sets how each sound enters. NATURAL comes in at once, BLOOM swells in over anything from a breath to four seconds, and SHIMMER adds one steady octave on top that stays in tune and doesn't climb.
[*]MOTION bends the tail both ways from noon. To the left it's a cassette on a tired deck, with wow, flutter, crinkle and dropouts that get worse the further you go. To the right it's an old AM radio, its narrow band with a slow notch wandering through, so the tail phases.
[*]WEAR ages the reverb with tape saturation, the breathing of a bucket-brigade delay, bit grit and a soft hiss, over a warm desk colour. It only colours the reverb. Your dry signal is never touched.
[/list]

Seven knobs, each with a CV input, two switches, a button and a gate, stereo in and out. It installs with Noise Engineering's own Firmware Swap web app, and you can go back to any of their firmware the same way.

It works the chip hard. A big CANYON, frozen, with someone playing over it takes most of what the Daisy has, and those moments were timed on the module itself. If you ever hear crackle, please tell me your settings, because that's a bug.

If you know Plateau Versio: Sublustris's PLATE follows the same public Dattorro paper, but none of Plateau's code is in it, and most of what Sublustris does lives in the other spaces, the endless hold, freeze and WEAR.

[b]A bit of background[/b]

As with Resilio, I'm a designer, not a DSP engineer. I made every sound decision by ear, and used Claude to write the DSP, firmware and tooling. The plate follows Dattorro's design, and HALL and CANYON are feedback delay networks after Jot. The reverbs whose character shaped it are the ones I play, like Valhalla Supermassive, the Meris Mercury7 and the Eventide Blackhole. Nothing changed in the sound until I'd heard it next to the version before, and more than forty decisions are written up with their reasons in the repo if you're curious.

The name is Latin for "dimly lit, faintly glowing", the half-light of a clouded night.

[b]Links[/b]

[list]
[*]Website, with audio demos you can blend dry to wet: [url=https://sublustris-versio.vercel.app]sublustris-versio.vercel.app[/url]
[*]Download the firmware and the plugin (v1.0.68): [url=https://sublustris-versio.vercel.app/#download]sublustris-versio.vercel.app/#download[/url]
[*]Install guide: [url=https://sublustris-versio.vercel.app/install/]sublustris-versio.vercel.app/install[/url]
[*]Manual: [url=https://sublustris-versio.vercel.app/manual/]sublustris-versio.vercel.app/manual[/url]
[*]Eight starting points, from an endless CANYON pad to a dark drone: [url=https://sublustris-versio.vercel.app/presets/]sublustris-versio.vercel.app/presets[/url]
[*]Source code: [url=https://github.com/jeffebauer/sublustris-versio]github.com/jeffebauer/sublustris-versio[/url]
[/list]

[b]I'd really like your feedback[/b]

This is the first public release, and I want to keep shaping it with the people playing it. If a tail sounds wrong, if a knob feels dead or too touchy, or if there's a sound you'd want it to make, please tell me. Replies here are great. If you have a GitHub account, there are two short forms for a [url=https://github.com/jeffebauer/sublustris-versio/issues/new?template=sound_idea.yml]sound idea[/url] or a [url=https://github.com/jeffebauer/sublustris-versio/issues/new?template=bug_report.yml]bug report[/url]. A short recording and your settings help a lot, and ideas that make it into a release get credited in the changelog.

One safety note if you flash it: never have USB and Eurorack power connected at the same time.

Sublustris is an independent project, not affiliated with or endorsed by Noise Engineering. Thanks to them for keeping the Versio open, and to everyone here whose firmware and advice got me this far.

Cheers,
Jesse
```
