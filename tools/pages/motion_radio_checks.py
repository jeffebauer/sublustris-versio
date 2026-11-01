#!/usr/bin/env python3
"""v1.0.60's checks (ADR 0036, the radio band on MOTION's right side): a build against today's (the reference).

1. Clicks: knob sweeps on a held pad (DECAY 0.7), every SPACE: MOTION noon -> full -> noon slow and quick (0.2 s
   jumps, and a jump to full left), WEAR 0 -> 1 -> 0 at MOTION full and at noon. The worst 1 ms frame of the highs
   (> 3 kHz) against its neighbours 2-4 ms each side; a build passes within 3 dB of the reference's same sweep, or
   under the reference's worst sweep anywhere.
2. Energy: every SPACE x SIZE 0 / 0.6 / 1 x DECAY 0.5 / 0.75 / 0.95 x MOTION 0 / 0.25 / 0.5 / 0.75 / 1 x WEAR 0 /
   0.5 / 1, 2 s of noise then 28 s of tail: an infinite (DECAY 0.95) tail in the build grows no more than 1 dB above
   where it stood 2 s after the noise, nothing is non-finite; the build's level against the reference per window is
   reported (where the band moved, it moves the level as it did on WEAR).
3. 10-minute infinite holds (DECAY 1, SIZE 0.5; 20 s of noise, then silence), every SPACE x MOTION 0.75 / 0.875 / 1
   x WEAR 0 / 0.5 / 1, and MOTION 0 / 0.5 at WEAR 1: 10 s windows from 60 s on, max - min < 3 dB, never above the
   60-70 s window + 1 dB, never below -60 dBFS.

Usage: .venv/bin/python tools/pages/motion_radio_checks.py --ref <today's iv_render> --bin <the build> --out <dir>
"""
import argparse
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
SPACES = {"PLATE": ["size=0.6"], "HALL": ["size=0.6"], "MASSIVE": ["size=0.7"]}


