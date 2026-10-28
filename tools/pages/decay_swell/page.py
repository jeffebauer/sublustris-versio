"""renders/decay_swell: render the cards (A today, B the make-up's rise capped) and write _verify.txt."""
# renders/decay_swell (28 Oct 2026; docs/research/decay-swell.md). Work files (material, scratch renders) live in
# renders/_decay_swell_work (DS_WORK); make the material first: python3 tools/pages/decay_swell/material.py

import json, os, shutil, subprocess, sys
import numpy as np, soundfile as sf

REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
S = os.environ.get("DS_WORK", REPO_ + "/renders/_decay_swell_work")
os.makedirs(S + "/out", exist_ok=True)
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")  # a renderer with IV_DECAY_RISE
PAGE = os.environ.get("DS_PAGE", REPO_ + "/renders/decay_swell")
K = "0.3"
# card folder, SPACE, SIZE, material, DECAY from, to, turn seconds
CARDS = [
    ("1_massive", "MASSIVE", 0.65, "playing", 0.92, 0.3, 1.0),
    ("1_massive", "MASSIVE", 0.65, "tail", 0.83, 0.2, 0.2),
    ("2_hall", "HALL", 0.6, "playing", 0.78, 0.2, 1.0),
    ("2_hall", "HALL", 0.6, "tail", 0.78, 0.2, 0.2),
]
T0 = {"playing": 18.0, "tail": 15.0}
# The page was rendered at 7cb493e, where IV_DECAY_RISE=k was a hook; B is built in since v1.0.55 (the hook is ignored),
# so to remake it run A with a v1.0.54 renderer (IV_RENDER_A) and B with today's.
VERS = [("A_today", {}), ("B_capped", {"IV_DECAY_RISE": K})]


def render(mat, sp, size, d0, auto, env, out):
    e = dict(os.environ); e.update(env)
    a = out + ".auto.json"; json.dump(auto, open(a, "w"))
    subprocess.run([R, f"{S}/mat/{mat}.wav", out, "--set", "mix=1", "--set", f"space={sp}", "--set", f"size={size}",
                    "--set", f"decay={d0}", "--auto", a, "--sidecar"], env=e, check=True, capture_output=True)
    os.remove(a)
    x, sr = sf.read(out)
    return x, sr


def frames(m, sr, sec):
    n = int(sec * sr); k = len(m) // n
    return np.mean(m[:k * n].reshape(k, n) ** 2, axis=1)


def hp3k(x, sr):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / sr); X[f < 3000] = 0
    return np.fft.irfft(X, len(x))


if os.path.isdir(PAGE): shutil.rmtree(PAGE)
os.makedirs(PAGE + "/_dry")
for mat in ("playing", "tail"): shutil.copy(f"{S}/mat/{mat}.wav", f"{PAGE}/_dry/{mat}.wav")
lines = []
for folder, sp, size, mat, d0, d1, dur in CARDS:
    t0 = T0[mat]
    auto = {"breakpoints": [{"t": 0, "key": "decay", "value": d0}, {"t": t0, "key": "decay", "value": d0},
                            {"t": t0 + dur, "key": "decay", "value": d1}]}
    os.makedirs(f"{PAGE}/{folder}", exist_ok=True)
    tmp = f"{S}/out/held.wav"
    held, sr = render(mat, sp, size, d0, {"breakpoints": [{"t": 0, "key": "decay", "value": d0}]}, {}, tmp)
    os.remove(tmp); os.remove(tmp[:-4] + ".json")
    hm = held.mean(1)
    hl = 10 * np.log10(np.convolve(frames(hm, sr, 0.05), np.ones(4) / 4, "same") + 1e-14)
    hhf = 10 * np.log10(frames(hp3k(hm, sr), sr, 0.05) + 1e-14)
    i0 = int(t0 / 0.05); i1 = i0 + int(12 / 0.05)
    lines.append(f"\n{folder} / {mat}: {sp} SIZE {size}, DECAY {d0} -> {d1} over {dur} s at {t0:.0f} s")
    outs = {}
    for tag, env in VERS:
        out = f"{PAGE}/{folder}/{mat}_{tag}.wav"
        x, _ = render(mat, sp, size, d0, auto, env, out)
        outs[tag] = x
        m = x.mean(1)
        p = frames(m, sr, 0.05); db = 10 * np.log10(p + 1e-14)
        sm = 10 * np.log10(np.convolve(p, np.ones(4) / 4, "same") + 1e-14)
        at = db[i0 - 2:i0].max(); pk = db[i0:i1].max(); tpk = (db[i0:i1].argmax()) * 0.05
        over = (sm[i0:i1] - hl[i0:i1]).max()
        # clicks: largest rise between 1 ms frames after the move, against the same in the held render
        f1 = 10 * np.log10(frames(m, sr, 0.001) + 1e-14); h1 = 10 * np.log10(frames(hm, sr, 0.001) + 1e-14)
        j0 = int(t0 / 0.001); j1 = j0 + 12000
        step = np.diff(f1[j0:j1])[f1[j0 + 1:j1] > -80].max(); hstep = np.diff(h1[j0:j1])[h1[j0 + 1:j1] > -80].max()
        hf = 10 * np.log10(frames(hp3k(m, sr), sr, 0.05) + 1e-14)
        burst = (hf[i0:i1] - hhf[i0:i1]).max()
        trace = " ".join("%.1f" % v for v in sm[i0 - 10:i1:10] - at)
        lines.append(f"  {tag:9s} level at the move {at:6.1f} dBFS; loudest 50 ms after {pk:6.1f} ({pk - at:+.1f} dB, at +{tpk:.2f} s); "
                     f"above the unmoved render at most {over:+.1f} dB; largest 1 ms step {step:.1f} dB (the unmoved render's {hstep:.1f}: the music's own); "
                     f">3 kHz vs unmoved at most {burst:+.1f} dB")
        lines.append(f"            level vs the move's start, every 0.5 s from -0.5 s: {trace}")
    a, b = outs["A_today"], outs["B_capped"]
    js = int(t0 * sr)
    pre_same = np.array_equal(a[:js], b[:js])
    d = a - b
    # clicks: B is A through a slower gain, so a click would be a jump in their level ratio between 1 ms frames
    fa = frames(a.mean(1), sr, 0.001); fb = frames(b.mean(1), sr, 0.001)
    j0 = int(t0 / 0.001); j1 = j0 + 12000; ok = fa[j0:j1] > 1e-6  # where A is above -60 dBFS
    rr = 10 * np.log10((fb[j0:j1] + 1e-14) / (fa[j0:j1] + 1e-14))
    lines.append(f"  clicks: the B/A level ratio moves at most {np.abs(np.diff(rr[ok])).max() if ok.sum() > 1 else 0:.2f} dB between "
                 f"consecutive 1 ms frames (a smooth gain; the music's own 1 ms steps are in the lines above)")
    lines.append(f"  null A-B: before the move {'bit-identical' if pre_same else 'DIFFERENT'}; whole render "
                 f"{10 * np.log10(np.mean(d ** 2) / np.mean(a ** 2) + 1e-30):.1f} dB vs A (the difference is the make-up's timing)")
open(f"{S}/verify_cards.txt", "w").write("\n".join(lines) + "\n")
print("\n".join(lines))
