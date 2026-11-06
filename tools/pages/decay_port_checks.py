#!/usr/bin/env python3
"""v1.0.72's port of renders/decay_curve's C (ADR 0048), against a reference: v1.0.71 with the prototype's IV_DECAY_CURVE
hook applied (hook off is v1.0.71 itself; IV_DECAY_CURVE=C the prototype's C on v1.0.71's base).

identity  (a) the port at a knob position against v1.0.71 at decayScale(knob), sample by sample, every SPACE, MOTION noon and
          both extremes: everything DECAY keys (the audit, docs/research/decay-curve-2026-11-06.md) follows, or it differs.
          Knob 0 and 1 are then v1.0.71's own settings.
          (b) the port against the prototype's C with DECAY moving (a sweep, flicks across the crossover), every SPACE.
clicks    1 ms frames (> 3 kHz, dB over neighbours, motion_radio_checks.clicks), a held pad: DECAY swept over the crossover
          and flicked across it (50 ms and 20 ms), every SPACE at MOTION noon, HALL (its ease) at both extremes too;
          beside v1.0.71 across its own crossover.
follow    endless follows the playing: the pad 8 s then silence. Playing (2-8 s) at the endless start against the finite
          top; then 18-28 s against 68-78 s. Beside v1.0.71 (0.85 against 0.80).

Usage: <venv>/bin/python tools/pages/decay_port_checks.py <port iv_render> <reference iv_render> [section ...]
"""
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motion_radio_checks as mc

SR = mc.SR
SPACES = ("PLATE", "HALL", "CANYON")
F, E, B = np.float32(0.92), np.float32(0.95), np.float32(0.328)


def scale(v):
    """Mappings.h decayScale, in float32."""
    v = np.float32(v)
    if v <= F:
        x = v / F
        return np.float32(0.8) * (x + B * x * (np.float32(1) - x))
    if v < E:
        return np.float32(0.8) + np.float32(0.05) * (v - F) / (E - F)
    return np.float32(0.85) + np.float32(0.15) * (v - E) / (np.float32(1) - E)


