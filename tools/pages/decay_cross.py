"""renders/decay_cross: DECAY turned into and out of endless, today against the crossover paced over 1.5 / 3 / 6 s.
Usage: IV_RENDER=<a renderer with IV_DECAY_CROSS> .venv/bin/python tools/pages/decay_cross.py   -> renders/decay_cross + _verify.txt
then: python3 tools/review/make_review.py renders/decay_cross --wet-only --dry renders/decay_cross/_dry --no-level-match
"""
# renders/decay_cross (1 Nov 2026; the owner on v1.0.60: "a very quick drop off in tail and amplitude that felt jarring").
# Material: renders/_material/pad.wav (tools/make_material.py) at +6 dB, a hot rack level.

import json, os, shutil, subprocess
import numpy as np, soundfile as sf

REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
MAT = os.environ.get("DC_MATERIAL", REPO_ + "/renders/_material/pad.wav")
PAGE = os.environ.get("DC_PAGE", REPO_ + "/renders/decay_cross")
GAIN = 2.0  # +6 dB
# column folder, SPACE, SIZE, material, DECAY from, to, turn seconds, at
CARDS = [
    ("1_hall_into_endless", "HALL", 0.9, "playing", 0.78, 1.0, 0.5, 20.0),
    ("1_hall_into_endless", "HALL", 0.9, "tail", 0.78, 1.0, 0.5, 15.0),
    ("2_hall_out_of_endless", "HALL", 0.9, "playing", 1.0, 0.75, 0.5, 20.0),
    ("2_hall_out_of_endless", "HALL", 0.9, "tail", 1.0, 0.75, 0.5, 20.0),
]
# MASSIVE is left out: its floor eases in from DECAY 0.70 (v1.0.56), so its crossing already glides (a card
# there measured the same in all four, within 0.3 dB).
VERS = [("A_today", "0.0001"), ("B_1s5", "1.5"), ("C_3s", "3"), ("D_6s", "6")]


def material(out_dir):
    x, sr = sf.read(MAT)
    x = x * GAIN
    seg = x[:int(17 * sr)]; f = sr; fi = np.linspace(0, 1, f)[:, None]
    play = np.concatenate([seg[:-f], seg[-f:] * (1 - fi) + seg[:f] * fi, seg[f:]])[:int(34 * sr)]
    g = int(1.5 * sr); play[-g:] *= np.linspace(1, 0, g)[:, None]
    sf.write(out_dir + "/playing.wav", np.concatenate([play, np.zeros((6 * sr, 2))]), sr, subtype="PCM_24")
    t = x[:int(14 * sr)].copy(); f = sr // 2; t[-f:] *= np.linspace(1, 0, f)[:, None]
    sf.write(out_dir + "/tail.wav", np.concatenate([t, np.zeros((26 * sr, 2))]), sr, subtype="PCM_24")


def render(mat, sp, size, auto, cross, out):
    e = dict(os.environ); e["IV_DECAY_CROSS"] = cross
    a = out + ".auto.json"; json.dump(auto, open(a, "w"))
    subprocess.run([R, f"{PAGE}/_dry/{mat}.wav", out, "--set", "mix=1", "--set", f"space={sp}", "--set", f"size={size}",
                    "--set", f"decay={auto['breakpoints'][0]['value']}", "--auto", a, "--sidecar"], env=e, check=True, capture_output=True)
    os.remove(a)
    return sf.read(out)


def levels(x, sr, sec=0.05):
    m = x.mean(1); n = int(sec * sr); k = len(m) // n
    p = np.mean(m[:k * n].reshape(k, n) ** 2, axis=1)
    return 10 * np.log10(np.convolve(p, np.ones(4) / 4, "same") + 1e-14)


if os.path.isdir(PAGE): shutil.rmtree(PAGE)
os.makedirs(PAGE + "/_dry")
material(PAGE + "/_dry")
lines = []
for folder, sp, size, mat, d0, d1, dur, t0 in CARDS:
    auto = {"breakpoints": [{"t": 0, "key": "decay", "value": d0}, {"t": t0, "key": "decay", "value": d0},
                            {"t": t0 + dur, "key": "decay", "value": d1}]}
    os.makedirs(f"{PAGE}/{folder}", exist_ok=True)
    lines.append(f"\n{folder} / {mat}: {sp} SIZE {size}, DECAY {d0} -> {d1} over {dur} s at {t0:.0f} s")
    outs = {}
    for tag, cross in VERS:
        x, sr = render(mat, sp, size, auto, cross, f"{PAGE}/{folder}/{mat}_{tag}.wav")
        outs[tag] = x
        L = levels(x, sr); i0 = int(t0 / 0.05)
        at = L[i0 - 4:i0].mean()
        fall = min(L[j + 10] - L[j] for j in range(i0, i0 + 120))  # steepest 0.5 s fall in the 6 s after the move
        trace = " ".join("%+.1f" % (v - at) for v in L[i0:i0 + 200:10])
        # clicks: the loudest 1 ms frame above the second around it, of the signal's 2nd difference (HF)
        y = np.diff(x.mean(1), n=2); n = sr // 1000; k = len(y) // n
        e = np.mean(y[:k * n].reshape(k, n) ** 2, 1) + 1e-20
        click = max(10 * np.log10(e[i] / np.median(e[i - 500:i + 500])) for i in range(int(t0 * 1000) - 500, int(t0 * 1000) + 6000))
        lines.append(f"  {tag:8s} steepest 0.5 s fall {fall:+5.1f} dB; 1 ms HF click score {click:4.1f} dB; "
                     f"level vs the move's start every 0.5 s: {trace}")
    a = outs["A_today"]; js = int(t0 * sr)
    for tag, _ in VERS[1:]:
        b = outs[tag]
        lines.append(f"  {tag}: before the move {'bit-identical to A' if np.array_equal(a[:js], b[:js]) else 'DIFFERENT from A'}; "
                     f"after it {'IDENTICAL (mislabelled!)' if np.array_equal(a, b) else 'differs from A'}")
open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), "decay_cross_README.txt"), PAGE + "/README.txt")
print("\n".join(lines))
