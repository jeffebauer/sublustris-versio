#!/usr/bin/env python3
"""Build the site-visuals review page.

Reads studies.json and shaders/*.glsl, writes into OUT (default: the main checkout's
renders/site_visuals): index.html, studies.js (shader sources), renderer.js, page.js,
page.css and capture.html. Sizes, frame times and contrast numbers come from
OUT/media/manifest.json, which capture.py writes; without it the page shows dashes.

    python3 build.py [--out DIR]
"""
import argparse
import html
import json
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = Path("/Users/jesse/Documents/Sites/immensio-versio/renders/site_visuals")
LETTERS = "AB"


def load():
    meta = json.loads((HERE / "studies.json").read_text())
    common = (HERE / "shaders" / "common.glsl").read_text()
    src = {}
    for st in meta["studies"]:
        body = (HERE / "shaders" / f"{st['id']}.glsl").read_text()
        for v in range(len(st["variants"])):
            src[f"{st['id']}-{v}"] = f"#define V {v}\n" + common + "\n" + body
    return meta, src


def kb(n):
    if n is None:
        return "–"
    return f"{n / 1024:.0f} KB" if n < 1024 * 1024 else f"{n / 1048576:.2f} MB"


def ms(x):
    return "–" if x is None else f"{x:.2f} ms"


def ratio(x):
    return "–" if x is None else f"{x:.1f}:1"


def code(st, v):
    return f"{st['id']} {LETTERS[v]}"


def live(st, v, mode, amt, cls="frame live"):
    still = f"media/{st['id']}-{LETTERS[v].lower()}{'-light' if mode == 'light' else ''}.jpg"
    return (f'<div class="{cls}" data-live data-study="{st["id"]}" data-variant="{v}" data-mode="{mode}" data-amt="{amt}">'
            f'<img class="fallback" src="{still}" alt="" loading="lazy" decoding="async" width="1280" height="720" style="opacity:{amt}">'
            f'<canvas aria-hidden="true"></canvas></div>')


