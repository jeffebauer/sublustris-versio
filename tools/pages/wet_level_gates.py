#!/usr/bin/env python3
"""v1.0.73's gates (ADR 0049), against the build before it: the picks' cards, the steady level over DECAY, turning DECAY,
played hot (the limiter, click frames, the module's peak at MIX noon), MIX noon and full. Knobs: the cards are at the T60s the
owner heard, through DECAY's curve. The limiter's share needs renderers with a counter, built in scratch and never committed.

Usage: <venv>/bin/python tools/pages/wet_level_gates.py picks|steady|turn|hot|own|mix --new R --old R [--v70 R] --scratch DIR
"""
import argparse
import itertools
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc  # noqa: E402
from wet_loudness_lib import integrated, SR  # noqa: E402

MAIN = os.path.abspath(os.path.join(HERE, "../..")).split("/.claude/worktrees/")[0]
DRY = MAIN + "/renders/_material_wet_loudness"
BASE = ["size=0.5", "tone=0.5", "drift=0.5", "time=0.2", "era=0", "attack=NATURAL"]
SOURCES = ("pad", "piano", "drums", "stabs")
SPACES = ("PLATE", "HALL", "CANYON")
RANGE = {"PLATE": (0.4, 30.0), "HALL": (0.8, 60.0), "CANYON": (2.0, 120.0)}
_n = itertools.count()
a = None


def scale(k):
    if k <= 0.92:
        x = k / 0.92
        return 0.8 * (x + 0.328 * x * (1 - x))
    return 0.8 + 0.05 * (k - 0.92) / 0.03 if k < 0.95 else 0.85 + 0.15 * (k - 0.95) / 0.05


def knob(s):
    """The knob that gives DECAY's scale s (tests' decayKnobFor)."""
    if s >= 1:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(50):
        m = (lo + hi) / 2
        lo, hi = (m, hi) if scale(m) < s else (lo, m)
    return lo


def t60(k, sp):
    lo, hi = RANGE[sp]
    return lo * (hi / lo) ** (min(scale(k), 0.8) / 0.8)


def float_src(name):
    p = f"{a.scratch}/{name}.wav"
    if not os.path.exists(p):
        x, sr = sf.read(f"{DRY}/{name}.wav", always_2d=True, dtype="float64")
        sf.write(p, x, sr, subtype="FLOAT")
    return p


def render(r, src, sets, auto=None):
    o = f"{a.scratch}/_g{next(_n)}.wav"
    cmd = [r, src, o]
    for s in BASE + sets:
        cmd += ["--set", s]
    if auto:
        cmd += ["--auto", auto]
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    p = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stderr)
    lim = {}
    for line in p.stderr.splitlines():
        if line.startswith("limiter:"):
            lim = {k: float(v) for k, v in (kv.split("=") for kv in line[8:].split())}
    y = sf.read(o, always_2d=True, dtype="float64")[0]
    os.unlink(o)
    return y, lim


def pmap(f, jobs):
    with ThreadPoolExecutor(8) as e:
        return list(e.map(f, jobs))


def pk(y):
    return float(20 * np.log10(np.abs(y).max() + 1e-30))


def n10(y):
    """1 ms frames over 3 kHz at least 10 dB over their neighbours, per 10 s, either channel's worst."""
    w = np.ones(len(y))
    m = int(0.1 * SR)
    w[:m], w[-m:] = np.hanning(2 * m)[:m], np.hanning(2 * m)[m:]
    best = 0.0
    for c in range(y.shape[1]):
        X = np.fft.rfft(y[:, c] * w)
        X[np.fft.rfftfreq(len(y), 1 / SR) < 3000] = 0
        h = np.fft.irfft(X, len(y))
        n = len(h) // 48
        e = (h[:n * 48].reshape(n, 48) ** 2).mean(axis=1)
        near = sum(np.roll(e, k) for k in (4, 3, 2, -2, -3, -4)) / 6
        live = e > 1e-9
        best = max(best, float(((10 * np.log10((e + 1e-30) / (near + 1e-30)) >= 10) & live).sum() / (len(y) / SR) * 10))
    return best


