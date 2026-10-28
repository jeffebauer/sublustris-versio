#!/usr/bin/env python3
# Renders at its original commit: build iv_render at a766750 (IV_TWIN_LIGHT is ignored since v1.0.43).
"""PROTOTYPE pick page (26 Oct 2026; round 15: a frozen MASSIVE with the cassette runs ~105 % of the module's
CPU in real time, and the twin is ~28 points of a frozen MASSIVE): a lighter twin, the second tank that plays
over a frozen pad. Renderer hook IV_TWIN_LIGHT (Engine::setTwinLightPrototype):

    A  as built (0)
    B  the twin's swell at 24 kHz, inside its half-rate stage (1)
    C  B, with no in-loop diffusion in the twin and two early taps a line instead of four (2)
    D  the twin's swell and tank at 12 kHz (3)

Usage: <main checkout>/.venv/bin/python tools/pages/frozen_twin.py [--render build-twin/iv_render]

Each render (30 s): a pad plays, is frozen at 4 s (a tap), then keys / vibes / voice play over the frozen pad
from 4.5 s; the button is held 9-12 s (Paint); the playing stops at 13.5 s and the twin fades; a tap
unfreezes at 24 s. MASSIVE, SIZE 0.9, DECAY 0.9 (where the module crackled), TONE / DRIFT 0.5, ATTACK NORMAL,
ERA at the Neve (0.1) or the cassette (0.7). 100 % wet (--wet-only --dry), level-matched, a pick page, in
the main checkout's renders/frozen_twin.

_verify.txt: each version against A, twice. The page's renders: null depth, level, and a 1 ms-frame click
score at every move (the freeze, the playing's start, the Paint's press and release, the playing's end, the
unfreeze): the loudest 1 ms frame above 5 kHz against the median of the second around it, A's and this
version's side by side. And a "twin alone" render (the same playing over silence frozen at 4 s, no Paint, so
all that sounds is the twin): level, its third-octave shape against A's (each normalised to its own total),
how long it takes to fall 60 dB after the playing stops, and the loudest 1 ms frame above 5 kHz at each move
against the broadband level of the 300 ms around it (a click reads near 0 dB or above; the freeze itself,
on silence, has no level to compare with).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, db  # noqa: E402

SR = 48000
LENGTH = 30.0
PAD_END, OVER_START, OVER_END = 5.0, 4.5, 13.5
FREEZE, PAINT, UNFREEZE = (4.0, 4.1), (9.0, 12.0), (24.0, 24.1)
EVENTS = [("freeze", FREEZE[0]), ("play", OVER_START), ("paint on", PAINT[0]), ("paint off", PAINT[1]),
          ("play ends", OVER_END), ("unfreeze", UNFREEZE[0])]
OVER = ["keys", "vibes", "voice"]
KNOBS = ["space=MASSIVE", "size=0.9", "decay=0.9", "tone=0.5", "drift=0.5", "attack=NORMAL", "time=0"]
COLS = [("1_neve", KNOBS + ["era=0.1"]), ("2_cassette", KNOBS + ["era=0.7"])]
VERSIONS = [("A_as_built", {}), ("B_swell_24k", {"IV_TWIN_LIGHT": "1"}),
            ("C_no_inloop_2taps", {"IV_TWIN_LIGHT": "2"}), ("D_twin_12k", {"IV_TWIN_LIGHT": "3"})]
CHAPTERS = "Pad:0,Frozen:4,Playing over:4.5,Paint:9,Twin fades:13.5,Unfreeze:24"
README = """A lighter twin: the second reverb that plays over a frozen pad. On the module a frozen MASSIVE with the cassette needs about 105 % of the CPU, so it would crackle; the twin is about 28 points of a frozen MASSIVE, so a cheaper twin is the first thing to try. The question: can you hear the cheaper twins, and does it matter?

A is today. B runs the twin's swell (the slow fade-in of what you play over the pad) at half the rate inside the twin; it should sound the same, maybe a touch softer at the very top. C is B with a plainer twin: no diffusion inside its loop and half its early reflections, so what you play may sound a little more like distinct echoes before it blurs. D runs the whole twin at a quarter of the rate: its top stops around 5 kHz, a darker, softer layer over a pad that keeps all its brightness.

