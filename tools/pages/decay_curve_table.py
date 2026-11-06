#!/usr/bin/env python3
"""renders/decay_curve: T60 at each hour of the DECAY knob, per version and SPACE (core/params/Mappings.h's decayRate,
massiveDecayRate and the prototype's decayCurveKnob, in Python). Hours: 7 o'clock = 0, each hour 0.1, 5 o'clock = 1.
HALL is at full SIZE (its nominal 0.8-60 s; a smaller SIZE scales every finite T60 by hallT60Scale), CANYON at SIZE 0, 0.5 and 1
(rows S0, S½, S1; the page's CANYON cards are at SIZE 0.5; its shortest tail follows SIZE: ~2.1 s at SIZE 0, 5 s at 0.5, 12 s at full). Run: python3 tools/pages/decay_curve_table.py
[--md | --html]
"""
import math
import sys

VERSIONS = {  # finite top f, endless from e, bend b (decayCurveKnob)
    "A": (0.80, 0.85, 0.0),
    "B": (0.92, 0.95, 0.0),
    "C": (0.92, 0.95, 0.328),
    "D": (0.95, 0.98, 0.395),
}
WHAT = {
    "A": "today: finite to 3 o'clock (0.80), crossover to 0.85, endless from about 3:30",
    "B": "today's curve stretched: finite to 0.92, crossover to 0.95, endless from 4:30",
    "C": "B with a bend: noon as today, more of the knob for the long tails; endless from 4:30",
    "D": "C's bend, endless only from 0.98 (about 4:50), crossover from 0.95",
}
RANGES = {"PLATE": (0.4, 30.0), "HALL": (0.8, 60.0), "CANYON": (2.0, 120.0)}
CANYON_SIZE = 0.5
HOURS = [("7", 0.0), ("8", 0.1), ("9", 0.2), ("10", 0.3), ("11", 0.4), ("12", 0.5), ("1", 0.6), ("2", 0.7),
         ("3", 0.8), ("3:30", 0.85), ("4", 0.9), ("4:30", 0.95), ("4:50", 0.98), ("5", 1.0)]


def knob(v, ver):
    f, e, b = VERSIONS[ver]
    if v <= f:
        x = v / f
        return 0.80 * (x + b * x * (1 - x))
    if v < e:
        return 0.80 + 0.05 * (v - f) / (e - f)
    return 0.85 + 0.15 * (v - e) / (1 - e)


def t60(v, ver, space, canyon_size=CANYON_SIZE):
    """Seconds; math.inf for endless. Past the finite top the tail keeps the top's T60 while you play (v1.0.61), and
    across the crossover the hold comes in once the input is quiet: shown as 'x' (crossover) with the top's T60."""
    u = knob(v, ver)
    lo, hi = RANGES[space]
    if space == "CANYON":
        lo = max(lo, 6.0 * 0.35 * (2.0 / 0.35) ** canyon_size)
    if u >= 0.85 - 1e-9:
        return math.inf
    if u > 0.80:
        return -hi  # the crossover: plays as the top, holds once quiet
    return lo * (hi / lo) ** (u / 0.80)


def fmt(t):
    if t == math.inf:
        return "endless"
    if t < 0:
        return f"cross ({-t:.0f})"
    return f"{t:.1f}" if t < 10 else f"{t:.0f}"


ROWSETS = [("PLATE", "PLATE", None), ("HALL", "HALL", None), ("CANYON S0", "CANYON", 0.0), ("CANYON S½", "CANYON", 0.5),
           ("CANYON S1", "CANYON", 1.0)]


def rows():
    out = []
    for label, space, size in ROWSETS:
        for ver in VERSIONS:
            out.append((label, ver, [fmt(t60(v, ver, space, size if size is not None else CANYON_SIZE)) for _, v in HOURS]))
    return out


def per_hour(space, size, ver="A"):
    """The T60 ratio per hour of knob over the finite part (today's exponential: constant)."""
    lo, hi = RANGES[space]
    lo = max(lo, 6.0 * 0.35 * (2.0 / 0.35) ** size) if space == "CANYON" else lo
    f = VERSIONS[ver][0]
    return (hi / lo) ** (0.1 / f)


def travel(ver, space, a, b):
    """Knob travel (0..1) over which the T60 goes from a to b seconds."""
    vs = [i / 10000 for i in range(10001)]
    ts = [t60(v, ver, space) for v in vs]
    inside = [v for v, t in zip(vs, ts) if 0 < t != math.inf and a <= t <= b]
    return (max(inside) - min(inside)) if inside else 0.0


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "--md"
    hdr = ["SPACE", "ver"] + [h for h, _ in HOURS]
    if mode == "--html":
        print("<table class='t60'><thead><tr>" + "".join(f"<th>{h}</th>" for h in hdr) + "</tr></thead><tbody>")
        for space, ver, cells in rows():
            print(f"<tr class='v{ver}'><td>{space}</td><td>{ver}</td>" + "".join(f"<td>{c}</td>" for c in cells) + "</tr>")
        print("</tbody></table>")
    else:
        print("| " + " | ".join(hdr) + " |")
        print("|" + "---|" * len(hdr))
        for space, ver, cells in rows():
            print(f"| {space} | {ver} | " + " | ".join(cells) + " |")
        print()
        for space, (a, b) in (("HALL", (15, 60)), ("CANYON", (30, 120)), ("PLATE", (8, 30))):
            print(f"{space} {a}-{b} s takes: " + ", ".join(f"{v} {travel(v, space, a, b) * 300:.0f} deg" for v in VERSIONS))
        for size in (0.0, 0.5, 1.0):
            print(f"CANYON SIZE {size}: shortest {t60(0, 'A', 'CANYON', size):.1f} s; x per hour " + ", ".join(f"{v} {per_hour('CANYON', size, v):.2f}" for v in VERSIONS) + "; flat hours 0")
        print("endless zone: " + ", ".join(f"{v} {(1 - VERSIONS[v][1]) * 300:.0f} deg" for v in VERSIONS))
