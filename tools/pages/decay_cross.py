"""renders/decay_cross: DECAY turned into and out of endless, today against the crossover paced over 1.5 / 3 / 6 s.
Usage: IV_RENDER=<a renderer with IV_DECAY_CROSS> .venv/bin/python tools/pages/decay_cross.py
  -> renders/decay_cross (WAVs, _dry, README, _verify.txt, index.html)
Needs renders/_material/voice.wav (tools/make_material.py voice); the rest is synthesized here.
"""
# renders/decay_cross (1 Nov 2026; the owner on v1.0.60: "a very quick drop off in tail and amplitude that felt jarring";
# on the first page: "chapter markers to denote transition points ... and more varied examples of timbre, pitch, and
# envelope shape"). One timeline for every card, so one chapter bar fits all: play, turn into endless, back out, stop,
# then the same two turns on the tail.

import json, os, shutil, subprocess, sys
import numpy as np, soundfile as sf

REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
VOICE = os.environ.get("DC_VOICE", REPO_ + "/renders/_material/voice.wav")
PAGE = os.environ.get("DC_PAGE", REPO_ + "/renders/decay_cross")
SR = 48000
SPACE, SIZE = "HALL", 0.9
FIN0, ENDLESS, FIN1, TURN = 0.78, 1.0, 0.75, 0.5   # just under 3 o'clock, fully CW, about 2:30; each turn 0.5 s
T_IN, T_OUT, T_STOP, T_IN2, T_OUT2, LENGTH = 8.0, 16.0, 24.0, 27.0, 35.0, 48.0
CHAPTERS = [("Playing", 0.0), ("↑ into endless", T_IN), ("↓ out of endless", T_OUT),
            ("Stop playing", T_STOP), ("↑ into endless (tail)", T_IN2), ("↓ out (tail)", T_OUT2)]
VERS = [("A_today", "0.0001"), ("B_1s5", "1.5"), ("C_3s", "3"), ("D_6s", "6")]
RMS_DB, PEAK_DB = -14.0, -1.0   # a hot rack level (active RMS), peaks at most -1 dBFS


# ---- Sources ---------------------------------------------------------------------------------------------
def env_ad(n, a, d):
    t = np.arange(n) / SR
    return np.minimum(1.0, t / max(a, 1e-4)) * np.exp(-t / d)


def pluck(f, dur=1.2):
    # Karplus-Strong: a sharp attack and a quick, darkening decay
    n = int(dur * SR); p = int(SR / f); rng = np.random.default_rng(int(f))
    buf = rng.uniform(-1, 1, p); out = np.zeros(n)
    for i in range(n):
        out[i] = buf[i % p]
        buf[i % p] = 0.5 * (buf[i % p] + buf[(i + 1) % p]) * 0.996
    return out


def saw(f, n, harm=None):
    t = np.arange(n) / SR; y = np.zeros(n)
    for k in range(1, (harm or int(16000 / f)) + 1):
        y += np.sin(2 * np.pi * f * k * t) / k
    return y


def onepole_lp(y, hz):
    a = np.exp(-2 * np.pi * hz / SR); out = np.zeros_like(y); s = 0.0
    for i in range(len(y)):
        s = (1 - a) * y[i] + a * s; out[i] = s
    return out


def bell(f, n):
    # two-operator FM, inharmonic (ratio 3.5), long ring
    t = np.arange(n) / SR; e = np.exp(-t / 1.6)
    return np.sin(2 * np.pi * f * t + 2.5 * e * np.sin(2 * np.pi * 3.5 * f * t)) * e * np.minimum(1, t / 0.002)


def hz(midi): return 440.0 * 2 ** ((midi - 69) / 12)


PHRASE = [(0, 0.0, 0.7), (3, 0.75, 0.7), (7, 1.5, 0.7), (10, 2.25, 1.4), (8, 3.75, 0.7), (7, 4.5, 1.4)]  # semitone, start, length


