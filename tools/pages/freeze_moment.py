#!/usr/bin/env python3
"""PROTOTYPE pick pages (26 Oct 2026), two questions, two folders (the page player holds one set of versions):

renders/freeze_moment: the owner, "the frozen sound seems to jump in amplitude once frozen compared to the
swelling reverb". At DECAY 0.9 the wet is ducked 2-4 dB while you play (ADR 0014) and a freeze lets the ducking
go in 50 ms, so the pad jumps up. Renderer hook IV_FREEZE_DUCK (Engine::setFreezeDuckPrototype):
    A  today (let go in 50 ms)
    B  hold: the pad keeps the ducking it had at the tap; let go over 1 s after the unfreeze ("1:1.0")
    C  glide: let go in a straight line over 1.5 s after the tap ("2:1.5")
Cards: MASSIVE (Neve, cassette), HALL, PLATE at DECAY 0.9 (ducking on), MASSIVE at DECAY 0.6 (ducking off: what is
left of the step). Material: a pad or a voice, frozen mid-swell at 3 s, the input gone by 3.6 s, nothing played
over it until 8 s, then keys / vibes over it to 14 s, unfreeze at 18 s.

renders/twin_louder: the owner, "even with the AS BUILT option, with wet at 100% i can hardly hear the secondary
reverb". The twin's tail is capped at 8 s while HALL's and MASSIVE's run 14 s to infinite, so it sits 12-15 dB
(A-weighted) under the pad here. Renderer hook IV_TWIN_GAIN_DB (Engine::setTwinGainPrototype; the twin's output in
HALL and MASSIVE, Paint unchanged):
    A  today
    B  louder: 6 dB under the pad (MASSIVE +7.7 dB, HALL +6.0, the measured means on this material)
    C  much louder: B + 4.5 dB, 1.5 dB under the pad
    D  no twin (what you play over the pad is heard only in the dry)
Cards: MASSIVE (Neve, cassette), HALL (Neve), SIZE 0.9, DECAY 0.9; keys / vibes / voice over a pad frozen at 4 s,
Paint held 9-12 s, the playing stops at 13.5 s, unfreeze at 24 s (tools/pages/frozen_twin.py's material).

Neither page is level-matched: on the freeze page the versions are the same sample for sample until the tap, so
the swell before it is the reference; on the twin page the pad is the same in every version and only the twin's
level changes (that is the question).

Usage: <main checkout>/.venv/bin/python tools/pages/freeze_moment.py [--render build-twin/iv_render]
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
import frozen_twin as ft  # noqa: E402

SR = 48000
KNOBS = ["tone=0.5", "drift=0.5", "attack=NORMAL", "time=0"]

# ---- the freeze moment
M_LEN, M_FREEZE, M_IN_END, M_OVER, M_UNFREEZE = 26.0, 3.0, 3.6, (8.0, 14.0), 18.0
M_EVENTS = [("freeze", M_FREEZE), ("play", M_OVER[0]), ("play ends", M_OVER[1]), ("unfreeze", M_UNFREEZE)]
M_MATERIAL = {"pad": "keys", "voice": "vibes"}  # frozen source -> played over it
M_COLS = [("1_massive_neve", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.1"]),
          ("2_massive_cassette", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.7"]),
          ("3_hall", ["space=HALL", "size=0.9", "decay=0.9", "era=0.1"]),
          ("4_plate", ["space=PLATE", "size=0.9", "decay=0.9", "era=0.1"]),
          ("5_massive_decay06", ["space=MASSIVE", "size=0.9", "decay=0.6", "era=0.1"])]
M_VERSIONS = [("A_today", {}), ("B_hold", {"IV_FREEZE_DUCK": "1:1.0"}), ("C_glide_1500ms", {"IV_FREEZE_DUCK": "2:1.5"})]
M_CHAPTERS = "Swelling:0,Frozen:3,Pad alone:3.6,Playing over:8,Unfreeze:18"
M_README = """The freeze moment: does the pad jump up when you freeze it? Your note: "the frozen sound seems to jump in amplitude once frozen compared to the swelling reverb". It does: at long DECAY the reverb ducks under what you play (2-4 dB here), and a freeze lets that ducking go in a twentieth of a second, so the pad comes up 2-3 dB just as you tap.

