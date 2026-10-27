#!/usr/bin/env python3
# Renders at its original commit: build iv_render at 23733bb (IV_TWIN_ALT is ignored since v1.0.43).
"""PROTOTYPE confirm page (26 Oct 2026; owner on twin_alternatives: "I quite like small room, but it sounds a little
boomy and low-end heavy. If we can skew the frequencies to remove some lows and increase the highs a bit, I think this
can replace today's twin"). Measured first: the room's lows sat where the twin's do (63-250 Hz within ~2 dB, each
against its own total); its top was 6-12 dB under the twin's above 2 kHz, because its in-loop one-pole and its
linear-interpolated reads (~33 passes a second) let the highs die ~11x sooner than the lows. Level-matched A-weighted,
that dark balance reads as boomy. Renderer hook IV_TWIN_ALT=mode:gainDb (dsp/TwinAlt.h):

    A  today's twin (v1.0.41)
    B  the small room as it was (2)
    C  the room voiced: its loss the twin tank's (the highs' T60 the twin's TONE share), allpass reads, a 150 Hz low cut (4)
    D  the same with the cut at 300 Hz, the still lows' split: the room takes only what the main tank's highs take (5)
    E  the same as C with a softer top (the highs' T60 0.6 of C's) (6)

Cards as on twin_alternatives (its material and in-key pads): 1-2 the layer alone (IV_LAYER_SOLO), MASSIVE Neve and
HALL Neve, seven sounds; 3-4 over an in-key pad, the five tonal sounds. Each room's level matched per card to A's
layer alone (A-weighted, the mean of the five tonal sounds), on its own output.

Usage: <main checkout>/.venv/bin/python tools/pages/small_room.py [--render build-twin/iv_render]
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
import twin_alternatives as ta  # noqa: E402

SPACES = [("massive_neve", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.1"]),
          ("hall_neve", ["space=HALL", "size=0.9", "decay=0.9", "era=0.1"])]
VERSIONS = [("A_today", 0), ("B_room_as_was", 2), ("C_room_cut150", 4), ("D_room_cut300", 5), ("E_room_softer", 6)]
README = """The small room, voiced. Your note: "I quite like small room, but it sounds a little boomy and low-end heavy. If we can skew the frequencies to remove some lows and increase the highs a bit, I think this can replace today's twin." Measured: the room's lows weren't louder than today's twin's; its highs were much quieter (6-12 dB above 2 kHz), so with the same loudness it sounded heavy at the bottom. The voiced rooms let their highs ring as long as the twin's do (TONE now plays them too) and take some lows out.

A is today's twin. B is the small room as you heard it. C: the room with its highs back and the lows below 150 Hz taken out. D: the same with the lows taken out below 300 Hz, where the big tank splits its lows off: the room keeps only what the main tank's highs take, so the pad keeps the bass and the room stays out of it. E: like C with the highs a little softer, between B and C.

Cards 1-2, the layer alone: only the layer is heard (the pad is frozen underneath but silent), each version set to the same loudness. Listen for the balance between low and high, the snare's tail, and any ring on the held lead. Cards 3-4, over a pad in the same key as what's played: listen for how each sits with the pad, and Paint (the button held from 9 to 12 s).

Each card: the pad plays and is frozen at 4 s; the sound plays from 4.5 s (the snare once, at 5 s); the button is held from 9 to 12 s; the playing stops by 13.5 s; a tap unfreezes at 24 s. SIZE 0.9, DECAY 0.9, TONE noon, ERA at the Neve. Renders are 100 % wet, so use the MIX fader for the dry (around 50 %).

1_layer_massive_neve      the layer alone, MASSIVE
2_layer_hall_neve         the layer alone, HALL
3_overpad_massive_neve    over the pad, MASSIVE
4_overpad_hall_neve       over the pad, HALL

