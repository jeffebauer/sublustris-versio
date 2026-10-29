#!/usr/bin/env python3
"""Player-facing panel diagram for the Sublustris Versio minisite.

Adapted from Resilio Versio's docs/minisite/tools/make_site_panel_svg.py (MIT,
the same author). The geometry is Noise Engineering's official Versio panel
template, to scale, 1 unit = 1 mm, read from docs/minisite/panel/
versio_panel_coordinates.csv (copied from Resilio's docs/panel/). Labelled
with the panel names only (BLEND ... DEPTH, SPACE, ONSET, FREEZE / PAINT, the
LED meters). Colours are CSS custom properties with fallbacks, so the website
can theme it (light / dark) without editing it.

Usage (from the repo root):
  python3 docs/minisite/tools/make_site_panel_svg.py [out.svg]
Default output: docs/minisite/assets/panel/sublustris-versio-panel.svg
"""
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CSV = ROOT / "docs/minisite/panel/versio_panel_coordinates.csv"
PANEL_W = 50.5   # mm (10 HP, as cut)
PANEL_H = 128.5  # mm (3U)

# SPEC §3: P1-P7 in reading order; SW1 (top) is SPACE, SW2 (bottom) ONSET.
KNOBS = {"P1": "BLEND", "P2": "DECAY", "P3": "TONE", "P4": "WEAR",
         "P5": "SIZE", "P6": "MOTION", "P7": "DEPTH"}
TOGGLES = {"SW1": ("SPACE", "PLATE · HALL · MASSIVE"), "SW2": ("ONSET", "NATURAL · BLOOM · SHIMMER")}
LEDS = {"LED1": "IN L", "LED2": "IN R", "LED3": "OUT L", "LED4": "OUT R"}
SCALE = 8  # px per mm for the default rendered size


def f(v):
    return f"{v:.3f}".rstrip("0").rstrip(".")


def parts():
    with CSV.open() as fh:
        for row in csv.DictReader(fh):
            yield (row["id"], row["kind"], float(row["x_mm_from_left"]),
                   float(row["y_mm_from_top"]), float(row["hole_diameter_mm"]))