def picks():
    print("The picks' cards at the T60s the owner heard. LU against the dry. `heard` = v1.0.70 + the pick (what the owner chose); `now` = v1.0.73 (fuller tail, curve, lift); now - heard: + is louder than they picked.")
    cards = [("PLATE", 0.3, "", dict(pad=4.5, piano=4.5, drums=4.5, stabs=4.5)), ("HALL", 0.5, "", dict(pad=4.5, piano=4.5, drums=4.5, stabs=4.5)),
             ("CANYON", 0.5, "", dict(pad=3, piano=3, drums=3, stabs=4.5)), ("HALL", 0.75, "", dict(pad=3, piano=3, drums=3, stabs=3)),
             ("PLATE", 1.0, "", dict(pad=4.5, piano=3, drums=3, stabs=3)), ("CANYON", 1.0, "_hot", dict(pad=3, piano=3, drums=3, stabs=3)),
             ("HALL", 1.0, "_hot", dict(pad=3, piano=3, drums=3, stabs=3))]

    def one(j):
        sp, s, h, m, p = j
        src = float_src(m + h)
        ld = integrated(sf.read(src, always_2d=True)[0])
        v70 = integrated(render(a.v70, src, [f"space={sp}", f"decay={s}", "mix=1"])[0]) - ld
        k = [f"space={sp}", f"decay={knob(s):.5f}", "mix=1"]
        old = integrated(render(a.old, src, k)[0]) - ld
        new = integrated(render(a.new, src, k)[0]) - ld
        return sp, s, h, m, p, v70, old, new
    rows = pmap(one, [(sp, s, h, m, p) for sp, s, h, ps in cards for m, p in ps.items()])
    print("SPACE  scale src        pick  v1.0.70 heard | v1.0.72 v1.0.73 | now-heard")
    tot = {}
    for sp, s, h, m, p, v70, old, new in rows:
        print(f"{sp:6s} {s:4.2f} {m + h:10s} {p:3.1f} {v70:+7.1f} {v70 + p:+6.1f} | {old:+7.1f} {new:+7.1f} | {new - v70 - p:+5.1f}")
        tot.setdefault((sp, s, h), []).append(new - v70 - p)
    print("Mean over each card's sources, now - heard: " + "; ".join(f"{sp} {s} {h or 'own'} {np.mean(v):+.1f}" for (sp, s, h), v in tot.items()))
    print("T60 by knob, v1.0.73: " + "; ".join(f"{sp} " + " ".join(f"{k}:{t60(k, sp):.1f}" for k in (0.25, 0.5, 0.65, 0.75, 0.85, 0.92)) for sp in SPACES))


def steady():
    print("Steady level over DECAY's knob, the wet against its dry (LU, pad and drums mean): v1.0.72, v1.0.73, the difference; step = v1.0.73 from the last row.")
    ks = [0.2, 0.3, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0]
    for sp in SPACES:
        def one(j):
            k, m = j
            src = float_src(m)
            ld = integrated(sf.read(src, always_2d=True)[0])
            sets = [f"space={sp}", f"decay={k}", "mix=1"]
            return k, m, integrated(render(a.old, src, sets)[0]) - ld, integrated(render(a.new, src, sets)[0]) - ld
        res = pmap(one, [(k, m) for k in ks for m in ("pad", "drums")])
        print(f"{sp}: knob T60  v1.0.72 v1.0.73 diff  step")
        prev = None
        steps = []
        for k in ks:
            r = [x for x in res if x[0] == k]
            o, n = np.mean([x[2] for x in r]), np.mean([x[3] for x in r])
            print(f"     {k:4.2f} {t60(k, sp):5.1f} {o:+6.1f} {n:+6.1f} {n - o:+5.2f}  {'' if prev is None else f'{n - prev:+5.1f}'}")
            if prev is not None:
                steps.append(n - prev)
            prev = n
        print(f"     biggest step per row: up {max(steps):+.1f}, down {min(steps):+.1f}")


