#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; owner: "Yes: a page with today's twin, blur and width, small room, and the twin
without its swell, all at -8 dB"; then, on its first version: "because there's a frozen pad underneath, it's hard to
hear the differences. Can you render various sounds with just the twin so I can hear it in isolation? ... the frozen
pad and the material over the top are in different keys"): cheaper things to play over a frozen pad than the twin's
second big tank (~28 points of the module's CPU while frozen). Renderer hook IV_TWIN_ALT=mode:gainDb
(Engine::setTwinAltPrototype, dsp/TwinAlt.h):

    A  today's twin (v1.0.41: ~8 dB A-weighted under the pad)
    B  blur and width: the twin's input diffuser, three allpasses a side (the first one wandering), a velvet-noise
       decorrelator a side; no loop, no tail of its own
    C  small room: eight short lines at 24 kHz (8-50 ms by SIZE), the twin's T60 (at most 8 s), lowpass and wander
    D  today's twin without its swell (MASSIVE only: HALL's twin has none, so D is A there)

Two groups of cards (MASSIVE Neve, MASSIVE cassette, HALL Neve; SIZE 0.9, DECAY 0.9):
  1-3 the layer alone: IV_LAYER_SOLO=1 (Engine::setLayerSoloPrototype) leaves the tank, so the frozen pad, out of the
      wet; the pad still freezes underneath as in use. Keys, vibes, voice, skank (plucks), lead (a held line), one snare
      hit, a drum loop.
  4-6 over the pad: the same, heard with the pad, for the five tonal sounds.
Each material's pad is in its key (pitch-class analysis in _verify.txt): keys D minor -> Dm9, vibes F major -> Fmaj9,
skank A (Am | D) -> Asus2, lead C minor -> Cm9 (make_material.py's pad voice); voice, the hit and the drums keep the
material's own A minor pad. The pad plays 0-5 s and is frozen at 4 s; the sound plays from 4.5 s (the hit at 5 s);
Paint 9-12 s; the playing stops by 13.5 s; unfreeze at 24 s; 27 s in all.

Levels: each version's gain is the one matched on the first version of this page (the layer alone, A-weighted, the
mean of keys / vibes / voice / skank / lead, per card), on its own output so Paint sends it at that level too.
_verify.txt gives each version's layer-alone level against A per sound. 100 % wet with --dry; not level-matched by
the page.

Usage: <main checkout>/.venv/bin/python tools/pages/twin_alternatives.py [--render build-twin/iv_render]
"""
import argparse
import importlib.util
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

_spec = importlib.util.spec_from_file_location("make_material", ROOT / "tools" / "make_material.py")
mm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mm)

SR = 48000
LENGTH, PAD_END, OVER, HIT_AT = 27.0, 5.0, (4.5, 13.5), 5.0
TONAL = ["keys", "vibes", "voice", "skank", "lead"]
ALL = TONAL + ["hit", "drums"]
CHORDS = {"keys": ("D minor", ("D2", "A2", "F3", "C4", "E4")), "vibes": ("F major", ("F2", "C3", "A3", "E4", "G4")),
          "skank": ("A (Am | D)", ("A2", "E3", "A3", "B3", "E4")), "lead": ("C minor", ("C2", "G2", "Eb3", "Bb3", "D4"))}
SPACES = [("massive_neve", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.1"]),
          ("massive_cassette", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.7"]),
          ("hall_neve", ["space=HALL", "size=0.9", "decay=0.9", "era=0.1"])]
# The gains matched on the page's first version (the layer alone, A-weighted, per card).
GAINS = {"massive_neve": (0.0, -1.15, -6.90, 0.33), "massive_cassette": (0.0, -0.63, -5.57, 0.12),
         "hall_neve": (0.0, -1.90, -7.66, 0.0)}
VERSIONS = [("A_today", 0), ("B_blur_width", 1), ("C_small_room", 2), ("D_no_swell", 3)]
NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
README = """Cheaper ways to play over a frozen pad, now with the layer on its own. Today the twin (the second reverb that takes what you play while the pad is frozen) is a whole second big tank, about a quarter of the module's CPU while frozen. These are lighter, all at today's twin's level.

A is today's twin. B, blur and width: no second reverb at all, only a short smear and a widening of what you play (about a tenth of a second); it takes no colour from the space, no ERA and no DRIFT, and SIZE and DECAY do nothing to it. C, small room: a small, dense room of its own that rings out within 8 s like the twin, with SIZE setting how big it is and DECAY how long; no space colour, ERA or DRIFT either. D is today's twin without its swell: in MASSIVE what you play arrives at once, so plucks may come back as two quick repeats; in HALL the twin has no swell, so D is the same as A there.

