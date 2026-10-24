#!/usr/bin/env python3
"""Summarises soak logs (tools/soak/soak.cpp): coverage, findings by type, hold and decay stats.

    python3 tools/soak/analyse.py 'build-wt/soak/run1/*.jsonl' [examples per type]
"""
import collections
import glob
import json
import sys


def main():
    files = sorted(glob.glob(sys.argv[1]))
    show = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    counts = collections.Counter()
    examples = collections.defaultdict(list)
    audio = wall = 0.0
    runs = 0
    rates = collections.Counter()
    blocks = collections.Counter()
    episodes = collections.Counter()
    holds = []
    decays = []
    sub_mem = 0
    spikes = 0
    for f in files:
        for line in open(f):
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            k = d["kind"]
            if k == "finding":
                key = (d["type"], d["episode_kind"])
                counts[key] += 1
                examples[key].append(d)
            elif k == "stat" and d["type"] == "hold":
                holds.append(d)
            elif k == "stat" and d["type"] == "decay":
                decays.append(d)
            elif k == "end":
                runs += 1
                audio += d["audio_s"]
                wall += d["wall_s"]
                rates[d["rate"]] += d["audio_s"] / 3600
                blocks["random" if d["block"] == 0 else d["block"]] += 1
                for e in ("chaos", "hold", "decay", "gesture", "flip"):
                    episodes[e] += d[e]
                sub_mem = max(sub_mem, d["subnormal_mem_max"])
                spikes += d["timing_spikes"]
    print(f"runs {runs}, audio {audio / 3600:.1f} h, wall {wall / 3600:.2f} CPU-h")
    print("hours by rate", {r: round(h, 1) for r, h in sorted(rates.items())})
    print("runs by block size", dict(sorted(blocks.items(), key=lambda kv: str(kv[0]))))
    print("episodes", dict(episodes))
    print(f"max subnormal floats in memory {sub_mem}, timing spikes {spikes}")
    print("findings (type, episode kind): count (first-few-per-episode logged)")
    for key, n in sorted(counts.items()):
        print("  ", key, n)
    for key, lst in sorted(examples.items()):
        if key[0].startswith("subnormal"):
            continue
        for d in lst[:show]:
            print("   ex", key[0], f"seed {d['seed']} rate {d['rate']} block {d['block']} sample {d['sample']}:", d["detail"], "|", d["stim"], json.dumps(d["settings"]))
    if holds:
        by = collections.defaultdict(list)
        for h in holds:
            sp = ["PLATE", "HALL", "MASSIVE"][h["settings"]["space_now"]]
            by[(sp, "freeze" if h["freeze"] else "infinite")].append(h)
        print("hold episodes: span of the 4 s level after settling (dB)")
        for key, lst in sorted(by.items()):
            ups = sorted(x["max_up_db"] for x in lst)
            downs = sorted(x["max_down_db"] for x in lst)
            within = sum(1 for x in lst if x["max_up_db"] <= 0.5 and x["max_down_db"] <= 0.5)
            print(f"   {key}: n={len(lst)} up median {ups[len(ups) // 2]:.2f} max {ups[-1]:.2f}; down median {downs[len(downs) // 2]:.2f} max {downs[-1]:.2f}; within ±0.5 dB {within}/{len(lst)}")
    if decays:
        silent = [x for x in decays if x["expect_silent"]]
        worst = max(decays, key=lambda x: x["worst_rise_db"])
        loudest_end = max(silent, key=lambda x: x["end_db"]) if silent else None
        print(f"decay episodes {len(decays)}, worst rise {worst['worst_rise_db']:.2f} dB, loudest end (of {len(silent)} expected silent) "
              f"{loudest_end['end_db'] if loudest_end else 'n/a'} dB")


if __name__ == "__main__":
    main()
