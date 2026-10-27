#!/usr/bin/env python3
# Renders at its original commit: build iv_render at bbd2635 (IV_DRIFT_* are ignored since v1.0.46).
"""PROTOTYPE pick page (26 Oct 2026; the owner: "In a realistic setting amongst a broader mix, the extreme
ends of the modulation knob don't create obvious results ... we should make them stronger to at least be
more noticeable in a mix", and of the right side's per-line movement: "I wonder if we should consider a more
straight sinewave-based modulation for the right side instead to juxtapose the more random modulation on the
left side of the knob, like we did with Resilio Versio"). Renderer hooks (map::driftAmountsPrototype):

  A today
  B stronger extremes: depth x1.4 and the wow's (and Wander's) rate x1.2 at each end (IV_DRIFT_GROW=0.4, IV_DRIFT_RATE_GROW=0.2)
  C strongest extremes: depth x1.8, wow rate x1.4 (IV_DRIFT_GROW=0.8, IV_DRIFT_RATE_GROW=0.4)
  D a sine right side, 1.5 -> 5.5 Hz, 16 cents a line at full (IV_DRIFT_SINE=1, IV_DRIFT_SINE_CENTS=16)
  E a slower sine right side, 1 -> 2 Hz, 14 cents (IV_DRIFT_SINE=2, IV_DRIFT_SINE_CENTS=14)

B and C grow only over the last half of each side (x^2), so up to 3/4 they are today and aren't rendered
there. The sines: every line at the same rate (+-6 %), phases spread over 3/4 of a cycle (the plate's two
branches in quadrature, Dattorro), so the chord's centre pitch doesn't sway.

Usage: <main checkout>/.venv/bin/python tools/pages/drift_rethink.py [--render build-drift/iv_render] [--no-page]

Writes the main checkout's renders/drift_rethink: 100 % wet renders (--wet-only --dry), level-matched, a pick
page; _hooks.txt (each render's hook line), _verify.txt (labels, measured cents, nulls, levels) and
_clicks.txt (a DRIFT sweep per version, 1 ms frames).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, trim, db  # noqa: E402

SR = 48000
VERSIONS = {
    "A": ("A_today", {}),
    "B": ("B_stronger", {"IV_DRIFT_GROW": "0.4", "IV_DRIFT_RATE_GROW": "0.2"}),
    "C": ("C_strongest", {"IV_DRIFT_GROW": "0.8", "IV_DRIFT_RATE_GROW": "0.4"}),
    "D": ("D_sine_fast", {"IV_DRIFT_SINE": "1", "IV_DRIFT_SINE_CENTS": "16"}),
    "E": ("E_sine_slow", {"IV_DRIFT_SINE": "2", "IV_DRIFT_SINE_CENTS": "14"}),
}
COLS = [("1_full_left", 0.0, "ABC"), ("2_three_quarter_right", 0.75, "ADE"), ("3_full_right", 1.0, "ABCDE")]
SPACES = {"hall": ["space=HALL", "size=0.6"], "massive": ["space=MASSIVE", "size=0.7"],
          "plate": ["space=PLATE", "size=0.6"]}
COMMON = ["decay=0.7", "era=0", "attack=NORMAL", "time=0"]
README = """DRIFT, stronger at its ends, and a sine on its right side. Judge it in the mix first.

Your notes: in a mix, DRIFT's ends don't do anything obvious, and on the right side the lines' movement reads only as faint panning. You suggested a sine on the right side, as on Resilio Versio.

