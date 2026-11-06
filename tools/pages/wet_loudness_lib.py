#!/usr/bin/env python3
"""Loudness measures for docs/research/wet-loudness-2026-11-05.md and renders/wet_loudness.

  kweight(x)            BS.1770 K-weighting (the two biquads at 48 kHz), applied in the frequency domain with
                        zero padding (a linear convolution to well under 0.01 dB for these minimum-phase filters)
  integrated(x)         BS.1770-4 integrated loudness, LUFS: 400 ms blocks, 75 % overlap, the -70 LUFS absolute
                        gate and the -10 LU relative gate; channels summed with weight 1 (L + R)
  short_term_max(x)     the loudest 3 s window (100 ms hop), LUFS, ungated
  peak_db(x)            the sample peak over both channels, dBFS
  ceiling_share(x)      the share of samples (both channels) within 0.01 dB of the wet limiter's ceiling
                        (kLimitCeiling 0.891, -1 dBFS): where the limiter is holding the wet down
"""
import numpy as np

SR = 48000
CEIL = 0.891


def kweight(x):
    x = np.atleast_2d(x.T).T
    n = len(x)
    N = 1 << int(np.ceil(np.log2(n + SR)))  # a second of padding for the filters' ring
    X = np.fft.rfft(x, N, axis=0)
    z = np.exp(-1j * np.linspace(0, np.pi, X.shape[0]))
    H = lambda b, a: (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)
    h = H([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585]) \
        * H([1, -2, 1], [1, -1.99004745483398, 0.99007225036621])
    return np.fft.irfft(X * h[:, None], N, axis=0)[:n]


def _block_power(y, win, hop):
    p = (y ** 2).sum(axis=1)  # channel weights 1, summed
    c = np.concatenate([[0.0], np.cumsum(p)])
    starts = np.arange(0, len(p) - win + 1, hop)
    return (c[starts + win] - c[starts]) / win


def _lufs(p):
    return -0.691 + 10 * np.log10(np.maximum(p, 1e-30))


def integrated(x, y=None):
    y = kweight(x) if y is None else y
    p = _block_power(y, int(0.4 * SR), int(0.1 * SR))
    p = p[_lufs(p) > -70]
    if len(p) == 0:
        return -np.inf
    rel = _lufs(p.mean()) - 10
    p = p[_lufs(p) > rel]
    return float(_lufs(p.mean()))


def short_term_max(x, y=None):
    y = kweight(x) if y is None else y
    p = _block_power(y, 3 * SR, int(0.1 * SR))
    return float(_lufs(p.max()))


def measures(x):
    y = kweight(x)
    return {
        "lufs": integrated(x, y),
        "st_max": short_term_max(x, y),
        "peak": float(20 * np.log10(np.abs(x).max() + 1e-30)),
        "ceil": float(np.mean(np.abs(x) >= CEIL * 10 ** (-0.01 / 20))),
    }
