# Site visuals: abstract reverb studies

A review page of quiet background layers for the Sublustris Versio site. Round 2 keeps the
five studies the owner chose (diffusion B, waves A and B, haze A and B), with numbered steps
where useful (diffusion B slow 1/2, haze A/B brighter 1/2). Each option is shown as a live
WebGL shader, a still and a seamless loop, then mocked behind the two dark sections (P1 hero,
P2 sound band). Nothing here is wired into the site.

Built page: `renders/site_visuals/index.html` in the main checkout
(http://localhost:8765/site_visuals/index.html).

## Files
- `shaders/common.glsl`: uniforms, 4D value noise (two space, two time axes), the time path,
  palette mapping, dither. Each study only defines `vec2 field(vec2 p)`.
- `shaders/<idea>.glsl`: the options, selected with `#define V 0|1`.
- `studies.json`: per option: key, name, shader + V, loop length, brightness `gain`, and for
  long loops a smaller video size/rate and CRFs (`vsize`, `vfps`, `crf264`, `crfvp9`) to stay
  under ~1.5 MB; plus placement strengths, render scale and frame cap.
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
Motion never runs backwards. Noise texture moves through a 2D time plane via `tpath(R)`:
the loop files go once round a circle of radius R (in noise cells; R ≈ 0.5 or more, or the
motion turns into a visible back-and-forth), live goes along a straight line at the same
speed, starting on the still frame, so it never repeats. Waves A's rings use whole
wavelengths per loop through a still noise "medium". Anything else that moves must be
periodic in `u_ph` or the loop will jump; capture.py prints a seam/step ratio (≈1 is clean).
