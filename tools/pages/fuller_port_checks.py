#!/usr/bin/env python3
"""v1.0.71's port of renders/fuller_tail (ADR 0047), against the prototype's renderer (commit a3478b8, IV_FULLER):

identity  the port against the prototype, sample by sample: C where the pick is C (CANYON everywhere, HALL below the
          ease), B at HALL endless, A (as v1.0.70) on PLATE; the largest difference in dB below the render's peak.
ease      HALL's DECAY across the crossover (0.70 -> 0.90, slow turn and 20 ms flick) on a tail and on a held tone:
          the worst 100 ms level step; the held note's spread (cents) in the windows before, during and after.
clicks    1 ms frames (> 3 kHz, dB over neighbours, motion_radio_checks.clicks) on DECAY sweeps and flicks across
          the ease, and on SIZE sweeps and flicks, every SPACE; the port beside v1.0.70.
late      prints the worst click frames of the HALL SIZE sweep (C's 13.0 at 24.8 s on the prototype) with their level.

Usage: <venv>/bin/python tools/pages/fuller_port_checks.py <port iv_render> <prototype iv_render> [section ...]
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

SR = 48000


def bp(points):
    return {"breakpoints": [{"t": t, "key": k, "value": x} for t, k, x in points]}


def render(bin_, env, src, sets, auto, tmp):
    e = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    e.update(env)
    fd, out = tempfile.mkstemp(suffix=".wav", dir=tmp)
    os.close(fd)
    args = [bin_, str(src), out, "--set", "mix=1"] + sum((["--set", f"{k}={v}"] for k, v in sets.items()), [])
    if auto:
        af = out + ".json"
        Path(af).write_text(json.dumps(auto))
        args += ["--auto", af]
    subprocess.run(args, check=True, capture_output=True, env=e)
    y, _ = sf.read(out, always_2d=True, dtype="float32")
    os.unlink(out)
    if auto:
        os.unlink(af)
    return y


def tone(play, tail, f=1000.0):
    n = int(play * SR)
    t = np.arange(n) / SR
    y = 0.25 * np.sin(2 * np.pi * f * t)
    y[:480] *= np.linspace(0, 1, 480)
    y[-480:] *= np.linspace(1, 0, 480)
    return np.concatenate([np.stack([y, y], 1), np.zeros((int(tail * SR), 2))]).astype(np.float32)


def cents_spread(seg, f0=1000.0, width=50.0):
    x = seg.mean(axis=1) * np.hanning(len(seg))
    N = 1 << 20
    T = np.abs(np.fft.rfft(x, N)) ** 2
    f = np.fft.rfftfreq(N, 1 / SR)
    c = 1200 * np.log2(np.maximum(f, 1e-9) / f0)
    w = np.abs(c) <= width
    p = T[w] - T[w].min()
    m = np.sum(p * c[w]) / np.sum(p)
    return float(np.sqrt(np.sum(p * (c[w] - m) ** 2) / np.sum(p)))


def max_step(y, a, b, ms=100):
    w = int(ms * SR / 1000)
    n = (b - a) * 1000 // ms
    e = [10 * np.log10(np.mean(y[int(a * SR) + i * w: int(a * SR) + (i + 1) * w] ** 2) + 1e-20) for i in range(int(n))]
    return float(np.max(np.abs(np.diff(e))))


if __name__ == "__main__":
    port, proto = sys.argv[1], sys.argv[2]
    secs = sys.argv[3:] or ["identity", "ease", "clicks", "late"]
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(max(1, (os.cpu_count() or 4) - 2)) as ex:
        tmp = Path(t)
        pad = tmp / "pad.wav"
        sf.write(str(pad), mc.pad(24.0, 4.0), SR, subtype="FLOAT")
        short = tmp / "short.wav"
        sf.write(str(short), mc.pad(6.0, 14.0), SR, subtype="FLOAT")
        ton = tmp / "tone.wav"
        sf.write(str(ton), tone(12.0, 18.0), SR, subtype="FLOAT")

        if "identity" in secs:
            size_move = bp([(0, "size", 0.2), (6, "size", 0.2), (10, "size", 0.9), (14, "size", 0.9), (14.02, "size", 0.4)])
            cases = []
            for dc in (0.3, 0.5, 0.75, 0.9, 1.0):
                for sz in (0.0, 0.6, 1.0):
                    cases.append(("C", {"space": "CANYON", "decay": dc, "size": sz}, None))
            for dc in (0.3, 0.5, 0.7, 0.79):
                for sz in (0.0, 0.6, 1.0):
                    cases.append(("C", {"space": "HALL", "decay": dc, "size": sz}, None))
            for dc in (0.85, 0.9, 1.0):
                for sz in (0.0, 0.6, 1.0):
                    cases.append(("B", {"space": "HALL", "decay": dc, "size": sz}, None))
            for dc in (0.5, 0.9):
                for sz in (0.0, 0.6, 1.0):
                    cases.append(("A", {"space": "PLATE", "decay": dc, "size": sz}, None))
            for sp, v, dc in (("CANYON", "C", 0.5), ("HALL", "C", 0.5), ("HALL", "B", 0.9), ("PLATE", "A", 0.5)):
                cases.append((v, {"space": sp, "decay": dc, "era": 0.8, "drift": 0.25, "tone": 0.25}, None))
                cases.append((v, {"space": sp, "decay": dc, "drift": 0.75}, None))
                cases.append((v, {"space": sp, "decay": dc}, size_move))

            def run(c):
                v, sets, auto = c
                a = render(port, {}, short, sets, auto, tmp)
                b = render(proto, {} if v == "A" else {"IV_FULLER": v}, short, sets, auto, tmp)
                d = np.max(np.abs(a.astype(np.float64) - b))
                return c, d == 0, 20 * np.log10(d / (np.max(np.abs(b)) + 1e-30) + 1e-30)

            res = list(ex.map(run, cases))
            for v in "ABC":
                r = [x for x in res if x[0][0] == v]
                same = sum(1 for x in r if x[1])
                print(f"identity: port vs prototype {v}: {same}/{len(r)} bit-identical; worst difference {max(x[2] for x in r):.1f} dB re peak")
            for c, s, d in res:
                if not s and d > -100:
                    print(f"  differs {d:.1f} dB: {c[0]} {c[1]} auto={bool(c[2])}")

        if "ease" in secs:
            base = {"space": "HALL", "size": 0.6, "drift": 0.5}
            slow = bp([(0, "decay", 0.70), (14, "decay", 0.70), (24, "decay", 0.90)])
            flick = bp([(0, "decay", 0.70), (16, "decay", 0.70), (16.02, "decay", 0.90)])
            flickd = bp([(0, "decay", 0.90), (16, "decay", 0.90), (16.02, "decay", 0.70)])
            jobs = [(n, tag, b, env, a) for n, a in (("slow turn", slow), ("flick up", flick), ("flick down", flickd))
                    for tag, b, env in (("port", port, {}), ("B", proto, {"IV_FULLER": "B"}), ("C", proto, {"IV_FULLER": "C"}))]

            def ease(j):
                n, tag, b, env, a = j
                y = render(b, env, ton, base, a, tmp)
                w = [cents_spread(y[int(s * SR):int((s + 3) * SR)]) for s in (10.0, 13.0, 16.0, 19.0, 22.0, 25.0)]
                return n, tag, max_step(y, 13, 28), w

            for n, tag, st, w in ex.map(ease, jobs):
                print(f"ease {n:10s} {tag:4s}: worst 100 ms level step {st:4.2f} dB; held 1 kHz spread (10/13/16/19/22/25 s) "
                      + " ".join(f"{x:4.1f}" for x in w) + " cents")

        if "clicks" in secs:
            jobs = []
            for sp in ("PLATE", "HALL", "CANYON"):
                base = {"space": sp, "decay": 0.7, "drift": 0.5}
                b2 = {"space": sp, "size": 0.6, "drift": 0.5}
                for name, sets, a in (
                        ("SIZE 0 -> 1 -> 0", base, bp([(0, "size", 0.0), (2, "size", 0.0), (12, "size", 1.0), (22, "size", 0.0)])),
                        ("SIZE flicks 0.3 <-> 0.8", base, bp([(0, "size", 0.3), (6, "size", 0.3), (6.02, "size", 0.8), (12, "size", 0.8), (12.02, "size", 0.3), (18, "size", 0.3)])),
                        ("DECAY 0.3 -> 0.95 -> 0.3", b2, bp([(0, "decay", 0.3), (2, "decay", 0.3), (12, "decay", 0.95), (22, "decay", 0.3)])),
                        ("DECAY across the ease 0.7 -> 0.9 -> 0.7", b2, bp([(0, "decay", 0.7), (6, "decay", 0.7), (12, "decay", 0.9), (18, "decay", 0.7)])),
                        ("DECAY flicks 0.7 <-> 0.9", b2, bp([(0, "decay", 0.7), (6, "decay", 0.7), (6.02, "decay", 0.9), (12, "decay", 0.9), (12.02, "decay", 0.7), (20, "decay", 0.7)]))):
                    for tag, b in (("v1.0.70", proto), ("port", port)):
                        jobs.append((sp, name, tag, b, sets, a))

            def click(j):
                sp, name, tag, b, sets, a = j
                return sp, name, tag, mc.clicks(render(b, {}, pad, sets, a, tmp))

            res = list(ex.map(click, jobs))
            for i in range(0, len(res), 2):
                sp, name, _, va = res[i]
                print(f"clicks {sp:6s} {name:42s} v1.0.70 {va:5.1f}  port {res[i + 1][3]:5.1f}")

        if "late" in secs:
            a = bp([(0, "size", 0.0), (2, "size", 0.0), (12, "size", 1.0), (22, "size", 0.0)])
            sets = {"space": "HALL", "decay": 0.7, "drift": 0.5}
            for tag, b, env in (("port", port, {}), ("proto C", proto, {"IV_FULLER": "C"}), ("v1.0.70", proto, {})):
                y = render(b, env, pad, sets, a, tmp).astype(np.float64)
                X = np.fft.rfft(y.mean(axis=1))
                X[np.fft.rfftfreq(len(y), 1 / SR) < 3000.0] = 0
                h = np.fft.irfft(X, len(y))
                n = len(h) // 48
                e = (h[:n * 48].reshape(n, 48) ** 2).mean(axis=1)
                near = sum(np.roll(e, k) for k in (4, 3, 2, -2, -3, -4)) / 6.0
                idx = np.arange(504, n - 5)
                idx = idx[e[idx] > 1e-9]
                r = 10 * np.log10(e[idx] / (near[idx] + 1e-30))
                top = idx[np.argsort(r)[-6:][::-1]]
                print(f"late {tag}: worst " + ", ".join(f"{i / 1000:.3f} s {10 * np.log10(e[i] / (near[i] + 1e-30)):.1f} dB (frame {10 * np.log10(e[i]):.0f} dB)" for i in top))
