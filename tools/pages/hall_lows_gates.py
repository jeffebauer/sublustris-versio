#!/usr/bin/env python3
"""v1.0.76's gates (ADR 0052): the port of the owner's pick D on renders/hall_lows, against the prototype's hook and v1.0.75.

hook:     patch and bass at SIZE noon, 0.35 and 0.8: the port against the prototype's iv_render with IV_HALL_LOWS=D;
identity: PLATE and CANYON to the bit against v1.0.75, 72 renders;
lean:     the fine pitch scan (renders/hall_lows' scan.py), six hits a pitch, 40-240 Hz, 10 cents apart;
waver:    a held sine at 60 and 110 Hz, the wet's pitch spread 12-32 s in, cents;
clicks:   the worst 1 ms frame over its neighbours on SIZE, DECAY and SPACE moves: pad above 3 kHz, low notes above 600 Hz;
flick:    five SIZE flicks on a held note: how much of the sound is off the note;
holds:    ten-minute endless holds of low material: within 3 dB, never above the 60-70 s window + 1 dB, never under -60 dBFS.

tools/pages/energy_check.py (--bin port --ref-bin v1.0.75) is run alongside.
Usage: <venv>/bin/python tools/pages/hall_lows_gates.py --port <iv_render> --ref <v1.0.75 iv_render> --proto <prototype iv_render> [part ...]
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc

MAIN = os.path.abspath(os.path.join(HERE, "../..")).split("/.claude/worktrees/")[0]
SRC = MAIN + "/renders/_material"
SR = 48000
PARTS = ("hook", "identity", "lean", "waver", "clicks", "flick", "holds")


def pr(*a):
    print(*a, flush=True)


def pmap(fn, items, jobs=9):
    with ThreadPoolExecutor(jobs) as ex:
        return list(ex.map(fn, items))


def f_of_midi(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def midi_of_f(f):
    return 69 + 12 * np.log2(f / 440)


def saw_blep(f0s):
    ph = np.cumsum(f0s / SR) % 1.0
    dt = f0s / SR
    s = 2 * ph - 1
    m1 = ph < dt
    s[m1] -= ph[m1] / dt[m1] * 2 - (ph[m1] / dt[m1]) ** 2 - 1
    m2 = ph > 1 - dt
    x = (ph[m2] - 1) / dt[m2]
    s[m2] -= x * x + 2 * x + 1
    return s


def svf_lp(x, fc, q):
    g = np.tan(np.pi * np.clip(fc, 5, SR * 0.45) / SR)
    k = 1.0 / q
    a1 = 1 / (1 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    ic1 = ic2 = 0.0
    y = np.zeros_like(x)
    for i in range(len(x)):
        v3 = x[i] - ic2
        v1 = a1[i] * ic1 + a2[i] * v3
        v2 = ic2 + a2[i] * ic1 + a3[i] * v3
        ic1 = 2 * v1 - ic1
        ic2 = 2 * v2 - ic2
        y[i] = v2
    return y


def voice(n, notes, q=3.0, fc_floor=80.0, fc_mult=10.0, attack=0.004, decay=0.3):
    """The patch: an oscillator into a resonant low-pass that an envelope opens on each note. notes: (start_s, dur_s, midi)."""
    f0 = np.zeros(n) + f_of_midi(notes[0][2])
    fc = np.zeros(n)
    amp = np.zeros(n)
    for st, du, m in notes:
        a = int(st * SR)
        b = min(n, a + int(du * SR))
        f0[a:] = f_of_midi(m)
        t = np.arange(n - a) / SR
        env = np.minimum(t / attack, 1.0) * np.exp(-np.maximum(t - attack, 0) / decay)
        top = max(f_of_midi(m) * fc_mult, 400.0)
        fc[a:] = fc_floor * (top / fc_floor) ** env
        g = np.ones(n - a)
        rel = np.arange(n - a) - (b - a)
        g[rel > 0] = np.exp(-rel[rel > 0] / (0.12 * SR))
        amp[a:] = g
    return svf_lp(saw_blep(f0), fc, q) * amp


def norm(x, peak_db):
    return x / (np.max(np.abs(x)) + 1e-12) * 10 ** (peak_db / 20)


def pedal(f0, reps=2, pk=-15.0, tail=8.0):
    """A low note with a few around it, 0.6 s a note, on and on."""
    root = midi_of_f(f0)
    pat = [0, 0, 7, 0, 12, 0, 3, 5, 0, 0, 7, 10, 0, 0, 3, 0]
    t, notes = 0.1, []
    for _ in range(reps):
        for p in pat:
            notes.append((t, 0.4, root + p))
            t += 0.6
    return norm(voice(int((t + tail) * SR), notes), pk)


def walk(freqs, step=1.2, pk=-15.0, tail=8.0):
    notes = [(0.1 + i * step, step * 0.7, midi_of_f(f)) for i, f in enumerate(freqs)]
    return norm(voice(int((0.1 + len(freqs) * step + tail) * SR), notes), pk)


def hits(f0, n=6, pk=-15.0, secs=9.0):
    return norm(voice(int(secs * SR), [(0.1 + i * 1.0, 0.6, midi_of_f(f0)) for i in range(n)]), pk)


def knobs(decay, wear=0.5, size=0.5, space="HALL", drift=0.5):
    return [f"space={space}", "mix=1", f"decay={decay}", f"era={wear}", f"size={size}", "tone=0.5", f"drift={drift}", "time=0.5"]


def render(binary, x, sets, env_extra=None, auto=None):
    """x: an array or a path. Returns the stereo render as float64."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra or {})
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        out = f.name
    inp = x
    tmps = [out]
    if not isinstance(x, str):
        inp = out + ".in.wav"
        tmps.append(inp)
        y = np.asarray(x, dtype=np.float32)
        sf.write(inp, np.stack([y, y], 1) if y.ndim == 1 else y, SR, subtype="FLOAT")
    args = [binary, inp, out, "--block", "48"]
    for s in sets:
        args += ["--set", s]
    if auto:
        af = out + ".json"
        tmps.append(af)
        with open(af, "w") as g:
            json.dump({"breakpoints": [{"t": t, "key": k, "value": v} for t, k, v in auto]}, g)
        args += ["--auto", af]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(p.stderr[-400:])
    y = sf.read(out, always_2d=True, dtype="float64")[0]
    for t in tmps:
        os.unlink(t)
    return y


