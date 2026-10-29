#!/usr/bin/env python3
"""Site demos: the owner's showcase listening page, from what ships.

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/make_page.py <work dir> <audio dir> <page dir>

Decodes each clip's MP3 pair (so the page plays exactly the site's files, with their shared
gain), writes <page dir>/<id>-<slug>_wet.wav + the Renderer's sidecar next to it and
<page dir>/_dry/<id>-<slug>.wav, a README.txt (the intro: each clip's caption, settings and
default BLEND), then tools/review/make_review.py --showcase --wet-only --dry (no picks, no
blind mode, notes only; Web Audio, sample-locked, the MIX fader blending the dry in live with
the module's equal-power law). <page dir> is rebuilt from scratch each run.
"""
import json
import shutil
import subprocess
import sys
from pathlib import Path

import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import render as R  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]


def main():
    work, audio, page = (Path(a) for a in sys.argv[1:4])
    if page.exists():
        shutil.rmtree(page)
    (page / "_dry").mkdir(parents=True)
    doc = json.loads((audio / "demos.json").read_text())
    lines = ["The eight site demos, in the site's order (1, 2, 3 first), playing the MP3s that ship "
             "(ffmpeg libmp3lame, 160 kbps), each pair at one gain so the clip sits at -20.5 LUFS at its own "
             "default BLEND. The MIX fader is one for the whole page and starts at noon: set it to a clip's "
             "default (listed here) to hear it as the site starts it. Notes only, no picks. The chips use the "
             "page tool's older names (ATTACK = ONSET, DRIFT = MOTION, ERA = WEAR, TIME = DEPTH, MIX = BLEND) and "
             "show a turned knob's end value (WEAR in 7, SIZE in 10)."]
    for e in doc["clips"]:
        stem = f"{e['id']}-{R.CLIPS[e['id']]['slug']}"
        for side, name in (("wet", e["file"]), ("dry", e["dry"])):
            x = R.decode(audio / name)
            out = page / f"{stem}_wet.wav" if side == "wet" else page / "_dry" / f"{stem}.wav"
            sf.write(str(out), x.astype("float32"), R.SR, subtype="FLOAT")
        side = json.loads((work / f"{e['id']}_wet.json").read_text())
        side["file"] = f"{stem}_wet.wav"
        (page / f"{stem}_wet.json").write_text(json.dumps(side))
        st = "; ".join(f"{k} {v}" for k, v in e["settings"].items())
        lines.append(f"{e['id']}  {e['title']} ({e['duration_s']:g} s, default BLEND {e['blend_clock']} = "
                     f"{round(e['blend'] * 100)} %): {e['caption']} {st}.")
    lines += ["", "wet    the clip, 100 % wet; the MIX fader blends its dry back in"]
    # Everything before the first blank line is the page's intro.
    (page / "README.txt").write_text("\n".join(lines) + "\n")
    # The dry inputs must sit outside the page folder's own scan (folders starting with "_" are skipped).
    subprocess.run([sys.executable, str(ROOT / "tools" / "review" / "make_review.py"), str(page),
                    "--title", "Sublustris site demos", "--no-level-match", "--showcase",
                    "--dry", str(page / "_dry"), "--wet-only"], check=True)
    print(page / "index.html")


if __name__ == "__main__":
    main()
