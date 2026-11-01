"""renders/decay_endless_match: DECAY turned into and out of endless, today against endless following the playing.
Usage: IV_RENDER=<this branch's renderer> IV_RENDER_TODAY=<main's renderer> .venv/bin/python tools/pages/decay_endless_match.py
  -> renders/decay_endless_match in the main checkout (WAVs, _dry, traces, README, _verify.txt, index.html)
Needs renders/_material/voice.wav (tools/make_material.py voice); the rest is decay_cross.py's sources.
"""
# renders/decay_endless_match (2 Nov 2026; the owner on decay_blend: "the shifts in and out of endless shouldn't have
# these audible shifts and the transition between them should be imperceptible from an amplitude perspective"; picks:
# endless follows the playing, the crossing pace folded in). decay_cross's timeline, sources and chapter bar, each
# source played quiet and hot (the owner heard the step flip with the input level), and each card's level trace.

import json, os, shutil, subprocess, sys
import numpy as np, soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
MAIN = REPO_.split("/.claude/worktrees/")[0]   # pages land in the main checkout's renders/
os.environ.setdefault("DC_VOICE", MAIN + "/renders/_material/voice.wav")
sys.path.insert(0, HERE)
import decay_cross as dc

R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
R_TODAY = os.environ.get("IV_RENDER_TODAY", R)
PAGE = os.environ.get("DEM_PAGE", MAIN + "/renders/decay_endless_match")
SR, SIZE, LENGTH = dc.SR, dc.SIZE, dc.LENGTH
QUIET_DB = -12.0   # quiet: the same take 12 dB down (active RMS about -26 dBFS)
# (tag, renderer, env)
VERS = [("A_today", R_TODAY, {}),
        ("B_paced", R, {"IV_ENDLESS_MATCH": "0"}),
        ("C_follows", R, {"IV_ENDLESS_MATCH": "1", "IV_DECAY_CROSS": "0.0001"}),
        ("D_follows_paced", R, {"IV_ENDLESS_MATCH": "1"})]
LABELS = {"A_today": "A today", "B_paced": "B today + 3 s", "C_follows": "C follows", "D_follows_paced": "D follows + 3 s"}
TURNS = [("into", dc.T_IN), ("out", dc.T_OUT), ("into tail", dc.T_IN2), ("out tail", dc.T_OUT2)]
SRC = {m: (d, f) for m, d, f in dc.SOURCES}
ROWS = [(f"{i + 1:02d}_{m}_{lv}", m, lv, "HALL") for i, (m, lv) in enumerate((m, lv) for m, _, _ in dc.SOURCES for lv in ("quiet", "hot"))]
ROWS += [("13_bright_chords_hot_plate", "5_bright_chords", "hot", "PLATE"), ("14_low_synth_hot_massive", "2_low_synth", "hot", "MASSIVE")]
COLOURS = ["#6b7280", "#c2410c", "#2563eb", "#059669"]


def render(R, dry, space, env, out):
    e = dict(os.environ); e.update(env)
    a = out + ".auto.json"; json.dump(dc.auto(), open(a, "w"))
    subprocess.run([R, dry, out, "--set", "mix=1", "--set", f"space={space}", "--set", f"size={SIZE}", "--set", f"decay={dc.FIN0}",
                    "--auto", a, "--sidecar"], env=e, check=True, capture_output=True)
    os.remove(a)
    x = sf.read(out)[0]; sf.write(out, x, SR, subtype="PCM_24")
    return x


def held(R, dry, space, env, t0, out):
    """The control: the same render with DECAY left where it was at t0."""
    e = dict(os.environ); e.update(env)
    a = out + ".auto.json"
    json.dump({"breakpoints": [p for p in dc.auto()["breakpoints"] if p["t"] <= t0 + 1e-9]}, open(a, "w"))
    subprocess.run([R, dry, out, "--set", "mix=1", "--set", f"space={space}", "--set", f"size={SIZE}", "--set", f"decay={dc.FIN0}",
                    "--auto", a], env=e, check=True, capture_output=True)
    os.remove(a)
    x = sf.read(out)[0]; os.remove(out)
    return x


def svg_trace(curves, title):
    """The wet's level over the card's timeline, one line per version."""
    W, H, l, r, t, b = 960, 230, 44, 10, 22, 26
    lo, hi = -60.0, -6.0
    X = lambda s: l + (W - l - r) * s / LENGTH
    Y = lambda d: t + (H - t - b) * (hi - min(max(d, lo), hi)) / (hi - lo)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui,sans-serif" font-size="12">',
           f'<rect width="{W}" height="{H}" fill="#fff"/>', f'<text x="{l}" y="14" fill="#333">{title}</text>']
    for d in range(-60, -5, 10):
        out.append(f'<line x1="{l}" x2="{W - r}" y1="{Y(d):.1f}" y2="{Y(d):.1f}" stroke="#e5e5e5"/>'
                   f'<text x="{l - 6}" y="{Y(d) + 4:.1f}" text-anchor="end" fill="#777">{d}</text>')
    for name, s in dc.CHAPTERS[1:]:
        out.append(f'<line x1="{X(s):.1f}" x2="{X(s):.1f}" y1="{t}" y2="{H - b}" stroke="#999" stroke-dasharray="3 3"/>'
                   f'<text x="{X(s) + 3:.1f}" y="{H - b + 14}" fill="#555">{name}</text>')
    for k, (label, L, w) in enumerate(curves):
        pts = " ".join(f"{X(i * 0.05):.1f},{Y(v):.1f}" for i, v in enumerate(L) if i % 2 == 0)
        out.append(f'<polyline fill="none" stroke="{COLOURS[k % 4]}" stroke-width="{w}" points="{pts}"/>')
        out.append(f'<text x="{W - r - 4}" y="{t + 14 + 14 * k}" text-anchor="end" fill="{COLOURS[k % 4]}">{label}</text>')
    out.append(f'<text x="{l}" y="{H - 4}" fill="#777">the wet alone, dBFS (50 ms RMS, smoothed over 200 ms); dashed: the chapter marks</text></svg>')
    return "\n".join(out)