Estimated saving on the module while frozen (estimates from what each twin stops reading and writing; only the module can confirm them): B 4.5-6 points of the CPU, C 10-15, D 15-17.5. A frozen cassette needs about 10 to stop crackling, so B alone isn't enough. In HALL only D changes anything (HALL's twin has no swell, no in-loop diffusion and two taps already).

Each card: a pad plays and is frozen at 4 s; keys, vibes or a voice play over it from 4.5 s; the button is held from 9 to 12 s (Paint: a little of the twin goes into the pad); the playing stops at 13.5 s and the twin fades; a tap unfreezes at 24 s. The bar under each card jumps to those moments. MASSIVE, SIZE 0.9, DECAY 0.9, TONE and DRIFT at noon, ATTACK NORMAL; card 1 the Neve, card 2 the cassette. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_neve                 MASSIVE, ERA at the Neve
2_cassette             MASSIVE, ERA at the cassette (the setting that crackles)

A_as_built             today's twin
B_swell_24k            the twin's swell at half the rate (should sound the same)
C_no_inloop_2taps      B, plus a plainer twin: no in-loop diffusion, two early reflections a line
D_twin_12k             the whole twin at a quarter of the rate: darker, top around 5 kHz"""


def fade(x, a, b):
    """x faded in over a seconds and out over the last b seconds."""
    x = x.copy()
    na, nb = int(a * SR), int(b * SR)
    if na:
        x[:na] *= np.linspace(0.0, 1.0, na)[:, None]
    x[-nb:] *= np.linspace(1.0, 0.0, nb)[:, None]
    return x


def stimulus(material: Path, over: str, with_pad: bool) -> np.ndarray:
    out = np.zeros((int(LENGTH * SR), 2))
    if with_pad:
        pad, _ = sf.read(material / "pad.wav", always_2d=True)
        out[:int(PAD_END * SR)] += fade(pad[:int(PAD_END * SR)], 0.0, 0.4)
    x, _ = sf.read(material / (over + ".wav"), always_2d=True)
    seg = fade(x[:int((OVER_END - OVER_START) * SR)], 0.02, 0.15)
    i0 = int(OVER_START * SR)
    out[i0:i0 + len(seg)] += seg
    return out


def render(binary, src, out, sets, env_extra, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}  env {}  ->  {}\n".format(out.name, env_extra or "{}",
                                              p.stderr.strip().replace("\n", " | ") or "(no hook line)"))


def highpass(x, lo):
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / SR)
    X[f < lo] = 0
    return np.fft.irfft(X, x.shape[0])


def click_score(h, t, frame_s=0.001):
    """Around move time t (-0.05 .. +0.3 s): the loudest 1 ms frame of h (the highs) against the median of
    the second around it, in dB."""
    n = int(frame_s * SR)
    e = (h[:len(h) // n * n].reshape(-1, n) ** 2).mean(axis=1)
    i0, i1 = int((t - 0.05) / frame_s), int((t + 0.3) / frame_s)
    j0, j1 = max(0, int((t - 0.5) / frame_s)), int((t + 0.5) / frame_s)
    return db(e[i0:i1].max()) - db(np.median(e[j0:j1]) + 1e-30)


def fall_60(x, t0):
    """Seconds after t0 until the 50 ms RMS stays 60 dB under its level just before t0."""
    m = x.mean(axis=1)
    n = int(0.05 * SR)
    e = db((m[:len(m) // n * n].reshape(-1, n) ** 2).mean(axis=1) + 1e-30)
    k0 = int(t0 / 0.05)
    ref = e[k0 - 10:k0].max()
    above = np.nonzero(e[k0:int((UNFREEZE[0] - 0.1) / 0.05)] > ref - 60.0)[0]
    return (above[-1] + 1) * 0.05 if len(above) else 0.0


def thirds(x, t0, t1):
    """Third-octave levels 250 Hz .. 16 kHz of x[t0:t1], each against the segment's total, dB."""
    seg = x[int(t0 * SR):int(t1 * SR)].mean(axis=1)
    P = np.abs(np.fft.rfft(seg)) ** 2
    f = np.fft.rfftfreq(len(seg), 1.0 / SR)
    cs = [250 * 2 ** (k / 3) for k in range(19)]
    return cs, [db(P[(f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))].sum() / P.sum() + 1e-30) for c in cs]


