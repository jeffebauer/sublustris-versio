"""renders/decay_blend: is the drop at DECAY's endless crossover in the reverb, or in the blend? Full mixes at BLEND
100 / 75 / 50 / 30 % (no level match), today's sound, plus one row with the crossover paced over 3 s.
Usage: IV_RENDER_TODAY=<main's iv_render> IV_RENDER=<a renderer with IV_DECAY_CROSS> .venv/bin/python tools/pages/decay_blend.py
Needs renders/_material/pad.wav and keys.wav (tools/make_material.py); the hits come from decay_cross.py.
"""
# renders/decay_blend (2 Nov 2026; the owner: "how much the dry/wet balance and curve is impacting my perception";
# docs/research/wet-dry-balance.md, option 1). A showcase: notes only.

import json, os, shutil, subprocess, sys
import numpy as np, soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decay_cross as dc  # noqa: E402  (the plucked hits, the level and click measures)

REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
R_TODAY = os.environ.get("IV_RENDER_TODAY", REPO_ + "/build/iv_render")
R_FIX = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
MATDIR = os.environ.get("DB_MATERIAL", REPO_ + "/renders/_material")
PAGE = os.environ.get("DB_PAGE", REPO_ + "/renders/decay_blend")
SR = 48000
SPACE, SIZE = "HALL", 0.9
T_IN, T_OUT, T_IN2, T_OUT2, T_STOP, LENGTH = 8.0, 16.0, 24.0, 30.0, 36.0, 48.0
CHAPTERS = [("Playing", 0.0), ("↑ into endless", T_IN), ("↓ out to 0.78", T_OUT), ("↑ into endless", T_IN2),
            ("↓ out to 0.7", T_OUT2), ("Stop playing", T_STOP)]
TURNS = [("into endless", T_IN), ("out to 0.78", T_OUT), ("into endless", T_IN2), ("out to 0.7", T_OUT2), ("stop playing", T_STOP)]
BLENDS = [("A_blend100", 1.0, "100 % (wet alone)"), ("B_blend75", 0.75, "75 %"), ("C_blend50", 0.5, "50 % (noon)"), ("D_blend30", 0.3, "30 %")]
LEVELS = {"quiet": -26.0, "hot": -14.0}  # active RMS, dBFS


def auto():
    pts = [(0, 0.78), (T_IN, 0.78), (T_IN + 0.5, 1.0), (T_OUT, 1.0), (T_OUT + 0.5, 0.78), (T_IN2, 0.78), (T_IN2 + 0.5, 1.0),
           (T_OUT2, 1.0), (T_OUT2 + 0.5, 0.7)]
    return {"breakpoints": [{"t": t, "key": "decay", "value": v} for t, v in pts]}


def looped(x, seconds):
    n = int(seconds * SR); f = SR // 2
    out = x[:n].copy()
    while len(out) < n:  # 0.5 s crossfade loops
        fi = np.linspace(0, 1, f)
        out = np.concatenate([out[:-f], out[-f:] * (1 - fi) + x[:f] * fi, x[f:]])
    return out[:n]


def source(name, rms_db):
    if name == "hits":
        y = dc.plucks(); y = np.concatenate([y, y])[: int(T_STOP * SR)]
    else:
        x, sr = sf.read(f"{MATDIR}/{name}.wav"); x = x.mean(1)
        x = x[: np.nonzero(np.abs(x) > 1e-4)[0][-1]]  # the phrase without its silence
        y = looped(x, T_STOP)
    f = SR // 2; y[-f:] *= np.linspace(1, 0, f)
    a = y[np.abs(y) > 1e-3]
    g = min(10 ** (rms_db / 20) / np.sqrt(np.mean(a ** 2)), 10 ** (-1 / 20) / np.abs(y).max())
    y = np.concatenate([y * g, np.zeros(int((LENGTH - T_STOP) * SR))])
    return np.stack([y, y], 1)


