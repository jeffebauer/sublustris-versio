#!/usr/bin/env python3
"""renders/decay_curve: where DECAY's long tails and endless sit on the knob (a pick page, A/B/C/D).
Page built from a1701d9 (off v1.0.70), whose Renderer has IV_DECAY_CURVE; v1.0.72 ported C and the hook is gone.

The owner on the module (5 Nov 2026): "the decay seems to bias towards shorter decays until noon, then 12-3 controls
longer decays, which is a small zone to dial in longer decays with precision ... is there any granularity in effect
past 3? ... Should we shift endless to fully CW or last 'hour' of the knob?" The versions (core/params/Mappings.h
decayCurveKnob, the Renderer's IV_DECAY_CURVE; tools/pages/decay_curve_table.py prints the T60 per hour):

  A_today       finite to 3 o'clock, endless from about 3:30 (today)
  B_stretched   today's curve stretched: endless from 4:30                                    IV_DECAY_CURVE=B
  C_bent        B, bent so noon stays where it is and the long tails get more knob           IV_DECAY_CURVE=C
  D_late        C's bend, endless only from about 4:50                                        IV_DECAY_CURVE=D

Cards: HALL (SIZE full, its nominal 0.8-60 s) and CANYON (SIZE noon) at 9, 11, noon, 1, 2, 3, 4 o'clock and fully CW,
each with the pad, the stabs and the piano (their first 7 s, then 18 s of tail); then one slow DECAY sweep per SPACE
(7 o'clock to fully CW over 30 s, chords every 4 s, then 12 s held at fully CW), and CANYON's again at full SIZE. 100 % wet (--wet-only, the page's MIX
fader blends the dry in). NOT level-matched: the length is the question. Writes 16-bit WAVs to fit the disk budget.
Usage: <venv>/bin/python tools/pages/decay_curve.py  (IV_RENDER: the prototype's iv_render) -> MAIN/renders/decay_curve
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decay_curve_table as dt
import motion_radio_checks as mc

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
MAIN = REPO_.split("/.claude/worktrees/")[0]
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
PAGE = os.environ.get("DC_PAGE", MAIN + "/renders/decay_curve")
SRC = MAIN + "/renders/_material"
SR = 48000
MATERIALS = ["pad", "stabs", "piano"]
PLAY_S, TAIL_S = 7.0, 18.0
VERS = [("A_today", "A"), ("B_stretched", "B"), ("C_bent", "C"), ("D_late", "D")]
BASE = ["time=0.3", "drift=0.5", "era=0.3", "tone=0.5"]
SPACES = {"HALL": ["space=HALL", "size=1"], "CANYON": ["space=CANYON", "size=0.5"], "PLATE": ["space=PLATE", "size=0.6"],
          "CANYON_SIZE1": ["space=CANYON", "size=1"]}
ONLY = os.environ.get("DC_ONLY")  # a card prefix, e.g. "20_": render just that card into the existing page
CLOCKS = [("9oclock", 0.2), ("11oclock", 0.4), ("noon", 0.5), ("1oclock", 0.6), ("2oclock", 0.7), ("3oclock", 0.8),
          ("4oclock", 0.9), ("fullCW", 1.0)]


def trimmed(m):
    x, sr = sf.read(f"{SRC}/{m}.wav", always_2d=True)
    n, f = int(PLAY_S * SR), int(0.3 * SR)
    y = x[:n].copy()
    y[-f:] *= np.linspace(1, 0, f)[:, None]
    return np.concatenate([y, np.zeros((int(TAIL_S * SR), 2))])


def sweep_pad():
    """Chords of 2 s every 4 s for 32 s (mc.pad's voice, a new chord each time), then 12 s of silence."""
    rng = [(48, 51, 55, 58, 62), (46, 50, 53, 57, 60), (44, 48, 51, 55, 58), (43, 47, 50, 53, 58)]
    out = np.zeros((int(44 * SR), 2))
    t = np.arange(int(2 * SR)) / SR
    for k in range(8):
        y = np.zeros_like(t)
        for m in rng[k % 4]:
            for c in (-3.0, 3.0):
                f = 440.0 * 2 ** ((m - 69) / 12 + c / 1200)
                y += 2 * ((f * t + 0.37 * m) % 1.0) - 1
        Y = np.fft.rfft(y); fr = np.fft.rfftfreq(len(y), 1 / SR)
        y = np.fft.irfft(Y / np.sqrt(1 + (fr / 1800.0) ** 4), len(y))
        env = np.minimum(1, np.minimum(t / 0.15, (2 - t) / 0.4))
        y *= env
        s = int((0.5 + 4 * k) * SR)
        out[s:s + len(y)] += y[:, None]
    return out * 10 ** (-18 / 20) / np.sqrt(np.mean(out[out != 0] ** 2))


def render(src, out, sets, ver, auto=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env["IV_DECAY_CURVE"] = ver
    args = [R, src, out, "--set", "mix=1"]
    for s in BASE + sets:
        args += ["--set", s]
    if auto:
        af = out + ".auto.json"
        json.dump({"breakpoints": [{"t": t, "key": k, "value": v} for t, k, v in auto]}, open(af, "w"))
        args += ["--auto", af]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if auto:
        os.unlink(af)
    if p.returncode != 0 or f"IV_DECAY_CURVE={ver}" not in p.stderr:
        sys.exit("render failed or hook missing: {}\n{}".format(" ".join(args), p.stderr))
    y = sf.read(out, always_2d=True, dtype="float32")[0]
    return y


def tail_db(y, t0, t1):
    return 10 * np.log10(np.mean(y[int(t0 * SR):int(t1 * SR)].astype(np.float64) ** 2) + 1e-30)


if __name__ == "__main__":
    if os.path.isdir(PAGE) and not ONLY:
        shutil.rmtree(PAGE)
    os.makedirs(PAGE + "/_dry", exist_ok=True)
    for m in MATERIALS:
        sf.write(f"{PAGE}/_dry/{m}.wav", trimmed(m), SR, subtype="PCM_16")
    sf.write(f"{PAGE}/_dry/sweep.wav", sweep_pad(), SR, subtype="PCM_16")
    jobs = []
    card = 0
    for sp in ("HALL", "CANYON"):
        for clock, d in CLOCKS:
            card += 1
            col = f"{card:02d}_{sp.lower()}_{clock}"
            for m in MATERIALS:
                for tag, ver in VERS:
                    jobs.append((col, m, tag, ver, SPACES[sp] + [f"decay={d}"], None))
    for sp in ("PLATE", "HALL", "CANYON", "CANYON_SIZE1"):
        card += 1
        col = f"{card:02d}_{sp.lower()}_sweep"
        auto = [(0, "decay", 0.0), (2, "decay", 0.0), (32, "decay", 1.0), (44, "decay", 1.0)]
        for tag, ver in VERS:
            jobs.append((col, "sweep", tag, ver, SPACES[sp] + ["decay=0"], auto))

    def run(j):
        col, m, tag, ver, sets, auto = j
        os.makedirs(f"{PAGE}/{col}", exist_ok=True)
        path = f"{PAGE}/{col}/{m}_{tag}.wav"
        tmp = path + ".f32.wav"
        sf.write(tmp, sf.read(f"{PAGE}/_dry/{m}.wav", always_2d=True)[0], SR, subtype="FLOAT")
        y = render(tmp, tmp.replace(".f32.wav", ".out.wav"), sets, ver, auto)
        os.unlink(tmp); os.unlink(tmp.replace(".f32.wav", ".out.wav"))
        pk = float(np.max(np.abs(y)))
        sf.write(path, np.clip(y, -1, 1), SR, subtype="PCM_16")
        return j, y, pk

    if ONLY:
        jobs = [j for j in jobs if j[0].startswith(ONLY)]
    with ThreadPoolExecutor(max_workers=8) as ex:
        res = list(ex.map(run, jobs))
    lines = ["renders/decay_curve: verification. Per render: sha1 of the float samples, peak dBFS, the tail's level 2-4 s",
             "and 14-16 s after the playing stops (dB), and the worst 1 ms click frame (> 3 kHz, dB over its neighbours).",
             "Not level-matched.", ""]
    by = {}
    for (col, m, tag, ver, sets, auto), y, pk in res:
        by.setdefault((col, m), {})[tag] = y
        stop = 32.0 if m == "sweep" else PLAY_S
        sha = hashlib.sha1(np.ascontiguousarray(y).tobytes()).hexdigest()[:12]
        lines.append(f"{col:22s} {m:6s} {tag:12s} sha1 {sha}  peak {20 * np.log10(pk + 1e-30):6.1f}"
                     f"  tail +2-4 s {tail_db(y, stop + 2, stop + 4):6.1f}  +10-12 s {tail_db(y, stop + 10, stop + 12):6.1f}"
                     f"  click {mc.clicks(y.astype(np.float64)):5.1f}  {'clipped' if pk > 1 else ''}")
    lines.append("")
    for (col, m), d in sorted(by.items()):
        tags = list(d)
        same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(d[p], d[q])]
        if same:
            lines.append(f"{col} {m}: identical pairs {same}")
    if ONLY:  # keep the earlier cards' lines
        old = open(f"{PAGE}/_verify.txt").read().split("\n")[4:]
        lines = lines[:4] + [l for l in old if l and not l.startswith(ONLY)] + lines[4:]
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    readme = open(os.path.join(HERE, "decay_curve_README.txt")).read()
    open(f"{PAGE}/README.txt", "w").write(readme)
    subprocess.run([sys.executable, os.path.join(REPO_, "tools/review/make_review.py"), PAGE, "--title", "DECAY curve",
                    "--no-level-match", "--dry", PAGE + "/_dry", "--wet-only"], check=True)
    # The T60 table, the main thing to read, goes in at the top of the page.
    html = open(f"{PAGE}/index.html").read()
    table = subprocess.run([sys.executable, os.path.join(HERE, "decay_curve_table.py"), "--html"], capture_output=True, text=True).stdout
    block = ("<section style='max-width:1100px;margin:16px auto;padding:0 16px;font:14px/1.4 system-ui,sans-serif'>"
             "<h2 style='font-size:18px;margin:0 0 6px'>T60 (seconds) at each hour of DECAY</h2>"
             "<p style='margin:0 0 8px;opacity:.8'>7 o'clock is fully left, 5 o'clock fully right. HALL at full SIZE "
             "(smaller SIZEs shorten every HALL time by the same share); CANYON at SIZE noon (its shortest tail follows "
             "SIZE: about 2 s at SIZE 0, 12 s at full). The cards below use these settings. Not level-matched.</p>"
             "<style>.t60{border-collapse:collapse;font-variant-numeric:tabular-nums;width:100%;font-size:13px}"
             ".t60 td,.t60 th{border:1px solid color-mix(in srgb,currentColor 20%,transparent);padding:3px 5px;text-align:right}"
             ".t60 td:nth-child(-n+2),.t60 th:nth-child(-n+2){text-align:left}.t60 tr.vA td{opacity:.75}"
             ".t60wrap{overflow-x:auto}</style><div class='t60wrap'>" + table + "</div></section>")
    i = html.find("<body")
    j = html.find(">", i) + 1
    open(f"{PAGE}/index.html", "w").write(html[:j] + block + html[j:])
    print("Wrote", PAGE + "/index.html")
