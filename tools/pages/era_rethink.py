#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; the owner on ERA: keep the grit they like, "the reason I like the tape
saturation", the radio's "phasing and the band-pass like tone", without ERA darkening the tail (TONE's job)
or moving its pitch (DRIFT's), within the module's CPU and flash). Three layouts of ERA against today's
(v1.0.40), behind the renderer hook IV_ERA_LAYOUT=A|B|C (core/params/Mappings.h eraLayoutMix,
core/dsp/EraTexture.h; docs/research/era-rethink.md):

  A  grit path: separate characters in quarters, mild -> gritty (tape, Midiverb, BBD, radio)
  B  saturation clean -> driven over the first half, then texture on top (BBD, grain), the radio's band last
  C  one wear amount: the whole chain in from the start, deepening together

Usage: <main checkout>/.venv/bin/python tools/pages/era_rethink.py [--render build-era/iv_render]

Cards: HALL and MASSIVE at ERA 0.2, 0.4, 0.6, 0.8, 1.0, PLATE at 0.7, and ERA swept 0 -> 1 over 16 s in
MASSIVE. Material: the dub mix (the reverb on its send: stabs and rims; the page's dry is the whole mix) and
a solo chord then two snare hits (the grit on transients). 100 % wet (--wet-only --dry), level-matched, a
pick page, in the main checkout's renders/era_rethink. _hooks.txt: each render's hook line; _verify.txt:
the label check (every version differs from today and from the others; its hook line names its layout),
LUFS against today at the same card, the top's share against ERA 0 (no darkening), and a 1 ms-frame click
check across ERA sweeps of a held pad and the mix in HALL and MASSIVE, every layout.
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
CC0 = Path("/Users/jesse/Documents/Sites/resilio-versio/test_audio/demo_sources/cc0")
HALL = ["space=HALL", "size=0.6", "decay=0.6"]
MASSIVE = ["space=MASSIVE", "size=0.7", "decay=0.6"]
PLATE = ["space=PLATE", "size=0.5", "decay=0.6"]
COMMON = ["attack=NORMAL", "time=0", "drift=0.5", "tone=0.5"]
POINTS = (0.2, 0.4, 0.6, 0.8, 1.0)
SWEEP = 16.0  # seconds, ERA 0 -> 1
COLS = ([("{:02d}_hall_era_{:.1f}".format(i + 1, e), HALL, e, None) for i, e in enumerate(POINTS)]
        + [("{:02d}_massive_era_{:.1f}".format(i + 6, e), MASSIVE, e, None) for i, e in enumerate(POINTS)]
        + [("11_plate_era_0.7", PLATE, 0.7, None), ("12_massive_era_sweep", MASSIVE, 0.0, SWEEP)])
VERSIONS = [("A_grit_path", {"IV_ERA_LAYOUT": "A"}), ("B_saturation_then_texture", {"IV_ERA_LAYOUT": "B"}),
            ("C_one_wear_amount", {"IV_ERA_LAYOUT": "C"}), ("T_today_v1.0.40", {})]
# What each layout puts at each point (Mappings.h eraLayoutMix), for the README and the label check.
AT = {
    "A": {0.2: "tape, nearly fully driven", 0.4: "Midiverb, 12 bits", 0.6: "BBD breathing, its clock's grit coming in",
          0.7: "BBD breathing and clock grit", 0.8: "radio band and dip, mild", 1.0: "radio band and dip, deep"},
    "B": {0.2: "tape, lightly driven", 0.4: "tape, well driven", 0.6: "tape driven + BBD breathing and clock",
          0.7: "+ grain coming in", 0.8: "tape + BBD + grain, all full", 1.0: "all that + the radio's band and dip"},
    "C": {0.2: "every stage at a fifth", 0.4: "every stage at two fifths", 0.6: "every stage at three fifths",
          0.7: "every stage at 70 %", 0.8: "every stage at four fifths", 1.0: "every stage full"},
    "T": {0.2: "Neve -> Midiverb boundary", 0.4: "Midiverb -> BBD boundary", 0.6: "BBD -> cassette boundary",
          0.7: "cassette", 0.8: "cassette -> radio boundary", 1.0: "radio, gritty end"},
}
README = """ERA rethought as texture only. Your notes: keep the grit you like (the Midiverb's and the BBD's grit, the tape's saturation, the radio's phasing and band-pass tone), drop what TONE and DRIFT already do (darkening the tail, wobbling its pitch), and fit the module's tight CPU and flash. Three layouts, against today's ERA (T). Which layout do you want ERA to have?

All three start as today at ERA 0 (the Neve's warmth in the loop, the base), never darken past it, and never move the pitch (no cassette wow or flutter, no radio detune). The grit is on the reverb's output (constant CPU, it can't build up in the tail), except A's Midiverb, whose 13-bit grain lives in the loop as on the real unit, so a fading tail turns grainy and stops a little sooner.

A, grit path: four separate characters, one per quarter of the knob, mild to gritty, each sweet to gritty across its quarter. Tape (saturation where the highs squash first, a little compression, faint hiss) to 0.25, the Midiverb's grain to 0.5, the BBD to 0.75 (its compander breathing with your playing, then its clock's gritty, aliased top), the radio to the end (band-pass, over-modulation, the slow wandering dip). I put the Midiverb before the BBD: its 13-bit grain is the finest grit, the BBD's breathing noise and aliasing are rougher.

B, saturation, then texture on top: the tape goes from clean to driven over the first half and stays. On top of it the BBD's breathing and clock come in from 0.5 to 0.65, then a level-following digital grain from 0.65 to 0.8 (it stays the same share of the tail as it fades, rather than buzzing). The last fifth adds the radio's band-pass and dip. Each stage stays once in: a straight line for CV.

C, one wear amount: the whole chain (tape, compander, clock, grain, band-pass with the dip) comes in at the start and every stage deepens together as you turn up; the band-pass comes in more slowly so the low end keeps its weight longer.

Listen in the mix (column 'mix': the reverb is on the send, the stabs and rims, as in a dub mix) and on the solo chord and two snare hits (column 'hits': the grit on the transients). Card 12 sweeps ERA from 0 to 1 over 16 s in MASSIVE: listen for the hand-overs in A and today, and how evenly B and C grow. HALL SIZE 0.6, MASSIVE SIZE 0.7, PLATE SIZE 0.5, DECAY 0.6, ATTACK NORMAL, TONE and DRIFT at noon. Renders are 100 % wet: use the MIX fader for the dry. Level-matched (measured within 1 dB on music; the page also matches loudness).

{cards}

A_grit_path                separate characters, mild -> gritty
B_saturation_then_texture  tape across the first half, then texture on top
C_one_wear_amount          one chain, everything deepening together
T_today_v1.0.40            today's ERA: Neve, Midiverb, BBD, cassette, radio"""


def card_lines():
    out = []
    for col, sets, era, sweep in COLS:
        if sweep:
            out.append("{:24s} MASSIVE, ERA swept 0 -> 1 over 16 s (A's hand-overs at 4, 8, 12 s; B's stages at 8, 10.4, 12.8 s)".format(col))
        else:
            space = sets[0].split("=")[1]
            at = "; ".join("{} {}".format(k, AT[k][era]) for k in "ABCT")
            out.append("{:24s} {} ERA {:.1f}: {}".format(col, space, era, at))
    return "\n".join(out)


def make_material(dst: Path, material: Path):
    """mix.wav / mix_send.wav: the dub mix and its send (16.9 s + 5 s of tail); hits.wav: a solo chord
    (the keys' first, a rolled Dm9), then two snare rimshots, each with room to ring."""
    dst.mkdir(parents=True, exist_ok=True)
    trim(material / "fullmix.wav", dst / "mix.wav", 16.9, 5.0)
    trim(material / "fullmix_send.wav", dst / "mix_send.wav", 16.9, 5.0)
    if not (dst / "hits.wav").exists():
        keys, sr = sf.read(material / "keys.wav", always_2d=True)
        n = int(2.4 * sr)
        chord = keys[:n].copy()
        chord[-int(0.3 * sr):] *= np.linspace(1, 0, int(0.3 * sr))[:, None]
        snare, ssr = sf.read(CC0 / "virtuosity" / "snare_rim" / "snaremic_snare_rimshot_vl12.flac", always_2d=True)
        assert ssr == sr, ssr
        if snare.shape[1] == 1:
            snare = np.repeat(snare, 2, axis=1)
        out = np.zeros((int(13.0 * sr), 2))
        out[:n] += chord
        lvl = np.sqrt(np.mean(chord ** 2)) / np.sqrt(np.mean(snare[:int(0.2 * sr)] ** 2)) * 1.4
        for t in (5.0, 8.5):
            i = int(t * sr)
            m = min(len(snare), len(out) - i)
            out[i:i + m] += lvl * snare[:m]
        out *= 0.5 / np.abs(out).max()
        sf.write(dst / "hits.wav", out, sr, subtype="PCM_24")
    # The click check's held pad (not on the page): four chords swelling into each other.
    trim(material / "pad.wav", dst / "_pad.wav", 16.5, 2.0)


def render(binary, src, out, sets, env_extra, auto=None):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1"]
    if auto:
        args += ["--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    return "{}  env {}  ->  {}\n".format(out.name, env_extra or "{}", p.stderr.strip().replace("\n", " | ") or "(no hook line)")


def lufs(x):
    """BS.1770 K-weighted loudness (ungated), from the spectrum (Parseval)."""
    X = np.fft.rfft(x, axis=0)
    z = np.exp(-1j * np.linspace(0, np.pi, X.shape[0]))
    H = lambda b, a: (b[0] + b[1] * z + b[2] * z * z) / (1 + a[1] * z + a[2] * z * z)
    h = H([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585]) \
        * H([1, -2, 1], [1, -1.99004745483398, 0.99007225036621])
    p = np.sum(np.abs(X * h[:, None]) ** 2) / len(x) ** 2 * 2 / x.shape[1]
    return 10 * np.log10(p + 1e-30) - 0.691


def top_share(x, sr=SR, lo=5000.0):
    X = np.abs(np.fft.rfft(x.mean(axis=1))) ** 2
    f = np.fft.rfftfreq(len(x), 1.0 / sr)
    return db(X[f >= lo].sum() / X[f >= 100].sum())


def click_scan(x, sr=SR, frame_s=0.001, lo=5000.0):
    """The loudest 1 ms frame above 5 kHz against the median of the 0.5 s around it, in dB, over the
    sweep (its first and last 0.3 s left out). Music's own transients read up to ~15-20 dB on the mix."""
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * sr)
    m = len(h) // n
    e = (h[:m * n].reshape(m, n) ** 2).mean(axis=1) + 1e-20
    half = int(0.25 / frame_s)
    best, at = -99.0, 0.0
    for i in range(int(0.3 / frame_s), min(m - half, int(SWEEP / frame_s))):
        med = np.median(e[max(0, i - half):i + half])
        s = db(e[i] / med)
        if s > best:
            best, at = s, i * frame_s
    return best, at


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-era" / "iv_render")
    ap.add_argument("--jobs", type=int, default=8)
    a = ap.parse_args()
    main_root = main_checkout()
    out = main_root / "renders" / "era_rethink"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_era_rethink"
    make_material(dry, main_root / "renders" / "_material")
    binary = a.render.resolve()
    sweep_auto = out / "_era_sweep.json"
    sweep_auto.write_text(json.dumps({"breakpoints": [{"t": 0.0, "key": "era", "value": 0.0},
                                                      {"t": SWEEP, "key": "era", "value": 1.0}]}))
    jobs = []
    for col, sets, era, sweep in COLS:
        mats = ["mix"] if sweep else ["mix", "hits"]
        for tag, env in VERSIONS:
            for m in mats:
                src = dry / ("mix_send.wav" if m == "mix" else m + ".wav")
                jobs.append((src, out / col / "{}_{}.wav".format(m, tag), sets + COMMON + ["era={}".format(era)], env,
                             sweep_auto if sweep else None))
    # The click check's sweeps (not on the page): a held pad and the mix, HALL and MASSIVE, every version.
    clicks = out / "_clicks"
    for space, sets in (("hall", HALL), ("massive", MASSIVE)):
        for m, src in (("pad", dry / "_pad.wav"), ("mix", dry / "mix_send.wav")):
            for tag, env in VERSIONS:
                jobs.append((src, clicks / "{}_{}_{}.wav".format(space, m, tag), sets + COMMON + ["era=0"], env, sweep_auto))
    with ThreadPoolExecutor(a.jobs) as ex:
        lines = list(ex.map(lambda j: render(binary, *j), jobs))
    (out / "_hooks.txt").write_text("".join(lines))

    with open(out / "_verify.txt", "w") as v:
        v.write("era_rethink label check and levels. Each card, each material: every version's hook line, "
                "its null against today and the other layouts (IDENTICAL = mislabelled), its LUFS against today, "
                "and its share of energy above 5 kHz against ERA 0 of the same layout (negative = darker).\n\n")
        hooks = (out / "_hooks.txt").read_text().splitlines()
        for tag, env in VERSIONS:
            want = "ERA layout {}".format(env["IV_ERA_LAYOUT"]) if env else None
            mine = [h for h in hooks if "_{}.wav".format(tag) in h.split()[0]]
            ok = all((want in h) if want else ("IV_ERA_LAYOUT" not in h) for h in mine)
            v.write("hook lines for {:28s} {} ({} renders)\n".format(tag, "OK" if ok else "WRONG", len(mine)))
        v.write("\n")
        # ERA 0 per layout for the top's share: render once per version, HALL and MASSIVE, the mix.
        zero = {}
        for space, sets in (("HALL", HALL), ("MASSIVE", MASSIVE)):
            for tag, env in VERSIONS:
                p = out / "_zero_{}_{}.wav".format(space, tag)
                render(binary, dry / "mix_send.wav", p, sets + COMMON + ["era=0"], env)
                x, _ = sf.read(p, always_2d=True)
                zero[(space, tag)] = top_share(x)
                p.unlink()
        for col, sets, era, sweep in COLS:
            space = sets[0].split("=")[1]
            for m in (["mix"] if sweep else ["mix", "hits"]):
                xs = {tag: sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)[0] for tag, _ in VERSIONS}
                ref = xs[VERSIONS[-1][0]]
                parts = []
                for tag, _ in VERSIONS:
                    x = xs[tag]
                    same = [o[:1] for o, _ in VERSIONS if o != tag and np.array_equal(x, xs[o])]
                    null = db(np.mean((x - ref) ** 2) / np.mean(ref ** 2)) if tag != VERSIONS[-1][0] else 0.0
                    z = zero.get((space, tag))
                    top = "{:+.1f}".format(top_share(x) - z) if z is not None and m == "mix" else "  - "
                    parts.append("{} {}null {:+.0f} LU {:+.1f} top {}".format(
                        tag[:1], "IDENTICAL to " + ",".join(same) + " " if same else "", null, lufs(x) - lufs(ref), top))
                v.write("{:22s} {:5s} {}\n".format(col, m, " | ".join(parts)))
        v.write("\nClick check: ERA swept 0 -> 1 over 16 s; the loudest 1 ms frame above 5 kHz against the median of "
                "the half second around it (dB, and when). Today alongside; a click reads far above the music's own.\n")
        for space in ("hall", "massive"):
            for m in ("pad", "mix"):
                parts = []
                for tag, _ in VERSIONS:
                    x, _ = sf.read(clicks / "{}_{}_{}.wav".format(space, m, tag), always_2d=True)
                    s, t = click_scan(x)
                    parts.append("{} {:5.1f} dB at {:4.1f} s".format(tag[:1], s, t))
                v.write("{:8s} {:4s} {}\n".format(space, m, " | ".join(parts)))
    shutil.rmtree(clicks)
    (out / "README.txt").write_text(README.format(cards=card_lines()) + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "ERA: grit path, saturation then texture, or one wear amount", "--level-match",
                    "--dry", str(dry), "--wet-only"], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
