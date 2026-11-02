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
import math
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = Path("/Users/jesse/Documents/Sites/sublustris-versio/renders/site_visuals")


def options(meta):
    for st in meta["studies"]:
        for o in st["options"]:
            yield st, o


def prog(o):
    return f"{o['shader']}-{o['V']}"


def load():
    meta = json.loads((HERE / "studies.json").read_text())
    common = (HERE / "shaders" / "common.glsl").read_text()
    still = meta["still_phase"] * 2 * math.pi
    src = {}
    for _, o in options(meta):
        body = (HERE / "shaders" / f"{o['shader']}.glsl").read_text()
        src[prog(o)] = f"#define V {o['V']}\n#define STILL_PH {still:.6f}\n" + common + "\n" + body
    return meta, src


def kb(n):
    if n is None:
        return "–"
    return f"{n / 1024:.0f} KB" if n < 1024 * 1024 else f"{n / 1048576:.2f} MB"


def ms(x):
    return "–" if x is None else f"{x:.1f} ms"


def ratio(x):
    return "–" if x is None else f"{x:.1f}:1"


def live(o, amt, cls="frame live"):
    return (f'<div class="{cls}" data-live data-option="{o["key"]}" data-amt="{amt}">'
            f'<img class="fallback" src="media/{o["key"]}.jpg" alt="" loading="lazy" decoding="async" width="1280" height="720" style="opacity:{amt}">'
            f'<canvas aria-hidden="true"></canvas></div>')


