#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; the owner on era_switch: keep the overlaps, "although I feel we could
narrow the crossover areas, or only trigger a crossfade when the knob/CV value passes the intersection
between ERAs"): ERA's overlaps as built (each era fades in over the first third of its fifth), narrower
(a sixth: renderer hook IV_ERA_FADE_SHARE=0.1667) and a timed crossfade at each boundary (0.75 s:
IV_ERA_SWITCH=0.75). Narrower or timed, two eras' stages run together over less of the knob (or only for
the fade), which the module's CPU likes.

Usage: <main checkout>/.venv/bin/python tools/pages/era_overlap.py [--render build-scratch/iv_render]

Columns 1-2: ERA sweeps 0.12 -> 0.95 over 16 s (boundaries at ~1.5, 5.4, 9.3, 13.1 s). Column 3: ERA
moves back and forth across the cassette | radio boundary, 0.72 <-> 0.88 every 4 s, like a slow LFO on
its CV. Renders are 100 % wet (--wet-only --dry), level-matched, a pick page, in the main checkout's
renders/era_overlap; _hooks.txt and _verify.txt as for era_switch.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, trim, third_octaves, db  # noqa: E402

SWEEP = (0.12, 0.95, 16.0)  # ERA from, to, over seconds
BOUNDARIES = (0.2, 0.4, 0.6, 0.8)
MATERIALS = {"pad": (17, 9), "skank": (17, 9), "drums": (17, 8)}
LFO = [(0.0, 0.72)] + [(2.0 * k, 0.88 if k % 2 else 0.72) for k in range(1, 9)]  # 4 s a round trip
COLS = [("1_massive", ["space=MASSIVE", "size=0.7", "decay=0.6", "attack=NORMAL", "time=0"], None),
        ("2_hall", ["space=HALL", "size=0.6", "decay=0.6", "attack=NORMAL", "time=0"], None),
        ("3_massive_cv", ["space=MASSIVE", "size=0.7", "decay=0.6", "attack=NORMAL", "time=0"], LFO)]
VERSIONS = [("A_overlap_third_as_built", {}), ("B_overlap_sixth", {"IV_ERA_FADE_SHARE": "0.1667"}),
            ("C_timed_750ms", {"IV_ERA_SWITCH": "0.75"})]
README = """ERA's transitions: today's overlaps, narrower ones, or a timed crossfade. Your note on era_switch: keep the smooth crossfades (they help when ERA moves on CV), but perhaps narrow the crossover areas, or only start a crossfade when the knob passes the boundary between two eras.

A is today: each era fades in across the first third of its stretch of the knob while the one before fades out. B narrows that to a sixth: the same blend, over half the turn. C only crossfades when the knob crosses a boundary, over 0.75 s whatever the knob does (each era alone otherwise; a small sticky zone keeps a resting knob from flipping). B and C both mean two eras run together over less of the knob (C only during the fade), which helps the module's CPU.

Columns 1 and 2: ERA sweeps slowly from the Neve to the radio over 16 s, crossing the four boundaries at about 1.5, 5.4, 9.3 and 13.1 s. Column 3: ERA moves back and forth across the cassette | radio boundary, 0.72 to 0.88 and back every 4 s, like a slow LFO on ERA's CV. Which transitions do you like best? MASSIVE SIZE 0.7, HALL SIZE 0.6, DECAY 0.6, ATTACK NORMAL. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_massive                  MASSIVE, ERA swept across all five eras
2_hall                     HALL, the same sweep
3_massive_cv               MASSIVE, ERA swinging across cassette | radio, 4 s a round trip

A_overlap_third_as_built   today: each era fades in over a third of its stretch
B_overlap_sixth            the same, over a sixth (half the turn)
C_timed_750ms              a 0.75 s crossfade when the knob crosses a boundary"""


def boundary_times():
    a, b, secs = SWEEP
    return [(x - a) / (b - a) * secs for x in BOUNDARIES]


def render(binary, src, out, sets, env_extra, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar", "--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}  env {}  ->  {}\n".format(out.name, env_extra or "{}",
                                              p.stderr.strip().replace("\n", " | ") or "(no hook line)"))


def hf_frames(x, sr, frame_s=0.005, lo=5000.0):
    """Energy above lo Hz in frame_s frames: a click shows as one frame far above its neighbours."""
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * sr)
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1), frame_s


def click_score(x, sr, t):
    """Around boundary time t (+-0.4 s): the loudest 5 ms frame above 5 kHz against the median of the
    second around it, in dB. Music's own transients read ~6-12 dB; a click reads far more."""
    e, fs = hf_frames(x, sr)
    i0, i1 = int((t - 0.4) / fs), int((t + 0.4) / fs)
    j0, j1 = max(0, int((t - 1.0) / fs)), int((t + 1.0) / fs)
    return db(e[i0:i1].max()) - db(np.median(e[j0:j1]))


def verify(a_path, b_path):
    a, sr = sf.read(a_path, always_2d=True)
    b, _ = sf.read(b_path, always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    if np.array_equal(a, b):
        return "IDENTICAL (mislabelled!)"
    null = db(np.mean((a - b) ** 2) / np.mean(a ** 2))
    ta, tb = third_octaves(a, sr), third_octaves(b, sr)
    level = db(np.mean(b ** 2)) - db(np.mean(a ** 2))
    clicks = ["{:.1f}s A {:+.1f} B {:+.1f}".format(t, click_score(a, sr, t), click_score(b, sr, t))
              for t in boundary_times()]
    return "null {:.1f} dB; level B-A {:+.2f} dB; HF peak/median at boundaries: {}".format(
        null, level, ", ".join(clicks))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-scratch" / "iv_render")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "era_overlap"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_era_switch" # the same trimmed material
    for m, (play, tail) in MATERIALS.items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    binary = a.render.resolve()
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets, lfo in COLS:
            pts = lfo or [(0.0, SWEEP[0]), (SWEEP[2], SWEEP[1])]
            auto = out / "_era_{}.json".format(col)
            auto.write_text(json.dumps({"breakpoints": [{"t": t, "key": "era", "value": v} for t, v in pts]}))
            sets = sets + ["era={}".format(pts[0][1])]
            for tag, env in VERSIONS:
                for m in MATERIALS:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, auto, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("A vs B and A vs C in every card (era_overlap); sweep boundaries at {} (column 3: the LFO crosses "
                "0.8 at 1, 3, 5 ... s)\n".format(", ".join("{:.1f} s".format(t) for t in boundary_times())))
        for col, _, _ in COLS:
            for m in MATERIALS:
                for other in VERSIONS[1:]:
                    v.write("{:14s} {:6s} {} vs A: {}\n".format(col, m, other[0][:1], verify(
                        out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), out / col / "{}_{}.wav".format(m, other[0]))))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "ERA: overlaps, narrower, or timed", "--level-match", "--dry", str(dry), "--wet-only"],
                   check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
