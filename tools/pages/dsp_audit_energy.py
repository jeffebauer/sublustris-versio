"""Energy check for a loop prototype (the method of docs/research/still-lows.md), DSP audit 24 Oct 2026.

  sweep: a 1 s noise burst through every SPACE x SIZE 0/0.6/1 x DECAY 0.5/0.75/0.95 x DRIFT 0/.25/.5/.75/1,
         today (A) and the prototype (B). Per band (full, < 250 Hz, 600 Hz-2 kHz, > 4 kHz) and window
         (1.5-3, 3-6, 6-12, 12-20, 20-29 s): B's level re A's (worst = build-up against today), B's own growth
         (a later window above an earlier one), and each finite tail's decay slope per band, B re A.
  holds: 20 s of hard noise, then 10 minutes at DECAY 1, judged like test_massive infiniteHoldsTheFloor
         (10 s windows from 60 s: within 3 dB, no growth past +1 dB, not below -60 dBFS).
Usage: dsp_audit_energy.py sweep|holds ENV=VAL[,ENV=VAL] [key=value ...]   (extra --set for A and B, e.g. era=0.8)
"""
import os, sys, itertools
from multiprocessing import Pool
import numpy as np
from dsp_audit_lib import render, SCRATCH, SR, bandpass_fft

BANDS = (('full', 20, 20000), ('<250', 20, 250), ('600-2k', 600, 2000), ('>4k', 4000, 20000))
WINS = ((1.5, 3), (3, 6), (6, 12), (12, 20), (20, 29))
FLOOR = -80.0  # dBFS: windows below this are left out (the burst peaks near -10)


def levels(l, r):
    out = {}
    for name, lo, hi in BANDS:
        bl, br = bandpass_fft(l, lo, hi), bandpass_fft(r, lo, hi)
        out[name] = [10 * np.log10(0.5 * (np.mean(bl[int(a * SR):int(b * SR)] ** 2) + np.mean(br[int(a * SR):int(b * SR)] ** 2)) + 1e-30)
                     for a, b in WINS]
    return out


def sweep_one(args):
    sets, env = args
    la = levels(*render(os.path.join(SCRATCH, 'burst30.wav'), sets))
    lb = levels(*render(os.path.join(SCRATCH, 'burst30.wav'), sets, env))
    return sets, la, lb


def sweep(env, extra):
    grid = [dict(extra, space=sp, size=sz, decay=dc, drift=dr, mix=1, time=0)
            for sp, sz, dc, dr in itertools.product(('PLATE', 'HALL', 'MASSIVE'), (0, 0.6, 1), (0.5, 0.75, 0.95), (0, 0.25, 0.5, 0.75, 1))]
    with Pool(8) as p:
        res = p.map(sweep_one, [(g, env) for g in grid])
    worst, growth, slope = {}, [], {}
    for sets, la, lb in res:
        for name, _, _ in BANDS:
            for w, (a, b) in enumerate(zip(la[name], lb[name])):
                if a > FLOOR:  # ignore windows already down at the floor (quantisation, dither-level noise)
                    d = b - a
                    if d > worst.get(name, (-99, None))[0]:
                        worst[name] = (d, sets, WINS[w])
            v, u = lb[name], la[name]
            for i in range(len(v)):
                for j in range(i + 1, len(v)):
                    # B grows where A doesn't (by more than 1 dB), above the floor
                    if (v[j] - v[i]) - max(0.0, u[j] - u[i]) > 1.0 and v[j] > v[i] + 1.0 and min(v[i], v[j]) > FLOOR:
                        growth.append((name, sets, WINS[i], WINS[j], v[j] - v[i]))
            if sets['decay'] < 0.85:
                sa = (la[name][1] - la[name][3]) / 12.0  # dB/s between the 3-6 s and 12-20 s windows
                sb = (lb[name][1] - lb[name][3]) / 12.0
                if min(la[name][3], lb[name][3]) > FLOOR and sa > 0.5:
                    k = (sets['space'], name)
                    slope.setdefault(k, []).append(100 * (sb / sa - 1))
    print(f'sweep {env} {extra}: {len(res)} settings')
    for name, (d, sets, w) in worst.items():
        print(f'  worst B re A, {name:7s}: {d:+5.1f} dB at {sets["space"]} SIZE {sets["size"]} DECAY {sets["decay"]} DRIFT {sets["drift"]} ({w[0]}-{w[1]} s)')
    print(f'  growth in B (a later window > 1 dB above an earlier one): {len(growth)}')
    for g in growth[:8]:
        print(f'    {g[0]} {g[1]["space"]} SIZE {g[1]["size"]} DECAY {g[1]["decay"]} DRIFT {g[1]["drift"]}: {g[2]} -> {g[3]} s {g[4]:+.1f} dB')
    for k, v in sorted(slope.items()):
        print(f'  decay slope B re A, {k[0]:7s} {k[1]:7s}: {min(v):+5.1f} .. {max(v):+5.1f} %')


def hold_one(args):
    sets, env = args
    l, r = render(os.path.join(SCRATCH, 'hold620.wav'), sets, env)
    win = 10 * SR
    n = len(l) // win
    p = 0.5 * (l[:n * win] ** 2 + r[:n * win] ** 2).reshape(n, win).mean(1)
    db = 10 * np.log10(p + 1e-24)
    first = db[6]
    lo, hi = db[6:].min(), db[6:].max()
    ok = hi - lo < 3.0 and hi <= first + 1.0 and lo > -60.0 and np.all(np.isfinite(l))
    return sets, ok, first, lo, hi


def holds(env, extra):
    grid = [dict(extra, space=sp, size=sz, decay=1, mix=1, time=0, **({'era': 0} if 'era' not in extra else {}))
            for sp in ('PLATE', 'HALL', 'MASSIVE') for sz in (0, 0.5, 1)]
    with Pool(8) as p:
        res = p.map(hold_one, [(g, env) for g in grid])
    print(f'holds {env} {extra}: {sum(r[1] for r in res)}/{len(res)} pass')
    for sets, ok, first, lo, hi in res:
        print(f'  {"PASS" if ok else "FAIL"} {sets["space"]:7s} SIZE {sets["size"]}: 60-70 s {first:6.1f} dB, then {lo:6.1f} .. {hi:6.1f}')


if __name__ == '__main__':
    mode = sys.argv[1]
    env = dict(kv.split('=') for kv in sys.argv[2].split(',') if kv)
    extra = {}
    for kv in sys.argv[3:]:
        k, v = kv.split('=')
        extra[k] = float(v)
    (sweep if mode == 'sweep' else holds)(env, extra)