def page(meta, man):
    e = html.escape
    fsz = man.get("files", {})
    tm = man.get("timing", {})
    ct = man.get("contrast", {})
    seam = man.get("seam", {})
    gpu = man.get("gpu", "this Mac")
    vinfo = man.get("video", {})

    def vid(k):
        v = vinfo.get(k)
        return f"{v[0]}×{v[1]}, {v[2]} fps" if v else "–"
    hw, hh = man.get("timing_size", [1440, 800])
    out, nav = [], []
    for i, st in enumerate(meta["studies"], 1):
        nav.append(f'<a href="#{st["id"]}">{i} {e(st["title"])}</a>')
        nav.append('<span class="sub">' + "<br>".join(e(o["code"]) for o in st["options"]) + "</span>")
    n = len(meta["studies"]) + 1
    nav.append(f'<a href="#placements">{n} Placements</a><span class="sub">P1 hero · P2 sound band</span>')
    nav.append('<a href="#notes">Notes and costs</a>')
    nav.append('<button type="button" id="motion-toggle" aria-pressed="false">Pause motion</button>')

    for i, st in enumerate(meta["studies"], 1):
        out.append(f'<section class="study" id="{st["id"]}"><header class="study-head"><p class="num">{i:02d}</p>'
                   f'<h2>{e(st["title"])}</h2><p class="line">{e(st["line"])}</p></header>')
        for o in st["options"]:
            k = o["key"]
            t = tm.get(k, {})
            cost = (f'<b>Live shader</b> · {ms(t.get("half"))} per frame for a full-width hero ({hw}×{hh} px, i.e. 0.5× on a Retina screen); '
                    f'{ms(t.get("full"))} at full resolution · requestAnimationFrame held {t.get("raf_fps", "–")} fps · measured on this Mac ({e(gpu)}). '
                    f'Live time only runs forward, so the live version never repeats.')
            out.append(f'<div class="variant" id="{k}"><h3><span class="code">{e(o["code"])}</span> {e(o["name"])}</h3>'
                       f'<p>{e(o["line"])}</p>'
                       f'<figure>{live(o, 1)}<figcaption>{cost}</figcaption></figure>'
                       f'<div class="pair">'
                       f'<figure><div class="frame"><img src="media/{k}.jpg" alt="Still frame of {e(o["code"])}" loading="lazy" decoding="async" width="1280" height="720"></div>'
                       f'<figcaption><b>Still</b> · JPEG 1280×720 · {kb(fsz.get(k + ".jpg"))}</figcaption></figure>'
                       f'<figure><div class="frame"><video data-loop muted loop playsinline preload="none" controls poster="media/{k}.jpg" aria-label="Loop of {e(o["code"])}">'
                       f'<source src="media/{k}.webm" type="video/webm"><source src="media/{k}.mp4" type="video/mp4"></video></div>'
                       f'<figcaption><b>Loop</b> · {o["loop"]} s seamless, {vid(k)}, no sound · MP4 (H.264) {kb(fsz.get(k + ".mp4"))} · WebM (VP9) {kb(fsz.get(k + ".webm"))}</figcaption></figure>'
                       f'</div></div>')
        out.append("</section>")

    pl = meta["placements"]
    out.append(f'<section id="placements"><header><p class="num muted" style="font:500 13px/1 var(--mono);margin-bottom:10px">{n:02d}</p><h2>Placements</h2>'
               '<p>Every option above, mocked behind the two dark sections: <b>P1</b> behind the hero title and <b>P2</b> behind the “What it sounds like” band with demo cards over it.</p>'
               '<p class="muted">Contrast is measured against the brightest pixel the layer reaches anywhere in the frame across its whole loop, so it is the worst case for text placed anywhere. '
               f'Strength per placement: P1 {pl[0]["amt"]}, P2 {pl[1]["amt"]} of the study pages (times the option\'s own brightness step). The copy in the mock-ups is placeholder.</p></header>')
    cards = "".join(f'<div class="card"><div class="play"></div><div><div class="t">{t}</div><div class="m">{m}</div><div class="bar"></div></div></div>'
                    for t, m in (("Endless pad", "SIZE 4:00 · DECAY 11:30"), ("Bloom on a pluck", "BLOOM 2:00 · TONE 1:00"), ("Freeze and play over", "FREEZE · MIX 12:00")))
    for st, o in options(meta):
        k = o["key"]
        out.append(f'<article class="place-study" id="place-{k}"><h3><span class="code">{e(o["code"])}</span> {e(o["name"])} in place</h3>')
        for p in pl:
            c = ct.get(f"{k}-{p['id']}", {})
            ok = c.get("secondary") is not None and c["secondary"] >= 4.5
            cline = (f'Over the brightest pixel: headings and titles (#f4f5fa) <b>{ratio(c.get("primary"))}</b>, '
                     f'secondary body text (#a4a7b9) <b>{ratio(c.get("secondary"))}</b> '
                     + ("(passes AA 4.5:1)." if ok else "(below AA 4.5:1)." if c else "."))
            label = f'<span class="code">{e(o["code"])} · {p["code"]}</span> {e(p["name"])}'
            if p["id"] == "hero":
                inner = ('<div class="copy"><p class="eyebrow">Firmware for the Noise Engineering Versio</p><h4>Sublustris Versio</h4>'
                         '<p class="lede">A huge, warm, lush reverb for the half-light.</p>'
                         '<div class="btns"><span class="btn">Download</span><span class="btn ghost">Listen</span></div></div>')
                cls = "mock mock-dark mock-hero"
            else:
                inner = (f'<div><h4>What it sounds like</h4><p class="sub">Three short demos, dry first, then the reverb. The page’s mix fader blends them live.</p>'
                         f'<div class="cards">{cards}</div></div>')
                cls = "mock mock-dark mock-band"
            out.append(f'<div class="mock-wrap"><p class="ptitle">{label}</p><div class="{cls}">{live(o, p["amt"], "live")}{inner}</div>'
                       f'<p class="contrast">{cline}</p></div>')
        out.append("</article>")
    out.append("</section>")

    rows = []
    for st, o in options(meta):
        k = o["key"]
        t = tm.get(k, {})
        c1 = ct.get(f"{k}-hero", {})
        c2 = ct.get(f"{k}-band", {})
        rows.append(f'<tr><td><span class="code">{e(o["code"])}</span></td><td>{ms(t.get("half"))}</td><td>{ms(t.get("full"))}</td>'
                    f'<td>{t.get("raf_fps", "–")}</td><td>{o["loop"]} s</td><td>{kb(fsz.get(k + ".jpg"))}</td><td>{kb(fsz.get(k + ".mp4"))}</td><td>{kb(fsz.get(k + ".webm"))}</td>'
                    f'<td>{ratio(c1.get("secondary"))}</td><td>{ratio(c2.get("secondary"))}</td></tr>')
    notes = (f'<section class="notes" id="notes"><h2>Notes and costs</h2><ul>'
             f'<li>Motion never runs backwards. The noise lives in four dimensions: two for space, two for time. Live, time moves along a straight line through that time plane, so the live version never repeats. '
             f'The loop files go once round a wide circle in the same plane, so they close seamlessly without a crossfade and without a back-and-forth; with a long loop (30–60 s) the repeat is hard to spot. '
             f'Waves A is the exception: its rings simply travel outward, three whole wavelengths per 10 s loop, through a still “medium” that breaks them up.</li>'
             f'<li>Frame times are GPU draw times measured on this Mac ({e(gpu)}, Chrome): 120 frames, each waited for, median shown. '
             f'Not a phone measurement: a mid-range phone is perhaps 5–10× slower, which is why the live versions render at half resolution and at most {meta["max_fps"]} fps.</li>'
             f'<li>“requestAnimationFrame held” is the frame rate the browser kept while one option ran full-width at 0.5× in a requestAnimationFrame loop for three seconds on this Mac.</li>'
             f'<li>Live versions pause when off screen, start on the same frame as the still, draw only that still frame under reduced motion (or with the Pause motion switch), and fall back to the still image without JavaScript or WebGL.</li>'
             f'<li>Total media on this page: {kb(man.get("total_bytes"))}.</li></ul>'
             f'<table><thead><tr><th>Option</th><th>0.5× hero</th><th>full res</th><th>rAF fps</th><th>loop</th><th>still</th><th>MP4</th><th>WebM</th><th>P1 text</th><th>P2 text</th></tr></thead><tbody>{"".join(rows)}</tbody></table>'
             f'<p class="muted" style="font-size:14px;margin-top:8px">P1/P2 text: secondary body text (#a4a7b9) over the brightest pixel.</p></section>')

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reverb Visual Studies</title>
<meta name="description" content="Abstract reverb visual studies for the Sublustris Versio site, round 2: live shaders, stills, loops and placements.">
<link rel="icon" href="data:,">
<link rel="stylesheet" href="page.css">
</head>
<body>
<div class="layout">
<aside class="index" aria-label="Index"><nav>{"".join(nav)}</nav></aside>
<main>
<header class="intro">
<h1>Reverb visual studies, round 2</h1>
<p>The five studies you kept, refined from your notes: diffusion B slowed right down with no back-and-forth, waves A and B dissolving as they travel, and haze A and B brighter. Each is shown as the live shader, a still and a loop, then mocked behind the hero and the sound band.</p>
<p>Where a step was useful there are numbered versions (<span class="code">diffusion B · slow 1</span> / <span class="code">slow 2</span>, <span class="code">haze A · brighter 1</span> / <span class="code">brighter 2</span>). Answer with the names. Notes only, nothing to click.</p>
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
  function U(key, ph, amt) { var o = IV_STUDIES.options[key]; return { ph: ph, t: 0, loopT: o.loop, live: 0, amt: amt * o.gain }; }
  function P(key) { return IV_STUDIES.options[key].prog; }
  window.frame = function (key, w, h, ph, amt) {
    R.draw(P(key), IV_STUDIES.src[P(key)], w, h, U(key, ph, amt), true);
    return R.canvas.toDataURL('image/png');
  };
  // brightest pixel over n loop phases (relative luminance and sRGB 0..255)
  window.extreme = function (key, w, h, amt, n) {
    var gl = R.gl, px = new Uint8Array(w * h * 4), best = null, bestL = -1;
    function lin(c) { c /= 255; return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4); }
    for (var i = 0; i < n; i++) {
      R.draw(P(key), IV_STUDIES.src[P(key)], w, h, U(key, i / n * Math.PI * 2, amt), true);
      gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, px);
      for (var j = 0; j < px.length; j += 4) {
        var L = 0.2126 * lin(px[j]) + 0.7152 * lin(px[j + 1]) + 0.0722 * lin(px[j + 2]);
        if (L > bestL) { bestL = L; best = [px[j], px[j + 1], px[j + 2]]; }
      }
    }
    return { L: bestL, rgb: best };
  };
  window.timeit = function (key, w, h, n) {
    var gl = R.gl, one = new Uint8Array(4), ts = [];
    for (var i = 0; i < n + 10; i++) {
      var t0 = performance.now();
      R.draw(P(key), IV_STUDIES.src[P(key)], w, h, U(key, i * 0.05, 1), true);
      gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, one);
      var t1 = performance.now();
      if (i >= 10) ts.push(t1 - t0);
    }
    ts.sort(function (a, b) { return a - b; });
    return ts[Math.floor(ts.length / 2)];
  };
  // run one option live (shared-renderer path, blit to a 2D canvas) in a rAF loop and report fps
  window.rafRun = function (key, w, h, seconds) {
    return new Promise(function (res) {
      var o = IV_STUDIES.options[key], shared = new IVRenderer();
      var c = document.createElement('canvas'); c.width = w; c.height = h; document.body.appendChild(c);
      var ctx = c.getContext('2d', { alpha: false }), n = 0, t0 = 0;
      function f(now) {
        if (!t0) t0 = now;
        var s = (now - t0) / 1000;
        shared.draw(o.prog, IV_STUDIES.src[o.prog], w, h, { ph: (s % o.loop) / o.loop * Math.PI * 2, t: s, loopT: o.loop, live: 1, amt: o.gain }, false);
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
    js = {k: meta[k] for k in ("fps", "still_phase", "render_scale", "max_fps")}
    js["options"] = {o["key"]: {"prog": prog(o), "loop": o["loop"], "gain": o["gain"]} for _, o in options(meta)}
    js["src"] = src
    (out / "studies.js").write_text("window.IV_STUDIES = " + json.dumps(js) + ";\n")
    for f in ("renderer.js", "page.js", "page.css"):
        shutil.copyfile(HERE / f, out / f)
    (out / "capture.html").write_text(CAPTURE)
    mf = out / "media" / "manifest.json"
    man = json.loads(mf.read_text()) if mf.exists() else {}
    (out / "index.html").write_text(page(meta, man))
    print(f"built {out / 'index.html'}")


if __name__ == "__main__":
    main()
