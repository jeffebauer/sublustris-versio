#!/usr/bin/env python3
"""Site demos (docs/minisite/demos-plan.md): build the dry, musical sources.

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/build_sources.py <out dir> [clip ids...]
Needs numpy and soundfile. Writes <out dir>/<id>.wav (48 kHz stereo float, peaking at
-6 dBFS, the clip's full length including the room for its tail) and, for clip 03, the
send <id>_send.wav (only the stabs, at their level in the mix: the reverb hears that,
the listener's dry is the whole groove).

Material, all publishable (CLAUDE.md, the brief):
  synthesised here    the pad, the electric piano, the leads, the piano, the drone, the stabs
  CC0 samples         Virtuosity Drums (kick, rimshot, closed hat) and Karoryfer's
                      Emilyguitar, from Resilio Versio's test_audio/demo_sources/cc0
                      (never in either repo; Resilio's fetch_samples.py fetches them)
The sampler and synth helpers (Bank, Kit, Mix, saw, svf_lp, adsr, detuned, piano_note)
are tools/make_material.py's.
"""
import re
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
import make_material as mm  # noqa: E402

SR = mm.SR
CC0 = Path.home() / "Documents" / "Sites" / "resilio-versio" / "test_audio" / "demo_sources" / "cc0"
LEAD = 0.25  # every source starts after this much silence
midi, hz, db, hum, vary = mm.midi, mm.hz, mm.db, mm.hum, mm.vary


def write(buf: np.ndarray, path: Path, peak_db: float = -6.0, gain: float | None = None) -> float:
    g = db(peak_db) / max(1e-9, np.abs(buf).max()) if gain is None else gain
    sf.write(str(path), (buf * g).astype(np.float32), SR, subtype="FLOAT")
    print(f"wrote {path.name}: {len(buf) / SR:.1f} s, gain {20 * np.log10(g):+.1f} dB")
    return g


def limit(x: np.ndarray, reduce_db: float, look_ms: float = 3.0, release_ms: float = 120.0) -> np.ndarray:
    """A gentle look-ahead peak limiter: takes the loudest transients down by about
    `reduce_db` (the ceiling sits that far under the peak), so a plucked part can sit at
    the clips' loudness without its picking peaking over -1 dBFS."""
    a = np.abs(x).max(axis=1) if x.ndim == 2 else np.abs(x)
    ceil = a.max() * db(-reduce_db)
    look = int(look_ms / 1000 * SR)
    pad = np.concatenate([a, np.zeros(look)])
    peak = np.lib.stride_tricks.sliding_window_view(pad, look + 1).max(axis=1)[: len(a)]
    want = np.minimum(1.0, ceil / np.maximum(peak, 1e-9))
    g = np.empty_like(want)
    rel = np.exp(-1 / (release_ms / 1000 * SR))
    cur = 1.0
    for i in range(len(want)):  # instant down (the look-ahead already leads it), slow up
        cur = want[i] if want[i] < cur else want[i] + (cur - want[i]) * rel
        g[i] = cur
    k = np.ones(look) / look  # smooth the steps over the look-ahead
    g = np.convolve(np.concatenate([np.full(look, g[0]), g]), k, mode="valid")[: len(a)]
    g = np.minimum(g, want * 1.02)
    return x * (g[:, None] if x.ndim == 2 else g)


def stereo(y: np.ndarray, width_ms: float = 9.0) -> np.ndarray:
    return np.stack([y, np.roll(y, int(width_ms / 1000 * SR))], axis=1)


# ---- Synth voices -----------------------------------------------------------------

def ep_note(m: float, vel: float, hold: float) -> np.ndarray:
    """A two-operator FM electric piano: a bell-ish attack (the index falls fast), a soft
    tine click, a long sine-like body; the damper at release."""
    f0 = hz(m)
    t60 = 6.0 * 2 ** (-(m - 48) / 24)
    n = int((hold + 0.25) * SR)
    t = np.arange(n) / SR
    index = (0.4 + 2.2 * vel) * np.exp(-t / 0.18) + 0.25
    body = np.sin(2 * np.pi * f0 * t + index * np.sin(2 * np.pi * f0 * t))
    tine = 0.08 * vel * np.sin(2 * np.pi * f0 * 13.9 * t) * np.exp(-t / 0.02)
    x = (body + tine) * np.exp(-6.91 * t / t60)
    x[: int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))
    rel = int(0.25 * SR)
    x[-rel:] *= np.exp(-np.linspace(0, 6.91, rel))
    return x * vel


