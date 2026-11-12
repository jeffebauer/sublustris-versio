#!/usr/bin/env python3
"""v1.0.74's gates (ADR 0050), renders/plate_hit's checks with the hook gone: A is the renderer before the port, D the port, PLATE:

1. The hit: one click, DECAY noon, DEPTH 0, WEAR 0, MOTION noon, SIZE 0 / noon / full: C80 (dB, "up front"), peak (ms),
   rise (ms), NED (echo density, 1 smooth), kurtosis (0 smooth, high grainy), the research note's measures (section 4).
2. The stereo pick (renders/plate_stereo, "more cohesive"): a hard-left burst, L over R (dB) in 0-20, 20-50, 50-100,
   100-300 ms. The first answer must stay on the right.
3. Metal: noise hits, the tail 0.3-3.3 s after: the renderer's calibrated ringing (dB, SPEC's check, flagged from 15),
   the largest raw peak over its 1/3-octave neighbourhood (tone_character.metallic), and upper_peaks (the fuller-tail
   note's "reedy" lines at 1.6-6.4 kHz, per mille; Valhalla's plates 12.4-12.6) at DECAY 9 o'clock, noon, 3 o'clock,
   endless x SIZE 0, noon, full. HALL at the same settings, for how far apart the two are. Held 1 kHz: the waver (cents).
4. Clicks, 1 ms frames above 3 kHz (motion_radio_checks.clicks, whole file; the worst over its neighbours): SIZE turned
   by hand through release's knob path on a pad and on stabs: slow turn, flicks, a slow LFO on CV, moves while frozen
   and Paint and the unfreeze, endless, a full shrink; the crossfade path and the glide path both. A plain freeze and
   unfreeze at SIZE 0, noon and full.
5. Colouring: at DEPTH 0, BLEND noon (mix 0.5), the early tap arrives with the dry: a click's first 5 ms against the dry
   alone, the largest move of the 1/3-octave spectrum (dB), and the wet's share of it.
6. A SIZE flick's level: does the wet jump? RMS 100 ms before a crossfaded move against 100 ms after, per version.

Usage: IV_RENDER_A=<before> IV_RENDER_D=<the port> <venv>/bin/python tools/pages/plate_hit_gates.py <out.txt> [section digits, default all]
"""
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc  # noqa: E402

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
R = {"A": os.environ["IV_RENDER_A"], "D": os.environ["IV_RENDER_D"]}
SR = 48000
VERSIONS = "AD"
COMMON = ["space=PLATE", "era=0", "time=0", "drift=0.5", "tone=0.5", "attack=NORMAL"]
SIZES = (("SIZE0", 0.0), ("noon", 0.5), ("full", 1.0))
DECAYS = (("9oclock", 0.2), ("noon", 0.5), ("3oclock", 0.8))


def db(p):
    return 10 * np.log10(p + 1e-30)


def render(ver, src, tmp, sets, auto=None, mix=1.0, binary=None):
    """Returns (samples, the renderer's text)."""
    fd, out = tempfile.mkstemp(suffix=".wav", dir=tmp)
    os.close(fd)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    args = [binary or R[ver], str(src), out, "--set", f"mix={mix}"]
    for s in sets:
        args += ["--set", s]
    af = None
    if auto:
        af = out + ".json"
        json.dump(auto, open(af, "w"))
        args += ["--auto", af]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if af:
        os.unlink(af)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    y = sf.read(out, always_2d=True, dtype="float64")[0]
    os.unlink(out)
    return y, p.stdout + p.stderr


# ---- The hit -----------------------------------------------------------------------------------------------------
def onset(y, t0, end):
    seg = y[int(t0 * SR):int(end * SR)]
    env = np.abs(seg).max(1)
    arr = int(np.argmax(env > env.max() * 0.05))
    w, h = int(0.02 * SR), int(0.01 * SR)
    neds = []
    for ch in range(2):
        x = seg[arr:arr + int(0.3 * SR), ch]
        neds.append([np.mean(np.abs(x[i:i + w]) > x[i:i + w].std()) / 0.3173 if x[i:i + w].std() > 0 else 0
                     for i in range(0, len(x) - w, h)])
    k = int(0.01 * SR)
    kk = []
    for ch in range(2):
        x = seg[arr:arr + int(0.3 * SR), ch]
        for i in range(0, len(x) - k, k):
            s = x[i:i + k]
            kk.append(((s - s.mean()) ** 4).mean() / (s.var() ** 2 + 1e-30) - 3)
    r = int(0.005 * SR)
    p = (seg ** 2).sum(1)
    e = np.convolve(p, np.ones(r) / r, "same")
    ip = int(np.argmax(e[:int(min(0.8, end - t0) * SR)]))
    edb = db(e)
    t20, t3 = int(np.argmax(edb[:ip + 1] > edb[ip] - 20)), int(np.argmax(edb[:ip + 1] > edb[ip] - 3))
    c = int(0.08 * SR) + arr
    return dict(c80=float(db(p[:c].sum()) - db(p[c:].sum())), peak=ip / SR * 1000, rise=(t3 - t20) / SR * 1000,
                ned=float(np.mean(neds)), kurt=float(np.median(kk)), arrive=arr / SR * 1000)


