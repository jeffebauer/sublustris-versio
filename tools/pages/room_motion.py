#!/usr/bin/env python3
"""PROTOTYPE decision page (27 Oct 2026; the owner on renders/room_level: "Twin reverb sounds like there's some slight
pitch bending when played over the top"). Measured: the small room's read wander (0.15 ms on every line, 0.21-0.58 Hz)
bends what it holds by 4-9 cents at SIZE 0.9 and up to ~40 cents at SIZE 0 (its short lines go round 20-125 times a
second and the bend builds up pass after pass); v1.0.41's twin 0-3 c, the main tank 2-8 c. Renderer hook
IV_ROOM_WANDER (dsp/TwinRoom.h setWanderPrototype; desktop only):

    A  today (0)
    B  scaled with SIZE: each line's wander a share of its length (today's at SIZE 0.9, smaller on shorter lines) (1)
    C  off (2)

The material is retuned first (A4 = 440): the in-key pads are made with their voices within 2 cents (they were
spread +-14 c, a chorus that beats), the held chord is synthesized in equal temperament, the vibes were already in
tune (0.0 c). Cards: the layer alone and over the pad, MASSIVE and HALL, SIZE 0 / 0.5 / 0.9, DECAY 0.9, WEAR 0; each
version level-matched to A on the layer alone (A-weighted, IV_TWIN_GAIN_DB).

Usage: <main checkout>/.venv/bin/python tools/pages/room_motion.py [--render build-wear/iv_render]
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
from review_cpu_pages import main_checkout  # noqa: E402
import frozen_twin as ft  # noqa: E402
from freeze_moment import render, lv, clicks  # noqa: E402
import twin_alternatives as ta  # noqa: E402

mm = ta.mm
SR = ta.SR
VERSIONS = [("A_today", "0"), ("B_scaled_with_size", "1"), ("C_off", "2")]
LABELS = {"0": "as built", "1": "room wander scaled with line length", "2": "room wander off"}
MATS = {"chord": ("D2", "A2", "F3", "C4", "E4"), "vibes": ("F2", "C3", "A3", "E4", "G4")}  # each sound's in-key pad
SETTINGS = [(sp, s) for sp in ("MASSIVE", "HALL") for s in ("0", "0.5", "0.9")]
NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]


def pad_for(notes, ref_rms):
    """The in-key pad (make_material.py's pad voice) with its voices within 2 cents (was +-14: a chorus that beats)."""
    n = int(ta.PAD_END * SR)
    x = sum(mm.detuned(mm.midi(c), n, 3, 2) for c in notes) / len(notes)
    lfo = 1 + 0.25 * np.sin(2 * np.pi * 0.15 * np.arange(n) / SR)
    y = mm.svf_lp(x, 1300 * lfo, 0.8) * mm.adsr(n, 1.5, 1.0, 1.0, 0.4, ta.PAD_END - 0.4)
    st = np.stack([y, np.roll(y, int(0.011 * SR))], axis=1)
    st *= ref_rms / np.sqrt(np.mean(st[int(1.5 * SR):int(4.0 * SR)] ** 2))
    return ft.fade(st, 0.0, 0.4)


def held_chord(ref_rms):
    """A held D minor chord (D3 F3 A3 C4 E4), equal temperament at A4 = 440, no detune, no vibrato: 4.5-13 s."""
    n = int((ta.OVER[1] - 0.5 - ta.OVER[0]) * SR)
    x = sum(mm.saw(np.full(n, mm.hz(mm.midi(c))), k * 0.13) for k, c in enumerate(("D3", "F3", "A3", "C4", "E4"))) / 5
    y = mm.svf_lp(x, np.full(n, 1800.0), 0.7) * mm.adsr(n, 0.05, 0.3, 0.8, 0.5, n / SR - 0.5)
    st = np.stack([y, y], axis=1)
    st *= ref_rms / np.sqrt(np.mean(st[int(0.5 * SR):int(4.0 * SR)] ** 2))
    return ft.fade(st, 0.01, 0.3)


def stimulus(material, m, ref_rms):
    out = np.zeros((int(ta.LENGTH * SR), 2))
    p = pad_for(MATS[m], ref_rms)
    out[:len(p)] += p
    if m == "chord":
        t, s = ta.OVER[0], held_chord(ref_rms)
    else:
        t, s = ta.sound_for(material, m)
    out[int(t * SR):int(t * SR) + len(s)] += s
    return out, p, s


def tuning(x, t0, t1, k=6, fmin=70, fmax=1400):
    """The strongest k notes' cents off equal temperament (A4 = 440), from parabolic-interpolated spectral peaks."""
    m = x[int(t0 * SR):int(t1 * SR)].mean(axis=1)
    nf = 1 << 20
    P = np.abs(np.fft.rfft(m * np.hanning(len(m)), nf))
    f = np.fft.rfftfreq(nf, 1 / SR)
    ok = np.where((f > fmin) & (f < fmax))[0]
    loc = sorted([i for i in ok[1:-1] if P[i] > P[i - 1] and P[i] >= P[i + 1]], key=lambda i: -P[i])
    res = []
    for i in loc:
        a, b, c = np.log(P[i - 1]), np.log(P[i]), np.log(P[i + 1])
        fr = f[i] + 0.5 * (a - c) / (a - 2 * b + c) * (f[1] - f[0])
        if all(abs(1200 * np.log2(fr / r)) > 60 for r in res):
            res.append(fr)
        if len(res) >= k:
            break
    out = []
    for fr in sorted(res):
        midi = 69 + 12 * np.log2(fr / 440)
        n = int(round(midi))
        out.append((NAMES[n % 12] + str(n // 12 - 1), 100 * (midi - n)))
    return out


def col(i, kind, sp, s):
    return "{:02d}_{}_{}_S{}".format(i + 1, kind, sp.lower(), s)


README = """The small room's pitch. Your note on room_level: "Twin reverb sounds like there's some slight pitch bending when played over the top." Measured: it's the room itself, not your samples. Its echoes move very slightly (that keeps a small room from sounding metallic), and because the room is so small, sound goes round it many times a second and the movement adds up into a pitch bend: a few cents at SIZE 0.9, up to about 40 cents at SIZE 0 (the main reverb moves 2-8 cents; the old twin hardly at all).

The question: which keeps the room steady in pitch without it ringing?

A is today. B scales the movement with SIZE: the same as today at SIZE 0.9, smaller as the room gets smaller, so the bend stays a few cents at every SIZE. C turns the movement off: no bend at all, but a small room with nothing moving can ring (a metallic, pitched hum on some notes, most of all on the vibes and the snare). The measurements of each are in _verify.txt.

The material is retuned for this page: the pads are now in tune to within 2 cents (they had a chorus that beat against what's played), the held chord is exactly in tune, and the vibes were already in tune. So any wobble you hear is the room.

Cards 1-6, the layer alone: only the room is heard (the pad is frozen underneath but silent). The three versions are set to the same loudness. Cards 7-12, over a pad in the same key. Each card: the pad plays and is frozen at 4 s; the chord or vibes play from 4.5 s; the button is held from 9 to 12 s (Paint); the playing stops by 13.5 s; a tap unfreezes at 24 s. DECAY 0.9, WEAR 0, TONE and MOTION noon. Renders are 100 % wet, so use the MIX fader for the dry (around 50 %).

""" + "\n".join(col(i, k, sp, s) + "   " + ("the room alone" if k == "layer" else "over the pad") + ", " + sp + ", SIZE " + s
                for i, (k, sp, s) in enumerate([("layer", sp, s) for sp, s in SETTINGS] + [("overpad", sp, s) for sp, s in SETTINGS])) + """

A_today              the room's movement as it is now
B_scaled_with_size   the movement scaled with SIZE (a few cents at every size)
C_off                no movement"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-wear" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "room_motion"
    dry = root / "renders" / "_material_room_motion"
    work = root / "renders" / "_room_motion_work"
    for d in (out, dry, work):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    ref, _ = sf.read(material / "pad.wav", always_2d=True)
    ref_rms = float(np.sqrt(np.mean(ref[int(1.5 * SR):int(4.0 * SR)] ** 2)))
    tune = []
    for m in MATS:
        x, p, s = stimulus(material, m, ref_rms)
        sf.write(dry / (m + ".wav"), x, SR, subtype="PCM_24")
        tune.append((m, tuning(p, 1.5, 3.9), tuning(s, 0.3, min(3.5, len(s) / SR - 0.1))))
    binary = a.render.resolve()
    paint = [list(ft.FREEZE), list(ft.PAINT), list(ft.UNFREEZE)]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    events = [(k, t) for k, t in ft.EVENTS]
    rows, gains = [], {}
    with open(out / "_hooks.txt", "w") as hooks:
        for sp, s in SETTINGS: # level match on the layer alone, A-weighted 4.5-15.5 s, the mean of the two sounds
            sets = ["space=" + sp, "size=" + s, "decay=0.9", "era=0"]
            alone = {(tag, m): ta.measured(binary, dry / (m + ".wav"), work / "cal.wav", sets,
                                           {"IV_ROOM_WANDER": w, "IV_LAYER_SOLO": "1"}, paint, hooks,
                                           lambda x: lv(x, 4.5, 15.5, True)) for tag, w in VERSIONS for m in MATS}
            for tag, _ in VERSIONS:
                gains[(sp, s, tag)] = float(np.mean([alone[("A_today", m)] - alone[(tag, m)] for m in MATS]))
        cards = [("layer", sp, s) for sp, s in SETTINGS] + [("overpad", sp, s) for sp, s in SETTINGS]
        for i, (kind, sp, s) in enumerate(cards):
            sets = ["space=" + sp, "size=" + s, "decay=0.9", "era=0"]
            for tag, w in VERSIONS:
                e = {"IV_ROOM_WANDER": w, "IV_TWIN_GAIN_DB": "{:.2f}".format(gains[(sp, s, tag)])}
                if kind == "layer":
                    e["IV_LAYER_SOLO"] = "1"
                for m in MATS:
                    path = out / col(i, kind, sp, s) / "{}_{}.wav".format(m, tag)
                    render(binary, dry / (m + ".wav"), path, sets, e, paint, hooks)
                    x, _ = sf.read(path, always_2d=True)
                    extra = ""
                    if kind == "overpad":
                        np_ = ta.measured(binary, dry / (m + ".wav"), work / "np.wav", sets, e, nopaint, hooks,
                                          lambda y: lv(y, 22.0, 23.9, True))
                        extra = "Paint {:+.2f} dB(A)".format(lv(x, 22.0, 23.9, True) - np_)
                    rows.append((col(i, kind, sp, s), m, tag, gains[(sp, s, tag)], extra, clicks(x, events),
                                 bool(np.all(np.isfinite(x)))))
    bad = []
    for line in (out / "_hooks.txt").read_text().splitlines():
        name = line.split("  env ")[0]
        for tag, w in VERSIONS:
            if name.endswith("_{}.wav".format(tag)) and LABELS[w] not in line:
                bad.append(line)
    with open(out / "_verify.txt", "w") as v:
        v.write("room_motion. Tuning (cents off equal temperament at A4 = 440, the strongest notes):\n")
        for m, p, s in tune:
            v.write("  {} pad: {}\n  {} sound: {}\n".format(m, " ".join("{} {:+.1f}".format(n, c) for n, c in p), m,
                                                        " ".join("{} {:+.1f}".format(n, c) for n, c in s)))
        v.write("label check: {}\n".format("all {} renders as labelled".format(len(rows)) if not bad else "MISMATCH:\n" + "\n".join(bad)))
        v.write("Per render: level-match gain (IV_TWIN_GAIN_DB, to A on the layer alone), Paint (over the pad: the pad at "
                "22-23.9 s, Paint minus none), finite, 1 ms click scores at " +
                ", ".join("{} {:.1f} s".format(k, t) for k, t in events) + ".\n")
        for c, m, tag, g, extra, ck, fin in rows:
            v.write("{:28s} {:5s} {:19s} gain {:+.2f} dB  {}  finite {}  clicks {}\n".format(c, m, tag, g, extra, fin, ck))
    shutil.rmtree(work)
    if bad:
        sys.exit("label check failed: see _verify.txt")
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "The small room's pitch", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
