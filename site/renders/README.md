# Site renders (Blender)

The scripts that make the site's photographs: the module (hero, front-on, panel macro, patched, share image) and the story section's gear. Ported from Resilio Versio's `site/renders/` (the owner's own MIT code); what changed is listed at the top of each file.

Blender 5.x, headless. Run everything from the repo root (or the site worktree). Outputs go to the **main checkout's** gitignored `renders/` (served at `http://localhost:8765/`), never into the repo.

```bash
BLENDER=/Applications/Blender.app/Contents/MacOS/Blender
```

## 1. The panel decal

```bash
python3 site/renders/make_panel_decal.py          # -> site/renders/textures/panel_print.png
python3 site/renders/make_panel_decal.py --full   # the art as drawn, for comparison
```

Reads the owner's art, `docs/minisite/assets/panel/sublustris-versio-panel-art.svg` (191 × 486 units = 50.5 × 128.5 mm), removes the drawn hardware (knobs, jacks, toggles, button, LEDs, screws: the 3D scene models those) and rasterises the print at 32 px/mm on transparency with headless Chrome. Hardware is recognised by what it is (Figma's shadow filters, the toggle bitmaps, the knob lines), not by layer order, and every part is matched to the Versio template's holes; the script prints the match and warns if a count is off.

**New art:** replace the SVG (same file name), run the decal step, re-render. Nothing else changes.

## 2. Clay pass (composition review)

```bash
$BLENDER -b --factory-startup --python site/renders/render_clay.py -- [--only hero,front_a] [--samples 48]
```

Every variant of every slot, 1920 px wide JPEGs (front-on at the site's exact 2116 × 1190), plus `index.html` (contact sheet, grouped by slot, camera notes, click to enlarge) and `variants.json`, into `renders/site_clay/`. `--only` takes slot names (`hero`, `front`, `macro`, `patched`, `og`, `story`) or id prefixes; `--exact` for exact ids; `--scale 30 --samples 8` for a quick test; `--index-only` rewrites the sheet. About 15–40 s per frame on the Metal GPU.

The front-on variants solve their camera so the panel lands on the annotated panel's box (`site/src/components/AnnotatedPanel.astro`: x 872, y 121, w 372, h 950 in 2116 × 1190), then render a mask of the panel alone and measure it; the measured box is printed and shown on the contact sheet.

| File | What |
|---|---|
| `common.py` | scene units (mm), Cycles setup, studio lights, sweep, cameras, materials |
| `module.py` | the Versio: panel with the decal, knobs, jacks, toggles, button, LEDs (freeze blue), screws, PCBs |
| `props.py` | patch cables (black L, freeze-blue R) |
| `story.py` | round 1 story gear (generic, unbranded): two decks sharing one tape, a 1U rack reverb, a plate in its frame |
| `studio.py` | round 2 story gear, researched (`STORY_RESEARCH.md`): two A77-layout decks cross-patched, a lamp-lit home studio with a rack of period units and the reverb's remote |
| `render_hero.py` | the animated hero: the reveal (frames 1-240) and the hold loop (241-360) |
| `make_hero_video.sh` | encodes the hero frames to MP4 + WebM and posters (`preview` = MP4 only) |
| `render_clay.py` | the variants, moods (lighting), cameras, front-on solver + measurement, contact sheet |
| `render_final.py` | the picked variants at final size and quality |

Round 2 (the owner's notes on round 1): `--round 2` renders only the round-2 variants into `renders/site_clay_r2/` and lists the round-1 picks there for reference.

## 2b. Hero film

```bash
$BLENDER -b --factory-startup --python site/renders/render_hero.py -- --frames /tmp/hero_frames [--samples 64] [--res 2400x1350]
site/renders/make_hero_video.sh /tmp/hero_frames docs/minisite/assets/renders   # hero-reveal + hero-hold-loop (.mp4, .webm), posters
```

A preview: `--samples 12 --res 960x540`, then `make_hero_video.sh FRAMES OUT preview`. Full quality is about 18 s a frame on the Metal GPU (2400 × 1350, 64 samples): 360 frames ≈ 1 h 50 min, ~150 MB of frames (delete them after encoding).

## 3. Finals (after the owner's picks)

Fill `PICKS` in `render_final.py` (clay id → site file name), then:

```bash
$BLENDER -b --factory-startup --python site/renders/render_final.py -- [--samples 192]
```

PNGs at each slot's final size into `renders/site_final/`, lightly sharpened. Convert to JPEG and drop into `docs/minisite/assets/renders/` under the slot names in `docs/minisite/README.md` ("Renders to come").

## Rules

- No manufacturer's logo, badge or product name on any rendered gear (the story gear is generic; the module carries only the owner's panel art).
- Reference photos of real gear are never copied into the repo; only measurements taken from them.
- `textures/` holds only the generated decal (from the owner's art). Nothing third-party.
