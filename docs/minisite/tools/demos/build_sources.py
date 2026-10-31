#!/usr/bin/env python3
"""Site demos (docs/minisite/demos-plan.md): build the dry, musical sources.

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/build_sources.py <out dir> [clip ids...]
Needs numpy and soundfile. Writes <out dir>/<id>.wav (48 kHz stereo float, peaking at
-6 dBFS, the clip's full length including the room for its tail) and, for clip 03, the
send <id>_send.wav (only the stabs, at their level in the mix: the reverb hears that,
the listener's dry is the whole groove).

Material, all publishable (CLAUDE.md, the brief):
  synthesised here    the pad, the lead (06), the poly synth (08), the drone, the stabs
  CC0 samples         Virtuosity Drums (kick, rimshot, closed hat) and Karoryfer's
                      Emilyguitar, from Resilio Versio's test_audio/demo_sources/cc0
                      (never in either repo; Resilio's fetch_samples.py fetches them);
                      the Versilian Community Sample Library's Steinway B grand (VCSL,
                      CC0; https://github.com/sgossner/VCSL, "Chordophones/Zithers/Grand
                      Piano, Steinway B/Sus", close mic, layers vl2 and vl3) for 02 and 05,
                      in test_audio/demo_sources/vcsl_steinway (gitignored; $IV_PIANO_DIR
                      overrides). Fetch: the JHPiano_Sus_Close_<note>_vl{2,3}_rr1.wav files
                      for D3 G#3 A#3 C4 E4 F#4 G#4 A#4 C5 D5 E5 F#5 G#5 A#5 from the repo.
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
PIANO = Path(__import__("os").environ.get("IV_PIANO_DIR", ROOT / "test_audio" / "demo_sources" / "vcsl_steinway"))
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


class Piano:
    """VCSL's Steinway B (sustain-pedal samples, close mic, sampled every whole tone): the
    nearest sampled note, resampled 44.1 -> 48 kHz and repitched in one step, stereo, at the
    library's own gain (its layers are normalised to line up), a damper fade at the release."""

    def __init__(self, folder: Path = PIANO):
        self.by: dict[tuple[int, int], Path] = {}
        for p in folder.glob("JHPiano_Sus_Close_*_rr1.wav"):
            g = re.match(r"JHPiano_Sus_Close_([A-G])(#?)(-?\d)_vl(\d)_", p.name)
            if g:
                m = 12 * (int(g.group(3)) + 1) + mm.PC[g.group(1).lower()] + (g.group(2) == "#")
                self.by[(m, int(g.group(4)))] = p
        if not self.by:
            raise SystemExit(f"no piano samples in {folder}")

    def note(self, m: int, layer: int, hold: float, damper: float = 0.35) -> np.ndarray:
        have = [k for k, l in self.by if l == layer]
        m0 = min(have, key=lambda k: (abs(k - m), k < m))
        x, sr = sf.read(str(self.by[(m0, layer)]), always_2d=True)
        a = np.abs(x).max(axis=1)
        x = x[max(0, int(np.argmax(a > 0.02 * a.max())) - int(0.002 * sr)):]
        r = 2 ** ((m - m0) / 12) * sr / SR  # source samples per output sample
        n = min(int((len(x) - 1) / r), int((hold + damper) * SR))
        pos = np.arange(n) * r
        y = np.stack([np.interp(pos, np.arange(len(x)), x[:, c]) for c in range(2)], axis=1)
        h = int(hold * SR)
        if h < n:  # the damper: a quick, smooth fall rather than a cut
            y[h:] *= np.exp(-np.linspace(0, 6.91, n - h))[:, None]
        return y


# ---- Synth voices -----------------------------------------------------------------

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
    """Endless pad: one chord (A minor add 9, the make_material pad's voicing), held for
    eight seconds, then nothing: the tail holds. (The owner, 31 Oct 2026: "just play a
    single chord instead of 4".)"""
    ch = ("A2", "E3", "B3", "C4", "G4")
    hold = 8.0
    m = mm.Mix(24.0)
    n = int((hold + 2.5) * SR)
    x = sum(mm.detuned(midi(c), n, 5, 14) for c in ch) / len(ch)
    lfo = 1 + 0.25 * np.sin(2 * np.pi * 0.12 * np.arange(n) / SR)
    y = mm.svf_lp(x, 1300 * lfo, 0.8) * mm.adsr(n, 1.5, 1.0, 1.0, 1.0, hold + 0.3)
    m.add(stereo(y, 11), LEAD)
    write(m.buf, out / "01.wav")


