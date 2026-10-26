#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; owner: "Yes: a page with today's twin, blur and width, small room, and the twin
without its swell, all at -8 dB. Skip the spring chirp."): cheaper things to play over a frozen pad than the twin's
second big tank (~28 points of the module's CPU while frozen). Renderer hook IV_TWIN_ALT=mode:gainDb
(Engine::setTwinAltPrototype, dsp/TwinAlt.h):

    A  today's twin (v1.0.41: ~8 dB A-weighted under the pad)
    B  blur and width: the twin's input diffuser, three allpasses a side (the first one wandering), a velvet-noise
       decorrelator a side; no loop, no tail of its own
    C  small room: eight short lines at 24 kHz (8-50 ms by SIZE), the twin's T60 (at most 8 s), lowpass and wander
    D  today's twin without its swell (MASSIVE only: HALL's twin has none, so D is A there)

Each version's level is matched per card to A's: the twin alone (the playing over silence frozen at 4 s, no Paint)
A-weighted over 6-13.5 s, averaged over the five materials; the gain goes on the version's own output, so Paint
sends it at that level too. Cards: MASSIVE (Neve, cassette), HALL (Neve), SIZE 0.9, DECAY 0.9; keys, vibes, voice,
skank (plucks) and lead (a held line) over a frozen pad (frozen_twin's material: pad frozen at 4 s, playing 4.5-13.5 s,
Paint 9-12 s, unfreeze 24 s). 100 % wet with --dry, not level-matched by the page (the levels are matched here).

Usage: <main checkout>/.venv/bin/python tools/pages/twin_alternatives.py [--render build-twin/iv_render]
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, db  # noqa: E402
import frozen_twin as ft  # noqa: E402
from freeze_moment import render, lv, clicks  # noqa: E402