def mono_lead(notes: list[tuple[float, float, str]], spb: float, cutoff: float = 1500.0,
              glide: float = 0.08, vib_after: float = 0.4, saw_share: float = 0.45) -> np.ndarray:
    """A soft mono lead (saw + triangle; saw_share 0 is a pure triangle, odd harmonics only),
    glide, delayed vibrato, a gentle filter swell."""
    end = (notes[-1][0] + notes[-1][1]) * spb + 1.0
    n = int(end * SR)
    f = np.zeros(n)
    gate = np.zeros(n)
    trig = np.zeros(n)
    cur = midi(notes[0][2])
    for b, l, nm in notes:
        i0, i1 = int(b * spb * SR), int((b + l) * spb * SR * 0.98)
        gate[i0:i1] = 1
        trig[i0] = 1
        tgt = midi(nm)
        g = int(glide * SR)
        f[i0:i0 + g] = np.linspace(cur, tgt, g)
        f[i0 + g:i1 + int(0.4 * SR)] = tgt
        cur = tgt
    for i in range(1, n):
        if f[i] == 0:
            f[i] = f[i - 1]
    f[f == 0] = midi(notes[0][2])
    since = np.zeros(n)
    c = 0
    for i in range(n):
        c = 0 if trig[i] else c + 1
        since[i] = c / SR
    vib = 0.15 * np.clip((since - vib_after) / 0.6, 0, 1) * np.sin(2 * np.pi * 5.0 * np.arange(n) / SR)
    fr = hz(f + vib)
    s = mm.saw(fr)
    sq = mm.saw(fr) - mm.saw(fr, 0.5)  # a band-limited square (odd harmonics)...
    tri = np.cumsum(sq) * (2 * fr / SR)  # ...integrated: a triangle, odd harmonics only
    tri -= np.convolve(tri, np.ones(2400) / 2400, mode="same")  # no DC drift
    x = saw_share * s + (1 - saw_share) * tri / max(1e-9, np.abs(tri).max())
    amp = np.zeros(n)
    fenv = np.zeros(n)
    a = fe = 0.0
    for i in range(n):
        tgt = 1.0 if gate[i] else 0.0
        a += (tgt - a) * (0.0025 if tgt > a else 0.0006)
        fe = 1.0 if trig[i] else fe * 0.99993
        amp[i], fenv[i] = a, fe
    return mm.svf_lp(x, cutoff + 1400 * fenv, 0.9) * amp


# ---- The clips --------------------------------------------------------------------

def clip01(out: Path):
    """Endless pad: four slow chords (the make_material pad's voicing), then nothing:
    the tail holds."""
    chords = [("A2", "E3", "B3", "C4", "G4"), ("F2", "C3", "A3", "E4", "G4"),
              ("D2", "A2", "F3", "C4", "E4"), ("E2", "B2", "D3", "A3", "E4")]
    hold = 4.0
    m = mm.Mix(27.0)
    for k, ch in enumerate(chords):
        n = int((hold + 2.5) * SR)
        x = sum(mm.detuned(midi(c), n, 5, 14) for c in ch) / len(ch)
        lfo = 1 + 0.25 * np.sin(2 * np.pi * 0.15 * np.arange(n) / SR + k)
        y = mm.svf_lp(x, 1300 * lfo, 0.8) * mm.adsr(n, 1.5, 1.0, 1.0, 1.0, hold + 0.3)
        m.add(stereo(y, 11), LEAD + k * hold)
    write(m.buf, out / "01.wav")


def clip02(out: Path):
    """Freeze, then paint: an electric piano chord (D minor 9), then a lead over it in D minor."""
    m = mm.Mix(28.0)
    for k, nm in enumerate(("D3", "A3", "C4", "E4", "F4")):
        m.add(ep_note(midi(nm), vary(0.75, 0.05), 3.2), LEAD + k * 0.02, 1.0, -0.3 + 0.15 * k)
    spb = 60 / 80
    t0 = 4.0 / spb  # the lead starts at 4 s
    notes = [(0, 1.5, "A4"), (1.5, 0.5, "C5"), (2, 2, "D5"),
             (6, 1, "F5"), (7, 1, "E5"), (8, 1, "D5"), (9, 3, "C5"),
             (15, 0.5, "A4"), (15.5, 0.5, "C5"), (16, 1, "D5"), (17, 1, "E5"), (18, 4, "A5")]
    y = mono_lead([(b + t0, l, nm) for b, l, nm in notes], spb, cutoff=1400)
    m.add(stereo(y, 0.0) * 0.55, 0.0, 1.0)
    write(m.buf, out / "02.wav")


