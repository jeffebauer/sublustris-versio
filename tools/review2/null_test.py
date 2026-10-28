#!/usr/bin/env python3
"""Null test between two renders of the same settings (review2/cpu, 28 Oct 2026): the largest sample difference,
its level in dBFS, the residual's RMS against the render's own (dB), and where the largest difference sits.

    null_test.py a.wav b.wav [label]
"""
import sys
import numpy as np
import soundfile as sf


def db(x):
    return 20.0 * np.log10(max(float(x), 1e-30))


def main():
    a, sr = sf.read(sys.argv[1], dtype="float64", always_2d=True)
    b, _ = sf.read(sys.argv[2], dtype="float64", always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    d = a - b
    peak = np.max(np.abs(d))
    at = np.unravel_index(np.argmax(np.abs(d)), d.shape)[0] / sr
    rms_a = np.sqrt(np.mean(a * a))
    rms_d = np.sqrt(np.mean(d * d))
    first = np.flatnonzero(np.any(d != 0.0, axis=1))
    label = sys.argv[3] if len(sys.argv) > 3 else ""
    print(f"{label:28s} max|diff| {peak:.3e} ({db(peak):7.1f} dBFS) at {at:6.2f} s | residual RMS {db(rms_d):7.1f} dBFS, "
          f"{db(rms_d) - db(rms_a):7.1f} dB under the render | peak {db(np.max(np.abs(a))):6.1f} dBFS | "
          f"first diff {first[0] / sr if len(first) else -1:.3f} s")


if __name__ == "__main__":
    main()