def hook(a):
    patch = pedal(90.8)
    cases = [(n, m, s, d, w) for n, m in (("patch", patch), ("bass", SRC + "/bass.wav"))
             for s, d, w in ((0.5, 0.7, 0.0), (0.35, 0.7, 0.0), (0.8, 0.6, 0.5))]
    def one(c):
        n, m, s, d, w = c
        k = knobs(d, w, s)
        h = render(a.proto, m, k, env_extra={"IV_HALL_LOWS": "D"})
        p = render(a.port, m, k)
        return c, 10 * np.log10(np.mean((h - p) ** 2) / np.mean(h ** 2) + 1e-30)
    for c, d in pmap(one, cases, 6):
        pr(f"  port vs hook, {c[0]} SIZE {c[2]} DECAY {c[3]} WEAR {c[4]}: {d:.1f} dB re the hook")


def identity(a):
    ph = pedal(90.8, reps=1)
    cases = [(sp, m, size, dec, wear) for sp in ("PLATE", "CANYON")
             for m in (ph, SRC + "/bass.wav", SRC + "/pad.wav", SRC + "/piano.wav") for size in (0.35, 0.5, 0.8)
             for dec, wear in ((0.6, 0.5), (0.7, 0.0), (0.97, 0.5))]
    def one(c):
        k = knobs(c[3], c[4], c[2], space=c[0])
        return np.array_equal(render(a.ref, c[1], k), render(a.port, c[1], k))
    r = pmap(one, cases)
    pr(f"  PLATE and CANYON to the bit against v1.0.75: {sum(r)}/{len(cases)}")


def db(v):
    return 20 * np.log10(np.maximum(v, 1e-12))


def rms(x):
    return np.sqrt(np.mean(x ** 2) + 1e-30)