def stab_chord(n: int, open_: float) -> np.ndarray:
    chord = [midi(c) for c in ("C3", "G3", "Bb3", "Eb4", "D4")]
    x = sum(mm.detuned(c, n, 3, 9) for c in chord) / len(chord)
    cut = 350 + (700 + 2600 * open_) * np.exp(-np.arange(n) / (0.08 * SR))
    return mm.svf_lp(x, cut, 1.1) * mm.adsr(n, 0.003, 0.09, 0.0, 0.05, 0.25)


def clip03(out: Path):
    """Dub techno at 118 bpm in C minor: kick on every beat, a rimshot and a quiet hat,
    minor-ninth stabs on the offbeats. Only the stabs go to the send. Eight bars, then
    everything stops and the tail is left."""
    vd = CC0 / "virtuosity"
    kit = mm.Kit({"kick": sorted((vd / "kick").glob("*.flac")), "rim": sorted((vd / "snare_rim").glob("*.flac")),
                  "hh": sorted((vd / "hh").glob("*.flac"))})
    spb, bars = 60 / 118, 8
    dur = 24.0
    full, send = mm.Mix(dur), mm.Mix(dur)
    T = lambda bar, b: LEAD + (bar * 4 + b) * spb + hum(2)
    n_stab = int(0.45 * SR)
    patterns = [(0.5, 2.5), (0.5, 1.75, 3.5), (0.5, 2.5), (0.5, 2.75, 3.5)]
    for bar in range(bars):
        for b in range(4):
            full.add(kit.hit("kick", vary(0.8)), T(bar, b), db(-3))
        for b in ((1.75, 3.0) if bar % 2 else (3.0,)):
            full.add(kit.hit("rim", vary(0.6, 0.2)), T(bar, b), db(-12), 0.2)
        for b in (0.5, 1.5, 2.5, 3.5):
            full.add(kit.hit("hh", vary(0.4, 0.1)), T(bar, b), db(-22), -0.3)
        open_ = 0.5 - 0.5 * np.cos(2 * np.pi * (bar + 0.5) / bars)
        for b in patterns[bar % 4]:
            y, t, v = stab_chord(n_stab, open_), T(bar, b), db(-4) * vary(0.9, 0.05)
            full.add(y, t, v, 0.1)
            send.add(y, t, v, 0.1)
    full.add(kit.hit("kick", 0.8), T(bars, 0), db(-3))  # the downbeat that ends it
    g = write(full.buf, out / "03.wav")
    # The send pushed 9 dB over the stabs' level in the mix, as a dub engineer rides it: at the
    # mix's level the stabs reached only about 4 dB over the duck's threshold (-24 dBFS), so the
    # endless tail hardly ducked. Peaks about -1.7 dBFS into the reverb.
    write(send.buf, out / "03_send.wav", gain=g * db(9.0))


def clip05(out: Path):
    """Bloom swell: single piano notes, spaced, in A minor."""
    # 4.2 s apart: BLOOM at DEPTH 2 o'clock takes about 2.5 s to swell, so at 3 s apart (the first
    # draft) each swell peaked just as the next note came and they ran together.
    m = mm.Mix(27.0)
    for k, (nm, v) in enumerate((("A3", 0.7), ("E4", 0.65), ("C5", 0.6), ("B4", 0.6),
                                 ("G4", 0.65), ("A4", 0.6))):
        x = mm.piano_note(midi(nm), vary(v, 0.04), 2.4)
        m.add(x, LEAD + k * 4.2 + hum(3), 1.0, 0.5 * (midi(nm) - 64) / 30)
    write(m.buf, out / "05.wav")