def phrase(voice, root):
    y = np.zeros(int(T_STOP * SR))
    for rep in range(int(T_STOP // 6)):
        for st, t0, ln in PHRASE:
            n = int(ln * SR); i = int((rep * 6 + t0) * SR)
            if i + n > len(y): continue
            if voice == "low":
                x = onepole_lp(saw(hz(root + st), n, 60), 900) * env_ad(n, 0.01, ln * 1.5) * (1 - np.linspace(0, 1, n) ** 8)
            else:
                x = bell(hz(root + st), n)
            y[i:i + n] += x
    return y


def plucks():
    y = np.zeros(int(T_STOP * SR)); notes = [64, 71, 67, 76, 62, 69, 74, 66]
    t = 0.0; k = 0
    while t < T_STOP - 1.2:
        x = pluck(hz(notes[k % len(notes)])); i = int(t * SR); y[i:i + len(x)] += x
        t += [1.25, 1.75, 1.0, 2.0][k % 4]; k += 1   # gaps between the hits
    return y


def chords(bright):
    y = np.zeros(int(T_STOP * SR)); n = 4 * SR
    prog = [[60, 64, 67, 71, 74], [57, 60, 64, 67, 72], [53, 57, 60, 64, 69], [55, 59, 62, 67, 71]] if bright else \
           [[36, 43, 48, 51], [34, 41, 46, 49], [32, 39, 44, 48], [31, 38, 43, 46]]
    fade = np.minimum(1, np.minimum(np.arange(n), n - np.arange(n)) / (0.3 * SR))
    for c in range(int(T_STOP // 4)):
        x = np.zeros(n)
        for m in prog[c % 4]:
            x += saw(hz(m) * (1 + 0.002 * np.random.default_rng(m).standard_normal()), n, None if bright else 6)
        x = x if bright else onepole_lp(x, 400)
        y[c * n:(c + 1) * n] += x * fade
    return y


def voice():
    x, sr = sf.read(VOICE); x = x.mean(1)[: int(14.5 * sr)]
    return np.concatenate([x, x])[: int(T_STOP * SR)]


SOURCES = [("1_hits", "Plucked hits with gaps (a sharp attack, quick decay)", plucks),
           ("2_low_synth", "A phrase on a low synth (filtered saw, C2)", lambda: phrase("low", 36)),
           ("3_high_bell", "The same phrase on a high bell (FM, C6)", lambda: phrase("bell", 84)),
           ("4_voice", "A voice (spoken phrases)", voice),
           ("5_bright_chords", "Sustained chords, bright (open saws, mid register)", lambda: chords(True)),
           ("6_dark_chords", "Sustained chords, dark (low register, closed filter)", lambda: chords(False))]


def finish(y):
    f = int(0.5 * SR); y = y.copy(); y[-f:] *= np.linspace(1, 0, f)
    active = y[np.abs(y) > 1e-3]
    g = 10 ** (RMS_DB / 20) / np.sqrt(np.mean(active ** 2))
    g = min(g, 10 ** (PEAK_DB / 20) / np.abs(y).max())
    y = np.concatenate([y * g, np.zeros(int((LENGTH - T_STOP) * SR))])
    return np.stack([y, y], 1)


# ---- Renders ---------------------------------------------------------------------------------------------
def auto():
    pts = [(0, FIN0), (T_IN, FIN0), (T_IN + TURN, ENDLESS), (T_OUT, ENDLESS), (T_OUT + TURN, FIN1),
           (T_IN2, FIN1), (T_IN2 + TURN, ENDLESS), (T_OUT2, ENDLESS), (T_OUT2 + TURN, FIN1)]
    return {"breakpoints": [{"t": t, "key": "decay", "value": v} for t, v in pts]}


def render(mat, cross, out):
    e = dict(os.environ); e["IV_DECAY_CROSS"] = cross
    a = out + ".auto.json"; json.dump(auto(), open(a, "w"))
    subprocess.run([R, f"{PAGE}/_dry/{mat}.wav", out, "--set", "mix=1", "--set", f"space={SPACE}", "--set", f"size={SIZE}",
                    "--set", f"decay={FIN0}", "--auto", a, "--sidecar"], env=e, check=True, capture_output=True)
    os.remove(a)
    return sf.read(out)[0]


def levels(x, sec=0.05):
    m = x.mean(1); n = int(sec * SR); k = len(m) // n
    p = np.mean(m[:k * n].reshape(k, n) ** 2, axis=1)
    return 10 * np.log10(np.convolve(p, np.ones(4) / 4, "same") + 1e-14)


def hf_frames(x):
    y = np.diff(x.mean(1), n=2); n = SR // 1000; k = len(y) // n
    return np.mean(y[:k * n].reshape(k, n) ** 2, 1) + 1e-20


def click_score(e, t0):
    # the loudest 1 ms frame of the 2nd difference above the median of the second around it, over each turn's 6 s
    return max(10 * np.log10(e[i] / np.median(e[max(0, i - 500):i + 500])) for i in range(int(t0 * 1000) - 200, int(t0 * 1000) + 6000))


if __name__ == "__main__":
    if os.path.isdir(PAGE): shutil.rmtree(PAGE)
    os.makedirs(PAGE + "/_dry")
    lines = [f"{SPACE} SIZE {SIZE}; DECAY {FIN0} -> {ENDLESS} at {T_IN:.0f} s, -> {FIN1} at {T_OUT:.0f} s, input stops at "
             f"{T_STOP:.0f} s, -> {ENDLESS} at {T_IN2:.0f} s, -> {FIN1} at {T_OUT2:.0f} s (each turn {TURN} s)"]
    turns = [("into", T_IN), ("out", T_OUT), ("into tail", T_IN2), ("out tail", T_OUT2)]
    for mat, _, make in SOURCES:
        sf.write(f"{PAGE}/_dry/{mat}.wav", finish(make()), SR, subtype="PCM_24")
        outs = {tag: render(mat, cross, f"{PAGE}/{mat}_{tag}.wav") for tag, cross in VERS}
        lines.append(f"\n{mat}")
        for tag, _ in VERS:
            x = outs[tag]; L = levels(x); e = hf_frames(x); parts = []
            for name, t0 in turns:
                i0 = int(t0 / 0.05); at = L[i0 - 4:i0].mean()
                fall = min(L[j + 10] - L[j] for j in range(i0, i0 + 100))  # steepest 0.5 s fall in the 5 s after the turn
                parts.append(f"{name}: +1 s {L[i0 + 20] - at:+5.1f} / +3 s {L[i0 + 60] - at:+5.1f} dB, steepest 0.5 s {fall:+5.1f}, click {click_score(e, t0):4.1f}")
            lines.append(f"  {tag:8s} " + " | ".join(parts))
        a = outs["A_today"]; j = int(T_IN * SR)
        for tag, _ in VERS[1:]:
            b = outs[tag]
            lines.append(f"  {tag}: before the first turn {'bit-identical to A' if np.array_equal(a[:j], b[:j]) else 'DIFFERENT from A'}; "
                         f"after it {'IDENTICAL (mislabelled!)' if np.array_equal(a, b) else 'differs from A'}")
        tags = [t for t, _ in VERS]
        same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(outs[p], outs[q])]
        lines.append("  every pair of versions differs" if not same else f"  IDENTICAL PAIRS: {same}")
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), "decay_cross_README.txt"), PAGE + "/README.txt")
    print("\n".join(lines))

    # The page: make_review's layout, with one chapter bar for every card (the shared timeline) and the source's name.
    # DECAY moves on every card (the chapter bar says when); a "DECAY 0.75" chip (its last value) would mislead.
    for mat, _, _ in SOURCES:
        for tag, _ in VERS:
            j = f"{PAGE}/{mat}_{tag}.json"; d = json.load(open(j)); d["params"].pop("decay", None); json.dump(d, open(j, "w"), indent=2)
    sys.path.insert(0, os.path.join(REPO_, "tools/review"))
    import listen
    from pathlib import Path
    out_path = Path(PAGE) / "index.html"
    items, source, name = listen.collect(Path(PAGE), with_spec=True)
    data = listen.build_page_data(Path(PAGE), out_path, items, [], rows=None, columns="none", variants=None,
                                  title="DECAY across 3 o'clock", level_match=False, dry_dir=PAGE + "/_dry")
    names = {m: d for m, d, _ in SOURCES}
    for row in data["rows"]:
        row["marks"] = [[n, t] for n, t in CHAPTERS]
        row["loud"] = [T_IN, T_OUT, T_IN2, T_OUT2]
        if row.get("material") in names: row["name"] = names[row["material"]]
    data["choices"] = True
    listen.add_hidden_twin(data)
    data["wetOnly"] = True
    listen.write_audio_sidecars(data, out_path.parent)
    out_path.write_text(listen.render_html(data), encoding="utf-8")
    print("Wrote", out_path, listen.describe(data))