def click_vs_level(x, h, t):
    """The loudest 1 ms frame of h (the highs) in t-0.02 .. t+0.2 s against the broadband level of t-0.05 .. t+0.25 s."""
    n = int(0.001 * SR)
    i0, i1 = int((t - 0.02) * SR), int((t + 0.2) * SR)
    fr = (h[i0:i1][:(i1 - i0) // n * n].reshape(-1, n) ** 2).mean(axis=1)
    return db(fr.max() + 1e-30) - db(np.mean(x[int((t - 0.05) * SR):int((t + 0.25) * SR)] ** 2) + 1e-30)


def verify(a_path, b_path, twin_alone):
    a, _ = sf.read(a_path, always_2d=True)
    b, _ = sf.read(b_path, always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    if np.array_equal(a, b):
        return "IDENTICAL (mislabelled!)"
    null = db(np.mean((a - b) ** 2) / np.mean(a ** 2))
    level = db(np.mean(b ** 2)) - db(np.mean(a ** 2))
    ha, hb = highpass(a, 5000.0), highpass(b, 5000.0)
    s = "null {:.1f} dB; level {:+.2f} dB".format(null, level)
    if not twin_alone:
        return s + "; clicks A/this: " + " ".join("{} {:+.1f}/{:+.1f}".format(name, click_score(ha, t), click_score(hb, t))
                                                  for name, t in EVENTS)
    cs, ta = thirds(a, OVER_START, OVER_END + 2)
    _, tb = thirds(b, OVER_START, OVER_END + 2)
    shape = " ".join("{:g}k {:+.1f} ({:.0f})".format(round(c / 1000, 1), tb[i] - ta[i], tb[i]) for i, c in enumerate(cs) if i % 3 == 0)
    moves = [(k, t) for k, t in EVENTS if k not in ("paint on", "paint off")]
    return s + "; shape vs A (own level re total): {}; falls 60 dB in {:.1f} s (A {:.1f} s); 1 ms highs vs level A/this: {}".format(
        shape, fall_60(b, OVER_END), fall_60(a, OVER_END),
        " ".join("{} {:+.0f}/{:+.0f}".format(k, click_vs_level(a, ha, t), click_vs_level(b, hb, t)) for k, t in moves))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-twin" / "iv_render")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "frozen_twin"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_frozen_twin"
    alone = main_root / "renders" / "_frozen_twin_alone" # the twin-alone renders (not on the page)
    for d in (dry, alone):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    for m in OVER:
        sf.write(dry / (m + ".wav"), stimulus(material, m, True), SR, subtype="PCM_24")
        sf.write(alone / ("_in_" + m + ".wav"), stimulus(material, m, False), SR, subtype="PCM_24")
    auto = out / "_buttons.json"
    auto.write_text(json.dumps({"buttons": [list(FREEZE), list(PAINT), list(UNFREEZE)]}))
    auto_alone = alone / "_buttons.json" # no Paint: the twin alone, as it fades
    auto_alone.write_text(json.dumps({"buttons": [list(FREEZE), list(UNFREEZE)]}))
    binary = a.render.resolve()
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in COLS:
            for tag, env in VERSIONS:
                for m in OVER:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, auto, hooks)
                    render(binary, alone / ("_in_" + m + ".wav"), alone / col / "{}_{}.wav".format(m, tag), sets, env,
                           auto_alone, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("Each version against A (frozen_twin). Moves: " + ", ".join("{} {:.1f} s".format(k, t) for k, t in EVENTS)
                + ". Click scores: the loudest 1 ms frame above 5 kHz vs the second's median, dB, A/this.\n")
        for col, _ in COLS:
            for m in OVER:
                for tag, _ in VERSIONS[1:]:
                    for label, d, ta in (("page", out, False), ("twin alone", alone, True)):
                        v.write("{:11s} {:6s} {} {:10s}: {}\n".format(col, m, tag[:1], label, verify(
                            d / col / "{}_{}.wav".format(m, VERSIONS[0][0]), d / col / "{}_{}.wav".format(m, tag), ta)))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "A lighter twin over a frozen pad", "--level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