# ---- Metal -------------------------------------------------------------------------------------------------------
def _smooth_third(P, f):
    c = np.concatenate([[0.0], np.cumsum(P)])
    lo = np.searchsorted(f, f * 2 ** (-1 / 6))
    hi = np.searchsorted(f, f * 2 ** (1 / 6), side="right")
    return (c[hi] - c[lo]) / np.maximum(hi - lo, 1)


def upper_peaks(seg, lo=1600.0, hi=6400.0):
    """Per mille of bins over +10 dB against the 1/3-octave-smoothed spectrum, both channels' mean (the fuller-tail note's)."""
    v = []
    for ch in range(seg.shape[1]):
        P = np.abs(np.fft.rfft(seg[:, ch] * np.hanning(len(seg)))) ** 2
        f = np.fft.rfftfreq(len(seg), 1 / SR)
        d = db(P) - db(_smooth_third(P, f))
        v.append(1000 * np.mean(d[(f >= lo) & (f < hi)] > 10))
    return float(np.mean(v))


def metallic(y, t0, t1):
    """As tone_character.metallic: Welch, each bin against the median within +-1/6 octave, 100 Hz-10 kHz. dB, Hz."""
    s = y[int(t0 * SR):int(t1 * SR)].mean(axis=1)
    n = 8192
    w = np.hanning(n)
    P = np.mean([np.abs(np.fft.rfft(s[i:i + n] * w)) ** 2 for i in range(0, len(s) - n + 1, n // 2)], axis=0)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    L = db(P)
    best, at = -99.0, 0.0
    for i in np.where((f >= 100) & (f <= 10000))[0]:
        a, b = np.searchsorted(f, f[i] * 2 ** (-1 / 6)), np.searchsorted(f, f[i] * 2 ** (1 / 6))
        p = L[i] - np.median(L[a:b + 1])
        if p > best:
            best, at = p, f[i]
    return float(best), float(at)


def cents_spread(seg, f0=1000.0, width=50.0):
    x = seg.mean(axis=1) * np.hanning(len(seg))
    T = np.abs(np.fft.rfft(x, 1 << 20)) ** 2
    f = np.fft.rfftfreq(1 << 20, 1 / SR)
    c = 1200 * np.log2(np.maximum(f, 1e-9) / f0)
    w = np.abs(c) <= width
    p = T[w] - T[w].min()
    m = np.sum(p * c[w]) / np.sum(p)
    return float(np.sqrt(np.sum(p * (c[w] - m) ** 2) / np.sum(p)))


# ---- SIZE turned by hand -----------------------------------------------------------------------------------------
def hand(pts):
    def at(t):
        for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
            if t <= t1:
                u = (t - t0) / (t1 - t0) if t > t0 else 0.0
                return v0 + (v1 - v0) * u * u * (3.0 - 2.0 * u)
        return pts[-1][1]
    return at


def knob_steps(raw, seconds):
    """Release's knob path (firmware/main.cpp): 1 ms blocks, a 10 ms one-pole, sent past the 0.003 deadband."""
    k, smooth, sent, steps = 1.0 / 11.0, None, None, []
    for b in range(int(seconds * 1000)):
        r = raw(b / 1000.0)
        smooth = r if smooth is None else smooth + k * (r - smooth)
        if sent is None or abs(smooth - sent) > 0.003:
            sent = smooth
            steps.append((b / 1000.0, smooth))
    bp, prev = [{"t": 0.0, "key": "size", "value": steps[0][1]}], steps[0][1]
    for t, v in steps[1:]:
        bp += [{"t": t - 1e-6, "key": "size", "value": prev}, {"t": t, "key": "size", "value": v}]
        prev = v
    return bp


LFO = lambda t: 0.5 - 0.25 * math.cos(2 * math.pi * 0.15 * max(0.0, t - 3.0))  # noqa: E731
MOVES = [
    ("slow turn up", ["decay=0.7"], {"breakpoints": knob_steps(hand([(0, 0.15), (5.0, 0.15), (9.0, 0.9)]), 26)}),
    ("quick flicks", ["decay=0.7"], {"breakpoints": knob_steps(hand(
        [(0, 0.3), (5.0, 0.3), (5.12, 0.85), (9.0, 0.85), (9.12, 0.25), (12.0, 0.25), (12.1, 0.7), (12.5, 0.7), (12.6, 0.35)]), 26)}),
    ("slow LFO on CV", ["decay=0.7"], {"breakpoints": [{"t": 0.0, "key": "size", "value": LFO(0.0)}] + [
        {"t": b / 1000.0, "key": "size", "value": LFO(b / 1000.0)} for b in range(3000, 26000)]}),
    ("frozen moves, Paint, unfreeze", ["decay=0.7"], {"breakpoints": knob_steps(hand(
        [(0, 0.3), (8.0, 0.3), (8.15, 0.85), (11.0, 0.85), (11.15, 0.3)]), 26), "buttons": [[5.0, 5.1], [13.0, 15.0], [20.0, 20.1]]}),
    ("endless moves", ["decay=1.0"], {"breakpoints": knob_steps(hand(
        [(0, 0.3), (8.0, 0.3), (8.12, 0.85), (12.0, 0.85), (12.12, 0.2), (16.0, 0.2), (16.12, 0.6)]), 26)}),
    ("full shrink and back", ["decay=0.3"], {"breakpoints": knob_steps(hand([(0, 1.0), (6.0, 1.0), (6.15, 0.0), (12.0, 0.0), (12.15, 1.0)]), 26)}),
]


if __name__ == "__main__":
    out = Path(sys.argv[1])
    sections = sys.argv[2] if len(sys.argv) > 2 else "123456"
    tmpd = tempfile.TemporaryDirectory(dir=str(out.parent))
    tmp = Path(tmpd.name)
    lines = [f"renderer: {R}"]

    def say(s=""):
        lines.append(s)
        print(s, flush=True)

    pool = ThreadPoolExecutor(max_workers=int(os.environ.get("IV_JOBS", "7")))
    click = np.zeros((int(6 * SR), 2)); click[int(0.5 * SR)] = 0.5
    sf.write(str(tmp / "click.wav"), click.astype(np.float32), SR, subtype="FLOAT")
    left = np.zeros((int(4 * SR), 2)); n = int(0.003 * SR)
    left[int(0.5 * SR):int(0.5 * SR) + n, 0] = np.random.default_rng(1).normal(0, 0.2, n)
    sf.write(str(tmp / "left.wav"), left.astype(np.float32), SR, subtype="FLOAT")
    rng = np.random.default_rng(3)
    hit = np.zeros((int(9 * SR), 2))
    for k in range(3):
        hit[k * SR:k * SR + 1200, :] = (0.6 * rng.uniform(-1, 1, 1200) * np.exp(-np.arange(1200) / 200.0))[:, None]
    sf.write(str(tmp / "hits.wav"), hit.astype(np.float32), SR, subtype="FLOAT")
    sf.write(str(tmp / "pad.wav"), mc.pad(17.0, 9.0), SR, subtype="FLOAT")
    st = sf.read(os.environ.get("IV_STABS", REPO_.split("/.claude/worktrees/")[0] + "/renders/_material/stabs.wav"), always_2d=True)[0]
    sf.write(str(tmp / "stabs.wav"), st[:int(26 * SR)].astype(np.float32), SR, subtype="FLOAT")
    tn = np.arange(int(20 * SR)) / SR
    tone = 0.25 * np.sin(2 * np.pi * 1000 * tn); tone[:480] *= np.linspace(0, 1, 480)
    sf.write(str(tmp / "tone.wav"), np.stack([tone, tone], 1).astype(np.float32), SR, subtype="FLOAT")

    if "1" in sections:
        say("\n1. The hit (a click; DECAY noon, DEPTH 0): C80 dB / peak ms / rise ms / NED / kurtosis")

        def job(c):
            v, (nm, z) = c
            y, _ = render(v, tmp / "click.wav", tmp, COMMON + [f"size={z}", "decay=0.5"])
            return c, onset(y, 0.5, 5.9)
        res = dict(pool.map(job, [(v, s) for v in VERSIONS for s in SIZES]))
        for s in SIZES:
            for v in VERSIONS:
                d = res[(v, s)]
                say(f"  {s[0]:6s} {v}  C80 {d['c80']:+5.1f}  peak {d['peak']:5.0f}  rise {d['rise']:4.0f}  NED {d['ned']:.2f}  kurt {d['kurt']:5.1f}")

    if "2" in sections:
        say("\n2. The stereo pick: a hard-left burst, L over R in dB (negative: on the right). DECAY noon, DEPTH 0")
        W = {"0-20": (0, 20), "20-50": (20, 50), "50-100": (50, 100), "100-300": (100, 300)}

        def job(c):
            v, (nm, z) = c
            y, _ = render(v, tmp / "left.wav", tmp, COMMON + [f"size={z}", "decay=0.5"])
            r = {}
            for k, (a, b) in W.items():
                s = y[int(0.5 * SR) + a * 48:int(0.5 * SR) + b * 48]
                r[k] = float(db((s[:, 0] ** 2).sum()) - db((s[:, 1] ** 2).sum()))
            return c, r
        res = dict(pool.map(job, [(v, s) for v in VERSIONS for s in SIZES]))
        for s in SIZES:
            for v in VERSIONS:
                say(f"  {s[0]:6s} {v}  " + "  ".join(f"{k} ms {res[(v, s)][k]:+7.1f}" for k in W))

    if "3" in sections:
        say("\n3. Metal: 3 noise hits, the tail 0.3-3.3 s after. ringing dB (flagged from 15) / largest raw peak dB at Hz / lines per mille")
        dec = DECAYS + (("endless", 1.0),)

        def job(c):
            sp, v, (dn, d), (zn, z) = c
            y, txt = render(v, tmp / "hits.wav", tmp, [f"space={sp}"] + COMMON[1:] + [f"size={z}", f"decay={d}"])
            m = re.search(r"ringing[=: ]+(-?[\d.]+|nan|inf)", txt)
            pk = metallic(y, 2.3, 5.3)
            return c, (m.group(1) if m else "?", pk, upper_peaks(y[int(2.3 * SR):int(5.3 * SR)]))
        cases = [("PLATE", v, d, s) for v in VERSIONS for d in dec for s in SIZES] + [("HALL", "A", d, s) for d in dec for s in SIZES]
        res = dict(pool.map(job, cases))
        for d in dec:
            for s in SIZES:
                say(f"  DECAY {d[0]:8s} SIZE {s[0]:6s} " + "  ".join(
                    f"{v} {res[('PLATE', v, d, s)][0]:>5s} {res[('PLATE', v, d, s)][1][0]:4.1f}@{res[('PLATE', v, d, s)][1][1]:5.0f} {res[('PLATE', v, d, s)][2]:5.1f}" for v in VERSIONS)
                    + f"  | HALL {res[('HALL', 'A', d, s)][0]:>5s} {res[('HALL', 'A', d, s)][1][0]:4.1f} {res[('HALL', 'A', d, s)][2]:5.1f}")
        say("  The mean of the lines: " + "  ".join(
            f"{v} {np.mean([res[('PLATE', v, d, s)][2] for d in dec for s in SIZES]):.1f}" for v in VERSIONS)
            + f"  HALL {np.mean([res[('HALL', 'A', d, s)][2] for d in dec for s in SIZES]):.1f} (Valhalla's plates 12.4-12.6, on music)")
        say("  A held 1 kHz at DECAY 3 o'clock, SIZE 0.6: the wet's pitch spread (cents, 10-20 s), lower is steadier")

        def job2(v):
            y, _ = render(v, tmp / "tone.wav", tmp, COMMON + ["size=0.6", "decay=0.8"])
            return v, cents_spread(y[10 * SR:20 * SR])
        say("    " + "  ".join(f"{v} {c:5.2f}" for v, c in pool.map(job2, VERSIONS)))

    if "4" in sections:
        say("\n4. Clicks: the worst 1 ms frame above 3 kHz over its neighbours, dB, whole file (a click stands well above A's in the same case)")

        def job(c):
            v, m, (name, sets, auto) = c
            y, _ = render(v, tmp / f"{m}.wav", tmp, COMMON + ["size=0.3"] + sets, auto)
            return c, mc.clicks(y), bool(np.isfinite(y).all()), float(np.max(np.abs(y)))
        cases = [(v, m, mv) for mv in MOVES for m in ("pad", "stabs") for v in VERSIONS]
        res = {(c[0], c[1], c[2][0]): (a, b, p) for c, a, b, p in pool.map(job, cases)}
        worst = {v: 0.0 for v in VERSIONS}
        for mv in MOVES:
            for m in ("pad", "stabs"):
                a = res[("A", m, mv[0])][0]
                say(f"  {mv[0]:30s} {m:6s} " + "  ".join(f"{v} {res[(v, m, mv[0])][0]:5.1f}" for v in VERSIONS)
                    + ("" if all(res[(v, m, mv[0])][1] for v in VERSIONS) else "  NOT FINITE"))
                for v in VERSIONS:
                    worst[v] = max(worst[v], res[(v, m, mv[0])][0] - a)
        say("  the most any version stands above A in any case: " + "  ".join(f"{v} {worst[v]:+.1f} dB" for v in VERSIONS[1:]))
        say("  A plain freeze at 6 s, unfreeze at 12 s, on the pad and the stabs, DECAY 9 o'clock, 0.7 and endless")

        def job3(c):
            v, m, (zn, z), d = c
            y, _ = render(v, tmp / f"{m}.wav", tmp, COMMON + [f"size={z}", f"decay={d}"], {"breakpoints": [], "buttons": [[6.0, 6.1], [12.0, 12.1]]})
            return c, mc.clicks(y)
        res3 = dict(pool.map(job3, [(v, m, s, d) for v in VERSIONS for m in ("pad", "stabs") for s in SIZES for d in (0.2, 0.7, 1.0)]))
        for m in ("pad", "stabs"):
            for s in SIZES:
                for d in (0.2, 0.7, 1.0):
                    say(f"    {m:6s} SIZE {s[0]:6s} DECAY {d:3.1f} " + "  ".join(f"{v} {res3[(v, m, s, d)]:5.1f}" for v in VERSIONS))

    if "5" in sections:
        say("\n5. Colouring: DEPTH 0, BLEND noon: dry + wet's first 5 ms against the dry alone, a click; the 1/3-octave spectrum's largest move, dB, 200 Hz-10 kHz, and the first 5 ms of wet alone re the dry click (dB)")

        def job(c):
            v, (zn, z), (dn, d) = c
            y, _ = render(v, tmp / "click.wav", tmp, COMMON + [f"size={z}", f"decay={d}"], mix=0.5)
            w, _ = render(v, tmp / "click.wav", tmp, COMMON + [f"size={z}", f"decay={d}"], mix=1.0)
            d0, _ = render(v, tmp / "click.wav", tmp, COMMON + [f"size={z}", f"decay={d}"], mix=0.0)
            a = int(0.5 * SR); n5 = int(0.005 * SR)
            f = np.fft.rfftfreq(1 << 14, 1 / SR)
            P = lambda s: np.abs(np.fft.rfft(s[a - 24:a + n5].mean(axis=1), 1 << 14)) ** 2  # noqa: E731
            sm = lambda p: db(_smooth_third(p, f))  # noqa: E731
            sel = (f > 200) & (f < 10000)
            return c, float(np.max(np.abs(sm(P(y))[sel] - sm(P(d0))[sel]))), float(db(np.sum(w[a - 24:a + n5] ** 2)) - db(np.sum(d0[a - 24:a + n5] ** 2)))
        res = {c: (a, b) for c, a, b in pool.map(job, [(v, s, DECAYS[1]) for v in VERSIONS for s in SIZES])}
        for s in SIZES:
            say(f"  SIZE {s[0]:6s} " + "  ".join(f"{v} move {res[(v, s, DECAYS[1])][0]:4.2f} dB, wet {res[(v, s, DECAYS[1])][1]:+6.1f} dB" for v in VERSIONS))

    if "6" in sections:
        say("\n6. A crossfaded SIZE move's level on a held pad, DECAY 0.7, SIZE 0.3 -> 0.85 at 5 s: 3.9-4.9 s before it, 5.6-6.6 s after (dB)")

        def job(v):
            y, _ = render(v, tmp / "pad.wav", tmp, COMMON + ["size=0.3", "decay=0.7"], MOVES[1][2])
            return v, float(db(np.mean(y[int(3.9 * SR):int(4.9 * SR)] ** 2))), float(db(np.mean(y[int(5.6 * SR):int(6.6 * SR)] ** 2)))
        for v, a, b in pool.map(job, VERSIONS):
            say(f"  {v}  before {a:6.1f}  after {b:6.1f}  move {b - a:+5.2f} dB")

    out.write_text("\n".join(lines) + "\n")
    tmpd.cleanup()