def build():
    o = []
    a = o.append
    a('<?xml version="1.0" encoding="UTF-8"?>')
    a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{f(PANEL_W * SCALE)}" height="{f(PANEL_H * SCALE)}" '
      f'viewBox="0 0 {f(PANEL_W)} {f(PANEL_H)}" role="img" aria-labelledby="sv-title sv-desc">')
    a('  <title id="sv-title">Sublustris Versio panel map</title>')
    a('  <desc id="sv-desc">The Noise Engineering Versio front panel with Sublustris Versio\'s controls. '
      'Top row: BLEND (left knob) and DECAY (right knob), with four LEDs between them: input left, '
      'input right, output left, output right. Below, in the centre: TONE. Next row: WEAR (left) '
      'and SIZE (right). Centre: MOTION. Right, lower: DEPTH. Left: two three-way switches, '
      'SPACE (PLATE, HALL, MASSIVE) above ONSET (NATURAL, BLOOM, SHIMMER). Centre, lower: the '
      'FREEZE / PAINT button. Bottom: twelve jacks, labelled as printed on the module (seven CV '
      'inputs, one per knob, the gate input, audio in L and R, audio out L and R).</desc>')
    a('  <style>'
      'text{font-family:var(--sv-font,Helvetica,Arial,sans-serif)}'
      '.panel{fill:var(--sv-panel,#f4f2ee);stroke:var(--sv-ink,#1a1a1a);stroke-width:0.25}'
      '.knob{fill:var(--sv-knob,#2b2b2b)}'
      '.knobmark{stroke:var(--sv-panel,#f4f2ee);stroke-width:0.45;stroke-linecap:round}'
      '.name{font-size:2.5px;font-weight:700;fill:var(--sv-ink,#1a1a1a);letter-spacing:0.08px}'
      '.pos{font-size:1.1px;fill:var(--sv-ink-soft,#555)}'
      '.ledlbl{font-size:1.25px;font-weight:700;fill:var(--sv-ink-soft,#555)}'
      '.led{fill:var(--sv-led,#7bc86c);stroke:var(--sv-ink,#1a1a1a);stroke-width:0.12}'
      '.tog{fill:var(--sv-metal,#cfcfcf);stroke:var(--sv-ink,#333);stroke-width:0.15}'
      '.btn{fill:var(--sv-accent,#3b5bdb);stroke:var(--sv-ink,#333);stroke-width:0.15}'
      '.jack{fill:var(--sv-metal,#d9d9d9);stroke:var(--sv-ink,#333);stroke-width:0.15}'
      '.jackhole{fill:var(--sv-ink,#1a1a1a)}'
      '.screw{fill:none;stroke:var(--sv-ink-soft,#999);stroke-width:0.12}'
      '.note{font-size:1.35px;fill:var(--sv-ink-soft,#555)}'
      '.brand{font-size:2.9px;font-weight:800;fill:var(--sv-ink,#1a1a1a);letter-spacing:0.3px}'
      '</style>')
    a(f'  <rect class="panel" x="0" y="0" width="{f(PANEL_W)}" height="{f(PANEL_H)}" rx="0.6"/>')
    a(f'  <text class="brand" x="{f(PANEL_W / 2)}" y="8.4" text-anchor="middle">SUBLUSTRIS</text>')
    for pid, kind, cx, cy, d in parts():
        if kind == "mounting":
            a(f'  <circle class="screw" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2)}"/>')
        elif kind == "pot":
            r = d / 2 + 1.3
            a(f'  <g id="{KNOBS[pid].lower()}"><circle class="knob" cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}"/>')
            a(f'    <line class="knobmark" x1="{f(cx)}" y1="{f(cy - r + 0.9)}" x2="{f(cx)}" y2="{f(cy - r * 0.35)}"/>')
            if pid in ("P6", "P7"):  # names above: MOTION (switches below), DEPTH (the button's label)
                a(f'    <text class="name" x="{f(cx)}" y="{f(cy - r - 1.2)}" text-anchor="middle">{KNOBS[pid]}</text></g>')
            else:
                a(f'    <text class="name" x="{f(cx)}" y="{f(cy + r + 3.0)}" text-anchor="middle">{KNOBS[pid]}</text></g>')
        elif kind == "toggle":
            name, pos = TOGGLES[pid]
            a(f'  <g id="{name.lower()}"><circle class="tog" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2)}"/>')
            # Name beside the switch, its positions under it.
            a(f'    <text class="name" x="{f(cx + d / 2 + 0.9)}" y="{f(cy + 0.8)}" style="font-size:2.1px">{name}</text>')
            a(f'    <text class="pos" x="2.2" y="{f(cy + d / 2 + 2.0)}">{pos}</text></g>')
        elif kind == "button":
            a(f'  <g id="freeze-paint"><circle class="btn" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2)}"/>')
            a(f'    <text class="name" x="{f(cx)}" y="{f(cy + d / 2 + 2.6)}" text-anchor="middle" '
              f'style="font-size:2.1px">FREEZE / PAINT</text></g>')
        elif kind == "led":
            a(f'  <circle class="led" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2)}"/>')
            dy = 4.9 if pid in ("LED2", "LED4") else 3.0  # staggered: the LEDs are 5 mm apart
            a(f'  <text class="ledlbl" x="{f(cx)}" y="{f(cy - dy)}" text-anchor="middle">{LEDS[pid]}</text>')
        elif kind == "jack":
            a(f'  <circle class="jack" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2 + 0.9)}"/>')
            a(f'  <circle class="jackhole" cx="{f(cx)}" cy="{f(cy)}" r="{f(d / 2 - 1.2)}"/>')
    y = 117.6  # below the jacks
    for line in ("Jacks as printed on your module: each knob's",
                 "CV input follows its knob; gate, In L/R, Out L/R."):
        a(f'  <text class="note" x="{f(PANEL_W / 2)}" y="{f(y)}" text-anchor="middle">{line}</text>')
        y += 1.8
    a(f'  <text class="note" x="{f(PANEL_W / 2)}" y="{f(PANEL_H - 7.0)}" text-anchor="middle" '
      f'style="font-size:1.1px">Panel geometry: Noise Engineering Versio template</text>')
    a('</svg>')
    return "\n".join(o) + "\n"


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs/minisite/assets/panel/sublustris-versio-panel.svg"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(build())
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
