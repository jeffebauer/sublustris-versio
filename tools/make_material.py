#!/usr/bin/env python3
"""Listening material for the review pages: dry phrases across the sounds a dub,
dub techno or ambient track puts through a reverb (the owner, 22 Oct 2026: "pads,
percussion, synth leads etc ... a more well-rounded and holistic range of sounds").

Usage: .venv/bin/python tools/make_material.py [name ...]   (default: all)
Needs numpy and soundfile (python3 -m venv .venv && .venv/bin/pip install numpy soundfile).
Writes renders/_material/<name>.wav: 48 kHz stereo 24-bit, the phrase then 12 s of
silence so tails ring out, loudness-matched (active RMS -20 dBFS, peaks <= -3 dBFS).
tools/render_review_pages.sh picks up every file it finds there.

Material (skank and vibes are built by render_review_pages.sh itself):
  stabs   dub techno chord stabs on the offbeats, filter moving bar to bar (synth)
  pad     an ambient pad, slow swells through four chords (synth)
  lead    a mono synth lead with glide and delayed vibrato, phrases with rests (synth)
  drums   a dub techno groove: kick, rimshots, hats, cowbell, bongo fills (CC0 kits)
  bass    a dub bassline, low and round (CC0 Baby Blue bass)
  keys    electric piano chords, held and lightly rolled (Wurlitzer, CC BY 3.0)
  voice   spoken phrases with gaps: consonants and sibilance (macOS speech)
  voicelow  the same idea in a low male voice (macOS Ralph, ~74 Hz): the "metallic?" test
  fullmix a dense dub techno track (kick, rim, hats, bass, pad, stabs), 120 bpm, C minor; the
          reverb gets only fullmix_send.wav (stabs and rims, at their level in the mix), the
          page's dry is the whole mix: masking changes how loud a reverb seems (item 7)
  piano   a dry, close piano (synthesized: stretched partials, hammer knock, two-stage
          string decay), single notes, repeats and chords: a ring shows on it first

Samples come from Resilio Versio's test_audio/demo_sources/cc0 (never in either
repo): Virtuosity Drums and Rusty Drums (Karoryfer, CC0), Karoryfer Baby Blue bass
(CC0), VCSL bongos (CC0), TR-808 cowbell (CC0), Wurlitzer 200A (CC BY 3.0, see its
LICENSE; fine for private listening, credit it before publishing anything).
The sampler helpers (Bank, Kit, Mix, load, repitch) follow Resilio's
docs/minisite/tools/demos/v2/build_sources.py.
"""
import random
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
TAIL = 12.0
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "renders" / "_material"
CC0 = ROOT.parent / "resilio-versio" / "test_audio" / "demo_sources" / "cc0"
rng = random.Random(7)   # fixed: the same material every run
nrng = np.random.default_rng(7)

PC = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def midi(name: str) -> int:
    """'C3' / 'Eb4' / 'F#2' -> MIDI (C4 = 60)."""
    m = re.fullmatch(r"([A-Ga-g])([#b]?)(-?\d)", name)
    return 12 * (int(m.group(3)) + 1) + PC[m.group(1).lower()] + {"#": 1, "b": -1, "": 0}[m.group(2)]


def hz(m: float) -> float:
    return 440.0 * 2 ** ((m - 69) / 12)


def db(x: float) -> float:
    return 10 ** (x / 20)


# ---- Samples ----------------------------------------------------------------------

_cache: dict = {}


def load(path: Path) -> np.ndarray:
    """Mono, 48 kHz, leading silence trimmed (3 ms kept before the onset)."""
    if path in _cache:
        return _cache[path]
    x, sr = sf.read(str(path), always_2d=True)
    x = x.mean(axis=1)
    if sr != SR:
        n = int(round(len(x) * SR / sr))
        x = np.interp(np.linspace(0, len(x) - 1, n), np.arange(len(x)), x)
    a = np.abs(x)
    on = int(np.argmax(a > 0.02 * a.max()))
    x = x[max(0, on - int(0.003 * SR)):]
    _cache[path] = x
    return x


def repitch(x: np.ndarray, semis: float) -> np.ndarray:
    if abs(semis) < 1e-6:
        return x
    r = 2 ** (semis / 12)
    return np.interp(np.arange(int(len(x) / r)) * r, np.arange(len(x)), x)