def lean(a):
    freqs = 40 * 2 ** (np.arange(0, 1200 * np.log2(240 / 40), 10.0) / 1200)
    stim = dict(zip(freqs, pmap(hits, freqs)))
    for size, dec, wear in ((0.5, 0.7, 0.0), (0.5, 0.6, 0.5), (0.35, 0.7, 0.0), (0.8, 0.7, 0.0)):
        for name, b in (("v1.0.75", a.ref), ("port", a.port)):
            def one(f):
                y = render(b, stim[f], knobs(dec, wear, size))
                return db(rms(y[:, 0])), db(rms(y[:, 1]))
            rows = np.array(pmap(one, freqs))
            rL, rR = rows.T
            lr = rL - rR
            lvl = 10 * np.log10(0.5 * (10 ** (rL / 10) + 10 ** (rR / 10)))
            med = np.median(lvl)
            top = []
            for i in np.argsort(-lvl):
                if all(abs(1200 * np.log2(freqs[i] / freqs[t])) > 60 for t in top):
                    top.append(int(i))
                if len(top) >= 10:
                    break
            pr(f"  SIZE {size} DECAY {dec} WEAR {wear} {name:8s}: over 6 dB {np.mean(np.abs(lr) > 6) * 100:4.1f} %, furthest {lr.max():+5.1f}@{freqs[lr.argmax()]:.1f} / {lr.min():+5.1f}@{freqs[lr.argmin()]:.1f} dB, "
               f"ring {np.mean(lvl[top]) - med:+.1f} (top {lvl.max() - med:+.1f}), median level {med:.1f}")


def cents_spread(seg, f0, width=50.0):
    x = seg.mean(axis=1) * np.hanning(len(seg))
    N = 1 << 21
    T = np.abs(np.fft.rfft(x, N)) ** 2
    f = np.fft.rfftfreq(N, 1 / SR)
    c = 1200 * np.log2(np.maximum(f, 1e-9) / f0)
    w = np.abs(c) <= width
    p = T[w]
    m = np.sum(p * c[w]) / np.sum(p)
    return float(np.sqrt(np.sum(p * (c[w] - m) ** 2) / np.sum(p)))


def waver(a):
    t = np.arange(36 * SR) / SR
    for f0 in (60.0, 110.0):
        x = 10 ** (-15 / 20) * np.sin(2 * np.pi * f0 * t)
        for size in (0.5, 0.35, 0.8):
            r = pmap(lambda b: cents_spread(render(b, x, knobs(0.7, 0.0, size))[12 * SR:32 * SR], f0), (a.ref, a.port), 2)
            pr(f"  {f0:.0f} Hz SIZE {size} DECAY 0.7 WEAR 0: v1.0.75 {r[0]:.2f}, port {r[1]:.2f} cents")


def clicks(a):
    t = np.arange(26 * SR) / SR
    low = 10 ** (-12 / 20) * (0.6 * np.sin(2 * np.pi * 70 * t) + 0.4 * np.sin(2 * np.pi * 110.3 * t))
    low *= np.minimum(1, t / 0.2) * np.minimum(1, np.maximum(0, 22 - t) / 0.5)
    pad = mc.pad(22.0, 4.0)
    moves = {
        "SIZE slow .2 -> .9 -> .2": (dict(size=0.2), [(2, "size", 0.2), (11, "size", 0.9), (20, "size", 0.2)]),
        "SIZE across noon .4 -> .6": (dict(size=0.4), [(2, "size", 0.4), (6, "size", 0.6), (10, "size", 0.4), (14, "size", 0.6), (18, "size", 0.4)]),
        "SIZE flick .35 -> .8 (20 ms)": (dict(size=0.35), [(5, "size", 0.35), (5.02, "size", 0.8), (12, "size", 0.8), (12.02, "size", 0.35)]),
        "DECAY into endless .6 -> 1 -> .6": (dict(decay=0.6), [(2, "decay", 0.6), (11, "decay", 1.0), (20, "decay", 0.6)]),
        "DECAY flick .7 -> 1 (20 ms)": (dict(decay=0.7), [(6, "decay", 0.7), (6.02, "decay", 1.0), (14, "decay", 1.0), (14.02, "decay", 0.7)]),
        "SPACE HALL -> CANYON -> HALL": (dict(), [(6, "space", 0.5), (13, "space", 0.0)]),
    }
    for n, (base, auto) in moves.items():
        for mat, x, fc in (("pad", pad, 3000.0), ("low", low, 600.0)):
            s = dict(size=0.5, decay=0.7, wear=0.5)
            s.update(base)
            r = pmap(lambda b: mc.clicks(render(b, x, knobs(s["decay"], s["wear"], s["size"]), auto=auto), fc), (a.ref, a.port), 2)
            pr(f"  {n:34s} {mat}: v1.0.75 {r[0]:5.1f}, port {r[1]:5.1f} dB")


