#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; the owner, v1.0.40: drop-outs and the red late light "generally when
changing size in MASSIVE mode", with slow sweeps and quick turns alike): SIZE's crossfade in more groups.
Since v1.0.39 the eight lines crossfade in two groups (four lines, then the other four, 0.3 s each); the
lines crossfading read twice, and that extra memory work, on top of a heavy era (the cassette's tape) and
a large SIZE, is what still tips the module over. In four groups (renderer hook IV_SIZE_STAGGER=4) two
lines read twice at a time, in eight (=8) one; the move takes 1.2 s or 2.4 s to settle instead of 0.6 s.

Usage: <main checkout>/.venv/bin/python tools/pages/size_moves.py [--render build-main/iv_render]

SIZE is turned the way a hand turns it on the module (release's knob path: a 10 ms one-pole, the 0.003
deadband, firmware/main.cpp): a quick turn 0.3 -> 0.8 at 4 s, two quick twists at 9 s, a slow sweep down
over 13-16 s. Renders are 100 % wet (--wet-only --dry), level-matched, a pick page. Written to the main
checkout's renders/size_moves; _hooks.txt keeps every render's hook line and _verify.txt measures each
version against B (as built) and looks for clicks at every move in 1 ms frames.
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

MOVES = (4.0, 9.0, 13.0)  # seconds: a quick turn up, two quick twists, a slow sweep down to 16 s
MATERIALS = {"pad": (18, 9), "keys": (18, 9), "drums": (18, 8)}
COLS = [("1_massive_neve", ["space=MASSIVE", "decay=0.7", "era=0.1", "attack=NORMAL", "time=0"]),
        ("2_massive_cassette", ["space=MASSIVE", "decay=0.7", "era=0.7", "attack=NORMAL", "time=0"]),
        ("3_hall", ["space=HALL", "decay=0.6", "era=0.1", "attack=NORMAL", "time=0"])]
VERSIONS = [("B_two_groups_as_built", {"IV_SIZE_STAGGER": "2"}), ("C_four_groups", {"IV_SIZE_STAGGER": "4"}),
            ("D_eight_groups", {"IV_SIZE_STAGGER": "8"})]
README = """SIZE's crossfade in more, smaller steps. When you move SIZE in HALL or MASSIVE the eight delay lines swap to their new lengths through a 0.3 s crossfade (no pitch bend). Since v1.0.39 they go in two groups, four lines then the other four. The lines that are crossfading are read twice, and on the module that extra work, on top of the cassette's tape and a large SIZE, still tips MASSIVE over: your drop-outs with the red light.

B is today (two groups, the move settles in about 0.6 s). C crossfades two lines at a time in four steps (about 1.2 s to settle, half the extra work of B); D one line at a time in eight steps (about 2.4 s, a quarter). Each line still crossfades over 0.3 s, so there is no pitch bend in any of them; what changes is how gradually the room grows or shrinks after you move the knob.

SIZE is turned as your hand turns it on the module: a quick turn from 0.3 to 0.8 at 4 s, two quick twists at 9 s, a slow sweep down between 13 and 16 s. Does C or D still feel like the knob is doing what you asked? Listen just after 4 s, around 9-11 s and after 16 s. MASSIVE at DECAY 0.7 with the Neve and with the cassette, HALL at 0.6, ATTACK NORMAL. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_massive_neve        MASSIVE, Neve
2_massive_cassette    MASSIVE, cassette
3_hall                HALL, Neve

B_two_groups_as_built today: four lines, then the other four (0.3 s each)
C_four_groups         two lines at a time, four steps
D_eight_groups        one line at a time, eight steps"""


def raw_knob(t):
    """The hand: SIZE's knob position at t seconds (smoothstep between points, as a hand eases in and out)."""
    pts = [(0.0, 0.3), (4.0, 0.3), (4.12, 0.8), (9.0, 0.8), (9.15, 0.45), (9.5, 0.45), (9.65, 0.85),
           (10.0, 0.85), (10.15, 0.5), (13.0, 0.5), (16.0, 0.2)]
    for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
        if t <= t1:
            u = (t - t0) / (t1 - t0) if t > t0 else 0.0
            return v0 + (v1 - v0) * u * u * (3.0 - 2.0 * u)
    return pts[-1][1]


def knob_path(seconds):
    """Release's knob path (firmware/main.cpp AudioCallback): a 1 ms block, a 10 ms one-pole, sent once it
    moves past the 0.003 deadband from the last value sent (the end stops don't come into it here)."""
    k, smooth, sent, steps = 1.0 / 11.0, None, None, []
    for b in range(int(seconds * 1000)):
        r = raw_knob(b / 1000.0)
        smooth = r if smooth is None else smooth + k * (r - smooth)
        if sent is None or abs(smooth - sent) > 0.003:
            sent = smooth
            steps.append((b / 1000.0, smooth))
    return steps


def automation(seconds):
    steps = knob_path(seconds)
    bp, prev = [{"t": 0.0, "key": "size", "value": steps[0][1]}], steps[0][1]
    for t, v in steps[1:]:  # a step at each send: held until it, the new value from it
        bp += [{"t": t - 1e-6, "key": "size", "value": prev}, {"t": t, "key": "size", "value": v}]
        prev = v
    return {"breakpoints": bp}


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


def hf_frames(x, sr, frame_s=0.001, lo=5000.0):
    """Energy above lo Hz in 1 ms frames: a click shows as one frame far above its neighbours."""
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * sr)
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1), frame_s


