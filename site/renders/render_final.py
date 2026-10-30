"""Final stills for the site, from the owner's clay picks, at full quality.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \\
        --python site/renders/render_final.py -- [--samples 192] [--only ID,ID]

Same scenes and cameras as render_clay.py (one source of truth); this file
only picks the shots, the final size and the quality. Each pick is rendered
at its slot's final size (the clay frame scaled up, so the composition is
identical), as PNG into renders/site_final/ in the main checkout
(gitignored), then lightly sharpened (Resilio's luma unsharp mask). Convert
to the site's JPEGs and copy them into docs/minisite/assets/renders/ under
the slot names (docs/minisite/README.md, "Renders to come").

Ported from Resilio Versio's render_final.py (the owner's MIT code).
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import render_clay  # noqa: E402

FINAL_OUT = os.path.join(render_clay.MAIN_CHECKOUT, "renders", "site_final")

# The owner's picks: clay variant id -> site file.
# The owner's picks, 30 Oct 2026 (round 1: front, patched, og; round 2: hero, macro).
# The story waits for round 3. Rendered with the blue panel art. The hero still is no longer
# made here: it is the transparent film's frame 240 (render_hero.py --only 240 --samples 192).
PICKS = {
    "front_b": "front-on-wide.jpg",
    "patched_a": "patched.jpg",
    "og_a": "og-hero.jpg",
    "macro_d": "panel-macro.jpg",
}

# final width per slot (heights follow the clay frame's aspect)
FINAL_WIDTH = dict(hero=2400, front=2116, macro=2400, patched=2400, og=1200, story=2400)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    samples = argv[argv.index("--samples") + 1] if "--samples" in argv else "192"
    ids = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(PICKS)
    if not ids:
        sys.exit("render_final: no picks yet (fill PICKS from the clay contact sheet)")
    probe = {v["id"]: v for v in render_clay.module_variants(render_clay._dummy_module_ref())
             + render_clay.story_variants(render_clay._dummy_story_refs())}
    for vid in ids:
        v = probe[vid]
        scale = FINAL_WIDTH[v["slot"]] / v["res"][0]
        sys.argv = [sys.argv[0], "--", "--exact", "--only", vid, "--samples", samples, "--fmt", "PNG",
                    "--res-scale", f"{scale:.6f}", "--out", FINAL_OUT,
                    "--title", "Sublustris Versio site renders — final stills"]
        render_clay.main()
        sharpen(os.path.join(FINAL_OUT, vid + ".png"))


def sharpen(p):
    """A light unsharp mask (luma only): small white silkscreen at a grazing
    angle is only ~15 px tall, and a touch of sharpening makes it read crisp."""
    tmp = p + ".sharp.png"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", p, "-vf", "unsharp=5:5:0.7:5:5:0", tmp], check=True)
    os.replace(tmp, p)


if __name__ == "__main__":
    main()
