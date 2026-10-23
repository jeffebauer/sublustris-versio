"""Shared helpers for the DSP audit's measurements (docs/research/dsp-audit.md, 24 Oct 2026).

Renders through build-wt/iv_render (IV_RENDER overrides) into a scratch folder, reads the WAV back
and deletes it, so long holds don't fill the disk. Env vars select a prototype (host/render/main.cpp).
"""
import os, subprocess, tempfile
import numpy as np
import soundfile as sf

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RENDER = os.environ.get('IV_RENDER', os.path.join(ROOT, 'build-wt', 'iv_render'))
SCRATCH = os.environ.get('IV_SCRATCH', os.path.join(ROOT, 'build-wt', 'audit'))
SR = 48000


def render(inp, sets, env=None, auto=None, keep=None):
    """Renders `inp` with --set k=v for each item of `sets` (a dict) and extra env; returns (L, R)."""
    os.makedirs(SCRATCH, exist_ok=True)
    fd, out = tempfile.mkstemp(suffix='.wav', dir=SCRATCH)
    os.close(fd)
    args = [RENDER, inp, out]
    for k, v in sets.items():
        args += ['--set', f'{k}={v}']
    if auto:
        args += ['--auto', auto]
    e = dict(os.environ)
    for k in [k for k in e if k.startswith('IV_') and k not in ('IV_RENDER', 'IV_SCRATCH')]:
        del e[k]  # A is always today's default
    e.update(env or {})
    subprocess.run(args, check=True, env=e, stdout=subprocess.DEVNULL)
    x, sr = sf.read(out, dtype='float64')
    if keep:
        os.replace(out, keep)
    else:
        os.remove(out)
    return x[:, 0], x[:, 1]


def bandpass_fft(x, lo, hi, sr=SR):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / sr)
    X[(f < lo) | (f >= hi)] = 0
    return np.fft.irfft(X, len(x))


def rms_db(x):
    return 10 * np.log10(np.mean(x * x) + 1e-30)


def window_db(l, r, t0, t1, sr=SR):
    a, b = int(t0 * sr), int(t1 * sr)
    return 10 * np.log10(0.5 * (np.mean(l[a:b] ** 2) + np.mean(r[a:b] ** 2)) + 1e-30)


def envelope_db(l, r, win):
    n = len(l) // win
    p = 0.5 * (l[:n * win] ** 2 + r[:n * win] ** 2).reshape(n, win).mean(1)
    return 10 * np.log10(p + 1e-24)