def clip02(out: Path):
    """Freeze, then paint, all on the sampled grand (the owner, 31 Oct 2026: a real piano
    instead of the synth lead): a pedalled D minor 9 chord, softly rolled, then a melody over
    it in D minor (the old lead's notes)."""
    pno = Piano()
    m = mm.Mix(28.0)
    for k, nm in enumerate(("D3", "A3", "C4", "E4", "F4")):
        m.add(pno.note(midi(nm), 2, 3.2, 0.6), LEAD + k * 0.03 + abs(hum(2)), 0.55 * vary(1.0, 0.05))
    spb = 60 / 80
    t0 = 4.0 / spb  # the melody starts at 4 s
    notes = [(0, 1.5, "A4"), (1.5, 0.5, "C5"), (2, 2, "D5"),
             (6, 1, "F5"), (7, 1, "E5"), (8, 1, "D5"), (9, 3, "C5"),
             (15, 0.5, "A4"), (15.5, 0.5, "C5"), (16, 1, "D5"), (17, 1, "E5"), (18, 4, "A5")]
    for b, l, nm in notes:  # legato: each note held to the next, the damper falling as it lands
        m.add(pno.note(midi(nm), 3, l * spb + 0.05), (b + t0) * spb + hum(3), vary(0.9, 0.06))
    write(limit(m.buf, 3.0), out / "02.wav")


def stab_chord(n: int, open_: float) -> np.ndarray:
    chord = [midi(c) for c in ("C3", "G3", "Bb3", "Eb4", "D4")]
    x = sum(mm.detuned(c, n, 3, 9) for c in chord) / len(chord)
    cut = 350 + (700 + 2600 * open_) * np.exp(-np.arange(n) / (0.08 * SR))
    return mm.svf_lp(x, cut, 1.1) * mm.adsr(n, 0.003, 0.09, 0.0, 0.05, 0.25)


SEND_DB = 9.0


def clip03(out: Path):
    """Dub techno at 118 bpm in C minor: kick on every beat, a rimshot and a quiet hat,
    minor-ninth stabs on the offbeats. Only the stabs go to the send. Eight bars, then
    everything stops and the short tail dies away."""
    vd = CC0 / "virtuosity"
    kit = mm.Kit({"kick": sorted((vd / "kick").glob("*.flac")), "rim": sorted((vd / "snare_rim").glob("*.flac")),
                  "hh": sorted((vd / "hh").glob("*.flac"))})
    spb, bars = 60 / 118, 8
    dur = 21.0
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
    # The send at the stabs' own level in the mix (31 Oct 2026). The first cut pushed it 9 dB
    # hotter so an endless tail would duck; the owner heard a wall of sound, so the tail is now
    # short and finite (no duck, render.py) and the send sits where a mix send would.
    write(send.buf, out / "03_send.wav", gain=g * db(SEND_DB))


def clip05(out: Path):
    """Bloom swell: single notes on the sampled grand (the owner, 31 Oct 2026: a real piano
    instead of the synthesised one), spaced, in A minor, softly (the vl2 layer)."""
    # 4.2 s apart: BLOOM at DEPTH 2 o'clock takes about 2.5 s to swell, so at 3 s apart (the first
    # draft) each swell peaked just as the next note came and they ran together.
    pno = Piano()
    m = mm.Mix(27.0)
    for k, (nm, v) in enumerate((("A3", 0.95), ("E4", 0.9), ("C5", 0.85), ("B4", 0.85),
                                 ("G4", 0.9), ("A4", 0.85))):
        m.add(pno.note(midi(nm), 2, 2.4, 0.6), LEAD + k * 4.2 + hum(3), vary(v, 0.03))
    write(limit(m.buf, 2.0), out / "05.wav")


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