Rough CPU while frozen on the module (estimates): A about 28 points of the CPU, B about 4-6, C about 5-9, D about 17-19.

Cards 1-3, the layer alone: only the layer is in the wet (the pad is still frozen underneath, as in use, but silent), so you hear each one's own shape. Listen for the tail (the snare hit shows it best: B is gone in half a second, C rings like a small room, A and D like the big space), the width, and any metallic ring on the held lead. The four versions are set to the same loudness (the layer alone, A-weighted). Paint does nothing you can hear here (it goes into the silent pad).

Cards 4-6, over the pad: the same sounds over a frozen pad, now in the same key as what's played (keys over D minor, vibes over F major, skank over A, the lead over C minor; the voice over the usual pad). Listen for how each sits with the pad, and Paint (the button held from 9 to 12 s).

Each card: the pad plays and is frozen at 4 s; the sound plays from 4.5 s (the snare once, at 5 s); the button is held from 9 to 12 s; the playing stops by 13.5 s and the layer fades; a tap unfreezes at 24 s. SIZE 0.9, DECAY 0.9. Renders are 100 % wet, so use the MIX fader for the dry (around 50 %).

1_layer_massive_neve       the layer alone: MASSIVE, ERA at the Neve
2_layer_massive_cassette   the layer alone: MASSIVE, ERA at the cassette
3_layer_hall_neve          the layer alone: HALL, ERA at the Neve
4_overpad_massive_neve     over the pad: MASSIVE, ERA at the Neve
5_overpad_massive_cassette over the pad: MASSIVE, ERA at the cassette
6_overpad_hall_neve        over the pad: HALL, ERA at the Neve

A_today                    today's twin
B_blur_width               a smear and a widening, no reverb of its own
C_small_room               a small room of its own, fading within 8 s
D_no_swell                 today's twin without its swell (= A in HALL)"""


def fade(x, a, b):
    return ft.fade(x, a, b)


def pad_for(material, m, ref_rms):
    """The pad the sound plays over (0-5 s): a chord in the sound's key in make_material.py's pad voice, at the level of
    the material's own pad; or that pad itself (voice, hit, drums)."""
    if m not in CHORDS:
        pad, _ = sf.read(material / "pad.wav", always_2d=True)
        return fade(pad[:int(PAD_END * SR)], 0.0, 0.4)
    n = int(PAD_END * SR)
    ch = CHORDS[m][1]
    x = sum(mm.detuned(mm.midi(c), n, 5, 14) for c in ch) / len(ch)
    lfo = 1 + 0.25 * np.sin(2 * np.pi * 0.15 * np.arange(n) / SR)
    y = mm.svf_lp(x, 1300 * lfo, 0.8) * mm.adsr(n, 1.5, 1.0, 1.0, 0.4, PAD_END - 0.4)
    st = np.stack([y, np.roll(y, int(0.011 * SR))], axis=1)
    st *= ref_rms / np.sqrt(np.mean(st[int(1.5 * SR):int(4.0 * SR)] ** 2))
    return fade(st, 0.0, 0.4)


def sound_for(material, m):
    if m == "hit":
        h, _ = sf.read(ROOT / "test_audio" / "stimulus" / "02_hits.wav", always_2d=True)
        on = int(np.argmax(np.abs(h[:, 0]) > 0.01))
        seg = h[on:on + int(0.5 * SR)]
        return (HIT_AT, np.repeat(seg, 2, axis=1) if seg.shape[1] == 1 else seg)
    x, _ = sf.read(material / (m + ".wav"), always_2d=True)
    return (OVER[0], fade(x[:int((OVER[1] - OVER[0]) * SR)], 0.02, 0.15))


def stimulus(material, m, ref_rms):
    out = np.zeros((int(LENGTH * SR), 2))
    p = pad_for(material, m, ref_rms)
    out[:len(p)] += p
    t, s = sound_for(material, m)
    out[int(t * SR):int(t * SR) + len(s)] += s
    return out, p