def fade(x: np.ndarray, length: float | None, release: float = 0.05) -> np.ndarray:
    x = x.copy()
    if length is not None:
        n, r = int(length * SR), int(release * SR)
        x = x[: n + r]
        if len(x) > n:
            x[n:] *= np.linspace(1, 0, len(x) - n) ** 2
    return x


class Bank:
    """Pitched samples: nearest sampled note, resampled; round robins rotate."""

    def __init__(self, items: list[tuple[int, Path]]):
        self.by: dict[int, list[Path]] = {}
        for m, p in items:
            self.by.setdefault(m, []).append(p)
        if not self.by:
            raise SystemExit("empty sample bank")
        self.last: dict[int, Path] = {}

    def note(self, m: int) -> np.ndarray:
        m0 = min(self.by, key=lambda k: (abs(k - m), k < m))
        fs = self.by[m0]
        f = rng.choice([p for p in fs if p != self.last.get(m0)] or fs)
        self.last[m0] = f
        x = load(f)
        x = x / max(1e-9, np.sqrt(np.mean(x[: int(0.3 * SR)] ** 2))) * 0.25
        return repitch(x, m - m0)


class Kit:
    """Drum pieces: velocity picks the layer (the tone), the caller sets the level."""

    def __init__(self, pieces: dict[str, list[Path]]):
        for k, fs in pieces.items():
            if not fs:
                raise SystemExit(f"no samples for {k}")
        self.pieces = pieces
        self.last: dict[str, Path] = {}

    def hit(self, piece: str, vel: float) -> np.ndarray:
        fs = self.pieces[piece]
        lay = lambda f: int(m.group(1)) if (m := re.search(r"_vl?(\d+)", f.name)) else 1
        layers = sorted({lay(f) for f in fs})
        want = layers[min(len(layers) - 1, max(0, int(round(vel * (len(layers) - 1)))))]
        pick = [f for f in fs if lay(f) == want]
        f = rng.choice([p for p in pick if p != self.last.get(piece)] or pick)
        self.last[piece] = f
        x = load(f)
        return x / max(1e-9, np.abs(x).max())