A is today. B and C make both ends stronger: deeper movement (1.4x and 1.8x), and its wandering a little quicker (1.2x and 1.4x; the left side's fast flutter keeps its speed). They change only the last half of each side, so up to 3/4 the knob is still today's. That's why the 3/4-right column has no B or C.

D and E swap the right side's random Wander for a steady sine vibrato. D runs at 1.5 to 5.5 Hz, in the spirit of Resilio's Warble. E is slower, 1 to 2 Hz, more of a gentle chorus sway. On both, every delay line moves at the same speed but at a different point in the cycle. So the tail shimmers regularly while the chord as a whole stays in tune: the "seasick" sway of the old right side came from all the lines bending together.

What to listen for:
- In the mix rows (mix_dub, mix_keys): can you hear DRIFT at all with A? Does B or C make the end of the knob obvious without the chords sounding out of tune?
- On the right, D and E against A: is a regular vibrato easier to hear in the mix than the random movement? Does it sound like a different effect from the left side, rather than more of the same?
- The solo chord shows the movement bare. If C, D or E feels seasick there but right in the mix, say so: the mix is the main judge.

Rows: three spaces (HALL SIZE 0.6, MASSIVE 0.7, PLATE 0.6; DECAY 0.7; Neve), each with three materials:
- mix_dub: the dub track, with only its stabs and rims sent to the reverb.
- mix_keys: drums and bass dry, electric piano chords sent to the reverb.
- solo_chord: a held pad chord on its own.

Columns: DRIFT full left, 3/4 right and full right.

The renders are 100 % wet and level-matched; use the MIX fader to blend in the dry.

A_today        today
B_stronger     both ends stronger: depth x1.4, wandering 1.2x quicker
C_strongest    both ends much stronger: depth x1.8, wandering 1.4x quicker
D_sine_fast    the right side a sine vibrato, 1.5 to 5.5 Hz
E_sine_slow    the right side a slower sine, 1 to 2 Hz"""


def mixes(material: Path, dry: Path):
    """mix_keys: drums and bass dry, the keys both dry and (pushed 6 dB, a dub send) into the reverb."""
    if (dry / "_keysmix_full.wav").exists():
        return
    rd = lambda n: sf.read(material / (n + ".wav"), always_2d=True)[0]
    d, b, k = rd("drums"), rd("bass"), rd("keys")
    n = min(len(d), len(b), len(k))
    mix = 0.5 * d[:n] + 0.4 * b[:n] + 0.5 * k[:n]
    g = 0.5 / np.max(np.abs(mix))  # peaks at -6 dBFS
    sf.write(dry / "_keysmix_full.wav", g * mix, SR, subtype="PCM_24")
    sf.write(dry / "_keysmix_send_full.wav", 2.0 * g * 0.5 * k[:n], SR, subtype="PCM_24")


def render(binary, src, out, sets, env_extra, hooks=None, auto=None):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar"] + (["--auto", str(auto)] if auto else [])
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    line = p.stderr.strip().replace("\n", " | ") or "(no hook line)"
    if hooks is not None:
        hooks.append("{}/{}/{}  env {}  ->  {}".format(out.parent.parent.name, out.parent.name, out.name, env_extra or "{}", line))
    return line


def pitch_p95(binary, tone, tmp, sets, env):
    """p95 cents (and peak) of a held 1 kHz in the tail, 3-8 s (tools/pitch_track.py)."""
    out = tmp / ("pt_" + "_".join(s.split("=")[1] for s in sets) + "_" + "_".join(env.values()) + ".wav")
    render(binary, tone, out, sets, env)
    j = tmp / (out.stem + ".json")
    subprocess.run([sys.executable, str(ROOT / "tools/pitch_track.py"), str(out), "--start-s", "3", "--end-s", "8",
                    "--json", str(j)], check=True, capture_output=True)
    r = json.loads(j.read_text())
    r = r[0] if isinstance(r, list) else r
    out.unlink()
    out.with_suffix(".json").unlink(missing_ok=True)
    return r["p95_cents"]


def hf_frames(x, frame_s=0.001, lo=5000.0):
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / SR)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * SR)
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1)


def click_peak(path, t0, t1):
    """The loudest 1 ms frame above 5 kHz against the median of the 0.5 s around it, in dB, over t0..t1."""
    x, _ = sf.read(path, always_2d=True)
    e = hf_frames(x)
    worst = -99.0
    for i in range(int(t0 * 1000), min(len(e), int(t1 * 1000))):
        ref = np.median(e[max(0, i - 250): i + 250]) + 1e-20
        worst = max(worst, db(e[i] / ref))
    return worst


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-drift" / "iv_render")
    ap.add_argument("--no-page", action="store_true", help="skip the renders and page; only the checks")
    a = ap.parse_args()
    binary = a.render.resolve()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "drift_rethink"
    dry = main_root / "renders" / "_material_drift_rethink"
    dry.mkdir(parents=True, exist_ok=True)
    mixes(material, dry)
    # name: (dry the page plays, what the reverb gets)
    inputs = {"mix_dub": ("fullmix", "fullmix_send"), "mix_keys": ("_keysmix_full", "_keysmix_send_full"),
              "solo_chord": ("pad_held", "pad_held")}
    srcs = {}
    for m, (d, s) in inputs.items():
        play, tail = (17, 8) if m != "solo_chord" else (12, 9)
        base = material if (material / (d + ".wav")).exists() else dry
        trim(base / (d + ".wav"), dry / (m + ".wav"), play, tail)
        trim(base / (s + ".wav"), dry / "_send" / (m + ".wav"), play, tail)
        srcs[m] = dry / "_send" / (m + ".wav")

    if not a.no_page:
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        hooks, jobs = [], []
        for col, drift, letters in COLS:
            for sp, sets in SPACES.items():
                for L in letters:
                    tag, env = VERSIONS[L]
                    for m in inputs:
                        jobs.append((srcs[m], out / col / sp / "{}_{}.wav".format(m, tag),
                                     sets + COMMON + ["drift={}".format(drift)], env))
        with ThreadPoolExecutor(8) as ex:
            list(ex.map(lambda j: render(binary, j[0], j[1], j[2], j[3], hooks), jobs))
        (out / "_hooks.txt").write_text("\n".join(sorted(hooks)) + "\n")
    out.mkdir(parents=True, exist_ok=True)

    # Labels and depths: a held 1 kHz per version, space and column, and every render against A.
    tmp = out / "_tmp"
    tmp.mkdir(exist_ok=True)
    tone = tmp / "tone1k.wav"
    t = np.arange(int(9 * SR)) / SR
    x = 0.25 * np.sin(2 * np.pi * 1000 * t)
    x[-2400:] *= np.linspace(1, 0, 2400)
    sf.write(tone, np.stack([x, x], 1), SR, subtype="PCM_24")
    cases = [(col, drift, sp, L) for col, drift, letters in COLS for sp in SPACES for L in letters]
    with ThreadPoolExecutor(8) as ex:
        cents = dict(zip(cases, ex.map(lambda c: pitch_p95(binary, tone, tmp, SPACES[c[2]] + COMMON + [
            "drift={}".format(c[1])], VERSIONS[c[3]][1]), cases)))
    lines = ["Held 1 kHz, p95 pitch deviation in the tail (3-8 s), cents; then each card's renders against A "
             "(null = the difference's level under A; level B-A after level-matching is undone by the page)."]
    for col, drift, letters in COLS:
        lines.append("\n{} (DRIFT {}):".format(col, drift))
        for sp in SPACES:
            lines.append("  {:8s} ".format(sp) + "  ".join("{} {:5.1f} c".format(VERSIONS[L][0], cents[(col, drift, sp, L)])
                                                       for L in letters))
    if not a.no_page:
        lines.append("")
        for col, drift, letters in COLS:
            for sp in SPACES:
                for m in inputs:
                    ref, _ = sf.read(out / col / sp / "{}_{}.wav".format(m, VERSIONS["A"][0]), always_2d=True)
                    cells = []
                    for L in letters[1:]:
                        y, _ = sf.read(out / col / sp / "{}_{}.wav".format(m, VERSIONS[L][0]), always_2d=True)
                        n = min(len(ref), len(y))
                        if np.array_equal(ref[:n], y[:n]):
                            cells.append("{} IDENTICAL to A (mislabelled!)".format(L))
                        else:
                            cells.append("{} null {:.1f} dB, level {:+.2f} dB".format(
                                L, db(np.mean((ref[:n] - y[:n]) ** 2) / np.mean(ref[:n] ** 2)),
                                db(np.mean(y[:n] ** 2)) - db(np.mean(ref[:n] ** 2))))
                    lines.append("{:22s} {:8s} {:8s} {}".format(col, sp, m, "; ".join(cells)))
    (out / "_verify.txt").write_text("\n".join(lines) + "\n")

    # Clicks: DRIFT swept 0 -> 1 over 16 s, then 1 -> 0.5 in 0.2 s (a flick), on the held chord.
    auto = tmp / "drift_sweep.json"
    auto.write_text(json.dumps({"breakpoints": [{"t": 1.0, "key": "drift", "value": 0.0},
                                                {"t": 17.0, "key": "drift", "value": 1.0},
                                                {"t": 18.0, "key": "drift", "value": 1.0},
                                                {"t": 18.2, "key": "drift", "value": 0.5}]}))
    clicks = ["DRIFT swept 0 -> 1 over 1-17 s, then flicked 1 -> 0.5 in 0.2 s at 18 s, on the held chord: the "
              "loudest 1 ms frame above 5 kHz against the median of the half second around it (1-20 s). "
              "Today's is the yardstick: the music's own grain reads the same."]
    cjobs = [(sp, L) for sp in SPACES for L in VERSIONS]

    def cjob(c):
        sp, L = c
        p = tmp / "click_{}_{}.wav".format(sp, L)
        render(binary, srcs["solo_chord"], p, SPACES[sp] + COMMON + ["drift=0"], VERSIONS[L][1], auto=auto)
        v = click_peak(p, 1.0, 20.0)
        p.unlink()
        return c, v
    with ThreadPoolExecutor(8) as ex:
        res = dict(ex.map(cjob, cjobs))
    for sp in SPACES:
        clicks.append("  {:8s} ".format(sp) + "  ".join("{} {:+.1f} dB".format(VERSIONS[L][0], res[(sp, L)])
                                                     for L in VERSIONS))
    (out / "_clicks.txt").write_text("\n".join(clicks) + "\n")
    shutil.rmtree(tmp)

    if not a.no_page:
        (out / "README.txt").write_text(README + "\n")
        subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                        "DRIFT: stronger ends, and a sine right side", "--level-match", "--dry", str(dry),
                        "--wet-only"], check=True)
    print(out / "index.html")


if __name__ == "__main__":
    main()