def render(R, mat, mix, out, env=None):
    e = dict(os.environ); e.update(env or {})
    a = out + ".auto.json"; json.dump(auto(), open(a, "w"))
    subprocess.run([R, f"{PAGE}/_dry/{mat}.wav", out, "--set", f"mix={mix}", "--set", f"space={SPACE}", "--set", f"size={SIZE}",
                    "--set", "decay=0.78", "--auto", a, "--sidecar"], env=e, check=True, capture_output=True)
    os.remove(a)
    x, _ = sf.read(out)
    sf.write(out, x, SR, subtype="PCM_24")  # a quarter smaller than the renderer's float
    j = out[:-4] + ".json"; d = json.load(open(j)); d["params"].pop("decay", None); json.dump(d, open(j, "w"), indent=2)
    return x


def step(L, t0):
    """dB: the 1 s after the turn's first 1.5 s against the 1 s before it; and the steepest 0.5 s fall in the 4 s after."""
    i0 = int(t0 / 0.05)
    lin = lambda a, b: 10 * np.log10(np.mean(10 ** (L[a:b] / 10)))
    return lin(i0 + 30, i0 + 50) - lin(i0 - 20, i0), min(L[j + 10] - L[j] for j in range(i0, i0 + 80))


COLOURS = ["#c2410c", "#2563eb", "#059669", "#7c3aed", "#6b7280"]


def svg_trace(curves, title):
    """Level traces (dB over time) as an SVG: the wet alone bold, then each full mix."""
    W, H, l, r, t, b = 960, 230, 44, 10, 22, 26
    lo, hi = -50.0, -6.0
    X = lambda s: l + (W - l - r) * s / LENGTH
    Y = lambda d: t + (H - t - b) * (hi - min(max(d, lo), hi)) / (hi - lo)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui,sans-serif" font-size="12">',
           f'<rect width="{W}" height="{H}" fill="#fff"/>', f'<text x="{l}" y="14" fill="#333">{title}</text>']
    for d in range(-50, -5, 10):
        out.append(f'<line x1="{l}" x2="{W - r}" y1="{Y(d):.1f}" y2="{Y(d):.1f}" stroke="#e5e5e5"/>'
                   f'<text x="{l - 6}" y="{Y(d) + 4:.1f}" text-anchor="end" fill="#777">{d}</text>')
    for name, s in CHAPTERS[1:]:
        out.append(f'<line x1="{X(s):.1f}" x2="{X(s):.1f}" y1="{t}" y2="{H - b}" stroke="#999" stroke-dasharray="3 3"/>'
                   f'<text x="{X(s) + 3:.1f}" y="{H - b + 14}" fill="#555">{name}</text>')
    for k, (label, L, w) in enumerate(curves):
        pts = " ".join(f"{X(i * 0.05):.1f},{Y(v):.1f}" for i, v in enumerate(L) if i % 2 == 0)
        out.append(f'<polyline fill="none" stroke="{COLOURS[k % 5]}" stroke-width="{w}" points="{pts}"/>')
        out.append(f'<text x="{W - r - 4}" y="{t + 14 + 14 * k}" text-anchor="end" fill="{COLOURS[k % 5]}">{label}</text>')
    out.append(f'<text x="{l}" y="{H - 4}" fill="#777">level, dBFS (50 ms RMS, smoothed over 200 ms); dashed: the chapter marks</text></svg>')
    return "\n".join(out)


