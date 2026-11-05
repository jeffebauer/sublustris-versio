#!/usr/bin/env python3
"""renders/wear_drive's measures (tape_fold_lib.py's, on branch proto/tape-fold, for ADR 0041), on v1.0.69's taper.

  drive_db(version, v)        the tape's drive in dB at WEAR v for IV_WEAR_DRIVE A-D (dsp/EraTexture.h protoDriveDb)
  tape_model(x, t, drive_db)  EraTexture::tape() in numpy at tapeT t (the emphasis) and a given drive, its clip at 48 kHz,
                              8x oversampled (os_factor 8) or taken out (os_factor 0)
  products(x, t, drive_db)    every product the clip makes (harmonic and intermodulation: the 8x clip minus the linear
                              path), dB re the tape's output: ADR 0041's "the clip's products on a chord"
  lufs(y), crest(y)           K-weighted loudness (ungated), and the crest (99.99th-percentile peak over RMS, dB, mono)
"""
import numpy as np

SR = 48000
TAPER = 2.2


def w_of(v):
    return 1.0 - (1.0 - v) ** TAPER


def drive_db(version, v):
    w = w_of(v)
    if version == "B":
        return 13.75 * w
    if version == "C":
        return 16.25 * w
    d = 11.25 * w
    if version == "D":
        a = min(1.0, max(0.0, (v - 0.75) / 0.25))
        d += 5.0 * a * a * (3 - 2 * a)
    return d


def db(p):
    return 10 * np.log10(p + 1e-30)


def lufs(x):
    """BS.1770 K-weighted loudness (ungated), from the spectrum (era_rethink.py's)."""
    X = np.fft.rfft(x, axis=0)
    z = np.exp(-1j * np.linspace(0, np.pi, X.shape[0]))
    H = lambda b, a: (b[0] + b[1] * z + b[2] * z * z) / (1 + a[1] * z + a[2] * z * z)
    h = H([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585]) \
        * H([1, -2, 1], [1, -1.99004745483398, 0.99007225036621])
    p = np.sum(np.abs(X * h[:, None]) ** 2) / len(x) ** 2 * 2 / x.shape[1]
    return db(p) - 0.691


def crest(y):
    m = y.mean(axis=1) if y.ndim == 2 else y
    return 20 * np.log10(np.percentile(np.abs(m), 99.99) / (np.sqrt(np.mean(m ** 2)) + 1e-30))


def resample(x, up, down):
    X = np.fft.rfft(x)
    n = len(x) * up // down
    Y = np.zeros(n // 2 + 1, dtype=complex)
    m = min(len(X), len(Y))
    Y[:m] = X[:m]
    return np.fft.irfft(Y, n) * (n / len(x))


def clip(u):
    u = np.clip(u, -1.0, 1.0)
    return u - u ** 3 / 3.0


def onepole(x, w):
    L = min(len(x), int(np.ceil(np.log(1e-9) / np.log(1 - w))) + 1)
    h = w * (1 - w) ** np.arange(L)
    N = 1 << int(np.ceil(np.log2(len(x) + L)))
    return np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(h, N), N)[:len(x)]


def tape_model(x, t, ddb, os_factor=1):
    g = 10 ** (6.0 * t / 20)
    drive = 10 ** (ddb / 20)
    wpre = 1 - np.exp(-2 * np.pi * 2000 / SR)
    wde = 1 - np.exp(-2 * np.pi * 2000 / g / SR)
    wdc = 2 * np.pi * 5 / SR
    bias = 0.12 * t
    pre = onepole(x, wpre)
    u = drive * (x + (g - 1) * (x - pre)) + bias
    if os_factor == 0:
        c = u
    elif os_factor > 1:
        c = resample(clip(resample(u, os_factor, 1)), 1, os_factor)
    else:
        c = clip(u)
    yy = (c - (bias if os_factor == 0 else clip(bias))) / drive
    de = onepole(yy, wde)
    yy = yy + (1 / g - 1) * (yy - de)
    return yy - onepole(yy, wdc)


def products(x, t, ddb):
    b = tape_model(x, t, ddb, 8)
    lin = tape_model(x, t, ddb, 0)
    return db(np.sum((b - lin) ** 2)) - db(np.sum(b ** 2))