def page(meta, man):
    e = html.escape
    fsz = man.get("files", {})
    tm = man.get("timing", {})
    ct = man.get("contrast", {})
    gpu = man.get("gpu", "this Mac")
    hw, hh = man.get("timing_size", [1440, 800])
    loop_s = meta["loop_seconds"]
    out = []
    nav = []
    for i, st in enumerate(meta["studies"], 1):
        nav.append(f'<a href="#{st["id"]}">{i} {e(st["title"])}</a>')
        nav.append('<span class="sub">' + " · ".join(code(st, v) for v in range(len(st["variants"]))) + "</span>")
    nav.append('<a href="#placements">7 Placements</a>')
    nav.append('<span class="sub">P1 hero · P2 sound band · P3 page haze</span>')
    nav.append('<a href="#notes">Notes and costs</a>')
    nav.append('<button type="button" id="motion-toggle" aria-pressed="false">Pause motion</button>')

    for i, st in enumerate(meta["studies"], 1):
        out.append(f'<section class="study" id="{st["id"]}"><header class="study-head"><p class="num">{i:02d}</p>'
                   f'<h2>{e(st["title"])}</h2><p class="line">{e(st["line"])}</p></header>')
        for v, var in enumerate(st["variants"]):
            k = f"{st['id']}-{v}"
            base = f"{st['id']}-{LETTERS[v].lower()}"
            t = tm.get(k, {})
            cost = (f'<b>Live shader</b> · {ms(t.get("half"))} per frame for a full-width hero ({hw}×{hh} px, i.e. 0.5× on a Retina screen); '
                    f'{ms(t.get("full"))} at full resolution · requestAnimationFrame held {t.get("raf_fps", "–")} fps · measured on this Mac ({e(gpu)})')
            out.append(f'<div class="variant" id="{base}"><h3><span class="code">{code(st, v)}</span> {e(var["name"])}</h3>'
                       f'<p>{e(var["line"])}</p>'
                       f'<figure>{live(st, v, "dark", 1)}<figcaption>{cost}</figcaption></figure>'
                       f'<div class="pair">'
                       f'<figure><div class="frame"><img src="media/{base}.jpg" alt="Still frame of {code(st, v)}, {e(var["name"])}" loading="lazy" decoding="async" width="1280" height="720"></div>'
                       f'<figcaption><b>Still</b> · JPEG 1280×720 · {kb(fsz.get(base + ".jpg"))}</figcaption></figure>'
                       f'<figure><div class="frame"><video data-loop muted loop playsinline preload="none" controls poster="media/{base}.jpg" aria-label="Loop of {code(st, v)}">'
                       f'<source src="media/{base}.webm" type="video/webm"><source src="media/{base}.mp4" type="video/mp4"></video></div>'
                       f'<figcaption><b>Loop</b> · {loop_s} s seamless, 1280×720, {meta["fps"]} fps, no sound · MP4 (H.264) {kb(fsz.get(base + ".mp4"))} · WebM (VP9) {kb(fsz.get(base + ".webm"))}</figcaption></figure>'
                       f'</div></div>')
        out.append("</section>")

    pl = meta["placements"]
    out.append('<section id="placements"><header><p class="num muted" style="font:500 13px/1 var(--mono);margin-bottom:10px">07</p><h2>Placements</h2>'
               '<p>Each study\'s strongest candidate, mocked in three places on the site: <b>P1</b> behind the hero title, '
               '<b>P2</b> behind the dark “What it sounds like” band with demo cards over it, and <b>P3</b> as a barely-there haze behind the white page body.</p>'
               '<p class="muted">Contrast is measured against the brightest pixel the layer reaches anywhere in the frame across a whole loop, so it is the worst case for text placed anywhere. The copy in the mock-ups is placeholder. '
               'The strength is the same per placement for every study (P1 ' + str(pl[0]["amt"]) + ', P2 ' + str(pl[1]["amt"]) + ', P3 ' + str(pl[2]["amt"]) + ' of the study pages).</p></header>')
    for st in meta["studies"]:
        v = st["best"]
        k = f"{st['id']}-{v}"
        out.append(f'<article class="place-study" id="place-{st["id"]}"><h3><span class="code">{code(st, v)}</span> {e(st["variants"][v]["name"])} in place</h3>'
                   f'<p>{e(st["variants"][v]["line"])}</p>')
        for p in pl:
            c = ct.get(f"{k}-{p['id']}", {})
            if p["mode"] == "dark":
                cline = (f'Over the brightest pixel: headings and titles (#f4f5fa) <b>{ratio(c.get("primary"))}</b>, '
                         f'secondary body text (#a4a7b9) <b>{ratio(c.get("secondary"))}</b> (AA body text needs 4.5:1).')
            else:
                cline = (f'Over the brightest (here: darkest) pixel: body text (#0b0c12) <b>{ratio(c.get("primary"))}</b>, '
                         f'secondary text (#5b5e6e) <b>{ratio(c.get("secondary"))}</b> (AA body text needs 4.5:1).')
            label = f'<span class="code">{code(st, v)} · {p["code"]}</span> {e(p["name"])}'
            if p["id"] == "hero":
                inner = ('<div class="copy"><p class="eyebrow">Firmware for the Noise Engineering Versio</p><h4>Sublustris Versio</h4>'
                         '<p class="lede">A huge, warm, lush reverb for the half-light.</p>'
                         '<div class="btns"><span class="btn">Download</span><span class="btn ghost">Listen</span></div></div>')
                cls = "mock mock-dark mock-hero"
            elif p["id"] == "band":
                cards = "".join(f'<div class="card"><div class="play"></div><div><div class="t">{t}</div><div class="m">{m}</div><div class="bar"></div></div></div>'
                                for t, m in (("Endless pad", "SIZE 4:00 · DECAY 11:30"), ("Bloom on a pluck", "BLOOM 2:00 · TONE 1:00"), ("Freeze and play over", "FREEZE · MIX 12:00")))
                inner = (f'<div><h4>What it sounds like</h4><p class="sub">Three short demos, dry first, then the reverb. The page’s mix fader blends them live.</p>'
                         f'<div class="cards">{cards}</div></div>')
                cls = "mock mock-dark mock-band"
            else:
                inner = ('<div><h4>Built for the Versio</h4><p class="body">Mock copy: one DSP core runs on the module, in the plugin and in the offline renderer, '
                         'so what you hear in the plugin is what the module plays.</p><p class="body">Secondary text sits here, in the lighter grey, to test the harder contrast case.</p>'
                         '<div class="tiles"><div class="tile"><b>Huge</b><span>From a small room to an endless hold.</span></div>'
                         '<div class="tile"><b>Warm</b><span>The highs fade first, the lows linger.</span></div></div></div>')
                cls = "mock mock-light"
            out.append(f'<div class="mock-wrap"><p class="ptitle">{label}</p><div class="{cls}">{live(st, v, p["mode"], p["amt"], "live")}{inner}</div>'
                       f'<p class="contrast">{cline}</p></div>')
        out.append("</article>")
    out.append("</section>")

    rows = []
    for st in meta["studies"]:
        for v in range(len(st["variants"])):
            k = f"{st['id']}-{v}"
            b = f"{st['id']}-{LETTERS[v].lower()}"
            t = tm.get(k, {})
            rows.append(f'<tr><td><span class="code">{code(st, v)}</span></td><td>{ms(t.get("half"))}</td><td>{ms(t.get("full"))}</td>'
                        f'<td>{t.get("raf_fps", "–")}</td><td>{kb(fsz.get(b + ".jpg"))}</td><td>{kb(fsz.get(b + ".mp4"))}</td><td>{kb(fsz.get(b + ".webm"))}</td></tr>')
    notes = (f'<section class="notes" id="notes"><h2>Notes and costs</h2><ul>'
             f'<li>Frame times are GPU draw times measured on this Mac ({e(gpu)}, Chrome) by drawing 120 frames and waiting for each to finish; the median is shown. '
             f'They are not a phone measurement: a mid-range phone is perhaps 5–10× slower, which is why the live versions render at half resolution and at most {meta["max_fps"]} fps.</li>'
             f'<li>“requestAnimationFrame held” is the frame rate the browser kept while one study ran full-width at 0.5× in a requestAnimationFrame loop for three seconds on this Mac.</li>'
             f'<li>Live versions pause when off screen, draw a single still frame under reduced motion (or with the Pause motion switch), and fall back to the still image without JavaScript or WebGL.</li>'
             f'<li>All thirty canvases share one WebGL context (browsers allow about sixteen).</li>'
             f'<li>Loops are captured from the same shaders, frame by frame, so live, still and loop match; every motion repeats exactly once per {loop_s} s, so the loops are seamless.</li>'
             f'<li>Total media on this page: {kb(man.get("total_bytes"))}.</li></ul>'
             f'<table><thead><tr><th>Option</th><th>0.5× hero</th><th>full res</th><th>rAF fps</th><th>still</th><th>MP4</th><th>WebM</th></tr></thead><tbody>{"".join(rows)}</tbody></table></section>')

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reverb Visual Studies</title>
<meta name="description" content="Abstract reverb visual studies for the Sublustris Versio site: live shaders, stills, loops and placements.">
<link rel="icon" href="data:,">
<link rel="stylesheet" href="page.css">
</head>
<body>
<div class="layout">
<aside class="index" aria-label="Index"><nav>{"".join(nav)}</nav></aside>
<main>
<header class="intro">
<h1>Reverb visual studies</h1>
<p>Six abstract pictures of what a reverb does, as quiet background layers for the Sublustris Versio site. Each idea has two options, shown three ways: the live shader, a still image and a short loop. Then each idea's strongest option is mocked behind the hero, the sound band and the white page.</p>
<p>Answer with the names: “<span class="code">haze A</span> in <span class="code">P1</span>, as a loop”, or “<span class="code">waves B</span>, but slower”. Notes only, nothing to click.</p>
<noscript><p class="facts">JavaScript is off: you are seeing the still frames, and the loops have play buttons.</p></noscript>
<p class="facts">Live versions render at half resolution and at most {meta["max_fps"]} fps, pause off screen, and hold still under reduced motion. Frame times are from this Mac ({e(gpu)}).</p>
</header>
{"".join(out)}
{notes}
</main>
</div>
<script src="renderer.js"></script>
<script src="studies.js"></script>
<script src="page.js"></script>
</body>
</html>
"""


CAPTURE = """<!doctype html>
<html><head><meta charset="utf-8"><title>capture</title></head>
<body style="margin:0;background:#000">
<script src="renderer.js"></script>
<script src="studies.js"></script>
<script>
  window.R = new IVRenderer({ preserve: true });
  document.body.appendChild(R.canvas);
  window.frame = function (key, w, h, ph, light, amt, type, q) {
    R.draw(key, IV_STUDIES.src[key], w, h, ph, light, amt, true);
    return R.canvas.toDataURL(type || 'image/png', q);
  };
  // brightest (dark) or darkest (light) pixel over n phases, as sRGB 0..255
  window.extreme = function (key, w, h, light, amt, n) {
    var gl = R.gl, px = new Uint8Array(w * h * 4), best = null, bestL = light ? 2 : -1;
    function lin(c) { c /= 255; return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4); }
    for (var i = 0; i < n; i++) {
      R.draw(key, IV_STUDIES.src[key], w, h, i / n * Math.PI * 2, light, amt, true);
      gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, px);
      for (var j = 0; j < px.length; j += 4) {
        var L = 0.2126 * lin(px[j]) + 0.7152 * lin(px[j + 1]) + 0.0722 * lin(px[j + 2]);
        if (light ? L < bestL : L > bestL) { bestL = L; best = [px[j], px[j + 1], px[j + 2]]; }
      }
    }
    return { L: bestL, rgb: best };
  };
  window.timeit = function (key, w, h, n) {
    var gl = R.gl, one = new Uint8Array(4), ts = [];
    for (var i = 0; i < n + 10; i++) {
      var t0 = performance.now();
      R.draw(key, IV_STUDIES.src[key], w, h, i * 0.05, 0, 1, true);
      gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, one);
      var t1 = performance.now();
      if (i >= 10) ts.push(t1 - t0);
    }
    ts.sort(function (a, b) { return a - b; });
    return ts[Math.floor(ts.length / 2)];
  };
  // run one study in a rAF loop at w x h (shared-renderer path, blit to a 2D canvas) and report fps
  window.rafRun = function (key, w, h, seconds) {
    return new Promise(function (res) {
      var shared = new IVRenderer();
      var c = document.createElement('canvas'); c.width = w; c.height = h; document.body.appendChild(c);
      var ctx = c.getContext('2d', { alpha: false }), n = 0, t0 = 0;
      function f(now) {
        if (!t0) t0 = now;
        shared.draw(key, IV_STUDIES.src[key], w, h, (now / 1000 % 10) / 10 * Math.PI * 2, 0, 1, false);
        shared.blit(ctx, w, h);
        n++;
        if (now - t0 < seconds * 1000) requestAnimationFrame(f); else { c.remove(); res(n / ((now - t0) / 1000)); }
      }
      requestAnimationFrame(f);
    });
  };
</script>
</body></html>
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    a = ap.parse_args()
    out = a.out
    (out / "media").mkdir(parents=True, exist_ok=True)
    meta, src = load()
    js_meta = {k: meta[k] for k in ("loop_seconds", "fps", "still_phase", "render_scale", "max_fps")}
    js_meta["src"] = src
    (out / "studies.js").write_text("window.IV_STUDIES = " + json.dumps(js_meta) + ";\n")
    for f in ("renderer.js", "page.js", "page.css"):
        shutil.copyfile(HERE / f, out / f)
    (out / "capture.html").write_text(CAPTURE)
    mf = out / "media" / "manifest.json"
    man = json.loads(mf.read_text()) if mf.exists() else {}
    (out / "index.html").write_text(page(meta, man))
    print(f"built {out / 'index.html'}")


if __name__ == "__main__":
    main()
