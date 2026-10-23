"""A3: does a frozen chord smear over minutes? (docs/research/dsp-audit.md, experiment E6.)

A chord (A3 C#4 E4 and their octaves) is frozen at 2 s and held 5 minutes. Per 10 s window:
  focus  share of the 150-900 Hz energy within ±1.5 Hz of the six partials (1 = pure tones; less = smeared)
  width  the 220 Hz partial's -10 dB width in Hz
  level  the window's level (the pad must hold within ±0.5 dB, test_plate_freeze padHolds)
Usage: dsp_audit_a3.py [ENV=VAL ...]   (each variant is a comma-separated env list; '' = today)
"""
import json, os, sys
import numpy as np
from dsp_audit_lib import render, SCRATCH, SR, window_db

PARTIALS = (220.0, 277.18, 329.63, 440.0, 554.37, 659.26)


def focus(x):
    w = np.hanning(len(x))
    X = np.abs(np.fft.rfft(x * w)) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    band = (f >= 150) & (f < 900)
    near = np.zeros_like(band)
    for p in PARTIALS:
        near |= np.abs(f - p) <= 1.5
    fc = X[near & band].sum() / X[band].sum()
    k = np.argmax(np.where(np.abs(f - 220) < 5, X, 0))
    thr = X[k] / 10
    lo = k
    while lo > 0 and X[lo] > thr: lo -= 1
    hi = k
    while hi < len(X) - 1 and X[hi] > thr: hi += 1
    return fc, (hi - lo) * (f[1] - f[0])


def run(space, env, seconds=303):
    auto = os.path.join(SCRATCH, 'freeze_at2.json')
    with open(auto, 'w') as fh:
        json.dump({'buttons': [[2.0, 2.1]]}, fh)
    l, r = render(os.path.join(SCRATCH, 'chord303.wav'),
                  {'space': space, 'size': 0.6, 'decay': 0.6, 'era': 0, 'mix': 1, 'drift': 0.5}, env, auto)
    m = 0.5 * (l + r)
    rows = []
    for t0 in (10, 30, 60, 120, 180, 240, 290):
        a, b = int(t0 * SR), int((t0 + 10) * SR)
        fc, wd = focus(m[a:b])
        rows.append((t0, fc, wd, window_db(l, r, t0, t0 + 10)))
    return rows


def pad_holds(env):
    """test_plate_freeze padHolds, rendered: 3 s of noise, freeze at 2.5 s, 2 s windows every 25 s from 25 s: within 1 dB."""
    import soundfile as sf
    src = os.path.join(SCRATCH, 'pad305.wav')
    if not os.path.exists(src):
        x = np.zeros(305 * SR); x[:3 * SR] = 0.25 * np.random.default_rng(5).uniform(-1, 1, 3 * SR)
        sf.write(src, np.stack([x, x], 1), SR, subtype='FLOAT')
    auto = os.path.join(SCRATCH, 'freeze_at25.json')
    with open(auto, 'w') as fh:
        json.dump({'buttons': [[2.5, 2.6]]}, fh)
    for space in ('PLATE', 'HALL', 'MASSIVE'):
        l, r = render(src, {'space': space, 'mix': 1}, env, auto)
        v = [window_db(l, r, t, t + 2) for t in range(25, 300, 25)]
        print(f'  {"PASS" if max(v) - min(v) <= 1.0 else "FAIL"} {space:8s} pad holds 5 min: {min(v):.2f} .. {max(v):.2f} dB')


if __name__ == '__main__':
    if sys.argv[1:2] == ['holds']:
        for v in sys.argv[2:] or ['']:
            print(v or 'today')
            pad_holds(dict(kv.split('=') for kv in v.split(',') if kv))
        sys.exit(0)
    variants = sys.argv[1:] or ['']
    for space in ('HALL', 'MASSIVE', 'PLATE'):
        for v in variants:
            env = dict(kv.split('=') for kv in v.split(',') if kv)
            rows = run(space, env)
            lv = [r[3] for r in rows[1:]]
            print(f'{space:8s} {v or "today":24s} ' + ' '.join(f'{t}s:{fc:.2f}/{wd:.1f}Hz' for t, fc, wd, _ in rows)
                  + f'  level {min(lv):.2f}..{max(lv):.2f} dB (from 30 s)')
