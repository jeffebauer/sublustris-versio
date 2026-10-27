#!/usr/bin/env python3
"""PROTOTYPE decision page (27 Oct 2026): how MOTION's tape transport (v1.0.46, dsp/Transport.h) comes in. Renderer hook
IV_TRANSPORT_ALWAYS (Engine::setTransportAlwaysPrototype; desktop only):

    A  as built: the transport is out at noon and on the right side; turned onto the left side it crossfades in
       (~5 ms) and its centre glides 8 -> 1000 samples over 5 s (the tail up to ~11 c flat, a tape coming up to
       speed), the reverse on the way back
    B  always on (1): the transport always running at its 1000-sample centre: no glide, no dip in pitch crossing noon,
       but the wet ~21 ms later everywhere (like a little extra predelay); at noon and on the right it is at rest

Cards: a slow MOTION turn noon -> full left -> noon (~20 s), quick turns noon -> 3/4 left and back, and MOTION fixed at
noon (A and B differ only by the 21 ms); a held organ chord and the one-key dub mix (renders/_material_drift_rethink);
HALL (SIZE 0.6) and MASSIVE (SIZE 0.7), DECAY 0.7, WEAR 0, TONE noon. 100 % wet (--wet-only --dry), level-matched.

Usage: <main checkout>/.venv/bin/python tools/pages/motion_entry.py --render <build>/iv_render
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
from review_cpu_pages import main_checkout  # noqa: E402

SR = 48000
VERSIONS = [("A_as_built", {}), ("B_always_on", {"IV_TRANSPORT_ALWAYS": "1"})]
SPACES = [("hall", ["space=HALL", "size=0.6"]), ("massive", ["space=MASSIVE", "size=0.7"])]
COMMON = ["decay=0.7", "era=0", "attack=NORMAL", "time=0", "tone=0.5"]
SLOW = [(0.0, 0.5), (2.0, 0.5), (12.0, 0.0), (22.0, 0.5)]
QUICK = [(0.0, 0.5), (3.0, 0.5), (3.3, 0.1175), (9.0, 0.1175), (9.3, 0.5), (15.0, 0.5), (15.3, 0.1175), (18.0, 0.1175),
         (18.3, 0.5)]
MOVES = [("slow_turn", SLOW, "MOTION turned slowly from noon to full left (2-12 s) and back to noon (12-22 s)"),
         ("quick_turn", QUICK, "MOTION flicked from noon to 3/4 left and back, twice (3 s and 9 s, 15 s and 18 s)"),
         ("noon", [(0.0, 0.5)], "MOTION left at noon: A and B differ only by B's 21 ms")]
TURNS = {"slow_turn": [2.6, 21.4], "quick_turn": [3.0, 9.3, 15.0, 18.3], "noon": []}  # where the side changes


def cols():
    out = []
    for move, pts, desc in MOVES:
        for sp, sets in SPACES:
            out.append(("{:02d}_{}_{}".format(len(out) + 1, move, sp), move, pts, sp, sets, desc))
    return out


def fade(x, a, b):
    x = x.copy()
    if a:
        n = int(a * SR); x[:n] *= np.linspace(0, 1, n)[:, None]
    if b:
        n = int(b * SR); x[-n:] *= np.linspace(1, 0, n)[:, None]
    return x


def organ(seconds=24.0, tail=6.0):
    """A held C minor 9 organ chord (C3 Eb3 G3 Bb3 D4), equal temperament at A4 = 440, three harmonics a note."""
    t = np.arange(int(seconds * SR)) / SR
    y = np.zeros_like(t)
    for m in (48, 51, 55, 58, 62):
        f = 440.0 * 2 ** ((m - 69) / 12)
        y += np.sin(2 * np.pi * f * t) + 0.4 * np.sin(4 * np.pi * f * t) + 0.15 * np.sin(6 * np.pi * f * t)
    x = fade(np.stack([y, y], 1), 0.3, 0.4)
    x *= 10 ** (-18 / 20) / np.sqrt(np.mean(x ** 2))
    return np.concatenate([x, np.zeros((int(tail * SR), 2))])


def dub(src):
    """The one-key dub mix (120 bpm: 8 bars = 16 s) twice, then 4 s of tail."""
    x, _ = sf.read(src, always_2d=True)
    loop = x[:16 * SR]
    y = fade(np.concatenate([loop, loop]), 0.0, 0.3)
    return np.concatenate([y, np.zeros((4 * SR, 2))])


def render(binary, src, out, sets, pts, env_extra, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    auto = out.with_suffix(".json")
    auto.write_text(json.dumps({"breakpoints": [{"t": t, "key": "drift", "value": v} for t, v in pts]}))
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--set", "drift={}".format(pts[0][1]), "--auto", str(auto)]
    for s in sets + COMMON:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    auto.unlink()
    out.with_suffix(".json").unlink(missing_ok=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}/{}  env {}  ->  {}\n".format(out.parent.name, out.name, env_extra or "{}",
                                                 " | ".join(l for l in p.stderr.splitlines() if l.startswith("iv_render:")) or "(no hook line)"))


def highpass(x, fc):
    X = np.fft.rfft(x, axis=0)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X[f < fc] = 0
    return np.fft.irfft(X, len(x), axis=0)


def spikes(x, around=None):
    """1 ms frames of the highs (> 5 kHz) against their neighbours 2-4 ms each side, dB: the worst after 1 s (or within
    ±0.5 s of each time in around); frames under -90 dBFS left out."""
    h = highpass(x.mean(axis=1), 5000.0)
    n = len(h) // 48
    e = (h[:n * 48].reshape(n, 48) ** 2).mean(axis=1)
    worst = -99.0
    idx = range(1004, n - 5) if around is None else [i for t in around for i in range(int(t * 1000) - 500, int(t * 1000) + 500)
                                                     if 4 <= i < n - 5]
    for i in idx:
        if e[i] < 1e-9:
            continue
        near = (e[i - 4] + e[i - 3] + e[i - 2] + e[i + 2] + e[i + 3] + e[i + 4]) / 6.0
        worst = max(worst, 10 * np.log10(e[i] / (near + 1e-30)))
    return worst


def lufs_a(x):
    seg = x.mean(axis=1)
    X = np.fft.rfft(seg); f = np.fft.rfftfreq(len(seg), 1.0 / SR); f2 = f ** 2
    ra = (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2) + 1e-30)
    y = np.fft.irfft(X * ra, len(seg))
    return 10 * np.log10(np.mean(y ** 2) + 1e-30)


README = """How the tape side comes in. MOTION's left side now runs the reverb through a tape transport (the wow, the flutter, the crinkle you picked). The question: which way should the tape side come in?

