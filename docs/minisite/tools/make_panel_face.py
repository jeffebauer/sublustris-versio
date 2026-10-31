"""Make the panel face for the site: the owner's panel art without its knob pointers and toggle levers.

The manual's panel and the presets' set panels draw the owner's panel art (the one the renders
use: docs/minisite/assets/panel/sublustris-versio-panel-art.svg, exported from Figma) and put
the pointers and levers back on top, set to each patch (site/src/components/PanelArt.astro).
So this takes them out of the art and rasterises the rest once: the art's knob caps are
foreignObject conic gradients and ~900 filter primitives, too heavy to inline on every preset
and not drawn by every browser when the SVG is an <img>.

    .venv/bin/python docs/minisite/tools/make_panel_face.py
    # -> docs/minisite/assets/panel/sublustris-versio-panel-face.webp (764 x 1944: 4 px per art unit)

Needs Google Chrome (headless) and Pillow. Run it again whenever the panel art changes.
"""
import os
import re
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PANEL = os.path.join(HERE, "..", "assets", "panel")
ART = os.path.join(PANEL, "sublustris-versio-panel-art.svg")
OUT = os.path.join(PANEL, "sublustris-versio-panel-face.webp")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
SCALE = 4  # px per art unit (the art is 191 x 486 units = 50.5 x 128.5 mm)


def face_svg(svg):
    # The seven knobs' pointers (eight paths: BLEND's is drawn twice), white 1.5-unit strokes
    pointers = re.findall(r'<path d="M[\d.]+ [\d.]+V[\d.]+" stroke-width="1.5" stroke="white"/>', svg)
    # The two toggles' levers, embedded PNGs
    levers = re.findall(r'<image x="22.4" [^>]*/>', svg)
    if len(pointers) != 8 or len(levers) != 2:
        sys.exit(f"[face] expected 8 pointers and 2 levers, found {len(pointers)} and {len(levers)}: has the art changed?")
    for el in pointers + levers:
        svg = svg.replace(el, "", 1)
    return svg


def main():
    svg = face_svg(open(ART).read())
    w, h = 191 * SCALE, 486 * SCALE
    svg = re.sub(r'width="191" height="486"', f'width="{w}" height="{h}"', svg, count=1)
    html = ("<!doctype html><html><head><style>html,body{margin:0;background:#000}"
            f"svg{{display:block}}</style></head><body>{svg}</body></html>")
    with tempfile.TemporaryDirectory() as td:
        page, png = os.path.join(td, "face.html"), os.path.join(td, "face.png")
        open(page, "w").write(html)
        subprocess.run([CHROME, "--headless=new", "--hide-scrollbars", "--virtual-time-budget=5000",
                        f"--window-size={w},{h}", f"--screenshot={png}", "file://" + page],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        Image.open(png).convert("RGB").save(OUT, "WEBP", quality=88, method=6)
    print(f"[face] wrote {os.path.relpath(OUT)} ({w} x {h}, {os.path.getsize(OUT) // 1024} KB)")


if __name__ == "__main__":
    main()