def render(tmp, name, binary, src, sets, auto=None, hook=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    if hook:
        env["IV_DECAY_CURVE"] = hook
    o = tmp / f"{name}.wav"
    args = [binary, str(src), str(o), "--set", "mix=1"]
    for s in sets:
        args += ["--set", s]
    if auto:
        af = tmp / f"{name}.json"
        af.write_text(json.dumps({"breakpoints": [{"t": t, "key": k, "value": v} for t, k, v in auto]}))
        args += ["--auto", str(af)]
    subprocess.run(args, check=True, capture_output=True, env=env)
    y, _ = sf.read(str(o), always_2d=True, dtype="float32")
    o.unlink()
    return y


def win(y, a, b):
    return 10 * np.log10(np.mean(y[int(a * SR):int(b * SR)].astype(np.float64) ** 2) + 1e-30)


if __name__ == "__main__":
    PORT, REF = sys.argv[1], sys.argv[2]
    which = sys.argv[3:] or ["identity", "clicks", "follow"]
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(8) as ex:
        tmp = Path(t)
        hold = tmp / "hold.wav"; sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
        short = tmp / "short.wav"; sf.write(str(short), mc.pad(6.0, 6.0), SR, subtype="FLOAT")
        long_ = tmp / "long.wav"; sf.write(str(long_), mc.pad(30.0, 4.0), SR, subtype="FLOAT")
        if "identity" in which:
            knobs = (0, 0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.92, 0.93, 0.94, 0.95, 0.97, 1)
            cases = [(sp, k, d) for sp in SPACES for k in knobs for d in ("0.5", "0", "1")]

            def one(c):
                sp, k, d = c
                base = [f"space={sp}", "size=0.6", "era=0.3", f"drift={d}"]
                a = render(tmp, f"ia{sp}{k}{d}", PORT, short, base + [f"decay={k}"])
                b = render(tmp, f"ib{sp}{k}{d}", REF, short, base + [f"decay={float(scale(k)):.9g}"])
                return np.array_equal(a, b)
            r = list(ex.map(one, cases))
            bad = [c for c, ok in zip(cases, r) if not ok]
            print(f"identity (a): port at the knob = v1.0.71 at decayScale(knob): {sum(r)}/{len(r)} bit-identical" + (f"; differ: {bad}" if bad else ""))
            ends = [ok for c, ok in zip(cases, r) if c[1] in (0, 1)]
            print(f"  of which DECAY 0 and fully CW (v1.0.71's own settings): {sum(ends)}/{len(ends)}")
            moves = {"sweep 0 -> 1 -> 0.5": [(0, "decay", 0.0), (1, "decay", 0.0), (7, "decay", 1.0), (11, "decay", 0.5)],
                     "flicks over the crossover": [(0, "decay", 0.9), (3, "decay", 0.9), (3.02, "decay", 0.97), (7, "decay", 0.97), (7.05, "decay", 0.9)]}
            cases = [(sp, n, a) for sp in SPACES for n, a in moves.items()]

            def two(c):
                sp, n, au = c
                base = [f"space={sp}", "size=0.6", "era=0.3", "drift=0.5", f"decay={au[0][2]}"]
                a = render(tmp, f"ma{sp}{n[:3]}", PORT, short, base, au)
                b = render(tmp, f"mb{sp}{n[:3]}", REF, short, base, au, hook="C")
                return np.array_equal(a, b)
            r = list(ex.map(two, cases))
            bad = [c[:2] for c, ok in zip(cases, r) if not ok]
            print(f"identity (b): port = the prototype's C with DECAY moving: {sum(r)}/{len(r)} bit-identical" + (f"; differ: {bad}" if bad else ""))
        if "clicks" in which:
            rows = [(sp, "0.5") for sp in SPACES] + [("HALL", "0"), ("HALL", "1")]
            cases = []
            for sp, d in rows:
                for n, (lo, hi) in (("port", (0.90, 0.97)), ("v1.0.71", (0.78, 0.87))):
                    bin_ = PORT if n == "port" else REF
                    au = {"sweep": [(0, "decay", lo), (6, "decay", lo), (16, "decay", hi), (26, "decay", lo)],
                          "flick 50 ms": [(0, "decay", lo), (8, "decay", lo), (8.05, "decay", hi), (16, "decay", hi), (16.05, "decay", lo)],
                          "flick 20 ms": [(0, "decay", lo), (8, "decay", lo), (8.02, "decay", hi), (16, "decay", hi), (16.02, "decay", lo)]}
                    for m, a in au.items():
                        cases.append((sp, d, n, m, bin_, a))

            def three(i_c):
                i, (sp, d, n, m, b, a) = i_c
                return mc.clicks(render(tmp, f"c{i}", b, long_, [f"space={sp}", "size=0.6", "era=0.3", f"drift={d}", f"decay={a[0][2]}"], a).astype(np.float64))
            r = dict(zip([c[:4] for c in cases], ex.map(three, enumerate(cases))))
            print("clicks, dB (port / v1.0.71; the music's own read ~15-20):")
            for sp, d in rows:
                print(f"  {sp:6s} MOTION {d:3s} " + "  ".join(f"{m} {r[(sp, d, 'port', m)]:5.1f} / {r[(sp, d, 'v1.0.71', m)]:5.1f}" for m in ("sweep", "flick 50 ms", "flick 20 ms")))
        if "follow" in which:
            print("endless follows the playing: playing at the endless start re the finite top; held 18-28 s to 68-78 s (port / v1.0.71):")

            def f(c):
                b, sp, d = c
                return render(tmp, f"f{hash(c) & 0xffff}", b, hold, [f"space={sp}", "size=0.6", "era=0.3", "drift=0.5", f"decay={d}"])
            cases = [(b, sp, d) for sp in SPACES for b, d in ((PORT, 0.95), (PORT, 0.92), (REF, 0.85), (REF, 0.8))]
            r = dict(zip(cases, ex.map(f, cases)))
            for sp in SPACES:
                pe, pt, re_, rt = r[(PORT, sp, 0.95)], r[(PORT, sp, 0.92)], r[(REF, sp, 0.85)], r[(REF, sp, 0.8)]
                print(f"  {sp:6s} playing {win(pe, 2, 8) - win(pt, 2, 8):+5.2f} / {win(re_, 2, 8) - win(rt, 2, 8):+5.2f} dB; "
                      f"held {win(pe, 68, 78) - win(pe, 18, 28):+5.2f} / {win(re_, 68, 78) - win(re_, 18, 28):+5.2f} dB")