def click_score(e, fs, t0, t1):
    """Over t0..t1 (a move and every group's crossfade after it): the loudest 1 ms frame above 5 kHz
    against the median of the second around it, in dB (music's own transients read high too: compare
    versions, not the number alone)."""
    i0, i1 = int(t0 / fs), int(t1 / fs)
    worst = -1e9
    for i in range(i0, i1):
        j0, j1 = max(0, i - int(0.5 / fs)), i + int(0.5 / fs)
        worst = max(worst, db(e[i]) - db(np.median(e[j0:j1])))
    return worst


def verify(ref_path, path):
    a, sr = sf.read(ref_path, always_2d=True)
    b, _ = sf.read(path, always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    if np.array_equal(a, b):
        return "IDENTICAL (mislabelled!)"
    null = db(np.mean((a - b) ** 2) / np.mean(a ** 2))
    level = db(np.mean(b ** 2)) - db(np.mean(a ** 2))
    ea, fs = hf_frames(a, sr)
    eb, _ = hf_frames(b, sr)
    windows = [(4.0, 7.0), (9.0, 13.0), (13.0, 19.0)]  # each move until its last group has faded (D: 2.4 s)
    clicks = ["{:.0f}-{:.0f}s ref {:+.1f} this {:+.1f}".format(t0, t1, click_score(ea, fs, t0, t1), click_score(eb, fs, t0, t1))
              for t0, t1 in windows]
    return "null {:.1f} dB; level vs B {:+.2f} dB; 1 ms HF peak/median: {}".format(null, level, ", ".join(clicks))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-main" / "iv_render")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "size_moves"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_size_moves"
    for m, (play, tail) in MATERIALS.items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    binary = a.render.resolve()
    auto = out / "_size_moves.json"
    auto.write_text(json.dumps(automation(27.0)))
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in COLS:
            sets = sets + ["size=0.3"]
            for tag, env in VERSIONS:
                for m in MATERIALS:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, auto, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("Each version against B (as built), every card (size_moves); moves at {}\n".format(
            ", ".join("{:.1f} s".format(t) for t in MOVES)))
        for col, _ in COLS:
            for m in MATERIALS:
                ref = out / col / "{}_{}.wav".format(m, VERSIONS[0][0])
                for tag, _ in VERSIONS[1:]:
                    v.write("{:18s} {:6s} {:14s} {}\n".format(col, m, tag[:14], verify(ref, out / col / "{}_{}.wav".format(m, tag))))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "SIZE: the crossfade in two, four or eight steps", "--level-match", "--dry", str(dry), "--wet-only"],
                   check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