A is as built: at noon and on the right side the transport is out of the way. Turn onto the left side and it comes in like a tape coming up to speed: for about 5 seconds the tail sags very slightly in pitch (at most about a ninth of a semitone) and then settles; turning back to noon does the reverse.

B has the transport always running. Nothing happens in pitch when you cross noon, but the reverb always arrives about 21 ms later, everywhere on the knob (like a touch more predelay). At noon and on the right side the tape is still: no wow, nothing else changes.

Listen to the slow turn for the sag (A) against no sag (B), the quick turns for how it feels to flick the knob, and the noon cards for whether you'd notice B's 21 ms with the dry blended in (use the MIX fader around 50 %). Renders are 100 % wet and level-matched. HALL (SIZE 0.6) and MASSIVE (SIZE 0.7), DECAY 0.7, WEAR 0, TONE noon.

""" + "\n".join("{}   {}, {}".format(c, d, sp.upper()) for c, _, _, sp, _, d in cols()) + """

chord   a held organ chord (C minor 9), where any change of pitch is easiest to hear
dub     the one-key dub mix (120 bpm, C minor), the knob as you'd use it in a mix

A_as_built    the transport comes in on the left side only, gliding up to speed over 5 s
B_always_on   the transport always running: no glide, the reverb 21 ms later everywhere"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, required=True)
    a = ap.parse_args()
    root = main_checkout()
    src = root / "renders" / "_material_drift_rethink"
    out = root / "renders" / "motion_entry"
    dry = root / "renders" / "_material_motion_entry"
    for d in (out, dry):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    (dry / "_send").mkdir()
    sf.write(dry / "chord.wav", organ(), SR, subtype="FLOAT")
    shutil.copy(dry / "chord.wav", dry / "_send" / "chord.wav")
    sf.write(dry / "dub.wav", dub(src / "mix_dub_src.wav"), SR, subtype="FLOAT")
    sf.write(dry / "_send" / "dub.wav", dub(src / "mix_dub_send_src.wav"), SR, subtype="FLOAT")
    binary = a.render.resolve()
    rows = []
    with open(out / "_hooks.txt", "w") as hooks:
        for c, move, pts, sp, sets, _ in cols():
            for m in ("chord", "dub"):
                for tag, env in VERSIONS:
                    render(binary, dry / "_send" / (m + ".wav"), out / c / "{}_{}.wav".format(m, tag), sets, pts, env, hooks)
    bad = []
    for line in (out / "_hooks.txt").read_text().splitlines():
        always = "transport always on" in line
        if ("_B_always_on.wav" in line) != always:
            bad.append(line)
    with open(out / "_verify.txt", "w") as v:
        v.write("motion_entry. label check: {}\n".format("all renders as labelled" if not bad else "MISMATCH:\n" + "\n".join(bad)))
        v.write("Per card and material: level B - A (A-weighted, the whole render); 1 ms spikes of the highs (> 5 kHz) "
                "against their neighbours: the worst within 0.5 s of each change of side, and over the whole render, "
                "against the noon card's A; at noon, B against A moved 1000 samples later.\n")
        noonA = {}
        for c, move, pts, sp, sets, _ in cols():
            if move == "noon":
                for m in ("chord", "dub"):
                    noonA[(sp, m)] = spikes(sf.read(out / c / "{}_A_as_built.wav".format(m), always_2d=True)[0])
        for c, move, pts, sp, sets, _ in cols():
            for m in ("chord", "dub"):
                xa, _ = sf.read(out / c / "{}_A_as_built.wav".format(m), always_2d=True)
                xb, _ = sf.read(out / c / "{}_B_always_on.wav".format(m), always_2d=True)
                line = "{:20s} {:5s} level B-A {:+.2f} dB".format(c, m, lufs_a(xb) - lufs_a(xa))
                for tag, x in (("A", xa), ("B", xb)):
                    t = TURNS[move]
                    line += "  {} spikes {}whole {:+.1f}".format(tag, "turns {:+.1f}, ".format(spikes(x, t)) if t else "",
                                                                 spikes(x))
                line += "  (noon A {:+.1f})".format(noonA[(sp, m)])
                if move == "noon":
                    d = np.abs(xb[1000:] - xa[:-1000]).max()
                    line += "  B vs A+1000: {} (max |diff| {:.1e}, peak {:.2f})".format("bit-exact" if d == 0 else "differs",
                                                                                      d, np.abs(xa).max())
                    if not np.all(np.abs(xb[:1000]) < 1e-6):
                        line += ", B's first 1000 samples not silent"
                v.write(line + "\n")
        rows = len(cols()) * 4
        v.write("{} renders\n".format(rows))
    if bad:
        sys.exit("label check failed: see _verify.txt")
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "How the tape side comes in", "--level-match", "--dry", str(dry), "--wet-only"], check=True,
                   stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