SR = 48000
OVER = ["keys", "vibes", "voice", "skank", "lead"]
COLS = [("1_massive_neve", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.1"]),
        ("2_massive_cassette", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.7"]),
        ("3_hall_neve", ["space=HALL", "size=0.9", "decay=0.9", "era=0.1"])]
VERSIONS = [("A_today", 0), ("B_blur_width", 1), ("C_small_room", 2), ("D_no_swell", 3)]
README = """Cheaper ways to play over a frozen pad. Today the twin (the second reverb that takes what you play while the pad is frozen) is a whole second big tank, about a quarter of the module's CPU while frozen, which is why a frozen MASSIVE with the cassette crackles. These are lighter, all set to the same level as today's twin (about 8 dB under the pad).

A is today's twin. B, blur and width: no second reverb at all, only a short smear and a widening of what you play (about a tenth of a second): it takes no colour from the space, no ERA and no DRIFT, and SIZE and DECAY do nothing to it. C, small room: a small, dense room of its own that rings out within 8 s like the twin, with SIZE setting how big it is and DECAY how long; it doesn't take the space's colour, ERA or DRIFT either. D is today's twin without its swell: in MASSIVE what you play arrives at once, so plucks may come back as two quick repeats before they blur; in HALL the twin has no swell, so D is the same as A there.

Rough CPU while frozen on the module (estimates, to be measured): A about 28 points of the CPU, B about 4-6, C about 6-9, D about 17-19.

Each card: a pad is frozen at 4 s; keys, vibes, a voice, skank chords (plucks) or a held lead line play over it from 4.5 s; the button is held from 9 to 12 s (Paint); the playing stops at 13.5 s and the layer fades; a tap unfreezes at 24 s. SIZE 0.9, DECAY 0.9, TONE and DRIFT at noon. The levels are matched to today's twin; renders are 100 % wet, so use the MIX fader for the dry (around 50 %, where you listened last time).

1_massive_neve         MASSIVE, ERA at the Neve
2_massive_cassette     MASSIVE, ERA at the cassette
3_hall_neve            HALL, ERA at the Neve

A_today                today's twin
B_blur_width           a smear and a widening, no reverb of its own
C_small_room           a small room of its own, fading within 8 s
D_no_swell             today's twin without its swell (= A in HALL)"""


def measured(binary, src, path, sets, e, buttons, hooks, f):
    """Render, measure with f(x), delete the render (work renders are numbers only: disk)."""
    render(binary, src, path, sets, e, buttons, hooks)
    x, _ = sf.read(path, always_2d=True)
    path.unlink()
    return f(x)


def env(mode, gain):
    return {} if mode == 0 else {"IV_TWIN_ALT": "{}:{:.2f}".format(mode, gain)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-twin" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "twin_alternatives"
    dry = root / "renders" / "_material_twin_alternatives"
    work = root / "renders" / "_twin_alternatives_work" # calibration and measurement renders, removed at the end
    for d in (out, dry, work):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    for m in OVER:
        full, over = ft.stimulus(material, m, True), ft.stimulus(material, m, False)
        sf.write(dry / (m + ".wav"), full, SR, subtype="PCM_24")
        sf.write(work / ("_in_" + m + ".wav"), over, SR, subtype="PCM_24")
        sf.write(work / ("_pad_" + m + ".wav"), full - over, SR, subtype="PCM_24")
    binary = a.render.resolve()
    paint = [list(ft.FREEZE), list(ft.PAINT), list(ft.UNFREEZE)]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    gains, meas = {}, {}
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in COLS:
            # Calibrate: each version's twin alone against A's, A-weighted, the mean over the materials.
            alone = {}
            for tag, mode in VERSIONS:
                for m in OVER:
                    alone[(tag, m)] = measured(binary, work / ("_in_" + m + ".wav"), work / col / "cal.wav", sets,
                                               env(mode, 0.0), nopaint, hooks, lambda x: lv(x, 6, 13.5, True))
            for tag, mode in VERSIONS:
                gains[(col, tag)] = 0.0 if mode == 0 else float(np.mean([alone[("A_today", m)] - alone[(tag, m)] for m in OVER]))
            for tag, mode in VERSIONS:
                e = env(mode, gains[(col, tag)])
                for m in OVER:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, e, paint, hooks)
                    meas[(col, m, tag, "twin")] = measured(
                        binary, work / ("_in_" + m + ".wav"), work / col / "twin.wav", sets, e, nopaint, hooks,
                        lambda x: (lv(x, 6, 13.5), lv(x, 6, 13.5, True), ft.fall_60(x, ft.OVER_END)))
                    meas[(col, m, tag, "nopaint")] = measured(binary, dry / (m + ".wav"), work / col / "nopaint.wav", sets, e,
                                                              nopaint, hooks, lambda x: lv(x, 22, 23.9))
            for m in OVER:
                meas[(col, m, "pad")] = measured(binary, work / ("_pad_" + m + ".wav"), work / col / "pad.wav", sets, {},
                                                 nopaint, hooks, lambda x: (lv(x, 6, 13.5), lv(x, 6, 13.5, True)))
    with open(out / "_verify.txt", "w") as v:
        v.write("twin_alternatives. gain: each version's level, matched to A's twin alone (A-weighted, 6-13.5 s, the mean "
                "over the materials). twin - pad: the layer alone against the pad alone, broadband / A-weighted. falls: "
                "seconds to 60 dB under the playing after it stops. Paint: the pad at 22-23.9 s, Paint minus none. 1 ms "
                "click scores at: " + ", ".join("{} {:.1f} s".format(k, t) for k, t in ft.EVENTS) + "\n")
        for col, _ in COLS:
            v.write("{}: gains {}\n".format(col, ", ".join("{} {:+.2f} dB".format(t, gains[(col, t)]) for t, _ in VERSIONS)))
            for m in OVER:
                pb, pa = meas[(col, m, "pad")]
                a, _ = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
                for tag, _ in VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    tb, ta, fall = meas[(col, m, tag, "twin")]
                    same = "" if tag == VERSIONS[0][0] else ("; IDENTICAL to A" if np.array_equal(a, x) else
                                                            "; vs A null {:.1f} dB".format(db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                    v.write("  {:5s} {:13s} twin - pad {:+.1f} / {:+.1f} dB; falls {:.1f} s; Paint adds {:+.2f} dB{}; clicks {}\n".format(
                        m, tag, tb - pb, ta - pa, fall, lv(x, 22, 23.9) - meas[(col, m, tag, "nopaint")], same, clicks(x, ft.EVENTS)))
    shutil.rmtree(work)
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "Lighter layers over a frozen pad", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
