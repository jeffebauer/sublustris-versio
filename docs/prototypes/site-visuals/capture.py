#!/usr/bin/env python3
"""Capture stills, loops, frame times and contrast numbers for the site-visuals page.

Runs build.py, opens OUT/capture.html in headless Chrome (Playwright, channel="chrome"),
and for every study option:
  - times the GPU draw at hero size (0.5x and full Retina) and a rAF loop,
  - saves a JPEG still at the still phase,
  - streams every loop frame as PNG straight into ffmpeg (no frame dump on disk),
    writing a seamless MP4 (H.264) and WebM (VP9),
then measures the brightest pixel per placement, writes OUT/media/manifest.json and
rebuilds the page with the numbers.

    <venv-with-playwright>/bin/python capture.py [--out DIR] [--only diffusion-0,...] [--no-video]
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
LETTERS = "ab"
TEXT = {"dark": ("#f4f5fa", "#a4a7b9"), "light": ("#0b0c12", "#5b5e6e")}


def lin(c):
    c /= 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lum_hex(h):
    r, g, b = (int(h[i:i + 2], 16) for i in (1, 3, 5))
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def cr(a, b):
    hi, lo = max(a, b), min(a, b)
    return (hi + 0.05) / (lo + 0.05)


def data_url_bytes(u):
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
    fps, loop_s = meta["fps"], meta["loop_seconds"]
    nframes = fps * loop_s
    still_ph = meta["still_phase"] * 6.283185307179586
    mf = media / "manifest.json"
    man = json.loads(mf.read_text()) if mf.exists() else {}
    man.setdefault("files", {})
    man.setdefault("timing", {})
    man.setdefault("contrast", {})
    man["timing_size"] = [1440, 800]
    only = set(filter(None, a.only.split(",")))

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
        for st in meta["studies"]:
            for v in range(len(st["variants"])):
                key = f"{st['id']}-{v}"
                if only and key not in only:
                    continue
                base = f"{st['id']}-{LETTERS[v]}"
                print(f"[{key}] timing", flush=True)
                half = pg.evaluate(f"timeit('{key}', 1440, 800, 120)")
                full = pg.evaluate(f"timeit('{key}', 2880, 1600, 60)")
                fpsr = pg.evaluate(f"rafRun('{key}', 1440, 800, 3)")
                man["timing"][key] = {"half": half, "full": full, "raf_fps": round(fpsr)}
                stills = [("dark", 0, base + ".jpg")]
                if v == st["best"]:
                    stills.append(("light", 1, base + "-light.jpg"))
                for mode, light, name in stills:
                    u = pg.evaluate(f"frame('{key}', {W}, {H}, {still_ph}, {light}, 1, 'image/png')")
                    # JPEG 4:4:4 keeps the dither (and so the smooth gradients) far better than lossy WebP here
                    Image.open(io.BytesIO(data_url_bytes(u))).convert("RGB").save(media / name, "JPEG", quality=90, subsampling=0, optimize=True)
                    man["files"][name] = (media / name).stat().st_size
                if v == st["best"]:
                    for pl in meta["placements"]:
                        light = 1 if pl["mode"] == "light" else 0
                        r = pg.evaluate(f"extreme('{key}', 480, 270, {light}, {pl['amt']}, 24)")
                        t1, t2 = (lum_hex(h) for h in TEXT[pl["mode"]])
                        man["contrast"][f"{key}-{pl['id']}"] = {
                            "extreme_rgb": r["rgb"], "L": r["L"],
                            "primary": cr(t1, r["L"]), "secondary": cr(t2, r["L"])}
                if a.no_video:
                    continue
                print(f"[{key}] loop {nframes} frames", flush=True)
                mp4, webm = media / (base + ".mp4"), media / (base + ".webm")
                cmd = [FFMPEG, "-y", "-loglevel", "error", "-f", "image2pipe", "-framerate", str(fps), "-c:v", "png", "-i", "-",
                       "-c:v", "libx264", "-preset", "slow", "-tune", "grain", "-crf", str(a.crf264), "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-an", str(mp4),
                       "-c:v", "libvpx-vp9", "-b:v", "0", "-crf", str(a.crfvp9), "-row-mt", "1", "-deadline", "good", "-cpu-used", "2",
                       "-pix_fmt", "yuv420p", "-an", str(webm)]
                ff = subprocess.Popen(cmd, stdin=subprocess.PIPE)
                keep = {}
                for i in range(nframes):
                    ph = i / nframes * 6.283185307179586
                    u = pg.evaluate(f"frame('{key}', {W}, {H}, {ph}, 0, 1, 'image/png')")
                    png = data_url_bytes(u)
                    if i in (0, 1, nframes - 1):
                        keep[i] = png
                    ff.stdin.write(png)
                # seam check: the jump from the last frame back to the first should match an ordinary step
                im = {i: Image.open(io.BytesIO(v)).convert("L") for i, v in keep.items()}
                step = sum(ImageChops.difference(im[0], im[1]).getdata())
                seam = sum(ImageChops.difference(im[nframes - 1], im[0]).getdata())
                man.setdefault("seam", {})[key] = round(seam / max(step, 1), 2)
                ff.stdin.close()
                if ff.wait() != 0:
                    raise SystemExit(f"ffmpeg failed for {key}")
                man["files"][mp4.name] = mp4.stat().st_size
                man["files"][webm.name] = webm.stat().st_size
                print(f"[{key}] mp4 {man['files'][mp4.name] // 1024} KB, webm {man['files'][webm.name] // 1024} KB, seam/step {man['seam'][key]}", flush=True)
        b.close()
        if errs:
            print("console errors:", *errs[:10], sep="\n  ")
    man["total_bytes"] = sum(man["files"].values())
    mf.write_text(json.dumps(man, indent=1))
    subprocess.run([sys.executable, str(HERE / "build.py"), "--out", str(out)], check=True)


if __name__ == "__main__":
    main()