if __name__ == "__main__":
    if os.path.isdir(PAGE): shutil.rmtree(PAGE)
    os.makedirs(PAGE + "/_dry"); os.makedirs(PAGE + "/traces")
    rows = [(f"{i + 1}_{m}_{lv}", m, lv, R_TODAY, None) for i, (m, lv) in
            enumerate([(m, lv) for m in ("pad", "keys", "hits") for lv in ("quiet", "hot")])]
    rows.append(("7_pad_hot_paced", "pad", "hot", R_FIX, {"IV_DECAY_CROSS": "3"}))
    names = {}
    lines = [f"{SPACE} SIZE {SIZE}; DECAY 0.78 -> endless at {T_IN:.0f} s, -> 0.78 at {T_OUT:.0f} s, -> endless at {T_IN2:.0f} s, "
             f"-> 0.7 at {T_OUT2:.0f} s (each turn 0.5 s); the playing stops at {T_STOP:.0f} s. Full mixes at each BLEND.",
             "Per turn: the level 1.5-2.5 s after against the 1 s before (dB), then the steepest 0.5 s fall in the 4 s after."]
    for mat, m, lv, R, env in rows:
        dry = source(m, LEVELS[lv]); sf.write(f"{PAGE}/_dry/{mat}.wav", dry, SR, subtype="PCM_24")
        names[mat] = f"{ {'pad': 'Pad', 'keys': 'Keys', 'hits': 'Plucked hits'}[m] }, played {lv}" + (" — with the 3 s crossing (C on decay_cross)" if env else "")
        curves = []; lines.append(f"\n{mat}  ({names[mat]})")
        outs = {}
        for tag, mix, label in BLENDS:
            x = render(R, mat, mix, f"{PAGE}/{mat}_{tag}.wav", env)
            outs[tag] = x; L = dc.levels(x)
            curves.append((("wet alone" if mix == 1 else f"mix at BLEND {label}"), L, 2.4 if mix == 1 else 1.4))
            e = dc.hf_frames(x)
            parts = []
            for name, t0 in TURNS:
                s, f = step(L, t0)
                parts.append(f"{name} {s:+5.1f} ({f:+4.1f})")
            clicks = max(dc.click_score(e, t0) for _, t0 in TURNS[:4])
            lines.append(f"  {label:18s} " + " | ".join(parts) + f" | worst 1 ms click score {clicks:4.1f}")
        # the dry alone, for the trace
        curves.append(("the dry alone (input)", dc.levels(dry), 1.0))
        tags = [t for t, _, _ in BLENDS]
        same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(outs[p], outs[q])]
        lines.append("  every BLEND rendered differently" if not same else f"  IDENTICAL PAIRS: {same}")
        open(f"{PAGE}/traces/{mat}.svg", "w").write(svg_trace(curves, names[mat]))
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), "decay_blend_README.txt"), PAGE + "/README.txt")
    print("\n".join(lines))

    sys.path.insert(0, os.path.join(REPO_, "tools/review"))
    import listen
    from pathlib import Path
    out_path = Path(PAGE) / "index.html"
    items, _, _ = listen.collect(Path(PAGE), with_spec=True)
    data = listen.build_page_data(Path(PAGE), out_path, items, [], rows=None, columns="none", variants=None,
                                  title="DECAY's crossing, through the blend", level_match=False, dry_dir=None)
    order = []
    for row in data["rows"]:
        row["marks"] = [[n, t] for n, t in CHAPTERS]
        row["loud"] = [t for _, t in TURNS]
        order.append(row.get("material"))
        if row.get("material") in names: row["name"] = names[row["material"]]
    data["choices"] = False  # a showcase: notes only
    data["wetOnly"] = False   # full mixes: no MIX fader
    listen.write_audio_sidecars(data, out_path.parent)
    html = listen.render_html(data)
    # Each card's level trace under it (the rows are built in DATA's order).
    inject = ("<script>(function(){var t=" + json.dumps([f"traces/{m}.svg" for m in order]) + ";"
              "function add(){var s=document.querySelectorAll('#rows > section.row');if(!s.length){return setTimeout(add,100);}"
              "s.forEach(function(e,i){if(!t[i])return;var f=document.createElement('figure');f.style.margin='8px 0 0';"
              "var im=document.createElement('img');im.src=t[i];im.alt='level trace';im.style.width='100%';im.style.borderRadius='6px';"
              "f.appendChild(im);e.appendChild(f);});}"
              "if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',add);else add();})();</script>")
    html = html.replace("</body>", inject + "</body>")
    out_path.write_text(html, encoding="utf-8")
    print("Wrote", out_path, listen.describe(data))