def clip06(out: Path):
    """Shimmer melody: a slow lead in D minor, long notes with space between."""
    spb = 60 / 66
    notes = [(0, 2, "D4"), (2, 1, "F4"), (3, 3, "A4"),
             (8, 1.5, "G4"), (9.5, 0.5, "F4"), (10, 3, "E4"),
             (16, 1, "D4"), (17, 1, "E4"), (18, 1, "F4"), (19, 4, "A4")]
    y = mono_lead([(b + LEAD / spb, l, nm) for b, l, nm in notes], spb, cutoff=1100, glide=0.1, vib_after=0.6,
                  saw_share=0.0)  # a triangle: no octave of its own, so the reverb's octave is plain to hear
    m = mm.Mix(27.0)
    m.add(stereo(y, 0.0), 0.0)
    write(m.buf, out / "06.wav")


def guitar_bank() -> mm.Bank:
    items = []
    for p in sorted((CC0 / "guitar_emily").glob("*.wav")):
        g = re.match(r"([a-g])(b?)(\d)_", p.name)
        if g:  # Emilyguitar names its notes true (Resilio's OFFSETS: guitar 0)
            items.append((12 * (int(g.group(3)) + 1) + mm.PC[g.group(1)] - (g.group(2) == "b"), p))
    return mm.Bank(items)


def clip07(out: Path):
    """Worn guitar: slow strummed chords on a clean guitar, E minor, one every ~3.6 s."""
    gtr = guitar_bank()
    shapes = [[40, 47, 52, 55, 59, 66], [36, 43, 48, 52, 55, 59], [45, 52, 55, 59, 64, 67],
              [47, 54, 57, 62, 64, 66], [40, 47, 52, 55, 59, 66], [36, 43, 48, 52, 55, 59],
              [45, 52, 55, 60, 64, 67]]
    m = mm.Mix(28.0)
    for k, ch in enumerate(shapes):
        t = LEAD + k * 3.6
        for j, n in enumerate(ch):
            x = mm.fade(gtr.note(n), 3.4, 0.3)
            m.add(x, t + j * 0.045 + abs(hum(3)), db(-6) * vary(0.8, 0.06), (n - 52) / 30)
    write(limit(m.buf, 5.0), out / "07.wav")


def clip08(out: Path):
    """Tape side: electric piano chords, held and lightly rolled, with a few notes between
    (the make_material keys part, played on the FM piano), twice through."""
    spb = 60 / 84
    chords = [(0, 3, ("D3", "F3", "A3", "C4", "E4")), (4, 3, ("G2", "F3", "Bb3", "D4")),
              (8, 3, ("C3", "E3", "G3", "Bb3", "D4")), (12, 3, ("F2", "E3", "A3", "C4", "E4"))]
    fills = [(3.0, 0.5, "A4"), (3.5, 0.5, "G4"), (7.0, 1, "F4"), (11.0, 0.5, "E4"), (11.5, 0.5, "D4")]
    m = mm.Mix(27.0)
    for rep in range(2):
        o = rep * 16
        for b, l, ch in chords:
            for k, nm in enumerate(ch):
                m.add(ep_note(midi(nm), vary(0.7, 0.05), l * spb), LEAD + (o + b) * spb + k * 0.018 + hum(), 1.0,
                      -0.3 + 0.15 * k)
        for b, l, nm in fills if rep == 0 else fills[:3]:
            m.add(ep_note(midi(nm), vary(0.6, 0.05), l * spb), LEAD + (o + b) * spb + hum(), 1.0, 0.2)
    write(m.buf, out / "08.wav")


def clip10(out: Path):
    """Dark drone: a low D, its fifth and octave, slowly swelling and slowly opening, then let go."""
    dur = 28.0
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = (mm.detuned(midi("D1"), n, 3, 7) + 0.8 * mm.detuned(midi("A1"), n, 3, 9)
         + 0.6 * mm.detuned(midi("D2"), n, 3, 11)) / 2.4
    cut = 180 + 260 * (0.5 - 0.5 * np.cos(np.pi * np.clip(t / 18, 0, 1))) * (1 + 0.15 * np.sin(2 * np.pi * 0.07 * t))
    y = mm.svf_lp(x, cut, 0.9)
    env = np.clip((t - LEAD) / 3.0, 0, 1) * np.clip((21.0 - t) / 3.0, 0, 1)
    write(stereo(y * env, 13), out / "10.wav")


CLIPS = {"01": clip01, "02": clip02, "03": clip03, "05": clip05, "06": clip06, "07": clip07,
         "08": clip08, "10": clip10}

if __name__ == "__main__":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    if not CC0.is_dir():
        sys.exit(f"missing {CC0}")
    for cid in sys.argv[2:] or list(CLIPS):
        CLIPS[cid](out)
