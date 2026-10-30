#!/usr/bin/env python3
"""Check the built page in headless Chrome: console errors, live canvases, reduced motion,
375 px width (no horizontal scroll), no-JS fallback, and a screenshot per section.

    <venv-with-playwright>/bin/python verify.py [URL] [SHOT_DIR]
"""
import sys
from pathlib import Path
from playwright.sync_api import sync_playwright

URL = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8765/site_visuals/index.html"
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else "shots")
OUT.mkdir(parents=True, exist_ok=True)
SECTIONS = ["diffusion", "waves", "haze"]
PLACES = ["diffusion-b1", "diffusion-b2", "waves-a", "waves-b", "haze-a1", "haze-a2", "haze-b1", "haze-b2"]


def run(p, name, **ctx):
    b = p.chromium.launch(channel="chrome", headless=True)
    c = b.new_context(**ctx)
    pg = c.new_page()
    errs = []
    pg.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
    pg.on("pageerror", lambda e: errs.append(str(e)))
    pg.goto(URL, wait_until="load")
    pg.wait_for_timeout(800)
    return b, pg, errs


def scroll_all(pg):
    h = pg.evaluate("document.body.scrollHeight")
    for y in range(0, h, 600):
        pg.evaluate(f"window.scrollTo(0,{y})")
        pg.wait_for_timeout(60)


def canvas_changes(pg, sel):
    js = f"""async () => {{
      const el = document.querySelector('{sel}'); el.scrollIntoView();
      await new Promise(r => setTimeout(r, 600));
      const c = el.querySelector('canvas'); const a = c.toDataURL();
      await new Promise(r => setTimeout(r, 900));
      return [c.width + 'x' + c.height, a !== c.toDataURL(), el.classList.contains('is-live')];
    }}"""
    return pg.evaluate(js)


with sync_playwright() as p:
    # desktop
    b, pg, errs = run(p, "desktop", viewport={"width": 1440, "height": 900}, device_scale_factor=2)
    print("desktop live canvas (size, animating, drawn):", canvas_changes(pg, "#diffusion-b1 .live"))
    scroll_all(pg)
    print("desktop canvases drawn:", pg.evaluate("document.querySelectorAll('.is-live').length"), "of",
          pg.evaluate("document.querySelectorAll('[data-live]').length"))
    for s in SECTIONS:
        pg.locator(f"#{s}").screenshot(path=str(OUT / f"desktop-{s}.png"), scale="css")
    for s in PLACES:
        pg.locator(f"#place-{s}").screenshot(path=str(OUT / f"desktop-place-{s}.png"), scale="css")
    pg.evaluate("window.scrollTo(0,0)")
    pg.screenshot(path=str(OUT / "desktop-top.png"), scale="css")
    pg.locator("#notes").screenshot(path=str(OUT / "desktop-notes.png"), scale="css")
    vid = pg.evaluate("""async () => { const v = document.querySelector('#waves-a video'); v.scrollIntoView();
        await new Promise(r => setTimeout(r, 1500)); return [v.paused, v.currentTime > 0, v.currentSrc.split('/').pop()]; }""")
    print("desktop loop video (paused, advanced, src):", vid)
    print("desktop errors:", errs or "none")
    b.close()

    # reduced motion
    b, pg, errs = run(p, "reduced", viewport={"width": 1440, "height": 900}, reduced_motion="reduce")
    print("reduced-motion canvas (size, animating, drawn):", canvas_changes(pg, "#haze-a1 .live"))
    v = pg.evaluate("""async () => { const v = document.querySelector('#haze-a1 video'); v.scrollIntoView();
        await new Promise(r => setTimeout(r, 800)); return [v.paused, v.hasAttribute('controls')]; }""")
    print("reduced-motion video (paused, has controls):", v)
    print("reduced errors:", errs or "none")
    b.close()

    # phone
    b, pg, errs = run(p, "phone", viewport={"width": 375, "height": 812}, device_scale_factor=3, is_mobile=True, has_touch=True)
    scroll_all(pg)
    print("375 px: scrollWidth", pg.evaluate("document.documentElement.scrollWidth"), "clientWidth", pg.evaluate("document.documentElement.clientWidth"))
    pg.evaluate("window.scrollTo(0,0)")
    pg.screenshot(path=str(OUT / "phone-top.png"))
    pg.locator("#waves").screenshot(path=str(OUT / "phone-waves.png"))
    pg.locator("#place-haze-a2").screenshot(path=str(OUT / "phone-place-haze-a2.png"))
    print("phone errors:", errs or "none")
    b.close()

    # no JavaScript
    b = p.chromium.launch(channel="chrome", headless=True)
    c = b.new_context(java_script_enabled=False, viewport={"width": 1440, "height": 900})
    pg = c.new_page()
    pg.goto(URL, wait_until="load")
    pg.locator("#waves-a").scroll_into_view_if_needed()
    pg.wait_for_timeout(800)
    pg.locator("#waves-a").screenshot(path=str(OUT / "nojs-waves-a.png"))
    print("no-JS: still images loaded:", pg.evaluate("[...document.querySelectorAll('#waves-a img')].map(i => i.complete && i.naturalWidth > 0)"))
    b.close()
print("shots in", OUT.resolve())