def poly_note(m: float, n: int, sub: bool) -> np.ndarray:
    """One voice of a classic subtractive poly: two saws a few cents apart, a square an
    octave down under the bass note."""
    f = hz(m)
    x = 0.5 * (mm.saw(np.full(n, f * 2 ** (6 / 1200)), mm.rng.random())
               + mm.saw(np.full(n, f * 2 ** (-6 / 1200)), mm.rng.random()))
    if sub:
        x += 0.4 * (mm.saw(np.full(n, f / 2)) - mm.saw(np.full(n, f / 2), 0.5))
    return x


def poly_chord(notes: tuple[str, ...], hold: float, open_: float = 1.0) -> np.ndarray:
    """A chord on the poly synth: the voices through one resonant low-pass (two 2-pole
    stages, the first resonant: a 24 dB slope), the filter opened by its own envelope
    (quick attack, a slow fall to a warm sustain), a soft amp envelope."""
    n = int((hold + 0.9) * SR)
    ms = sorted(midi(c) for c in notes)
    x = sum(poly_note(mv, n, k == 0 and len(ms) > 1) for k, mv in enumerate(ms)) / np.sqrt(len(ms))
    fenv = mm.adsr(n, 0.015, 0.9, 0.35, 0.5, hold)
    cut = 420 + (1600 + 900 * open_) * fenv
    y = mm.svf_lp(mm.svf_lp(x, cut, 1.8), cut, 0.7)
    return y * mm.adsr(n, 0.03, 0.6, 0.8, 0.5, hold)


def clip08(out: Path):
    """Tape side: held chords on a classic subtractive poly synth (the owner, 31 Oct 2026:
    "a more traditional subtractive synth sound" instead of the electric piano), D minor 9,
    G minor 7, C 9, F major 7, twice through at 80 bpm, a few single notes between; the
    last chord held long. Sustained, so MOTION's wow and flutter have notes to bend."""
    spb = 60 / 80
    chords = [(0, 3.5, ("D3", "F3", "A3", "C4", "E4")), (4, 3.5, ("G2", "F3", "Bb3", "D4")),
              (8, 3.5, ("C3", "E3", "G3", "Bb3", "D4")), (12, 3.5, ("F2", "E3", "A3", "C4", "E4"))]
    fills = [(3.0, 1.0, "A4"), (7.0, 1.0, "F4"), (11.0, 0.5, "E4"), (11.5, 0.5, "D4")]
    m = mm.Mix(28.0)
    for rep in range(2):
        o = rep * 16
        for k, (b, l, ch) in enumerate(chords):
            last = rep == 1 and k == 3
            y = poly_chord(ch, (6.0 if last else l) * spb, 0.6 + 0.4 * ((k + rep) % 2))
            m.add(stereo(y, 7), LEAD + (o + b) * spb + hum(), vary(1.0, 0.04))
        for b, l, nm in fills if rep == 0 else fills[:2]:
            m.add(stereo(poly_chord((nm,), l * spb * 0.9, 0.5), 7) * 0.5, LEAD + (o + b) * spb + hum(), 1.0)
    write(m.buf, out / "08.wav")


def clip10(out: Path):
    """Dark drone: D minor in the low register (D1 under D2, A2, D3 and F3), slowly swelling and
    slowly opening, held while WEAR is swept (render.py), then let go. Voiced higher and opened
    further than the first cut (D1, A1, D2 under 440 Hz): WEAR's stages work in the mids and
    highs, and a drone with nothing there hid them (31 Oct 2026, measure.py m10)."""
    dur = 28.0
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = (0.6 * mm.detuned(midi("D1"), n, 3, 7) + mm.detuned(midi("D2"), n, 3, 9) + 0.8 * mm.detuned(midi("A2"), n, 3, 11)
         + 0.6 * mm.detuned(midi("D3"), n, 3, 12) + 0.4 * mm.detuned(midi("F3"), n, 3, 13)) / 2.4
    cut = 400 + 1400 * (0.5 - 0.5 * np.cos(np.pi * np.clip(t / 18, 0, 1))) * (1 + 0.15 * np.sin(2 * np.pi * 0.07 * t))
    y = mm.svf_lp(x, cut, 0.9)
    env = np.clip((t - LEAD) / 3.0, 0, 1) * np.clip((24.0 - t) / 3.0, 0, 1)
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
