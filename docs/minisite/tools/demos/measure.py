#!/usr/bin/env python3
"""Site demos: measure what each transcript claims, against a control render where one helps.

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/measure.py <src dir> <work dir> [clip ids...]
Reads render.py's <work>/<id>_wet.wav / <id>_dry.wav (before the shared gain); writes the
controls to <work>/controls/. Prints one block per clip: the numbers behind its transcript.
"""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import render as R  # noqa: E402
import check as C  # noqa: E402

SR = 48000


def rd(p: Path) -> np.ndarray:
    return sf.read(str(p), always_2d=True)[0]


def lvl(x: np.ndarray, a: float, b: float) -> float:
    s = x[int(a * SR):int(b * SR)]
    return 10 * np.log10(np.mean(s ** 2) + 1e-14)


def env(x: np.ndarray, ms: float = 50) -> np.ndarray:
    w = int(ms / 1000 * SR)
    m = (x ** 2).mean(axis=1)
    return 10 * np.log10(m[: len(m) // w * w].reshape(-1, w).mean(axis=1) + 1e-14)


def band(x: np.ndarray, lo: float, hi: float, a: float, b: float) -> float:
    s = x[int(a * SR):int(b * SR)].mean(axis=1)
    S = np.abs(np.fft.rfft(s * np.hanning(len(s)))) ** 2
    f = np.fft.rfftfreq(len(s), 1 / SR)
    return 10 * np.log10(S[(f >= lo) & (f < hi)].sum() + 1e-14)


def control(cid: str, src: Path, work: Path, tag: str, overrides: dict, auto=None, inp=None) -> np.ndarray:
    c = R.CLIPS[cid]
    d = work / "controls"
    d.mkdir(exist_ok=True)
    vals = {k: R.value(k, v) for k, v in c["set"].items()}
    vals.update(overrides)
    sets = sum((["--set", f"{k}={v}"] for k, v in vals.items()), [])
    a = []
    if auto is not None:
        (d / f"{cid}_{tag}_auto.json").write_text(json.dumps(auto))
        a = ["--auto", str(d / f"{cid}_{tag}_auto.json")]
    out = d / f"{cid}_{tag}.wav"
    inp = inp or src / (f"{cid}_send.wav" if c.get("send") else f"{cid}.wav")
    subprocess.run([str(R.RENDERER), str(inp), str(out), *sets, "--set", "mix=1", *a], check=True, capture_output=True)
    return rd(out)


def pitches(x: np.ndarray, a: float, b: float, k: int = 6) -> list[str]:
    s = x[int(a * SR):int(b * SR)].mean(axis=1)
    S = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    f = np.fft.rfftfreq(len(s), 1 / SR)
    m = (f > 60) & (f < 1200)
    S, f = S[m], f[m]
    names = "C C# D Eb E F F# G Ab A Bb B".split()
    found = []
    for i in np.argsort(S)[::-1]:
        nm = names[int(round(12 * np.log2(f[i] / 440) + 69)) % 12]
        if nm not in found:
            found.append(nm)
        if len(found) == k:
            break
    return found


def m01(src, work):
    w = rd(work / "01_wet.wav")
    e = env(w, 400)
    print("  wet level per 2 s (dB):", [round(lvl(w, t, t + 2), 1) for t in range(0, 24, 2)])
    print(f"  after the chord (12-22 s): range {np.ptp(e[int(12 / 0.4):int(22 / 0.4)]):.1f} dB;",
          "pitch classes 14-22 s:", pitches(w, 14, 22, 5), "(the chord: A E B C G)")
    print(f"  wet peak 0-10 s {20 * np.log10(np.abs(w[:10 * SR]).max()):.1f}, 14-22 s {20 * np.log10(np.abs(w[14 * SR:22 * SR]).max()):.1f} dBFS")


def m02(src, work):
    w = rd(work / "02_wet.wav")
    nop = control("02", src, work, "nopaint", {}, {"buttons": [[1.6, 1.7], [22.5, 22.6]]})
    nof = control("02", src, work, "nofreeze", {}, None)
    print("  wet per 1 s (dB):", [round(lvl(w, t, t + 1), 1) for t in range(0, 28)])
    print(f"  frozen pad 3-15 s: range of 1 s levels {np.ptp([lvl(w, t, t + 1) for t in range(3, 15)]):.1f} dB;"
          f" without freeze the same stretch falls {lvl(nof, 3, 4) - lvl(nof, 14, 15):.1f} dB")
    for a, b in ((15.4, 18), (18, 20.6), (20.6, 22.5), (23, 26)):
        print(f"  paint vs no paint {a}-{b} s: level {lvl(w, a, b) - lvl(nop, a, b):+.1f} dB, "
              f"band 800-1000 Hz (A5 = 880) {band(w, 800, 1000, a, b) - band(nop, 800, 1000, a, b):+.1f} dB, "
              f"600-700 (E5 = 659) {band(w, 600, 700, a, b) - band(nop, 600, 700, a, b):+.1f} dB")
    print(f"  lead into the frozen pad? wet vs no-freeze at 5-14 s: {lvl(w, 5, 14) - lvl(nof, 5, 14):+.1f} dB;"
          f" 1 kHz-3 kHz (the lead's harmonics) frozen {band(w, 1000, 3000, 5, 14):.1f} vs no freeze {band(nof, 1000, 3000, 5, 14):.1f}")


def m03(src, work):
    """The short send (31 Oct 2026): the tail's T60 from the last stab (the -5 to -25 dB slope
    times 3), the wet against the dry in the mix at the clip's BLEND, and the old endless
    setting (DECAY 4, SIZE 1, BLEND 11) on the same send as the control."""
    w = rd(work / "03_wet.wav")
    d = rd(work / "03_dry.wav")
    old = control("03", src, work, "old_endless", {"decay": R.c2v("4"), "size": R.c2v("1")})
    t_stab = 0.25 + (7 * 4 + 3.5) * 60 / 118  # the last stab (bar 8, beat 3.5)

    def t60(x):
        m = (x ** 2).mean(axis=1)
        e = 10 * np.log10(np.convolve(m, np.ones(480) / 480, mode="same") + 1e-14)
        seg = e[int((t_stab - 0.05) * SR):int((t_stab + 4.5) * SR)]
        seg = seg[int(np.argmax(seg[: SR // 2])):] - seg[: SR // 2].max()
        a, b = np.argmax(seg < -5), np.argmax(seg < -25)
        return 3 * (b - a) / SR if b > a else float("inf")

    for nm, x, bl in (("new", w, R.c2v(R.CLIPS["03"]["blend"])), ("old endless", old, R.c2v("11"))):
        rel = lvl(x, 0.5, 16.5) - lvl(d, 0.5, 16.5) + 20 * np.log10(np.tan(bl * np.pi / 2))
        print(f"  {nm}: T60 from the last stab {t60(x):.1f} s; wet vs dry in the mix (0.5-16.5 s) {rel:+.1f} dB; "
              f"wet 1 s / 2 s / 3 s after the last stab {[round(lvl(x, t_stab + a, t_stab + a + 0.25) - lvl(x, t_stab, t_stab + 0.25), 1) for a in (1, 2, 3)]} dB")


def onset_to_peak(w, times, span=2.8):
    e = env(w, 20)
    out = []
    for t in times:
        seg = e[int(t / 0.02):int((t + span) / 0.02)]
        out.append(round(np.argmax(seg) * 0.02, 2))
    return out


def m05(src, work):
    w = rd(work / "05_wet.wav")
    nat = control("05", src, work, "natural", {"attack": "NATURAL"})
    times = [0.25 + 4.2 * k for k in range(6)]
    for nm, x in (("BLOOM", w), ("NATURAL", nat)):
        e = env(x, 100)
        print(f"  {nm}: per note, wet rise from the onset to 1 s / 2 s / 2.5 s / 3.5 s after (dB):",
              [tuple(round(lvl(x, t + d - 0.1, t + d + 0.1) - lvl(x, t - 0.05, t + 0.15), 1) for d in (1, 2, 2.5, 3.5)) for t in times])
        print(f"    and the time of the wet's peak within 4 s of each onset (s): {onset_to_peak(x, times, 4.0)}")


def m06(src, work):
    w = rd(work / "06_wet.wav")
    d = rd(work / "06_dry.wav")
    nat = control("06", src, work, "natural", {"attack": "NATURAL"})
    spb = 60 / 66
    notes = [(0, 2, 62), (2, 1, 65), (3, 3, 69), (8, 1.5, 67), (10, 3, 64), (16, 1, 62), (19, 4, 69)]
    for b, l, m in notes:
        a = 0.25 + b * spb + 0.3
        z = a + l * spb - 0.3
        f = 440 * 2 ** ((m - 69) / 12)
        rel = lambda x: band(x, 2 * f * 0.97, 2 * f * 1.03, a, z) - band(x, f * 0.97, f * 1.03, a, z)
        print(f"  note {m} ({f:.0f} Hz): octave over the note, wet SHIMMER {rel(w):+.1f} dB, NATURAL {rel(nat):+.1f}, dry {rel(d):+.1f}")
    print(f"  wet vs dry level 0-24 s: {lvl(w, 0, 24) - lvl(d, 0, 24):+.1f} dB; NATURAL control {lvl(nat, 0, 24) - lvl(d, 0, 24):+.1f}")
    print(f"  4-octave check (4f band) over the last note: SHIMMER {band(w, 4 * 440 * 0.97, 4 * 440 * 1.03, 18, 21):.1f}"
          f" NATURAL {band(nat, 4 * 440 * 0.97, 4 * 440 * 1.03, 18, 21):.1f}")


def m07(src, work):
    w = rd(work / "07_wet.wav")
    z = control("07", src, work, "wear0", {"era": 0.0}, {"breakpoints": []})
    for a in (1, 6, 11, 16, 21, 24):
        b = a + 3
        f = lambda x: (band(x, 60, 200, a, b), band(x, 400, 3000, a, b), band(x, 6000, 16000, a, b))
        lo, mid, hi = f(w)
        lo0, mid0, hi0 = f(z)
        print(f"  {a}-{b} s vs WEAR 0: lows {lo - lo0:+.1f}, mids {mid - mid0:+.1f}, highs {hi - hi0:+.1f} dB, level {lvl(w, a, b) - lvl(z, a, b):+.1f}")
    # hiss/breath: the residual (wet minus WEAR-0 wet) is not meaningful (different grain), so use the highs in the quietest 200 ms
    e = env(w, 200)
    e0 = env(z, 200)
    print(f"  quietest 200 ms, first 8 s vs last 6 s of the sweep: {np.min(e[:40]):.1f} / {np.min(e[105:120]):.1f} dB (WEAR 0: {np.min(e0[:40]):.1f} / {np.min(e0[105:120]):.1f})")
    print("  WEAR at end of sweep with constant 0.7 for click comparison:")
    k = control("07", src, work, "wear07", {"era": 0.7}, {"breakpoints": []})
    d = rd(work / "07_dry.wav")
    for nm, x in (("swept", w), ("const 0", z), ("const 0.7", k)):
        fw = C.hp_frames(x)
        print(f"    {nm}: max 1 ms highs jump 1-24 s {C.max_jump_in(fw, 1, 24):.1f} dB, n>8dB away from dry onsets {len(C.jumps(x, d))}")


def m08(src, work):
    """Dropouts are hard to pick out of music, so count them on a steady pink noise through the
    same settings (after 10 s to settle): a dip is >= 50 ms of the highs (over 4 kHz, 10 ms frames)
    more than 2 dB under their 1 s running median. MOTION 0.03 (no dips) is the false-alarm check."""
    rng = np.random.default_rng(3)
    n = SR * 62
    X = np.fft.rfft(rng.standard_normal((n, 2)), axis=0)
    f = np.fft.rfftfreq(n, 1 / SR)
    X[1:] /= np.sqrt(f[1:])[:, None]
    X[0] = 0
    x = np.fft.irfft(X, n, axis=0)
    x *= 0.1 / np.sqrt(np.mean(x ** 2))
    pink = work / "controls" / "pink.wav"
    pink.parent.mkdir(exist_ok=True)
    sf.write(str(pink), x.astype(np.float32), SR, subtype="FLOAT")
    for d in (R.c2v(R.CLIPS["08"]["set"]["drift"]), 0.025, 0.03):
        y = control("08", src, work, f"pink_{d}", {"drift": d}, None, inp=pink)
        m = y.mean(axis=1)
        Y = np.fft.rfft(m)
        Y[np.fft.rfftfreq(len(m), 1 / SR) < 4000] = 0
        h = np.fft.irfft(Y, len(m))
        e = 10 * np.log10(np.mean(h[: len(h) // 480 * 480].reshape(-1, 480) ** 2, axis=1) + 1e-14)[1000:]
        med = np.array([np.median(e[max(0, i - 50):i + 50]) for i in range(len(e))])
        low = (e - med) < -2.0
        runs, i = 0, 0
        while i < len(low):
            if low[i]:
                j = i
                while j < len(low) and low[j]:
                    j += 1
                runs += j - i >= 5
                i = j
            else:
                i += 1
        print(f"  MOTION {d}: {runs * 60 / (len(e) / 100):.1f} dips a minute on steady noise")
    w = rd(work / "08_wet.wav")
    noon = control("08", src, work, "noon", {"drift": 0.5})
    print(f"  wet level vs MOTION noon: {lvl(w, 1, 25) - lvl(noon, 1, 25):+.1f} dB; highs over 5 kHz {band(w, 5000, 12000, 1, 25) - band(noon, 5000, 12000, 1, 25):+.1f} dB")


def m10(src, work):
    """WEAR's whole range on the drone: per 2 s, the wet's spectral centroid, its bands against a
    steady WEAR 0 render, and the 6 dB bandwidth around the spectrum's peak (the radio's band
    narrows it)."""
    w = rd(work / "10_wet.wav")
    k = control("10", src, work, "wear0", {"era": 0.0}, {"breakpoints": []})

    def spec(x, a, b):
        s = x[int(a * SR):int(b * SR)].mean(axis=1)
        S = np.abs(np.fft.rfft(s * np.hanning(len(s)))) ** 2
        return np.fft.rfftfreq(len(s), 1 / SR), S

    def centroid(x, a, b):
        f, S = spec(x, a, b)
        return float((f * S).sum() / S.sum())

    def bw_over(x, a, b, thr=-30):
        f, S = spec(x, a, b)
        sm = np.convolve(S, np.ones(64) / 64, mode="same")
        on = f[10 * np.log10(sm / sm.max() + 1e-20) > thr]
        return float(on.max()) if len(on) else 0.0

    for a in range(0, 28, 2):
        b = a + 2
        f = lambda x: (band(x, 20, 150, a, b), band(x, 150, 600, a, b), band(x, 600, 3000, a, b), band(x, 3000, 12000, a, b))
        lo, lm, mid, hi = (u - v for u, v in zip(f(w), f(k)))
        print(f"  {a:2d}-{b:2d} s: centroid {centroid(w, a, b):5.0f} Hz (WEAR 0: {centroid(k, a, b):4.0f}); top within 30 dB of the peak "
              f"{bw_over(w, a, b):5.0f} Hz ({bw_over(k, a, b):4.0f}); vs WEAR 0: <150 {lo:+.1f}, 150-600 {lm:+.1f}, "
              f"600-3k {mid:+.1f}, >3k {hi:+.1f} dB; level {lvl(w, a, b) - lvl(k, a, b):+.1f}")


def background(cid, work):
    w = rd(work / f"{cid}_wet.wav")
    fw = C.hp_frames(w)
    return C.max_jump_in(fw, 0.5, len(w) / SR - 2)


M = {"01": m01, "02": m02, "03": m03, "05": m05, "06": m06, "07": m07, "08": m08, "10": m10}

if __name__ == "__main__":
    src, work = Path(sys.argv[1]), Path(sys.argv[2])
    for cid in sys.argv[3:] or list(M):
        print(f"{cid} {R.CLIPS[cid]['title']}")
        M[cid](src, work)
