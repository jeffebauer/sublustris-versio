#!/usr/bin/env python3
"""Site demos: the checks before anyone listens (CLAUDE.md, the memory rules).

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/check.py <work dir> <audio dir> [clip ids...]

Per clip, from render.py's outputs:
  settings  the Renderer's sidecar (<work>/<id>_wet.json, the values it ended on) against
            render.py's CLIPS (automated keys: their last breakpoint); its log for warnings
  loudness  the decoded MP3 pair mixed at the clip's default BLEND: integrated LUFS
            (target -20.5 +/- 0.5), true peak, and the sample peak of each decoded file
            (<= -1 dBFS)
  clicks    1 ms frames of the wet's highs (over 3 kHz): any frame more than CLICK_DB over
            the median of the 40 ms either side of it, where the dry has no onset of its own
            within 60 ms; listed separately for the windows around the clip's events
            (freeze, paint, release) and knob sweeps
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import render as R  # noqa: E402

SR = 48000
CLICK_DB = 8.0


def decode(mp3: Path) -> np.ndarray:
    with tempfile.NamedTemporaryFile(suffix=".wav") as f:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(mp3), "-c:a", "pcm_f32le", f.name], check=True)
        x, sr = sf.read(f.name, always_2d=True)
    assert sr == SR
    return x


def hp_frames(x: np.ndarray, hz: float = 3000.0, ms: float = 1.0) -> np.ndarray:
    """1 ms RMS frames (dB) of the signal above `hz` (a brick-wall FFT high-pass, mono)."""
    m = x.mean(axis=1)
    X = np.fft.rfft(m)
    X[: int(hz / (SR / 2) * len(X))] = 0
    h = np.fft.irfft(X, len(m))
    w = int(ms / 1000 * SR)
    fr = h[: len(h) // w * w].reshape(-1, w)
    return 10 * np.log10(np.mean(fr ** 2, axis=1) + 1e-14)


def jumps(wet: np.ndarray, dry: np.ndarray) -> list[tuple[float, float]]:
    """(time, dB over the surrounding median) for wet highs frames that jump, the dry quiet nearby."""
    fw, fd = hp_frames(wet), hp_frames(dry)
    out = []
    ctx = 40
    floor = np.max(fw) - 70  # ignore frames down in the noise
    d_on = np.zeros(len(fd), bool)
    dd = np.diff(fd, prepend=fd[0])
    d_on[(dd > 6) & (fd > np.max(fd) - 50)] = True  # the dry's own onsets
    for i in range(ctx, len(fw) - ctx):
        if fw[i] < floor:
            continue
        around = np.concatenate([fw[i - ctx:i - 2], fw[i + 3:i + ctx]])
        over = fw[i] - np.median(around)
        if over > CLICK_DB and not d_on[max(0, i - 60):i + 60].any():
            out.append((i / 1000, over))
    # merge neighbours
    merged = []
    for t, o in out:
        if merged and t - merged[-1][0] < 0.01:
            merged[-1] = (merged[-1][0], max(merged[-1][1], o))
        else:
            merged.append((t, o))
    return merged


def event_windows(c: dict) -> list[tuple[str, float, float]]:
    w = [(k, t - 0.1, t + 0.15) for k, t in (c.get("events") or {}).items()]
    if c.get("sweep") and c["sweep"][0] != "size":
        _, t0, t1, _ = c["sweep"]
        w.append(("sweep", t0, t1))
    return w


def max_jump_in(fw: np.ndarray, a: float, b: float) -> float:
    """The biggest 1 ms highs jump over the 40 ms either side, inside [a, b] s."""
    best = -99.0
    for i in range(max(40, int(a * 1000)), min(len(fw) - 40, int(b * 1000))):
        around = np.concatenate([fw[i - 40:i - 2], fw[i + 3:i + 40]])
        best = max(best, fw[i] - np.median(around))
    return best


def main():
    work, audio = Path(sys.argv[1]), Path(sys.argv[2])
    doc = json.loads((audio / "demos.json").read_text())
    want = sys.argv[3:] or [e["id"] for e in doc["clips"]]
    for e in doc["clips"]:
        cid = e["id"]
        if cid not in want:
            continue
        c = R.CLIPS[cid]
        # settings
        side = json.loads((work / f"{cid}_wet.json").read_text())["params"]
        bad = []
        for k, v in c["set"].items():
            want_v = R.value(k, v)
            if c["auto"] and c["auto"].get("breakpoints"):
                last = [p for p in c["auto"]["breakpoints"] if p["key"] == k]
                if last:
                    want_v = last[-1]["value"]
            got = side.get(k)
            if isinstance(want_v, str):
                ok = str(got).upper() == want_v.upper()
            else:
                ok = abs(float(got) - want_v) < 1e-4
            if not ok:
                bad.append(f"{k}: want {want_v}, got {got}")
        if abs(float(side["mix"]) - 1.0) > 1e-6:
            bad.append(f"mix {side['mix']}")
        log = (work / f"{cid}_render.log").read_text().strip()
        # loudness from the decoded MP3s
        dry, wet = decode(audio / e["dry"]), decode(audio / e["file"])
        n = min(len(dry), len(wet))
        dry, wet = dry[:n], wet[:n]
        mix = R.mix_of(dry, wet, e["blend"])
        lufs, tp = R.lufs_and_peak(mix)
        pk = {k: 20 * np.log10(np.abs(x).max()) for k, x in (("dry", dry), ("wet", wet), ("mix", mix))}
        # clicks
        wet_raw = sf.read(str(work / f"{cid}_wet_final.wav"), always_2d=True)[0]
        dry_raw = sf.read(str(work / f"{cid}_dry_final.wav"), always_2d=True)[0]
        fw = hp_frames(wet_raw)
        ev = [(k, round(max_jump_in(fw, a, b), 1)) for k, a, b in event_windows(c)]
        js = jumps(wet_raw, dry_raw)
        evw = event_windows(c)
        outside = [i for i in range(40, len(fw) - 40)
                   if not any(a * 1000 - 50 <= i <= b * 1000 + 50 for _, a, b in evw)]
        bg = max(fw[i] - np.median(np.concatenate([fw[i - 40:i - 2], fw[i + 3:i + 40]])) for i in outside[::2])
        verdict = "clean" if all(v <= bg + 1.0 for _, v in ev) else "CHECK"
        print(f"{cid} {e['title']}: {n / SR:.1f} s | settings {'OK' if not bad else 'MISMATCH ' + '; '.join(bad)}"
              f"{' | log: ' + log if log else ''}")
        sl = {k: R.lufs_and_peak(x)[0] for k, x in (("dry", dry), ("wet", wet))}
        print(f"   LUFS {lufs:.2f} (BLEND {e['blend_clock']}), true peak {tp:.1f} dBTP, sample peaks dry {pk['dry']:.1f} "
              f"wet {pk['wet']:.1f} mix {pk['mix']:.1f} dBFS; alone: dry {sl['dry']:.1f}, wet {sl['wet']:.1f} LUFS")
        print(f"   clicks: events (max 1 ms highs jump over the 40 ms around, dB) {[(k, float(v)) for k, v in ev] or '-'};"
              f" the clip's own largest jump elsewhere {bg:.1f} dB -> {verdict}; {len(js)} frames > {CLICK_DB:g} dB away from dry onsets")


if __name__ == "__main__":
    main()
