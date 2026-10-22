#!/usr/bin/env python3
"""Reads a recording of the reference kit back in (docs/reference-recording-guide.md).

Usage: .venv/bin/python tools/ingest_reference.py <recording.wav> <calibrate|measure|music> <unit_algorithm[_target]>
  calibrate  prints each of the five clicks' decay (T60 per band), so a knob-only unit's
             positions can be mapped to 2 / 5 / 15 s
  measure    prints T60 per octave band from the click and the noise burst, and writes
             the impulse response (from the sweep) to test_audio/reference/<name>/ir.wav
  music      splits the recording into test_audio/reference/<name>/<name>_<material>.wav,
             ready for make_review.py --reference test_audio/reference/<name>
Recordings are never committed (CLAUDE.md, ADR 0006). The wet starts a little after each
cue (the unit's own predelay); parts are cut from the first sound after the sync click.
"""
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parent.parent
KIT = ROOT / "test_audio" / "reference_kit"
REF = ROOT / "test_audio" / "reference"
BANDS = [125, 250, 500, 1000, 2000, 4000, 8000]


def load(path: Path) -> np.ndarray:
    x, sr = sf.read(str(path), always_2d=True)
    if sr != SR:
        n = int(round(len(x) * SR / sr))
        x = np.stack([np.interp(np.linspace(0, len(x) - 1, n), np.arange(len(x)), x[:, c]) for c in range(x.shape[1])], axis=1)
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    return x[:, :2]


def sync_index(x: np.ndarray) -> int:
    """The sync click's response: the first sound 30 dB above the noise floor."""
    m = np.abs(x).max(axis=1)
    floor = np.percentile(m[: int(0.5 * SR)], 90) if len(m) > SR else 1e-6
    hit = np.nonzero(m > max(floor * 31.6, 10 ** (-60 / 20)))[0]
    if not len(hit):
        raise SystemExit("no sync click found")
    return int(hit[0])


def octave(x: np.ndarray, fc: float) -> np.ndarray:
    """Octave band around fc: a Gaussian window in log-frequency (zero phase). A brick-wall
    mask would ring for the whole segment and flatten the decay curve."""
    pad = SR  # silence on both sides, so the filter's ringing doesn't wrap round the ends
    xp = np.concatenate([np.zeros(pad), x, np.zeros(pad)])
    X = np.fft.rfft(xp)
    f = np.fft.rfftfreq(len(xp), 1 / SR)
    with np.errstate(divide="ignore"):
        w = np.exp(-0.5 * (np.log2(np.maximum(f, 1e-3) / fc) / 0.42) ** 2)
    return np.fft.irfft(X * w, len(xp))[pad: pad + len(x)]


def t60(x: np.ndarray) -> float:
    """From the backward-integrated decay (Schroeder), -5 to -25 dB, ×3. NaN if too short."""
    e = x[int(np.argmax(np.abs(x))):] ** 2
    edc = np.cumsum(e[::-1])[::-1]
    if edc[0] <= 0:
        return float("nan")
    d = 10 * np.log10(edc / edc[0] + 1e-30)
    a, b = np.argmax(d < -5), np.argmax(d < -25)
    return 3 * (b - a) / SR if b > a else float("nan")


def bands_line(seg: np.ndarray) -> str:
    mono = seg.mean(axis=1)
    return "  ".join(f"{fc if fc < 1000 else str(fc // 1000) + 'k':>4}: {t60(octave(mono, fc)):5.1f}s" for fc in BANDS)


def part(x: np.ndarray, s0: int, start: float, length: float) -> np.ndarray:
    i = s0 + int(start * SR)
    return x[i: i + int(length * SR)]


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    rec, kind, name = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    cues = json.loads((KIT / "cues.json").read_text())[kind]
    x = load(rec)
    s0 = sync_index(x)
    out = REF / name
    if kind == "calibrate":
        print(f"{name}: decay at each of the five positions (T60 per octave band)")
        for k in range(5):
            print(f"  position {k + 1}: " + bands_line(part(x, s0, cues[f'click{k + 1}'], 21.0)))
    elif kind == "measure":
        print(f"{name}: T60 from the click     " + bands_line(part(x, s0, cues["click"], 24.0)))
        print(f"{name}: T60 from the noise     " + bands_line(part(x, s0, cues["noise"] + 0.5, 23.5)))
        # Impulse response: the recorded sweep divided by the kit's sweep (regularised).
        kit = load(KIT / "measure.wav")
        k0 = sync_index(kit)
        ref = part(kit, k0, cues["sweep"], 10.0).mean(axis=1)
        rec_sw = part(x, s0, cues["sweep"], 35.0)
        n = 1 << int(np.ceil(np.log2(len(rec_sw) + len(ref))))
        R = np.fft.rfft(ref, n)
        reg = 1e-3 * np.abs(R).max()
        ir = np.stack([np.fft.irfft(np.fft.rfft(rec_sw[:, c], n) * np.conj(R) / (np.abs(R) ** 2 + reg ** 2), n)[: 25 * SR]
                       for c in range(2)], axis=1)
        ir /= max(1e-9, np.abs(ir).max())
        out.mkdir(parents=True, exist_ok=True)
        sf.write(str(out / "ir.wav"), (0.5 * ir).astype(np.float32), SR, subtype="PCM_24")
        print(f"wrote {out.relative_to(ROOT)}/ir.wav")
    elif kind == "music":
        out.mkdir(parents=True, exist_ok=True)
        names = list(cues)[1:]  # after "sync"
        for i, m in enumerate(names):
            end = cues[names[i + 1]] if i + 1 < len(names) else (len(x) - s0) / SR
            seg = part(x, s0, cues[m], end - cues[m])
            sf.write(str(out / f"{name}_{m}.wav"), seg.astype(np.float32), SR, subtype="PCM_24")
        print(f"wrote {len(names)} files to {out.relative_to(ROOT)}/")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