def turn():
    print("Turning DECAY through the 12-26 s bend, per SPACE, the worst 1 ms click frame (> 3 kHz, dB over its neighbours) and the worst 10 ms level step, v1.0.72 and v1.0.73, at MIX full and noon.")
    bends = {sp: [knob(0.8 * np.log(t / RANGE[sp][0]) / np.log(RANGE[sp][1] / RANGE[sp][0])) for t in (12.0, 26.0)] for sp in SPACES}
    print("The bend on the knob: " + "; ".join(f"{sp} {b[0]:.3f}-{b[1]:.3f}" for sp, b in bends.items()))
    rows = []
    for sp, (lo, hi) in bends.items():
        below, above = max(lo - 0.1, 0.02), min(hi + 0.1, 0.92)
        autos = {"slow up 12 s": [(0, below), (2, below), (14, above), (30, above)], "slow down 12 s": [(0, above), (2, above), (14, below), (30, below)],
                 "flick up 50 ms": [(0, below), (8, below), (8.05, above), (30, above)], "flick down 50 ms": [(0, above), (8, above), (8.05, below), (30, below)]}
        for name, bp in autos.items():
            f = f"{a.scratch}/_a_{sp}_{name.replace(' ', '_')}.json"
            json.dump({"breakpoints": [{"t": t, "key": "decay", "value": v} for t, v in bp]}, open(f, "w"))
            rows += [(sp, name, f, m, mix) for m in ("pad", "drums", "stabs") for mix in (1, 0.5)]

    def jump(y, floor):
        n = len(y) // 480
        e = 10 * np.log10((y[:n * 480].reshape(n, 480, -1) ** 2).mean(axis=(1, 2)) + 1e-12)
        both = (e[:-1] > floor) & (e[1:] > floor)  # neighbours only: a mask across a gap would read a jump that isn't there
        return float(np.abs(np.diff(e))[both].max()) if both.any() else 0.0

    def one(j):
        sp, name, f, m, mix = j
        src = float_src(m)
        sets = [f"space={sp}", f"mix={mix}"]
        o, _ = render(a.old, src, sets, auto=f)
        n, _ = render(a.new, src, sets, auto=f)
        return sp, name, m, mix, mc.clicks(o), mc.clicks(n), jump(o, -50.0), jump(n, -46.0), pk(n)
    res = pmap(one, rows)
    for sp in SPACES:
        for mix in (1, 0.5):
            r = [x for x in res if x[0] == sp and x[3] == mix]
            print(f"  {sp:6s} MIX {mix:3}: click worst {max(x[4] for x in r):5.1f} -> {max(x[5] for x in r):5.1f}; most raised by {max(x[5] - x[4] for x in r):+.1f}; step worst {max(x[6] for x in r):4.1f} -> {max(x[7] for x in r):4.1f}; peak {max(x[8] for x in r):+.1f}")
    print("A ringing tail as DECAY comes down: the pad into silence from a long tail (T60 about 40 s), DECAY then turned to short at 9 s, over 50 ms or 3 s. The 100 ms level's rise above the lowest it has reached since the turn (dB; the tail mustn't climb), and the level 3 s after, against v1.0.72.")
    x, sr = sf.read(f"{DRY}/pad.wav", always_2d=True, dtype="float64")
    in_ = f"{a.scratch}/_pad8.wav"
    z = np.zeros((int(30 * SR), 2))
    z[:int(8 * SR)] = x[:int(8 * SR)]
    z[int(7.9 * SR):int(8 * SR)] *= np.linspace(1, 0, int(0.1 * SR))[:, None]
    sf.write(in_, z, SR, subtype="FLOAT")
    for sp in SPACES:
        lo, hi = RANGE[sp]
        for dur in (0.05, 3.0):
            s_hi, s_lo = knob(0.8 * np.log(min(40.0, hi * 0.9) / lo) / np.log(hi / lo)), knob(0.8 * np.log(max(4.0, lo * 2) / lo) / np.log(hi / lo))
            f = f"{a.scratch}/_a_tail.json"
            json.dump({"breakpoints": [{"t": 0, "key": "decay", "value": s_hi}, {"t": 9, "key": "decay", "value": s_hi}, {"t": 9 + dur, "key": "decay", "value": s_lo}, {"t": 30, "key": "decay", "value": s_lo}]}, open(f, "w"))
            line = []
            for tag, r in (("v1.0.72", a.old), ("v1.0.73", a.new)):
                y, _ = render(r, in_, [f"space={sp}", "mix=1"], auto=f)
                b = int(0.1 * SR)
                st = int(9 * SR)
                e = 10 * np.log10((y[st:st + 40 * b].reshape(-1, b, 2) ** 2).mean(axis=(1, 2)) + 1e-14)
                live = e > -80
                rise = float((e - np.minimum.accumulate(e))[live].max()) if live.any() else 0.0
                line.append(f"{tag} rise {rise:+4.1f}, at +3 s {e[30] - e[0]:+6.1f} dB")
            print(f"  {sp:6s} over {dur:4.2f} s: " + " | ".join(line))