def chroma(x, hi=2000.0, w=8192):
    m = x.mean(axis=1)
    c = np.zeros(12)
    for i in range(0, len(m) - w + 1, w // 2):
        X = np.abs(np.fft.rfft(m[i:i + w] * np.hanning(w))) ** 2
        f = np.fft.rfftfreq(w, 1 / SR)
        ok = (f > 60) & (f < hi)
        pc = np.round(12 * np.log2(f[ok] / 261.63)) % 12
        for k in range(12):
            c[k] += X[ok][pc == k].sum()
    return c / c.sum()


def key_of(c):
    maj = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
    mnr = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
    sc = [(np.corrcoef(np.roll(maj, k), c)[0, 1], NAMES[k] + " major") for k in range(12)]
    sc += [(np.corrcoef(np.roll(mnr, k), c)[0, 1], NAMES[k] + " minor") for k in range(12)]
    sc.sort(reverse=True)
    return sc[0][1], " ".join(NAMES[k] for k in np.argsort(c)[::-1][:5])


def measured(binary, src, path, sets, e, buttons, hooks, f):
    render(binary, src, path, sets, e, buttons, hooks)
    x, _ = sf.read(path, always_2d=True)
    path.unlink()
    return f(x)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-twin" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "twin_alternatives"
    dry = root / "renders" / "_material_twin_alternatives"
    work = root / "renders" / "_twin_alternatives_work" # measurement renders, numbers only
    for d in (out, dry, work):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    ref, _ = sf.read(material / "pad.wav", always_2d=True)
    ref_rms = float(np.sqrt(np.mean(ref[int(1.5 * SR):int(4.0 * SR)] ** 2)))
    keys = []
    for m in ALL:
        x, p = stimulus(material, m, ref_rms)
        sf.write(dry / (m + ".wav"), x, SR, subtype="PCM_24")
        if m in TONAL:
            s, _ = sf.read(material / (m + ".wav"), always_2d=True)
            keys.append((m, key_of(chroma(s[:int(9 * SR)])), key_of(chroma(p[int(0.5 * SR):int(4.6 * SR)], 450.0, 65536))))
    binary = a.render.resolve()
    paint = [list(ft.FREEZE), list(ft.PAINT), [ft.UNFREEZE[0], ft.UNFREEZE[1]]]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    events = [(k, t) for k, t in ft.EVENTS]
    cards = [("{}_layer_{}".format(i + 1, sp), sp, sets, True, ALL) for i, (sp, sets) in enumerate(SPACES)]
    cards += [("{}_overpad_{}".format(i + 4, sp), sp, sets, False, TONAL) for i, (sp, sets) in enumerate(SPACES)]
    paint_lift = {}
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sp, sets, solo, mats in cards:
            for (tag, mode), g in zip(VERSIONS, GAINS[sp]):
                e = {} if mode == 0 else {"IV_TWIN_ALT": "{}:{:.2f}".format(mode, g)}
                if solo:
                    e["IV_LAYER_SOLO"] = "1"
                for m in mats:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, e, paint, hooks)
                    if not solo:
                        paint_lift[(col, m, tag)] = measured(binary, dry / (m + ".wav"), work / "np.wav", sets, e, nopaint,
                                                             hooks, lambda x: lv(x, 22, 23.9))
    with open(out / "_verify.txt", "w") as v:
        v.write("twin_alternatives (layer alone + over an in-key pad). Keys (pitch classes, Krumhansl profiles; the material 60-2000 Hz, the pads 60-450 Hz with a 65,536-point FFT, their fundamentals: a saw pad's harmonics pull a wider band, and semitones near 73 Hz are 4 Hz apart):\n")
        for m, (mk, mpc), (pk, ppc) in keys:
            v.write("  {:5s} material {} ({}); its pad {} ({}): {}\n".format(m, mk, mpc, CHORDS.get(m, ("the material's own", ()))[0]
                                                                          if m in CHORDS else "A minor", ppc, pk))
        v.write("Per render: the label check against A; level vs A (layer alone: the layer, A-weighted, 4.5-15.5 s); "
                "falls (layer alone: s to 60 dB under after the playing stops); Paint (over the pad: the pad at 22-23.9 s, "
                "Paint minus none); 1 ms click scores at " + ", ".join("{} {:.1f} s".format(k, t) for k, t in events) + "\n")
        for col, sp, sets, solo, mats in cards:
            v.write("{}: gains {}\n".format(col, ", ".join("{} {:+.2f} dB".format(t, g) for (t, _), g in zip(VERSIONS, GAINS[sp]))))
            for m in mats:
                aa, _ = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
                stop = HIT_AT + 0.5 if m == "hit" else OVER[1]
                for tag, _ in VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    same = "" if tag == VERSIONS[0][0] else ("; IDENTICAL to A" if np.array_equal(aa, x) else
                                                            "; vs A null {:.1f} dB".format(db(np.mean((aa - x) ** 2) / np.mean(aa ** 2))))
                    if solo:
                        extra = "level vs A {:+.1f} dB(A); falls {:.1f} s".format(lv(x, 4.5, 15.5, True) - lv(aa, 4.5, 15.5, True),
                                                                               ft.fall_60(x, stop))
                    else:
                        extra = "Paint adds {:+.2f} dB".format(lv(x, 22, 23.9) - paint_lift[(col, m, tag)])
                    v.write("  {:5s} {:13s} {}{}; clicks {}\n".format(m, tag, extra, same, clicks(x, events)))
    shutil.rmtree(work)
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "Lighter layers over a frozen pad", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