class Mix:
    def __init__(self, dur: float):
        self.buf = np.zeros((int(dur * SR), 2))

    def add(self, x: np.ndarray, t: float, gain: float = 1.0, pan: float = 0.0):
        i = max(0, int(round(t * SR)))
        if i >= len(self.buf):
            return
        if x.ndim == 1:
            l, r = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
            x = np.stack([x * l * np.sqrt(2), x * r * np.sqrt(2)], axis=1)
        x = x[: len(self.buf) - i] * gain
        self.buf[i: i + len(x)] += x

    def write(self, name: str, trim_db: float = 0.0, gain: float | None = None) -> float:
        """trim_db: by ear-ish loudness (LUFS) after the RMS match; sustained material reads louder.
        gain: use this gain instead of matching (a send written at its mix's level). Returns the gain."""
        y = self.buf
        mono = y.mean(axis=1)
        w = int(0.05 * SR)
        win = np.sqrt(np.mean(mono[: len(mono) // w * w].reshape(-1, w) ** 2, axis=1))
        act = win[win > win.max() * db(-30)]
        g = db(-20 + trim_db) / max(1e-9, np.sqrt(np.mean(act ** 2)))
        g = min(g, db(-3) / max(1e-9, np.abs(y).max())) if gain is None else gain
        y = np.concatenate([y * g, np.zeros((int(TAIL * SR), 2))])
        OUT.mkdir(parents=True, exist_ok=True)
        # Float when it peaks over full scale (the pushed send does), so nothing clips; the renderer reads both.
        sf.write(str(OUT / f"{name}.wav"), y.astype(np.float32), SR, subtype="FLOAT" if np.abs(y).max() > 1.0 else "PCM_24")
        print(f"wrote renders/_material/{name}.wav: {len(y) / SR:.1f} s (phrase {len(self.buf) / SR:.1f} s), gain {20 * np.log10(g):+.1f} dB")
        return g


def hum(sd_ms: float = 3.0) -> float:
    return rng.gauss(0, sd_ms / 1000)


def vary(v: float, amt: float = 0.08) -> float:
    return min(1.0, max(0.05, v + rng.uniform(-amt, amt)))


# ---- Synthesis --------------------------------------------------------------------

def saw(freq: np.ndarray, phase0: float = 0.0) -> np.ndarray:
    """Band-limited saw (PolyBLEP) for a per-sample frequency array."""
    dt = freq / SR
    ph = (phase0 + np.cumsum(dt)) % 1.0
    y = 2 * ph - 1
    t = ph / dt
    m = t < 1
    y[m] -= (t[m] + t[m] - t[m] * t[m] - 1)
    t = (ph - 1) / dt
    m = t > -1
    y[m] -= (t[m] * t[m] + t[m] + t[m] + 1)
    return y


def svf_lp(x: np.ndarray, cutoff: np.ndarray, q: float = 0.7) -> np.ndarray:
    """2-pole state-variable lowpass (Chamberlin, 2x oversampled) with a per-sample cutoff."""
    f = 2 * np.sin(np.pi * np.minimum(cutoff, SR / 5) / (2 * SR))
    damp = 1 / q
    lo = bp = 0.0
    y = np.empty_like(x)
    for i in range(len(x)):
        fi = f[i]
        for _ in range(2):
            lo += fi * bp
            hi = x[i] - lo - damp * bp
            bp += fi * hi
        y[i] = lo
    return y


def adsr(n: int, a: float, d: float, s: float, r: float, gate: float) -> np.ndarray:
    t = np.arange(n) / SR
    e = np.where(t < a, t / max(a, 1e-4), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))
    g = int(gate * SR)
    if g < n:
        e[g:] = e[min(g, n - 1)] * np.exp(-(t[g:] - gate) / max(r, 1e-4))
    return e


def detuned(m: float, n: int, voices: int, cents: float) -> np.ndarray:
    out = np.zeros(n)
    for k in range(voices):
        c = 0.0 if voices == 1 else cents * (2 * k / (voices - 1) - 1)
        out += saw(np.full(n, hz(m + c / 100)), rng.random())
    return out / np.sqrt(voices)


# ---- The material -----------------------------------------------------------------

def stabs():
    """Dub techno: one minor-ninth chord, stabbed on the offbeats, the filter opening and
    closing across 8 bars (Basic Channel-ish). 118 bpm."""
    spb = 60 / 118
    chord = [midi(n) for n in ("C3", "G3", "Bb3", "Eb4", "D4")]
    pattern = [0.5, 1.5, 2.5, 3.25, 3.5]   # beats in the bar
    bars = 8
    m = Mix(0.1 + bars * 4 * spb + 0.6)
    n = int(0.45 * SR)
    for bar in range(bars):
        open_ = 0.5 - 0.5 * np.cos(2 * np.pi * bar / bars)   # closed → open → closed
        for b in pattern:
            if bar % 4 == 3 and b == 3.25:
                continue
            x = sum(detuned(c, n, 3, 9) for c in chord) / len(chord)
            env = adsr(n, 0.003, 0.09, 0.0, 0.05, 0.25)
            cut = 350 + (700 + 2600 * open_) * np.exp(-np.arange(n) / (0.08 * SR))
            y = svf_lp(x, cut, 1.1) * env
            m.add(y, 0.1 + (bar * 4 + b) * spb + hum(), vary(0.9, 0.1), rng.uniform(-0.15, 0.15))
    m.write("stabs", +2.0)


def pad():
    """Ambient: four chords, each swelling in over ~1.5 s and overlapping the next."""
    chords = [("A2", "E3", "B3", "C4", "G4"), ("F2", "C3", "A3", "E4", "G4"),
              ("D2", "A2", "F3", "C4", "E4"), ("E2", "B2", "D3", "A3", "E4")]
    hold = 4.0
    m = Mix(len(chords) * hold + 2.5)
    for k, ch in enumerate(chords):
        n = int((hold + 2.5) * SR)
        x = sum(detuned(midi(c), n, 5, 14) for c in ch) / len(ch)
        lfo = 1 + 0.25 * np.sin(2 * np.pi * 0.15 * np.arange(n) / SR + k)
        y = svf_lp(x, 1300 * lfo, 0.8) * adsr(n, 1.5, 1.0, 1.0, 1.0, hold + 0.3)
        m.add(np.stack([y * 0.9, np.roll(y, int(0.011 * SR)) * 0.9], axis=1), 0.05 + k * hold)
    m.write("pad", -5.0)


def lead():
    """A mono synth lead: saw + square-ish blend, 60 ms glide, vibrato after 0.25 s, filter
    envelope per note; phrases with rests so the tails are heard. 100 bpm, C minor."""
    spb = 60 / 100
    notes = [  # (beat, length in beats, note) ; None = rest
        (0, 1.5, "G4"), (1.5, 0.5, "Bb4"), (2, 1, "C5"), (3, 0.5, "Eb5"), (3.5, 2.5, "D5"),
        (8, 0.5, "C5"), (8.5, 0.5, "Bb4"), (9, 1, "G4"), (10, 2, "F4"),
        (14, 0.75, "G4"), (14.75, 0.25, "Ab4"), (15, 1, "G4"), (16, 0.5, "Eb5"), (16.5, 3.5, "C5"),
        (22, 0.5, "G5"), (22.5, 0.5, "F5"), (23, 0.5, "Eb5"), (23.5, 0.5, "D5"), (24, 4, "C5"),
    ]
    end = (notes[-1][0] + notes[-1][1]) * spb + 1.0
    n = int(end * SR)
    f = np.zeros(n)
    gate = np.zeros(n)
    trig = np.zeros(n)
    cur = midi(notes[0][2])
    for b, l, nm in notes:
        i0, i1 = int(b * spb * SR), int((b + l) * spb * SR * 0.97)
        gate[i0:i1] = 1
        trig[i0] = 1
        tgt = midi(nm)
        g = int(0.06 * SR)
        f[i0:i0 + g] = np.linspace(cur, tgt, g)
        f[i0 + g:i1 + int(0.3 * SR)] = tgt
        cur = tgt
    # hold the pitch through rests
    for i in range(1, n):
        if f[i] == 0:
            f[i] = f[i - 1]
    # delayed vibrato: grows 0.25 s after each note start
    since = np.zeros(n)
    c = 0
    for i in range(n):
        c = 0 if trig[i] else c + 1
        since[i] = c / SR
    vib = 0.18 * np.clip((since - 0.25) / 0.4, 0, 1) * np.sin(2 * np.pi * 5.2 * np.arange(n) / SR)
    fr = hz(f + vib)
    x = 0.6 * saw(fr) + 0.4 * (saw(fr) - saw(fr, 0.5))  # saw + pulse
    # amp and filter envelopes from the gate
    amp = np.zeros(n)
    fenv = np.zeros(n)
    a, fe = 0.0, 0.0
    for i in range(n):
        tgt = 1.0 if gate[i] else 0.0
        a += (tgt - a) * (0.004 if tgt > a else 0.0012)
        fe = 1.0 if trig[i] else fe * 0.99985
        amp[i], fenv[i] = a, fe
    y = svf_lp(x, 900 + 3200 * fenv, 1.4) * amp
    m = Mix(end + 0.2)
    m.add(y, 0.1)
    m.write("lead")


def drums():
    """Dub techno groove, 122 bpm, 8 bars: four-on-the-floor kick, rimshots on syncopated
    16ths, closed hats, an 808 cowbell, bongo fills into bars 4 and 8."""
    vd, rd = CC0 / "virtuosity", CC0 / "rusty"
    g = lambda d: sorted(d.glob("*.flac"))
    kit = Kit({"kick": g(vd / "kick"), "rim": g(rd / "rimshot"), "hh": g(rd / "hh_closed"),
               "hh_open": sorted((vd / "hh_open").glob("*half*.flac"))})
    bh = sorted((CC0 / "bongos").glob("BongoH_Hit1_*.wav"))
    bl = sorted((CC0 / "bongos").glob("BongoL_Hit1_*.wav"))
    cow = load(CC0 / "cowbell_808" / "CB.WAV")
    spb = 60 / 122
    bars = 8
    m = Mix(0.1 + bars * 4 * spb + 1.0)
    T = lambda bar, b: 0.1 + (bar * 4 + b) * spb + hum(2)
    for bar in range(bars):
        for b in range(4):
            m.add(kit.hit("kick", vary(0.8)), T(bar, b), db(-2))
        for b in (0.75, 1.5, 2.75, 3.25) if bar % 2 else (0.75, 1.5, 2.75):
            m.add(kit.hit("rim", vary(0.6, 0.2)), T(bar, b), db(-6), 0.2)
        for s in range(16):
            b = s / 4
            if s % 4 == 2:
                m.add(kit.hit("hh_open", vary(0.5)), T(bar, b), db(-14), -0.3)
            else:
                m.add(kit.hit("hh", vary(0.35 if s % 2 else 0.6, 0.15)), T(bar, b), db(-15 if s % 2 else -12), -0.3)
        if bar % 4 == 1:
            m.add(cow / np.abs(cow).max(), T(bar, 3.5), db(-13), 0.4)
        if bar % 4 == 3:
            for b, side in ((2.5, bh), (2.75, bh), (3.0, bl), (3.5, bh), (3.75, bl)):
                x = load(rng.choice(side))
                m.add(x / np.abs(x).max(), T(bar, b), db(-9), 0.5 if side is bh else -0.5)
    m.write("drums")


def bass():
    """A dub bassline, 75 bpm, A minor: roots and fifths with space, long notes that bloom."""
    folder = CC0 / "bass_babyblue"
    items = []
    for p in sorted(folder.glob("*.wav")):
        mm = re.search(r"_([a-g])(b?)(\d)_", p.name)
        if mm:  # Baby Blue names its notes an octave high (Resilio's pitch tracker)
            items.append((12 * (int(mm.group(3)) + 1) + PC[mm.group(1)] - (mm.group(2) == "b") - 12, p))
    bank = Bank(items)
    spb = 60 / 75
    line = [(0, 1.5, "A1"), (1.5, 0.5, "A1"), (2, 1, "E2"), (3.5, 0.5, "G1"),
            (4, 2, "A1"), (6.5, 0.5, "C2"), (7, 1, "D2"),
            (8, 1.5, "F1"), (9.5, 0.5, "F1"), (10, 1, "C2"), (11.5, 0.5, "E1"),
            (12, 1.5, "G1"), (13.5, 0.5, "D2"), (14, 2, "E2")]
    m = Mix(0.1 + 16 * spb + 1.0)
    for b, l, nm in line:
        m.add(fade(bank.note(midi(nm)), l * spb * 0.95, 0.06), 0.1 + b * spb + hum(), vary(0.9))
    m.write("bass")


def keys():
    """Electric piano chords, held and lightly rolled, with a few single notes between."""
    folder = CC0 / "wurli" / "Samples"
    items = []
    for p in sorted(folder.glob("*mp.flac")):
        mm = re.match(r"([a-g])(b?)(\d)", p.name)
        items.append((12 * (int(mm.group(3)) + 1) + PC[mm.group(1)] - (mm.group(2) == "b"), p))
    bank = Bank(items)
    spb = 60 / 84
    chords = [(0, 3, ("D3", "F3", "A3", "C4", "E4")), (4, 3, ("G2", "F3", "Bb3", "D4")),
              (8, 3, ("C3", "E3", "G3", "Bb3", "D4")), (12, 2, ("F2", "E3", "A3", "C4", "E4"))]
    fills = [(3.0, 0.5, "A4"), (3.5, 0.5, "G4"), (7.0, 1, "F4"), (11.0, 0.5, "E4"), (11.5, 0.5, "D4")]
    m = Mix(0.1 + 16 * spb + 1.5)
    for b, l, ch in chords:
        for k, nm in enumerate(ch):
            m.add(fade(bank.note(midi(nm)), l * spb, 0.25), 0.1 + b * spb + k * 0.018 + hum(), vary(0.8), -0.3 + 0.15 * k)
    for b, l, nm in fills:
        m.add(fade(bank.note(midi(nm)), l * spb, 0.15), 0.1 + b * spb + hum(), vary(0.7), 0.2)
    m.write("keys")


def spoken(name: str, speaker: str, lines: list[str]):
    m = Mix(16.0)
    t = 0.1
    with tempfile.TemporaryDirectory() as d:
        for k, text in enumerate(lines):
            p = Path(d) / f"{k}.aiff"
            subprocess.run(["say", "-v", speaker, "-r", "150", "-o", str(p), text], check=True)
            x = load(p)
            m.add(x / np.abs(x).max(), t, 1.0, (-0.1, 0.1)[k % 2])
            t += len(x) / SR + (1.2 if k % 2 else 0.8)
    m.buf = m.buf[: int((t + 0.5) * SR)]
    m.write(name)


def voice():
    """Spoken phrases with gaps (macOS speech): consonants and sibilance are where a
    reverb's diffusion and highs show."""
    spoken("voice", "Daniel", ["Dub.", "Deep in the night, the sound system plays.", "Echo... and space.",
                               "Listen. It sustains, it shimmers, it fades away."])


def voicelow():
    """A low male voice: its dense low harmonics are where a tank's metallic ring shows
    (reverb-testing-practices item 1, Costello)."""
    spoken("voicelow", "Ralph", ["Low.", "Down in the basement, the bass rolls on.", "Room... and tone.",
                                 "Hold it. Let it ring. Now let it go."])


def fullmix():
    """A dense track the way a reverb meets it in a mix: everything plays dry, and only the
    stabs and rims go to the send (renders/_material/fullmix_send.wav, written 12 dB over the mix's
    own gain: a pushed dub send, about as loud as the single instruments)."""
    vd, rd = CC0 / "virtuosity", CC0 / "rusty"
    g = lambda d: sorted(d.glob("*.flac"))
    kit = Kit({"kick": g(vd / "kick"), "rim": g(rd / "rimshot"), "hh": g(rd / "hh_closed")})
    items = []
    for p in sorted((CC0 / "bass_babyblue").glob("*.wav")):
        mm = re.search(r"_([a-g])(b?)(\d)_", p.name)
        if mm:
            items.append((12 * (int(mm.group(3)) + 1) + PC[mm.group(1)] - (mm.group(2) == "b") - 12, p))
    bank = Bank(items)
    spb, bars = 60 / 120, 8
    dur = 0.1 + bars * 4 * spb + 0.8
    full, send = Mix(dur), Mix(dur)
    T = lambda bar, b: 0.1 + (bar * 4 + b) * spb + hum(2)
    chord = [midi(n) for n in ("C3", "G3", "Bb3", "Eb4", "D4")]
    n_stab = int(0.45 * SR)
    for bar in range(bars):
        for b in range(4):
            full.add(kit.hit("kick", vary(0.85)), T(bar, b), db(-1))
        for b in (0.75, 1.5, 2.75, 3.25) if bar % 2 else (0.75, 2.75):
            x, t = kit.hit("rim", vary(0.6, 0.2)), T(bar, b)
            full.add(x, t, db(-8), 0.2)
            send.add(x, t, db(-8), 0.2)
        for st in range(16):
            full.add(kit.hit("hh", vary(0.35 if st % 2 else 0.6, 0.15)), T(bar, st / 4), db(-16 if st % 2 else -13), -0.3)
        root = ("C1", "C1", "Ab0", "Bb0")[bar % 4]
        for b, l, off in ((0, 0.75, 0), (1.5, 0.5, 0), (2.5, 0.5, 7), (3.5, 0.4, 12)):
            full.add(fade(bank.note(midi(root) + off + 12), l * spb, 0.04), T(bar, b), db(-3))
        open_ = 0.5 - 0.5 * np.cos(2 * np.pi * bar / bars)
        for b in (0.5, 1.5, 2.5, 3.5):
            x = sum(detuned(c, n_stab, 3, 9) for c in chord) / len(chord)
            cut = 350 + (700 + 2600 * open_) * np.exp(-np.arange(n_stab) / (0.08 * SR))
            y, t = svf_lp(x, cut, 1.1) * adsr(n_stab, 0.003, 0.09, 0.0, 0.05, 0.25), T(bar, b)
            full.add(y, t, db(-4), 0.1)
            send.add(y, t, db(-4), 0.1)
    n_pad = int(bars * 4 * spb * SR)
    x = sum(detuned(midi(c), n_pad, 4, 12) for c in ("C3", "Eb3", "G3", "Bb3")) / 4
    pad_ = svf_lp(x, 900 * (1 + 0.2 * np.sin(2 * np.pi * 0.1 * np.arange(n_pad) / SR)), 0.7) * adsr(n_pad, 2.0, 1.0, 1.0, 1.0, n_pad / SR - 1.0)
    full.add(np.stack([pad_, np.roll(pad_, int(0.009 * SR))], axis=1), 0.1, db(-14))
    gain = full.write("fullmix")
    # The send 12 dB over the mix's own level, as a dub engineer pushes it (owner, renders/fullmix_send:
    # "both"; at the mix's level it reached the reverb 12 dB under the single instruments, -30 LUFS).
    send.write("fullmix_send", gain=gain * 10.0 ** (12.0 / 20.0))


def piano_note(m: int, vel: float, hold: float) -> np.ndarray:
    """One dry piano note: inharmonic partials (B rises toward the top, as short stiff
    strings do), struck at 1/7 of the string, each partial a pair 0.4 cents apart (two
    strings beating: the fast-then-slow decay), a short hammer knock, a damper at release."""
    f0 = hz(m)
    b = 0.0002 * 2 ** ((m - 48) / 18)
    t60 = 14.0 * 2 ** (-(m - 36) / 20)          # low strings ring longer
    n = int((hold + 0.12) * SR)
    t = np.arange(n) / SR
    x = np.zeros(n)
    bright = 0.6 + 0.6 * vel
    for k in range(1, 40):
        fk = k * f0 * np.sqrt(1 + b * k * k)
        if fk > 14000:
            break
        a = abs(np.sin(np.pi * k / 7)) / k ** (1.6 - 0.5 * bright)
        tk = t60 / (1 + 0.25 * k * (fk / 1000) ** 0.5)
        for det, share, slow in ((1.0, 0.6, 1.0), (2 ** (0.4 / 1200), 0.4, 3.0)):
            x += a * share * np.exp(-6.91 * t / (tk * slow)) * np.sin(2 * np.pi * fk * det * t + nrng.uniform(0, 6.28))
    knock = nrng.standard_normal(int(0.006 * SR)) * np.exp(-np.arange(int(0.006 * SR)) / (0.0012 * SR))
    x[: len(knock)] += 0.15 * vel * np.cumsum(knock) / 40
    x[: int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))
    rel = int(0.12 * SR)
    x[-rel:] *= np.exp(-np.linspace(0, 6.91, rel))  # the damper: ~0.12 s
    return x * vel


def piano():
    """A dry, close piano (no room): single notes high and low, a repeated note, a few
    chords, staccato against held. Pitched partials with sharp attacks are what make a
    metallic or ringing tank obvious."""
    spb = 60 / 76
    events = [  # beat, beats held, notes, velocity
        (0, 1, ("C5",), 0.8), (1, 1, ("G4",), 0.6), (2, 2, ("E5",), 0.7),
        (4, 0.25, ("A3",), 0.9), (4.5, 0.25, ("A3",), 0.8), (5, 0.25, ("A3",), 0.85), (5.5, 1.5, ("A3",), 0.7),
        (8, 3, ("C3", "G3", "E4", "B4"), 0.7), (11, 0.5, ("D6",), 0.6), (11.5, 0.5, ("B5",), 0.55),
        (12, 3, ("F2", "C3", "A3", "E4"), 0.75), (15, 0.25, ("G5", "B5", "D6"), 0.9),
        (16, 4, ("A1", "E2", "C4", "G4", "B4"), 0.8),
    ]
    m = Mix(0.1 + 20 * spb + 0.5)
    for beat, l, notes, v in events:
        for k, nm in enumerate(notes):
            m.add(piano_note(midi(nm), vary(v, 0.05), l * spb), 0.1 + beat * spb + k * 0.006 + hum(1.5), 1.0,
                  0.5 * (midi(nm) - 60) / 30)
    m.write("piano")


ALL = {"stabs": stabs, "pad": pad, "lead": lead, "drums": drums, "bass": bass, "keys": keys, "voice": voice,
       "voicelow": voicelow, "piano": piano, "fullmix": fullmix}

if __name__ == "__main__":
    want = sys.argv[1:] or list(ALL)
    if not CC0.is_dir():
        sys.exit(f"missing {CC0}")
    for name in want:
        ALL[name]()
