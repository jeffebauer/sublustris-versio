#!/usr/bin/env python3
"""Energy check for a prototype that reaches inside a tank's loop (the rule from
docs/research/still-lows.md; first used for the knob-feel audit, docs/research/knob-feel-audit.md).
The prototype is a renderer env hook, given as --hook NAME=VALUE (e.g. IV_STILL_LOWS=4).

1. Levels with and without the prototype across every SPACE x
   SIZE 0 / 0.6 / 1 x DECAY 0.5 / 0.75 / 0.95 x DRIFT 0 / 0.25 / 0.5 / 0.75 / 1, at each
   --set given (e.g. era=0.9): 2 s of noise, then 28 s of tail, the wet's RMS per window.
   Fails if B is ever more than 0.5 dB louder than A in a window (A: the louder of today at
   the same settings and today at --ref, e.g. era=0.85 where the Midiverb box is still out,
   for a prototype that moves a setting between two of today's), or if an infinite
   (DECAY 0.95) tail in B grows more than 1 dB above where it stood 2 s after the noise.
2. 10-minute infinite holds (DECAY 1, SIZE 0.5), fed and judged like infiniteHoldsTheFloor
   (host/tests/test_massive.cpp): uniform mono noise at 0.3 for 20 s, then 600 s of silence; over the 10 s
   windows from 60 s on, max - min < 3 dB, never above the 60-70 s window + 1 dB,
   never below -60 dBFS.

Usage: .venv/bin/python tools/pages/energy_check.py --hook IV_SOMETHING=1 --set era=0.9 [--set era=0.95] [--ref era=0.85]
       [--bin ...] [--no-holds]
"""
import argparse
import os
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parents[2]


def render(bin_, stim, out, sets, hook):
    env = dict(os.environ)
    for h in hook.split(",") if hook else []:  # NAME=VALUE[,NAME=VALUE]: the prototype's renderer env hooks ("" = today)
        name, value = h.split("=", 1)
        env[name] = value
    cmd = [bin_, str(stim), str(out), "--set", "mix=1"]
    for kv in sets:
        cmd += ["--set", kv]
    subprocess.run(cmd, check=True, capture_output=True, env=env)
    y, _ = sf.read(str(out), always_2d=True)
    os.unlink(out)
    return y


def windows_db(y, edges):
    return [10 * np.log10(np.mean(y[int(a * SR): int(b * SR)] ** 2) + 1e-20) for a, b in edges]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.environ.get("IV_RENDER", str(ROOT / "build" / "iv_render")))
    ap.add_argument("--hook", required=True, help="the prototype's renderer env hook, NAME=VALUE (several: comma-separated)")
    ap.add_argument("--set", action="append", default=[], help="k=v, one grid per value (repeatable)")
    ap.add_argument("--ref", help="k=v: A is also rendered with this, and the louder A counts")
    ap.add_argument("--ref-bin", help="A from this renderer instead of --bin (a port against the build before it)")
    ap.add_argument("--decay", action="append", help="the grid's DECAY values, repeatable (default 0.5 0.75 0.95)")
    ap.add_argument("--hold-decay", default="1", help="the 10-minute holds' DECAY (default 1)")
    ap.add_argument("--no-holds", action="store_true")
    ap.add_argument("--no-levels", action="store_true")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    args = ap.parse_args()
    rng = np.random.default_rng(7)
    fails = 0
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(args.jobs) as pool:
        tmp = Path(t)
        stim = tmp / "burst.wav"
        x = np.zeros((int(30 * SR), 2))
        x[: int(2 * SR)] = 0.3 * rng.uniform(-1, 1, (int(2 * SR), 1))  # the tests' noise: uniform, mono
        sf.write(str(stim), x.astype(np.float32), SR, subtype="FLOAT")
        edges = [(0, 2), (2, 4), (4, 8), (8, 16), (16, 30)]
        grid = [(sp, sz, dc, dr, extra)
                for extra in (args.set or [""])
                for sp in ("PLATE", "HALL", "MASSIVE") for sz in ("0", "0.6", "1")
                for dc in (args.decay or ("0.5", "0.75", "0.95")) for dr in ("0", "0.25", "0.5", "0.75", "1")]

        def job(g, k):
            sp, sz, dc, dr, extra = g
            sets = [f"space={sp}", f"size={sz}", f"decay={dc}", f"drift={dr}"] + ([extra] if extra else [])
            la = windows_db(render(args.ref_bin or args.bin, stim, tmp / f"a{k}.wav", sets, ""), edges)
            if args.ref:
                r = windows_db(render(args.ref_bin or args.bin, stim, tmp / f"r{k}.wav", sets[:4] + [args.ref], ""), edges)
                la = [max(u, v) for u, v in zip(la, r)]
            b = render(args.bin, stim, tmp / f"b{k}.wav", sets, args.hook)
            return g, la, windows_db(b, edges), bool(np.isfinite(b).all())

        worst, n = -99.0, 0
        for g, la, lb, ok in pool.map(lambda p: job(*p), [] if args.no_levels else [(g, k) for k, g in enumerate(grid)]):
            n += 1
            diff = max(b - a for a, b in zip(la, lb))
            worst = max(worst, diff)
            grow = lb[-1] - lb[1] if g[2] == "0.95" else -99.0
            bad = (not ok) or diff > 0.5 or grow > 1.0
            if bad:
                fails += 1
                print(f"FAIL {g}: B - A up to {diff:+.2f} dB, infinite growth {grow:+.2f} dB, finite={ok}")
        print(f"levels: {n} settings, B never more than {worst:+.2f} dB over A in any window"
              f" ({'pass' if fails == 0 else f'{fails} fail'})")

        if not args.no_holds:
            hold = tmp / "hold.wav"
            h = np.zeros((int(620 * SR), 2))
            h[: int(20 * SR)] = 0.3 * rng.uniform(-1, 1, (int(20 * SR), 1))  # as infiniteHoldsTheFloor
            sf.write(str(hold), h.astype(np.float32), SR, subtype="FLOAT")
            cases = [(sp, extra) for extra in (args.set or [""]) for sp in ("PLATE", "HALL", "MASSIVE")]

            def hjob(c, k):
                sp, extra = c
                sets = [f"space={sp}", f"decay={args.hold_decay}", "size=0.5", "time=0"] + ([extra] if extra else [])
                y = render(args.bin, hold, tmp / f"h{k}.wav", sets, args.hook)
                w = [20 * np.log10(np.sqrt(np.mean(y[i: i + 10 * SR] ** 2)) + 1e-12) for i in range(0, len(y) - 10 * SR + 1, 10 * SR)]
                return c, w[6], min(w[6:]), max(w[6:]), bool(np.isfinite(y).all())

            for c, first, lo, hi, ok in pool.map(lambda p: hjob(*p), [(c, k) for k, c in enumerate(cases)]):
                good = ok and hi - lo < 3.0 and hi <= first + 1.0 and lo > -60.0
                fails += not good
                print(f"hold {c[0]:8s} {c[1]:10s} 10 min: {lo:.1f} .. {hi:.1f} dBFS (60-70 s {first:.1f}) {'pass' if good else 'FAIL'}")
    print("energy check:", "pass" if fails == 0 else f"{fails} failure(s)")
    raise SystemExit(1 if fails else 0)


if __name__ == "__main__":
    main()
