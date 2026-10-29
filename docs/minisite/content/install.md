---
title: "Install Sublustris Versio: Versio firmware and macOS plugin"
description: "How to put Sublustris Versio on a Noise Engineering Versio with Noise Engineering's Firmware Swap web app, how to go back to stock firmware, and how to install the AU/VST3 plugin on macOS."
slug: "install"
order: 3
---

# Install

You can play Sublustris as firmware on a Noise Engineering Versio, or as a plugin in a DAW on a Mac. Both run the same sound engine.

<!-- DOWNLOADS: the same two buttons as the overview, with version, date and size. "Coming soon" until the first public release. -->

## What you need

**Firmware**
- A Noise Engineering Versio module.
- A computer with **Chrome**, for Noise Engineering's Firmware Swap web app.
- A micro-USB cable that carries data (not a charge-only cable).
- The Sublustris firmware file (`.bin`) from the download above.

**Plugin**
- A Mac with **macOS 12 or newer**, Apple Silicon or Intel. (It's built for both; I play it on Apple Silicon.)
- A DAW that loads **Audio Unit** or **VST3** plugins. (I use it day to day in Ableton Live.)
- There's no Windows or Linux version.

## On the Versio

Sublustris replaces the module's firmware entirely. The rest of the module stays as it is, and you can put Noise Engineering's own firmware back on with the same app (below).

> **Never connect USB and Eurorack power at the same time.** Power off, take the module out of the rack and unplug its Eurorack power cable before you plug in USB.

1. **Power off** your case. Take the Versio out and **unplug the Eurorack power cable** from the module.
2. Plug a micro-USB cable into the **Daisy Seed on the back of the module**, and the other end into your computer. The module runs on USB power alone.
3. In **Chrome**, open Noise Engineering's Firmware Swap app: [noiseengineering.us/portal/firmware](https://noiseengineering.us/portal/firmware). If it asks which module, choose your Versio.
4. Choose **Select Custom File** and pick the Sublustris `.bin` file you downloaded.
5. Click **Connect**, then **Change Firmware**. Wait until it reports that it's done.
6. **Unplug USB**, put the module back in the case, and reconnect the Eurorack power.
7. Power on. The four LEDs play a short sweep, cool blue to violet to white, to show Sublustris has loaded. Then they meter the input and output.

Your Versio's printed labels won't match Sublustris's controls, so keep the [panel map](/manual#panel-map) handy.

### Going back to Noise Engineering's firmware

Same app, same steps, USB only with the rack power unplugged. At step 4, instead of a custom file, pick the Noise Engineering firmware you want from the app's list (for example your module's original one), then **Connect** → **Change Firmware**. Sublustris doesn't install a bootloader or change anything else, so this fully restores the module.

### Updating Sublustris

Download the new `.bin` and repeat the steps above. You won't lose any settings, because the module's state is just its knobs and switches.

## In a DAW (macOS)

The download is a zip with the plugin in both formats, a read-me and the licence files.

1. **Unzip** it. You get `Sublustris Versio.vst3` (VST3) and `Sublustris Versio.component` (Audio Unit). Install whichever your DAW uses, or both.
2. In Finder, choose **Go → Go to Folder…** and paste:
   ```
   ~/Library/Audio/Plug-Ins/
   ```
   Drag `Sublustris Versio.vst3` into the **VST3** folder and `Sublustris Versio.component` into the **Components** folder. Create the folder if it's missing.
3. **Let macOS open it.** The plugin isn't notarised by Apple (it's a free download), so macOS blocks it at first. Open **Terminal** and paste these two lines, one at a time (each is a single line):
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Sublustris Versio.vst3"
   ```
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Sublustris Versio.component"
   ```
   These remove the "downloaded from the internet" flag from those two files and leave everything else alone.
4. **Restart your DAW and rescan plugins.** In Ableton Live: Settings → Plug-Ins, turn on the VST3 and/or Audio Units system folders, then hold Option and click **Rescan**. It shows up as **Sublustris Versio**, under Ombra Audio.

Use it on a return track with BLEND fully right, or as an insert with BLEND to taste.

<!-- OWNER/verify: the zip's contents and that the release isn't notarised, once tools/make_release.sh makes the public release. -->

### Updating the plugin

Quit your DAW, replace the two files with the new ones, run the two Terminal lines again, and rescan. Saved sets keep their settings across versions.

A set made before 27 October 2026 may show older control names until you rescan. The settings themselves are the same.

### Uninstalling

Delete `Sublustris Versio.vst3` from `~/Library/Audio/Plug-Ins/VST3/` and `Sublustris Versio.component` from `~/Library/Audio/Plug-Ins/Components/`, then rescan.

## Troubleshooting

| What you see | Try |
|---|---|
| Firmware Swap doesn't find the module | Use Chrome. Try another micro-USB cable (many only charge). Check the rack power is unplugged and USB goes to the Daisy Seed on the back. |
| No colour sweep at power-up | Check the module is getting power, then flash the firmware again. |
| The knobs don't do what the printed labels say | Expected: Sublustris has its own layout. See the [panel map](/manual#panel-map). |
| "Sublustris Versio can't be opened" / the DAW doesn't list it | Run the two Terminal lines (step 3) and rescan. |
| The output LEDs turn blue | The reverb is frozen. Tap the button (or send a short gate) to unfreeze. |
| Output LEDs red a lot | The output limiter is catching peaks. Lower the input level or BLEND. |
| Input LEDs red | Your source is near clipping at the jack. Turn it down. |
| The tail never stops | DECAY is past about 3 o'clock, where the tail is endless. Turn it down and the tail fades. |