A is today. B holds the pad at the level you heard at the tap; when you unfreeze, the ducking lets go over a second, with the tail. C lets the pad come up slowly instead, over 1.5 s, so the jump becomes a gentle rise to today's level.

Each card: a pad (or a voice) swells, is frozen at 3 s while it's still growing, rings alone from 3.6 s, then keys (or vibes) play over it from 8 to 14 s, and a tap unfreezes at 18 s. The bar under each card jumps to those moments. Cards 1-4 at DECAY 0.9 (where the ducking is on); card 5, MASSIVE at DECAY 0.6, has no ducking, so all three are the same there: it lets you hear the small step that is left (about 1.5 dB, the loop's losses stopping). Not level-matched: the versions are identical until the tap, so the swell before it is your reference. Renders are 100 % wet, so use the MIX fader for the dry.

1_massive_neve         MASSIVE, SIZE 0.9, DECAY 0.9, ERA at the Neve
2_massive_cassette     the same with the cassette
3_hall                 HALL, SIZE 0.9, DECAY 0.9
4_plate                PLATE, SIZE 0.9, DECAY 0.9
5_massive_decay06      MASSIVE at DECAY 0.6: no ducking, the step that is left

A_today                the ducking lets go in 50 ms at the tap: the pad jumps up
B_hold                 the pad stays at the tap's level until you unfreeze
C_glide_1500ms         the pad comes up to today's level over 1.5 s"""

# ---- the twin level
T_COLS = [("1_massive_neve", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.1"], 7.7),
          ("2_massive_cassette", ["space=MASSIVE", "size=0.9", "decay=0.9", "era=0.7"], 7.7),
          ("3_hall_neve", ["space=HALL", "size=0.9", "decay=0.9", "era=0.1"], 6.0)]
T_LOUDER = 4.5
T_VERSIONS = [("A_today", None), ("B_louder", 0.0), ("C_much_louder", T_LOUDER), ("D_no_twin", "off")]
T_README = """How loud should the twin be over a frozen pad? Your note: "even with the AS BUILT option, with wet at 100% i can hardly hear the secondary reverb". Measured: here the twin sits 12-15 dB under the pad, not the 6 dB it was set to. The twin always fades within 8 s, but at long DECAY the big tank rings much longer, so the same playing builds up far less in the twin.

A is today. B brings the twin up to 6 dB under the pad (+7.7 dB in MASSIVE, +6 dB in HALL). C is louder still, 1.5 dB under the pad: about as loud as the pad. D has no twin at all: what you play over the pad isn't reverberated, you hear it only in the dry, so turn the MIX fader down from 100 % to hear D properly (at 100 % wet the playing is silent in D). D also frees about a quarter of the module's CPU while frozen. Paint (the button held from 9 to 12 s) sends the same amount into the pad in A, B and C; in D there is nothing to paint.

Each card: a pad is frozen at 4 s; keys, vibes or a voice play over it from 4.5 s; the button is held from 9 to 12 s (Paint); the playing stops at 13.5 s and the twin fades; a tap unfreezes at 24 s. SIZE 0.9, DECAY 0.9, TONE and DRIFT at noon. Not level-matched: the pad is the same in every version and only the twin's level changes. Renders are 100 % wet, so use the MIX fader for the dry.

1_massive_neve         MASSIVE, ERA at the Neve
2_massive_cassette     MASSIVE, ERA at the cassette
3_hall_neve            HALL, ERA at the Neve

A_today                the twin as today, 12-15 dB under the pad
B_louder               6 dB under the pad
C_much_louder          1.5 dB under the pad
D_no_twin              no twin: the playing only in the dry (bring MIX down)"""


def render(binary, src, out, sets, env_extra, buttons, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    auto = out.with_suffix(".json")
    auto.write_text(json.dumps({"buttons": buttons}))
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--auto", str(auto)]
    for s in sets + KNOBS:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    auto.unlink()
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}/{}  env {}  ->  {}\n".format(out.parent.name, out.name, env_extra or "{}",
                                                 " | ".join(l for l in p.stderr.splitlines() if l.startswith("iv_render:")) or "(no hook line)"))


