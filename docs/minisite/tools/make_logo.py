#!/usr/bin/env python3
"""Make the Sublustris Versio logos (docs/minisite/assets/logo/), in Resilio Versio's logo construction.

The family resemblance is intended (BRIEF section 10): the letters are Resilio's, drawn on its grid
(8 units wide per stroke, 40 units tall, 8 units between letters, 24 between words). R, E, S, I, L,
O and V are taken unchanged from the owner's Resilio logo (read from the sibling repo, path below).
U, B and T don't occur in "Resilio Versio", so they are built here the same way: rounded strokes
(r = 3) that overlap at square joints and touch corner to corner at the stepped joints, where a
small fillet (r = 3) bridges them, as in Resilio's O, S and V.

Run from the repo root:  python3 docs/minisite/tools/make_logo.py
Stdlib only. Writes sublustris-versio-logo.svg (the full name) and sublustris-logo.svg (phones).
"""
import re
from pathlib import Path

RESILIO = Path.home() / 'Documents/Sites/resilio-versio/docs/minisite/assets/logo/resilio-versio-logo.svg'
OUT = Path(__file__).resolve().parents[1] / 'assets' / 'logo'

R = 3  # corner radius, stroke ends and stepped-joint fillets

# Resilio's glyphs: (letter, x of its left edge in resilio-versio-logo.svg, width).
RESILIO_GLYPHS = {'R': (0, 40), 'E': (48, 40), 'S': (96, 40), 'I': (144, 8), 'L': (160, 36), 'O': (220, 44), 'V': (288, 40)}

# New glyphs: strokes as rectangles (x0, y0, x1, y1) and the corner-to-corner joints (x, y, dx, dy):
# dx, dy point at one of the two EMPTY notches beside the joint (the other is opposite).
NEW_GLYPHS = {
    'U': (44, [(0, 0, 8, 32), (36, 0, 44, 32), (8, 32, 36, 40)], [(8, 32, 1, -1), (36, 32, -1, -1)]),
    'B': (40, [(0, 0, 8, 40), (0, 0, 32, 8), (32, 8, 40, 16), (0, 16, 32, 24), (32, 24, 40, 32), (0, 32, 32, 40)],
          [(32, 8, 1, -1), (32, 16, 1, 1), (32, 24, 1, -1), (32, 32, 1, 1)]),
    'T': (40, [(0, 0, 40, 8), (16, 0, 24, 40)], []),
}


def resilio_paths():
    """Each glyph's <path> element in Resilio's logo (R E S I L I O V E R S I O, in order), keyed by its left edge."""
    text = RESILIO.read_text()
    found = list(re.finditer(r'<path\b([^>]*?)d="([^"]+)"', text))
    lefts = [0, 48, 96, 144, 160, 204, 220, 288, 336, 384, 432, 480, 496]
    assert len(found) == len(lefts), 'Resilio logo changed: check RESILIO_GLYPHS'
    return {x: (m.group(2), ' fill-rule="evenodd" clip-rule="evenodd"' if 'evenodd' in m.group(1) else '')
            for x, m in zip(lefts, found)}


def f(v):
    return f'{v:g}'


def new_glyph(rects, joints, dx):
    parts = [f'<rect x="{f(x0 + dx)}" y="{f(y0)}" width="{f(x1 - x0)}" height="{f(y1 - y0)}" rx="{R}"/>'
             for x0, y0, x1, y1 in rects]
    parts += [fillet(x + dx, y, sx, sy) for x, y, sx, sy in joints]
    return parts


def arc(start, end, centre):
    """A radius-R quarter circle from start to end around centre (SVG, y down)."""
    cross = (start[0] - centre[0]) * (end[1] - centre[1]) - (start[1] - centre[1]) * (end[0] - centre[0])
    return f'A{R} {R} 0 0 {1 if cross > 0 else 0} {f(end[0])} {f(end[1])}'


def fillet(x, y, sx, sy):
    """The filled shape bridging a corner-to-corner joint at (x, y): a 2R square on the joint with
    a radius-R quarter circle taken out of each empty notch (one towards (sx, sy), one opposite).
    The two strokes' rounded corners at the joint are filled back by the square."""
    n1, n2 = (x + sx * R, y + sy * R), (x - sx * R, y - sy * R)   # the notch corners = arc centres
    c1, c2 = (x + sx * R, y - sy * R), (x - sx * R, y + sy * R)   # the filled corners
    a1s, a1e = (x + sx * R, y), (x, y + sy * R)
    a2s, a2e = (x - sx * R, y), (x, y - sy * R)
    d = (f'M{f(c1[0])} {f(c1[1])}L{f(a1s[0])} {f(a1s[1])}{arc(a1s, a1e, n1)}'
         f'L{f(c2[0])} {f(c2[1])}L{f(a2s[0])} {f(a2s[1])}{arc(a2s, a2e, n2)}Z')
    return f'<path d="{d}"/>'


def build(word_groups):
    paths = resilio_paths()
    out, x = [], 0
    for gi, word in enumerate(word_groups):
        if gi:
            x += 24 - 8
        for ch in word:
            if ch in RESILIO_GLYPHS:
                src_x, w = RESILIO_GLYPHS[ch]
                d, rule = paths[src_x]
                shift = x - src_x
                tr = f' transform="translate({f(shift)} 0)"' if shift else ''
                out.append(f'<path{rule}{tr} d="{d}"/>')
            else:
                w, rects, joints = NEW_GLYPHS[ch]
                out.extend(new_glyph(rects, joints, x))
            x += w + 8
    width = x - 8
    body = '\n'.join(out)
    return (f'<svg width="{width}" height="40" viewBox="0 0 {width} 40" fill="none" xmlns="http://www.w3.org/2000/svg">\n'
            f'<g fill="black">\n{body}\n</g>\n</svg>\n'), width


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, words in [('sublustris-versio-logo.svg', ['SUBLUSTRIS', 'VERSIO']), ('sublustris-logo.svg', ['SUBLUSTRIS'])]:
        svg, w = build(words)
        (OUT / name).write_text(svg)
        print(f'{name}: {w} x 40')


if __name__ == '__main__':
    main()
