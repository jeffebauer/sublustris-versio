# Site visuals: abstract reverb studies

A review page of quiet background layers for the Sublustris Versio site: six ideas
(diffusion, waves, reflections, shimmer, haze, tail/freeze), two options each, shown as a
live WebGL shader, a still and a seamless loop, then each idea's strongest option mocked in
three placements (P1 hero, P2 sound band, P3 white page haze). Nothing here is wired into
the site.

Built page: `renders/site_visuals/index.html` in the main checkout
(http://localhost:8765/site_visuals/index.html).

## Files
- `shaders/common.glsl`: uniforms, noise (value noise whose time axis loops), palette
  mapping (dark band / white page), dither. Each study only defines `vec2 field(vec2 p)`.
- `shaders/<idea>.glsl`: both options, selected with `#define V 0|1`.
- `studies.json`: names, one-line descriptions, the option used in the placements (`best`),
  placement strengths, loop length, render scale and frame cap.
- `renderer.js`: one shared WebGL context that draws every target and copies it into each
  target's 2D canvas (browsers allow ~16 live contexts; the page has 30 canvases).
- `page.js`, `page.css`: page wiring (IntersectionObserver pause, 0.5× resolution, 30 fps
  cap, reduced motion still, loops play only on screen) and styles.
- `build.py`: writes the page, `studies.js` and `capture.html` into the output folder,
  filling sizes, frame times and contrast from `media/manifest.json`.
- `capture.py`: headless Chrome (Playwright, `channel="chrome"`) renders stills (JPEG
  4:4:4, which kept the dithered gradients far better than lossy WebP), streams loop frames
  as PNG straight into ffmpeg (MP4 H.264 + WebM VP9, no frame dump on disk), times each
  shader and measures the brightest pixel per placement for the contrast numbers.
- `verify.py`: console errors, live canvases, reduced motion, 375 px width, no-JS fallback,
  screenshots.

All shader code is written for this project (no Shadertoy or other third-party code).

## Rebuild
```bash
VENV=<a venv with playwright and pillow>   # e.g. the session's scratchpad imgvenv
python3 build.py                            # page only (after editing copy, CSS or JS)
$VENV/bin/python capture.py                 # everything: stills, loops, timing, contrast (~3 min)
$VENV/bin/python capture.py --only haze-0 --no-video   # one option, no loop
$VENV/bin/python verify.py http://localhost:8765/site_visuals/index.html <shot-dir>
```
Every motion term is periodic in `u_ph` (0..2π over `loop_seconds`): orbits use whole
turns, ripples whole wavelengths, noise travels a whole period of its looping z axis. Keep
that rule when editing a shader or the loop will jump.