if __name__ == "__main__":
    if os.path.isdir(PAGE): shutil.rmtree(PAGE)
    os.makedirs(PAGE + "/_dry"); os.makedirs(PAGE + "/traces")
    lines = [f"SIZE {SIZE}; DECAY {dc.FIN0} -> {dc.ENDLESS} at {dc.T_IN:.0f} s, -> {dc.FIN1} at {dc.T_OUT:.0f} s, input stops at "
             f"{dc.T_STOP:.0f} s, -> {dc.ENDLESS} at {dc.T_IN2:.0f} s, -> {dc.FIN1} at {dc.T_OUT2:.0f} s (each turn {dc.TURN} s). 100 % wet.",
             "Per turn: the step against a control with DECAY held (mean 0.25-0.75 s after the turn, dB); the click score over its 6 s."]
    names = {}
    for mat, src, lv, space in ROWS:
        dry = dc.finish(SRC[src][1]()) * (10 ** (QUIET_DB / 20) if lv == "quiet" else 1.0)
        dpath = f"{PAGE}/_dry/{mat}.wav"; sf.write(dpath, dry, SR, subtype="PCM_24")
        names[mat] = f"{SRC[src][0]}, played {lv}" + ("" if space == "HALL" else f" — {space}")
        lines.append(f"\n{mat}  ({space}, {names[mat]})")
        outs, curves = {}, []
        for tag, Rv, env in VERS:
            x = render(Rv, dpath, space, env, f"{PAGE}/{mat}_{tag}.wav"); outs[tag] = x
            L = dc.levels(x); e = dc.hf_frames(x); parts = []
            curves.append((LABELS[tag], L, 2.2 if tag.startswith("D") else 1.4))
            for name, t0 in TURNS:
                h = dc.levels(held(Rv, dpath, space, env, t0, f"{PAGE}/_held.wav")); i0 = int(t0 / 0.05)
                parts.append(f"{name} {float((L - h)[i0 + 5:i0 + 15].mean()):+5.1f} click {dc.click_score(e, t0):4.1f}")
            lines.append(f"  {tag:16s} " + " | ".join(parts))
        a, j = outs["A_today"], int(dc.T_IN * SR)
        for tag, _, _ in VERS[1:]:
            b = outs[tag]
            lines.append(f"  {tag}: before the first turn {'bit-identical to A' if np.array_equal(a[:j], b[:j]) else 'DIFFERENT from A'}; "
                         f"after it {'IDENTICAL (mislabelled!)' if np.array_equal(a, b) else 'differs from A'}")
        tags = [t for t, _, _ in VERS]
        same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(outs[p], outs[q])]
        lines.append("  every pair of versions differs" if not same else f"  IDENTICAL PAIRS: {same}")
        open(f"{PAGE}/traces/{mat}.svg", "w").write(svg_trace(curves, names[mat]))
        print(*lines[-7:], sep="\n", flush=True)
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    shutil.copy(os.path.join(HERE, "decay_endless_match_README.txt"), PAGE + "/README.txt")

    # The page: decay_cross's layout (one chapter bar for every card, pick buttons), wet-only with the MIX fader at noon,
    # and each card's level trace under it.
    for mat, _, _, _ in ROWS:
        for tag, _, _ in VERS:
            p = f"{PAGE}/{mat}_{tag}.json"; d = json.load(open(p)); d["params"].pop("decay", None); json.dump(d, open(p, "w"), indent=2)
    sys.path.insert(0, os.path.join(REPO_, "tools/review"))
    import listen
    from pathlib import Path
    out_path = Path(PAGE) / "index.html"
    items, _, _ = listen.collect(Path(PAGE), with_spec=True)
    data = listen.build_page_data(Path(PAGE), out_path, items, [], rows=None, columns="none", variants=None,
                                  title="Endless at the tail's level", level_match=False, dry_dir=PAGE + "/_dry")
    order = []
    for row in data["rows"]:
        row["marks"] = [[n, t] for n, t in dc.CHAPTERS]
        row["loud"] = [t for _, t in TURNS]
        order.append(row.get("material"))
        if row.get("material") in names: row["name"] = names[row["material"]]
    data["choices"] = True
    listen.add_hidden_twin(data)
    data["wetOnly"] = True
    listen.write_audio_sidecars(data, out_path.parent)
    html = listen.render_html(data)
    # Each card's level trace under it, found by the card's name (the hidden twin is one of the rows, anywhere).
    tmap = {names[m]: f"traces/{m}.svg" for m in order if m in names}
    inject = ("<script>(function(){var t=" + json.dumps(tmap) + ";"
              "function add(){var s=document.querySelectorAll('#rows > section.row');if(!s.length){return setTimeout(add,100);}"
              "s.forEach(function(e){var h=e.querySelector('h2'),src=h&&t[h.textContent.trim()];if(!src)return;"
              "var f=document.createElement('figure');f.style.margin='8px 0 0';"
              "var im=document.createElement('img');im.src=src;im.alt='level trace';im.style.width='100%';im.style.borderRadius='6px';"
              "f.appendChild(im);e.appendChild(f);});}"
              "if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',add);else add();})();</script>")
    out_path.write_text(html.replace("</body>", inject + "</body>"), encoding="utf-8")
    print("Wrote", out_path, listen.describe(data))