def hot(tier="_hot", what="Played hot (each phrase to -1 dBFS)"):
    print(f"{what}, WEAR 0: the limiter over 1 dB (% of samples) and over 0.1 dB, click frames per 10 s (dry's: pad 2.3, drums 3.1, stabs 3.1, piano 0), the module's peak (x0.874, MIX 40 to 80 %, the worst), v1.0.72 -> v1.0.73.")
    cells = [(sp, s) for sp in SPACES for s in (0.3, 0.5, 0.75, 1.0)]

    def one(j):
        sp, s, m = j
        src = float_src(m + tier)
        d = sf.read(src, always_2d=True, dtype="float64")[0]
        k = [f"space={sp}", f"decay={knob(s):.5f}"]
        o, lo = render(a.old, src, k + ["mix=1"])
        n, ln = render(a.new, src, k + ["mix=1"])
        mod = lambda w: max(pk(0.874 * (np.cos(v * np.pi / 2) * d[:len(w)] + np.sin(v * np.pi / 2) * w[:len(d)])) for v in (0.4, 0.5, 0.6, 0.7, 0.8))
        return sp, s, m, lo, ln, n10(o), n10(n), mod(o), mod(n), mc.clicks(o), mc.clicks(n)
    res = pmap(one, [(sp, s, m) for sp, s in cells for m in SOURCES])
    print("SPACE  scale src    lim>1 %        lim>0.1 %      n10/10 s     worst click    module's worst peak")
    for r in res:
        sp, s, m, lo, ln, no, nn, po, pn, co, cn = r
        print(f"{sp:6s} {s:4.2f} {m:6s} {lo.get('over1', 0) * 100:5.1f} -> {ln.get('over1', 0) * 100:5.1f}  {lo.get('over01', 0) * 100:5.1f} -> {ln.get('over01', 0) * 100:5.1f}  {no:4.1f} -> {nn:4.1f}  {co:5.1f} -> {cn:5.1f}  {po:+5.2f} -> {pn:+5.2f}")
    for sp in SPACES:
        r = [x for x in res if x[0] == sp]
        print(f"  {sp}: cells with lim>1 dB {sum(x[3].get('over1', 0) > 0 for x in r)} -> {sum(x[4].get('over1', 0) > 0 for x in r)} of {len(r)}, worst {max(x[3].get('over1', 0) for x in r) * 100:.1f} -> {max(x[4].get('over1', 0) for x in r) * 100:.1f} %; module's peak worst {max(x[7] for x in r):+.2f} -> {max(x[8] for x in r):+.2f}")


def mix():
    print("MIX noon and full, own level: the LU over the dry and the peak, v1.0.72 -> v1.0.73 (over the SPACEs, DECAY scale 0.3, 0.5, 0.75, endless, pad and drums); and MIX 0 (dry only) must be the same bits.")

    def one(j):
        sp, s, m = j
        src = float_src(m)
        k = [f"space={sp}", f"decay={knob(s):.5f}"]
        ld = integrated(sf.read(src, always_2d=True)[0])
        out = [sp, s, m]
        for mx in (0.5, 1):
            o, _ = render(a.old, src, k + [f"mix={mx}"])
            n, _ = render(a.new, src, k + [f"mix={mx}"])
            out += [integrated(o) - ld, integrated(n) - ld, pk(o), pk(n)]
        z0, _ = render(a.old, src, k + ["mix=0"])
        z1, _ = render(a.new, src, k + ["mix=0"])
        return out + [bool(np.array_equal(z0, z1))]
    res = pmap(one, [(sp, s, m) for sp in SPACES for s in (0.3, 0.5, 0.75, 1.0) for m in ("pad", "drums")])
    for sp in SPACES:
        for s in (0.3, 0.5, 0.75, 1.0):
            r = [x for x in res if x[0] == sp and x[1] == s]
            mv = lambda i: float(np.mean([x[i] for x in r]))
            print(f"  {sp:6s} scale {s:4.2f}: noon {mv(3):+5.1f} -> {mv(4):+5.1f} LU, peak {max(x[5] for x in r):+5.1f} -> {max(x[6] for x in r):+5.1f}; full {mv(7):+5.1f} -> {mv(8):+5.1f} LU, peak {max(x[9] for x in r):+5.1f} -> {max(x[10] for x in r):+5.1f}")
    print("  MIX 0 identical: " + str(all(x[11] for x in res)))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("what")
    ap.add_argument("--new", required=True)
    ap.add_argument("--old", required=True)
    ap.add_argument("--v70")
    ap.add_argument("--scratch", required=True)
    a = ap.parse_args()
    {"picks": picks, "steady": steady, "turn": turn, "hot": hot, "own": lambda: hot("", "At the material's own level"), "mix": mix}[a.what]()