def render(binary, src, out, sets, auto=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    args = [str(binary), str(src), str(out), "--set", "mix=1"]
    for s in sets:
        args += ["--set", s]
    if auto:
        af = out.with_name(out.stem + "_auto.json")
        af.write_text('{"breakpoints": [' + ", ".join('{"t": %s, "key": "%s", "value": %s}' % a for a in auto) + "]}")
        args += ["--auto", str(af)]
    subprocess.run(args, check=True, capture_output=True, env=env)
    if auto:
        af.unlink()
    y, _ = sf.read(str(out), always_2d=True)
    out.unlink()
    return y


def pad(play, tail):
    """A held C minor 9 pad (two saws a note, +-3 cents), low-passed, a 1 s swell (renders/dip_motion's)."""
    n = int(play * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    for m in (48, 51, 55, 58, 62):
        for c in (-3.0, 3.0):
            f = 440.0 * 2 ** ((m - 69) / 12 + c / 1200)
            y += 2 * ((f * t + 0.37 * m) % 1.0) - 1
    Y = np.fft.rfft(y)
    fr = np.fft.rfftfreq(n, 1 / SR)
    y = np.fft.irfft(Y / np.sqrt(1 + (fr / 1800.0) ** 4), n)
    x = np.stack([y, y], 1)
    x[:SR] *= np.linspace(0, 1, SR)[:, None]
    k = int(0.6 * SR)
    x[-k:] *= np.linspace(1, 0, k)[:, None]
    x *= 10 ** (-18 / 20) / np.sqrt(np.mean(x ** 2))
    return np.concatenate([x, np.zeros((int(tail * SR), 2))])


def clicks(x, fc=3000.0):
    X = np.fft.rfft(x.mean(axis=1))
    X[np.fft.rfftfreq(len(x), 1 / SR) < fc] = 0
    h = np.fft.irfft(X, len(x))
    n = len(h) // 48
    e = (h[:n * 48].reshape(n, 48) ** 2).mean(axis=1)
    near = sum(np.roll(e, k) for k in (4, 3, 2, -2, -3, -4)) / 6.0
    idx = np.arange(504, n - 5)
    idx = idx[e[idx] > 1e-9]
    return float((10 * np.log10(e[idx] / (near[idx] + 1e-30))).max()) if len(idx) else -99.0


def db(y, a, b):
    return 10 * np.log10(np.mean(y[int(a * SR): int(b * SR)] ** 2) + 1e-20)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=8)
    a = ap.parse_args()
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(7)
    held, burst, hold = out / "_held.wav", out / "_burst.wav", out / "_hold.wav"
    sf.write(held, pad(40.0, 2.0), SR, subtype="FLOAT")
    x = np.zeros((int(30 * SR), 2))
    x[: 2 * SR] = 0.3 * rng.uniform(-1, 1, (2 * SR, 1))
    sf.write(burst, x.astype(np.float32), SR, subtype="FLOAT")
    h = np.zeros((int(620 * SR), 2))
    h[: 20 * SR] = 0.3 * rng.uniform(-1, 1, (20 * SR, 1))
    sf.write(hold, h.astype(np.float32), SR, subtype="FLOAT")
    lines, fails = [], 0
    k = [0]

    def tmp():
        k[0] += 1
        return out / "t{}.wav".format(k[0])

    sweeps = {
        "motion_slow": (["drift=0.5", "era=0.5"], [(0.0, "drift", 0.5), (4.0, "drift", 0.5), (20.0, "drift", 1.0), (36.0, "drift", 0.5)]),
        "motion_quick": (["drift=0.5", "era=1"], [(0.0, "drift", 0.5), (4.0, "drift", 0.5), (5.0, "drift", 1.0), (12.0, "drift", 1.0),
                                                   (12.5, "drift", 0.5), (20.0, "drift", 0.5), (20.2, "drift", 1.0), (30.0, "drift", 1.0),
                                                   (30.2, "drift", 0.0)]),
        "wear_at_full": (["drift=1", "era=0"], [(0.0, "era", 0.0), (4.0, "era", 0.0), (16.0, "era", 1.0), (28.0, "era", 0.0)]),
        "wear_at_noon": (["drift=0.5", "era=0"], [(0.0, "era", 0.0), (4.0, "era", 0.0), (16.0, "era", 1.0), (28.0, "era", 0.0)]),
    }
    with ThreadPoolExecutor(a.jobs) as pool:
        # 1. clicks
        sj = [(n, sp, b) for n in sweeps for sp in SPACES for b in (a.ref, a.bin)]
        sr = list(pool.map(lambda j: (j, clicks(render(j[2], held, tmp(), ["space=" + j[1], "decay=0.7"] + SPACES[j[1]] + sweeps[j[0]][0],
                                                                    sweeps[j[0]][1]))), sj))
        got = {(j[0], j[1], j[2]): c for j, c in sr}
        ref_worst = max(c for (n, sp, b), c in got.items() if b == a.ref)
        lines.append("1. Clicks on knob sweeps (held pad, DECAY 0.7): the worst 1 ms frame of the highs against its neighbours, "
                     "dB; build (reference). Pass: within 3 dB of the reference's, or under its worst anywhere ({:+.1f}).".format(ref_worst))
        for n in sweeps:
            for sp in SPACES:
                c, r = got[(n, sp, a.bin)], got[(n, sp, a.ref)]
                ok = c <= r + 3.0 or c <= ref_worst
                fails += not ok
                lines.append("  {:13s} {:8s} {:+5.1f} ({:+5.1f}) {}".format(n, sp, c, r, "pass" if ok else "FAIL"))
        # 2. energy
        grid = [(sp, sz, dc, dr, w) for sp in SPACES for sz in ("0", "0.6", "1") for dc in ("0.5", "0.75", "0.95")
                for dr in ("0", "0.25", "0.5", "0.75", "1") for w in ("0", "0.5", "1")]
        edges = [(0, 2), (2, 4), (4, 8), (8, 16), (16, 30)]

        def ejob(g):
            sp, sz, dc, dr, w = g
            sets = ["space=" + sp, "size=" + sz, "decay=" + dc, "drift=" + dr, "era=" + w]
            yb = render(a.bin, burst, tmp(), sets)
            ya = render(a.ref, burst, tmp(), sets)
            lb = [db(yb, *e) for e in edges]
            la = [db(ya, *e) for e in edges]
            return g, lb, la, bool(np.isfinite(yb).all())

        er = list(pool.map(ejob, grid))
        grow_fail, worst_grow, offs = 0, -99.0, {}
        for g, lb, la, fin in er:
            if g[2] == "0.95":
                worst_grow = max(worst_grow, lb[-1] - lb[1])
                grow_fail += (lb[-1] - lb[1] > 1.0) or not fin
            elif not fin:
                grow_fail += 1
            key = ("right" if float(g[3]) > 0.53 else "left/noon", g[4])
            offs.setdefault(key, []).append(max(b - c for b, c in zip(lb, la)))
        fails += grow_fail
        lines.append("")
        lines.append("2. Energy grid ({} settings): infinite tails' growth after 2 s at most {:+.2f} dB (limit +1); {}".format(
            len(grid), worst_grow, "pass" if grow_fail == 0 else "{} FAIL".format(grow_fail)))
        lines.append("   the build's loudest window against the reference's, dB (median / max), where the band moved:")
        for key in sorted(offs):
            v = offs[key]
            lines.append("   MOTION {:9s} WEAR {:3s}: {:+.2f} / {:+.2f}".format(key[0], key[1], float(np.median(v)), max(v)))
        # 3. holds
        hc = [(sp, d, w) for sp in SPACES for d in ("0.75", "0.875", "1") for w in ("0", "0.5", "1")]
        hc += [(sp, d, "1") for sp in SPACES for d in ("0", "0.5")]

        def hjob(c):
            sp, d, w = c
            y = render(a.bin, hold, tmp(), ["space=" + sp, "decay=1", "size=0.5", "time=0", "drift=" + d, "era=" + w])
            ws = [20 * np.log10(np.sqrt(np.mean(y[i: i + 10 * SR] ** 2)) + 1e-12) for i in range(0, len(y) - 10 * SR + 1, 10 * SR)]
            return c, ws[6], min(ws[6:]), max(ws[6:]), bool(np.isfinite(y).all())

        lines.append("")
        lines.append("3. 10-minute infinite holds (DECAY 1, SIZE 0.5): 10 s windows from 60 s on; pass: max - min < 3 dB, at most "
                     "the 60-70 s window + 1 dB, above -60 dBFS.")
        for (sp, d, w), first, lo, hi, fin in pool.map(hjob, hc):
            ok = fin and hi - lo < 3.0 and hi <= first + 1.0 and lo > -60.0
            fails += not ok
            lines.append("  {:8s} MOTION {:5s} WEAR {:3s} {:6.1f} .. {:6.1f} dBFS (60-70 s {:6.1f}) {}".format(
                sp, d, w, lo, hi, first, "pass" if ok else "FAIL"))
    for f in (held, burst, hold):
        f.unlink()
    lines.append("")
    lines.append("checks: " + ("pass" if fails == 0 else "{} failure(s)".format(fails)))
    (out / "checks.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
    raise SystemExit(1 if fails else 0)


if __name__ == "__main__":
    main()