def moment_stimulus(material, src, over):
    x = np.zeros((int(M_LEN * SR), 2))
    a, _ = sf.read(material / (src + ".wav"), always_2d=True)
    x[:int(M_IN_END * SR)] += ft.fade(a[:int(M_IN_END * SR)], 0.0, 0.3)
    b, _ = sf.read(material / (over + ".wav"), always_2d=True)
    seg = ft.fade(b[:int((M_OVER[1] - M_OVER[0]) * SR)], 0.02, 0.15)
    x[int(M_OVER[0] * SR):int(M_OVER[0] * SR) + len(seg)] += seg
    return x


def lv(x, t0, t1, a=False):
    seg = x[int(t0 * SR):int(t1 * SR)].mean(axis=1)
    if a:
        X = np.fft.rfft(seg); f = np.fft.rfftfreq(len(seg), 1.0 / SR); f2 = f ** 2
        ra = (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2) + 1e-30)
        seg = np.fft.irfft(X * ra, len(seg))
    return db(np.mean(seg ** 2) + 1e-30)


def clicks(x, events):
    h = ft.highpass(x, 5000.0)
    return " ".join("{} {:+.1f}".format(k, ft.click_score(h, t)) for k, t in events)


def moment_page(binary, material, root):
    out = root / "renders" / "freeze_moment"
    dry = root / "renders" / "_material_freeze_moment"
    for d in (out, dry):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    for src, over in M_MATERIAL.items():
        sf.write(dry / (src + ".wav"), moment_stimulus(material, src, over), SR, subtype="PCM_24")
    buttons = [[M_FREEZE, M_FREEZE + 0.1], [M_UNFREEZE, M_UNFREEZE + 0.1]]
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in M_COLS:
            for tag, env in M_VERSIONS:
                for src in M_MATERIAL:
                    render(binary, dry / (src + ".wav"), out / col / "{}_{}.wav".format(src, tag), sets, env, buttons, hooks)
            for src in M_MATERIAL: # the same without a freeze: the swell's own rise, for the step
                render(binary, dry / (src + ".wav"), out / "_unfrozen" / col / "{}.wav".format(src), sets, {}, [], hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("freeze_moment. Steps: level 400 ms after minus 400 ms before, dB; the freeze's minus the same render "
                "without a freeze (the swell's own rise). 1 ms click scores (loudest 1 ms frame above 5 kHz vs the "
                "second's median) at: " + ", ".join("{} {:.1f} s".format(k, t) for k, t in M_EVENTS) + "\n")
        for col, _ in M_COLS:
            for src in M_MATERIAL:
                a, _ = sf.read(out / col / "{}_{}.wav".format(src, M_VERSIONS[0][0]), always_2d=True)
                nf, _ = sf.read(out / "_unfrozen" / col / "{}.wav".format(src), always_2d=True)
                rise = lv(nf, M_FREEZE, M_FREEZE + 0.4) - lv(nf, M_FREEZE - 0.4, M_FREEZE)
                for tag, _ in M_VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(src, tag), always_2d=True)
                    same = "" if tag == M_VERSIONS[0][0] else (
                        "; IDENTICAL to A (mislabelled!)" if np.array_equal(a, x) else "; vs A null {:.1f} dB".format(
                            db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                    pre = np.array_equal(a[:int(M_FREEZE * SR)], x[:int(M_FREEZE * SR)])
                    v.write("{:20s} {:5s} {:15s} freeze step {:+.1f} dB (A-wtd {:+.1f}); pad 6-8 s vs A {:+.1f} dB; unfreeze step "
                            "{:+.1f} dB (A {:+.1f}); before the tap {}{}; clicks {}\n".format(
                                col, src, tag,
                                lv(x, M_FREEZE, M_FREEZE + 0.4) - lv(x, M_FREEZE - 0.4, M_FREEZE) - rise,
                                lv(x, M_FREEZE, M_FREEZE + 0.4, True) - lv(x, M_FREEZE - 0.4, M_FREEZE, True)
                                - (lv(nf, M_FREEZE, M_FREEZE + 0.4, True) - lv(nf, M_FREEZE - 0.4, M_FREEZE, True)),
                                lv(x, 6, 8) - lv(a, 6, 8),
                                lv(x, M_UNFREEZE, M_UNFREEZE + 0.4) - lv(x, M_UNFREEZE - 0.4, M_UNFREEZE),
                                lv(a, M_UNFREEZE, M_UNFREEZE + 0.4) - lv(a, M_UNFREEZE - 0.4, M_UNFREEZE),
                                "identical to A" if pre else "DIFFERS from A", same, clicks(x, M_EVENTS)))
    shutil.rmtree(out / "_unfrozen")
    (out / "README.txt").write_text(M_README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "The freeze moment: jump, hold or glide", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", M_CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


def twin_env(gain, extra):
    if gain is None:
        return {}
    if gain == "off":
        return {"IV_TWIN_GAIN_DB": "off"}
    return {"IV_TWIN_GAIN_DB": "{:.1f}".format(extra + gain)}


def twin_page(binary, material, root):
    out = root / "renders" / "twin_louder"
    dry = root / "renders" / "_material_frozen_twin" # frozen_twin's material, the same files
    alone = root / "renders" / "_twin_louder_alone"   # measurement only, removed at the end
    for d in (out, alone):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    dry.mkdir(parents=True, exist_ok=True)
    for m in ft.OVER:
        sf.write(dry / (m + ".wav"), ft.stimulus(material, m, True), SR, subtype="PCM_24")
        sf.write(alone / ("_in_" + m + ".wav"), ft.stimulus(material, m, False), SR, subtype="PCM_24")
        sf.write(alone / ("_pad_" + m + ".wav"), ft.stimulus(material, m, True) - ft.stimulus(material, m, False), SR,
                 subtype="PCM_24")
    paint = [list(ft.FREEZE), list(ft.PAINT), list(ft.UNFREEZE)]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets, b in T_COLS:
            for tag, g in T_VERSIONS:
                env = twin_env(g, b)
                for m in ft.OVER:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, paint, hooks)
                    render(binary, alone / ("_in_" + m + ".wav"), alone / col / "twin_{}_{}.wav".format(m, tag), sets, env,
                           nopaint, hooks)
                    render(binary, dry / (m + ".wav"), alone / col / "nopaint_{}_{}.wav".format(m, tag), sets, env, nopaint,
                           hooks)
            for m in ft.OVER:
                render(binary, alone / ("_pad_" + m + ".wav"), alone / col / "pad_{}.wav".format(m), sets, {}, nopaint, hooks)
    events = [(k, t) for k, t in ft.EVENTS]
    with open(out / "_verify.txt", "w") as v:
        v.write("twin_louder. twin - pad: the twin alone (playing over silence frozen at 4 s, no Paint) against the pad "
                "alone, 6-13.5 s, broadband / A-weighted. Paint: the pad at 22-23.9 s, with Paint minus without "
                "(the twin is 50-60 dB down by then). 1 ms click scores at: "
                + ", ".join("{} {:.1f} s".format(k, t) for k, t in events) + "\n")
        for col, _, _ in T_COLS:
            for m in ft.OVER:
                pad, _ = sf.read(alone / col / "pad_{}.wav".format(m), always_2d=True)
                a, _ = sf.read(out / col / "{}_{}.wav".format(m, T_VERSIONS[0][0]), always_2d=True)
                for tag, _ in T_VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    tw, _ = sf.read(alone / col / "twin_{}_{}.wav".format(m, tag), always_2d=True)
                    np_, _ = sf.read(alone / col / "nopaint_{}_{}.wav".format(m, tag), always_2d=True)
                    same = "" if tag == T_VERSIONS[0][0] else ("; IDENTICAL to A (mislabelled!)" if np.array_equal(a, x) else "")
                    v.write("{:18s} {:5s} {:14s} twin - pad {:+.1f} / {:+.1f} dB; Paint adds {:+.2f} dB to the pad; pad before "
                            "the playing (4.1-4.5 s) vs A {:+.2f} dB{}; clicks {}\n".format(
                                col, m, tag, lv(tw, 6, 13.5) - lv(pad, 6, 13.5), lv(tw, 6, 13.5, True) - lv(pad, 6, 13.5, True),
                                lv(x, 22, 23.9) - lv(np_, 22, 23.9), lv(x, 4.1, 4.5) - lv(a, 4.1, 4.5), same, clicks(x, events)))
    shutil.rmtree(alone)
    (out / "README.txt").write_text(T_README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "How loud is the twin over a frozen pad?", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-twin" / "iv_render")
    ap.add_argument("--only", choices=["moment", "twin"], default=None)
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    binary = a.render.resolve()
    if a.only != "twin":
        print(moment_page(binary, material, root))
    if a.only != "moment":
        print(twin_page(binary, material, root))


if __name__ == "__main__":
    main()