A_today                   today's twin
B_room_as_was             the small room as it was
C_room_cut150             voiced: highs back, lows cut below 150 Hz
D_room_cut300             voiced: highs back, lows cut below 300 Hz (the big tank keeps the bass)
E_room_softer             like C, the highs a little softer"""


def env(mode, gain, solo):
    e = {} if mode == 0 else {"IV_TWIN_ALT": "{}:{:.2f}".format(mode, gain)}
    if solo:
        e["IV_LAYER_SOLO"] = "1"
    return e


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-twin" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "small_room"
    dry = root / "renders" / "_material_small_room"
    work = root / "renders" / "_small_room_work"
    for d in (out, dry, work):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    ref, _ = sf.read(material / "pad.wav", always_2d=True)
    ref_rms = float(np.sqrt(np.mean(ref[int(1.5 * ta.SR):int(4.0 * ta.SR)] ** 2)))
    for m in ta.ALL:
        x, _ = ta.stimulus(material, m, ref_rms)
        sf.write(dry / (m + ".wav"), x, ta.SR, subtype="PCM_24")
    binary = a.render.resolve()
    paint = [list(ft.FREEZE), list(ft.PAINT), list(ft.UNFREEZE)]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    events = [(k, t) for k, t in ft.EVENTS]
    gains, paint_np = {}, {}
    with open(out / "_hooks.txt", "w") as hooks:
        for sp, sets in SPACES: # level match: the layer alone, A-weighted 4.5-15.5 s, the mean of the tonal sounds
            alone = {}
            for tag, mode in VERSIONS:
                for m in ta.TONAL:
                    alone[(tag, m)] = ta.measured(binary, dry / (m + ".wav"), work / "cal.wav", sets, env(mode, 0.0, True),
                                                  paint, hooks, lambda x: lv(x, 4.5, 15.5, True))
            for tag, mode in VERSIONS:
                gains[(sp, tag)] = 0.0 if mode == 0 else float(np.mean([alone[("A_today", m)] - alone[(tag, m)] for m in ta.TONAL]))
        cards = [("{}_layer_{}".format(i + 1, sp), sp, sets, True, ta.ALL) for i, (sp, sets) in enumerate(SPACES)]
        cards += [("{}_overpad_{}".format(i + 3, sp), sp, sets, False, ta.TONAL) for i, (sp, sets) in enumerate(SPACES)]
        for col, sp, sets, solo, mats in cards:
            for tag, mode in VERSIONS:
                e = env(mode, gains[(sp, tag)], solo)
                for m in mats:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, e, paint, hooks)
                    if not solo:
                        paint_np[(col, m, tag)] = ta.measured(binary, dry / (m + ".wav"), work / "np.wav", sets, e, nopaint,
                                                              hooks, lambda x: lv(x, 22, 23.9))
    bands = [63, 125, 250, 500, 1000, 2000, 4000, 8000]

    def octaves(x):
        s = x[int(4.5 * ta.SR):int(15.5 * ta.SR)].mean(axis=1)
        P = np.abs(np.fft.rfft(s)) ** 2
        f = np.fft.rfftfreq(len(s), 1.0 / ta.SR)
        tot = P[(f > 40) & (f < 16000)].sum()
        return np.array([db(P[(f >= b / np.sqrt(2)) & (f < b * np.sqrt(2))].sum() / tot) for b in bands])

    with open(out / "_verify.txt", "w") as v:
        v.write("small_room. gains: each room's level matched to A's layer alone (A-weighted, 4.5-15.5 s, the mean of the "
                "tonal sounds). Layer alone: level vs A (A-weighted), octave balance vs A (each against its own total; "
                "bands {} Hz), the fall to -60 dB after the playing; over the pad: Paint (the pad at 22-23.9 s, Paint "
                "minus none). 1 ms click scores at ".format(bands) + ", ".join("{} {:.1f} s".format(k, t) for k, t in events)
                + " (the layer alone is silent before the freeze, so its scores there mean nothing).\n")
        for sp, _ in SPACES:
            v.write("{}: gains {}\n".format(sp, ", ".join("{} {:+.2f} dB".format(t, gains[(sp, t)]) for t, _ in VERSIONS)))
        for col, sp, sets, solo, mats in cards:
            v.write(col + "\n")
            for m in mats:
                aa, _ = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
                stop = ta.HIT_AT + 0.5 if m == "hit" else ta.OVER[1]
                for tag, _ in VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    same = "" if tag == VERSIONS[0][0] else ("; IDENTICAL to A" if np.array_equal(aa, x) else
                                                            "; vs A null {:.1f} dB".format(db(np.mean((aa - x) ** 2) / np.mean(aa ** 2))))
                    if solo:
                        extra = "level vs A {:+.1f} dB(A); octaves vs A {}; falls {:.1f} s".format(
                            lv(x, 4.5, 15.5, True) - lv(aa, 4.5, 15.5, True),
                            " ".join("{:+.1f}".format(d) for d in octaves(x) - octaves(aa)), ft.fall_60(x, stop))
                    else:
                        extra = "Paint adds {:+.2f} dB".format(lv(x, 22, 23.9) - paint_np[(col, m, tag)])
                    v.write("  {:5s} {:14s} {}{}; clicks {}\n".format(m, tag, extra, same, clicks(x, events)))
    shutil.rmtree(work)
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "The small room, voiced", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
