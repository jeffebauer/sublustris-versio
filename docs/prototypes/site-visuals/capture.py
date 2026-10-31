#!/usr/bin/env python3
"""Capture stills, loops, frame times and contrast numbers for the site-visuals page.

Runs build.py, opens OUT/capture.html in headless Chrome (Playwright, channel="chrome"),
and for every option in studies.json:
  - times the GPU draw at hero size (0.5x and full Retina) and a live rAF loop,
  - saves a JPEG still at the still phase (4:4:4; lossy WebP banded the dark gradients),
  - streams every loop frame as PNG straight into ffmpeg (no frame dump on disk),
    writing a seamless MP4 (H.264) and WebM (VP9), and checks the seam,
  - measures the brightest pixel per placement for the contrast numbers,
then writes OUT/media/manifest.json and rebuilds the page with the numbers.

    <venv-with-playwright-and-pillow>/bin/python capture.py [--out DIR] [--only key,...] [--no-video]
"""
import argparse
import base64
import io
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build  # noqa: E402
from PIL import Image, ImageChops  # noqa: E402

FFMPEG = "/opt/homebrew/bin/ffmpeg"
TEXT = ("#f4f5fa", "#a4a7b9")   # headings, secondary body text on the dark bands
TAU = 6.283185307179586


def lin(c):
    c /= 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lum_hex(h):
    r, g, b = (int(h[i:i + 2], 16) for i in (1, 3, 5))
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def cr(a, b):
    hi, lo = max(a, b), min(a, b)
    return (hi + 0.05) / (lo + 0.05)


def png(u):
    return base64.b64decode(u.split(",", 1)[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=build.DEFAULT_OUT)
    ap.add_argument("--only", default="")
    ap.add_argument("--no-video", action="store_true")
    ap.add_argument("--crf264", type=int, default=20)
    ap.add_argument("--crfvp9", type=int, default=20)
    a = ap.parse_args()
    out = a.out
    media = out / "media"
    subprocess.run([sys.executable, str(HERE / "build.py"), "--out", str(out)], check=True)
    meta, _ = build.load()
    W, H = meta["capture_size"]
    fps = meta["fps"]
    still_ph = meta["still_phase"] * TAU
    mf = media / "manifest.json"
    man = json.loads(mf.read_text()) if mf.exists() else {}
    for k in ("files", "timing", "contrast", "seam"):
        man.setdefault(k, {})
    man["timing_size"] = [1440, 800]
    only = set(filter(None, a.only.split(",")))
    t1, t2 = (lum_hex(h) for h in TEXT)

    from playwright.sync_api import sync_playwright
    with sync_playwright() as p:
        b = p.chromium.launch(channel="chrome", headless=True)
        pg = b.new_page(viewport={"width": 1500, "height": 900})
        errs = []
        pg.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
        pg.on("pageerror", lambda e: errs.append(str(e)))
        pg.goto((out / "capture.html").as_uri())
        man["gpu"] = pg.evaluate("""()=>{const g=R.gl;const e=g.getExtension('WEBGL_debug_renderer_info');
            const s=e?g.getParameter(e.UNMASKED_RENDERER_WEBGL):g.getParameter(g.RENDERER);
            const m=s.match(/Apple M[0-9A-Za-z ]+?(?=,|$)/);return m?m[0].trim():s}""")
        for st, o in build.options(meta):
            key = o["key"]
            if only and key not in only:
                continue
            print(f"[{key}] timing, still, contrast", flush=True)
            man["timing"][key] = {"half": pg.evaluate(f"timeit('{key}', 1440, 800, 120)"),
                                  "full": pg.evaluate(f"timeit('{key}', 2880, 1600, 60)"),
                                  "raf_fps": round(pg.evaluate(f"rafRun('{key}', 1440, 800, 3)"))}
            Image.open(io.BytesIO(png(pg.evaluate(f"frame('{key}', {W}, {H}, {still_ph}, 1)")))).convert("RGB").save(
                media / f"{key}.jpg", "JPEG", quality=90, subsampling=0, optimize=True)
            man["files"][f"{key}.jpg"] = (media / f"{key}.jpg").stat().st_size
            for pl in meta["placements"]:
                r = pg.evaluate(f"extreme('{key}', 480, 270, {pl['amt']}, 36)")
                man["contrast"][f"{key}-{pl['id']}"] = {"extreme_rgb": r["rgb"], "L": r["L"],
                                                        "primary": cr(t1, r["L"]), "secondary": cr(t2, r["L"])}
            print(f"[{key}] P1 {man['contrast'][key + '-hero']['secondary']:.2f}  P2 {man['contrast'][key + '-band']['secondary']:.2f}", flush=True)
            if a.no_video:
                continue
            # long loops: smaller frame and lower rate keep each file under ~1.5 MB (the content is soft and slow)
            vw, vh = o.get("vsize", [W, H])
            vfps = o.get("vfps", fps)
            nframes = vfps * o["loop"]
            mp4, webm = media / f"{key}.mp4", media / f"{key}.webm"
            cmd = [FFMPEG, "-y", "-loglevel", "error", "-f", "image2pipe", "-framerate", str(vfps), "-c:v", "png", "-i", "-",
                   "-c:v", "libx264", "-preset", "slow", "-tune", "grain", "-crf", str(o.get("crf264", a.crf264)), "-pix_fmt", "yuv420p",
                   "-movflags", "+faststart", "-an", str(mp4),
                   "-c:v", "libvpx-vp9", "-b:v", "0", "-crf", str(o.get("crfvp9", a.crfvp9)), "-row-mt", "1", "-deadline", "good", "-cpu-used", "2",
                   "-pix_fmt", "yuv420p", "-an", str(webm)]
            ff = subprocess.Popen(cmd, stdin=subprocess.PIPE)
            keep = {}
            for i in range(nframes):
                data = png(pg.evaluate(f"frame('{key}', {vw}, {vh}, {i / nframes * TAU}, 1)"))
                if i in (0, 1, nframes - 1):
                    keep[i] = data
                ff.stdin.write(data)
            ff.stdin.close()
            if ff.wait() != 0:
                raise SystemExit(f"ffmpeg failed for {key}")
            # seam check: the jump from the last frame back to the first vs an ordinary step
            im = {i: Image.open(io.BytesIO(v)).convert("L") for i, v in keep.items()}
            step = sum(ImageChops.difference(im[0], im[1]).get_flattened_data())
            seam = sum(ImageChops.difference(im[nframes - 1], im[0]).get_flattened_data())
            man["seam"][key] = round(seam / max(step, 1), 2)
            man["files"][mp4.name] = mp4.stat().st_size
            man["files"][webm.name] = webm.stat().st_size
            print(f"[{key}] {o['loop']} s: mp4 {man['files'][mp4.name] // 1024} KB, webm {man['files'][webm.name] // 1024} KB, "
                  f"seam/step {man['seam'][key]}", flush=True)
        b.close()
        if errs:
            print("console errors:", *errs[:10], sep="\n  ")
    keys = {o["key"] for _, o in build.options(meta)}
    man["video"] = {o["key"]: [*o.get("vsize", [W, H]), o.get("vfps", fps)] for _, o in build.options(meta)}
    man["files"] = {k: v for k, v in man["files"].items() if k.rsplit(".", 1)[0] in keys}
    man["total_bytes"] = sum(man["files"].values())
    mf.write_text(json.dumps(man, indent=1))
    subprocess.run([sys.executable, str(HERE / "build.py"), "--out", str(out)], check=True)


if __name__ == "__main__":
    main()