def flick(a):
    """Five SIZE flicks 0.1 <-> 0.9 on a held note (host/tests/test_fdn.cpp's move): the share of the left channel more than 3 % off the
    note, still (3-6 s) and through the flicks (6-8.5 s). The hook shows what the low tank's allpasses did while they followed the glided SIZE."""
    t = np.arange(9 * SR) / SR
    auto = [(0, "size", 0.1)]
    for k in range(5):  # breakpoints ramp, so each flick is a pair 1 ms apart
        auto += [(6 + 0.3 * k, "size", 0.1 if k % 2 == 0 else 0.9), (6.001 + 0.3 * k, "size", 0.9 if k % 2 == 0 else 0.1)]
    def off(seg, f0):
        S = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
        f = np.fft.rfftfreq(len(seg), 1 / SR)
        return 10 * np.log10(S[(f < f0 * 0.97) | (f > f0 * 1.03)].sum() / S.sum() + 1e-15)
    for f0 in (70.0, 110.0, 220.0, 440.0):
        x = 0.2 * np.sin(2 * np.pi * f0 * t)
        k = knobs(0.5, 0.0, 0.1)
        ys = pmap(lambda v: render(v[0], x, k, env_extra=v[1], auto=auto), ((a.ref, None), (a.proto, {"IV_HALL_LOWS": "D"}), (a.port, None)), 3)
        pr(f"  {f0:.0f} Hz, still / flicked, dB off the note: " + ",  ".join(
            f"{n} {off(y[3 * SR:6 * SR, 0], f0):.1f} / {off(y[6 * SR:int(8.5 * SR), 0], f0):.1f}" for n, y in zip(("v1.0.75", "hook", "port"), ys)))


def holds(a):
    s = walk([62, 80, 90.8, 115.8, 73, 98, 140, 55] * 2, step=1.2, pk=-6, tail=0.0)[:20 * SR]
    x = np.concatenate([np.stack([s, s], 1), np.zeros((600 * SR, 2))])
    jobs = [(n, b, size, wear) for n, b in (("v1.0.75", a.ref), ("port", a.port)) for size in (0.35, 0.5, 0.8) for wear in (0.0, 0.5)]
    def one(j):
        y = render(j[1], x, knobs(1.0, j[3], j[2]))
        w = np.array([10 * np.log10(np.mean(y[int(t * SR):int((t + 10) * SR)] ** 2) + 1e-30) for t in range(60, 610, 10)])
        lr = [10 * np.log10(np.mean(y[t * SR:(t + 10) * SR, 0] ** 2) / (np.mean(y[t * SR:(t + 10) * SR, 1] ** 2) + 1e-30) + 1e-30) for t in (60, 300, 600)]
        return j, w.max() - w.min(), w.max() - w[0], w.min(), w[0], lr, np.isfinite(y).all()
    bad = {}
    for j, spread, up, lo, first, lr, fin in pmap(one, jobs, 7):
        good = spread < 3 and up <= 1 and lo > -60 and fin
        bad[j[0]] = bad.get(j[0], 0) + (not good)
        pr(f"  {j[0]:8s} SIZE {j[2]} WEAR {j[3]}: first {first:6.1f} dB, spread {spread:4.2f}, above first {up:+5.2f}, lowest {lo:6.1f}; L/R at 60/300/600 s {lr[0]:+5.1f} {lr[1]:+5.1f} {lr[2]:+5.1f}" + ("" if good else "   FAILS"))
    pr("  " + ", ".join(f"{k} {6 - v}/6 hold" for k, v in bad.items()))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--proto", required=True)
    ap.add_argument("parts", nargs="*", default=list(PARTS))
    args = ap.parse_args()
    for part in args.parts or PARTS:
        pr(part + ":")
        globals()[part](args)
