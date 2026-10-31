"""Story subjects for "a short history of the endless hold": generic, unbranded
gear in simple, believable geometry (clay pass; the owner picks one subject to
take to final).

  tape_loop()   two reel-to-reel decks side by side, ONE tape running from the
                first deck's supply reel past its heads, across the gap, past
                the second deck's heads to its take-up reel: the two-machine
                tape delay (Eno / Fripp, 1972-73).
  rack_reverb() a 1980s 19-inch rack digital reverb (1U, a small red segment
                display, a row of square buttons, a big data wheel, level
                LEDs) in a small rack with other generic units (Lexicon
                224 / PCM70-era "infinite" reverb).
  plate()       a steel reverb plate hung on springs in a wooden frame, its
                driver and two pickups on the plate, a damper panel behind
                (EMT 140-like).

No names, logos or badges on anything: every label area is blank or a plain
shape. Scene units mm (see common.py); every builder stands its subject on
the floor (z = 0), front facing -Y, and returns reference points for cameras.
Reel and tape materials are ported from Resilio's props.py (the owner's MIT
code); the rest is new.
"""
import math

import bmesh
import bpy
from mathutils import Vector

import common as C

TAPE_W = 6.35  # quarter-inch tape


# ------------------------------------------------------------------ materials

def _m(name, value=0.5, rough=0.5, rgb=None, metallic=0.0, aniso=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    return C.clay(name, value, rough, rgb=rgb, metallic=metallic, aniso=aniso)


def mats():
    return dict(
        body=_m("st_body", 0.30, 0.55),              # cabinet / chassis clay
        face=_m("st_face", 0.10, 0.45),              # dark faceplates
        alu=C.steel("st_alu", 0.75, 0.30, aniso=0.4),  # brushed metal
        steel=C.steel("st_steel", 0.70, 0.18),
        knob=_m("st_knob", 0.03, 0.40),
        rubber=_m("st_rubber", 0.015, 0.75),
        wood=_m("st_wood", rgb=(0.32, 0.22, 0.14), rough=0.6),
        felt=_m("st_felt", 0.20, 0.95),
        void=_m("st_void", 0.005, 0.9),
        table=_m("st_table", rgb=(0.20, 0.15, 0.11), rough=0.55),
    )


def tape_material():
    """Oxide side glossy brown-black, base side matte grey-black (Resilio)."""
    m = bpy.data.materials.get("tape")
    if m:
        return m
    m = bpy.data.materials.new("tape")
    m.use_nodes = True
    nt = m.node_tree
    front = nt.nodes["Principled BSDF"]
    # a shade lighter than Resilio's (0.075, 0.040, 0.022): a 6 mm strand seen
    # nearly edge-on in a dim room has to read
    front.inputs["Base Color"].default_value = (0.20, 0.11, 0.055, 1)
    front.inputs["Roughness"].default_value = 0.22
    front.inputs["Coat Weight"].default_value = 0.4
    back = nt.nodes.new("ShaderNodeBsdfPrincipled")
    back.inputs["Base Color"].default_value = (0.08, 0.075, 0.07, 1)
    back.inputs["Roughness"].default_value = 0.75
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(geo.outputs["Backfacing"], mix.inputs["Fac"])
    nt.links.new(front.outputs[0], mix.inputs[1])
    nt.links.new(back.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    return m


def glow(name, rgb_hex, strength):
    return C.emissive(name, rgb_hex, strength, base=0.3)


# ------------------------------------------------------------------ helpers

def _y_box(name, x0, x1, z0, z1, y0, y1, mat, bevel=0.0, ox=0.0, oz=0.0):
    return C.box(name, (x1 - x0, y1 - y0, z1 - z0),
                 (ox + (x0 + x1) / 2, (y0 + y1) / 2, oz + (z0 + z1) / 2), mat, bevel=bevel)


def _knob(name, x, z, r, h, mat, y_face=0.0, skirt=None):
    """A knob on a vertical face (axis -Y)."""
    C.cylinder(name, r, h, (x, y_face - h / 2, z), mat, axis="X", segs=48, r2=r * 0.9, bevel=min(1.0, r * 0.15))
    if skirt:
        C.cylinder(name + "_skirt", r * 1.35, 2.0, (x, y_face - 1.0, z), skirt, axis="X", segs=48, bevel=0.4)


def _tangent(c, R, p, cw):
    """Touch point on circle (c, R) of the tangent from p (x, z tuples). cw
    picks which of the two tangents (the side the tape wraps)."""
    dx, dz = p[0] - c[0], p[1] - c[1]
    d = math.hypot(dx, dz)
    a = math.atan2(dz, dx)
    b = math.acos(min(1.0, R / d))
    t = a + (b if cw else -b)
    return (c[0] + R * math.cos(t), c[1] + R * math.sin(t))


def _wrap(c, R, a0, a1, n=10):
    """Points on an arc of circle (c, R) from angle a0 to a1 (radians)."""
    return [(c[0] + R * math.cos(a0 + (a1 - a0) * k / n), c[1] + R * math.sin(a0 + (a1 - a0) * k / n))
            for k in range(n + 1)]


def tape_strip(name, pts_xz, y_mid, width=TAPE_W):
    """A flat tape ribbon through (x, z) points in the plane y = y_mid; the
    tape's width runs along Y (square to a deck's faceplate)."""
    bm = bmesh.new()
    rows = [(bm.verts.new((x, y_mid - width / 2, z)), bm.verts.new((x, y_mid + width / 2, z))) for x, z in pts_xz]
    for (a0, a1), (b0, b1) in zip(rows, rows[1:]):
        if (a0.co - b0.co).length > 1e-4:
            bm.faces.new((a0, b0, b1, a1))
    return C.mesh_obj(name, bm, tape_material(), smooth=True)


def reel(name, cx, cz, y_face, radius, pack_r, hub_r, flange_mat, m):
    """A reel on a deck's spindle, axis along Y: two metal flanges with three
    windows, a hub, the tape pack between them (pack_r 0 = empty reel)."""
    gap = TAPE_W + 1.6
    yc = y_face - 14.0  # reel centre plane, in front of the faceplate
    for s in (-1, 1):
        fl = C.cylinder(f"{name}_flange", radius, 1.2, (cx, yc + s * gap / 2, cz), flange_mat, axis="X",
                        segs=128, bevel=0.3)
        fl.rotation_euler = (math.pi / 2, 0, 0)
        cuts = []
        for k in range(3):
            a = 2 * math.pi * k / 3 + math.pi / 2
            w = _window(hub_r + 10, radius - 12, a - 0.55, a + 0.55, 8)
            w.location = (cx, yc + s * gap / 2, cz)
            cuts.append(w)
        C.boolean_cut(fl, cuts)
    C.cylinder(f"{name}_hub", hub_r, gap + 2.4, (cx, yc, cz), m["alu"], axis="X", segs=64, bevel=0.4)
    C.cylinder(f"{name}_spindle", 4.0, 26, (cx, y_face - 13, cz), m["steel"], axis="X", segs=24)
    C.cylinder(f"{name}_lock", 9.0, 5, (cx, yc - gap / 2 - 4, cz), m["knob"], axis="X", segs=32, bevel=0.8)
    if pack_r > hub_r:
        C.cylinder(f"{name}_pack", pack_r, TAPE_W, (cx, yc, cz), tape_material(), axis="X", segs=128)
    return yc


def _window(r_in, r_out, a0, a1, depth, n=20):
    """A rounded-sector cutter (reel window), a prism along Y."""
    bm = bmesh.new()
    pts = [(r_out * math.cos(a0 + (a1 - a0) * k / n), r_out * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    pts += [(r_in * math.cos(a0 + (a1 - a0) * k / n), r_in * math.sin(a0 + (a1 - a0) * k / n))
            for k in range(n, -1, -1)]
    vs = [bm.verts.new((x, -depth / 2, z)) for x, z in pts]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    bmesh.ops.translate(bm, vec=Vector((0, depth, 0)),
                        verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return C.mesh_obj("window_cutter", bm)


# ------------------------------------------------------------------ (a) tape loop

DECK_W, DECK_H, DECK_D = 440.0, 500.0, 230.0
REEL_Z, REEL_DX, REEL_R = 345.0, 112.0, 89.0     # 7-inch reels, centres above the head block
HEAD_Z = 190.0                                   # the tape's height across the heads


def _deck(ox, oz, m, supply, takeup):
    """An upright open-reel deck, front at y = 0. supply/takeup: tape pack
    radius on the left/right reel (0 = empty reel, None = no reel)."""
    _y_box("deck_cab", -DECK_W / 2, DECK_W / 2, 0, DECK_H, 0, DECK_D, m["body"], bevel=6, ox=ox, oz=oz)
    # reel-area plate and the lower control panel, both slightly proud
    _y_box("deck_plate", -DECK_W / 2 + 10, DECK_W / 2 - 10, 150, DECK_H - 10, -3, 1, m["face"], bevel=1.5,
           ox=ox, oz=oz)
    _y_box("deck_ctrl", -DECK_W / 2 + 10, DECK_W / 2 - 10, 10, 140, -3, 1, m["alu"], bevel=1.5, ox=ox, oz=oz)
    # head block: a cover over three heads, the capstan and pinch roller to its right
    # (upright deck: the tape runs along the heads' lower faces, the cover above them)
    _y_box("head_cover", -62, 62, HEAD_Z + 18, HEAD_Z + 52, -34, -3, m["alu"], bevel=4, ox=ox, oz=oz)
    for k, hx in enumerate((-36, 0, 36)):
        _y_box(f"head{k}", hx - 7, hx + 7, HEAD_Z + 0.4, HEAD_Z + 20, -30, -3, m["steel"], bevel=2, ox=ox, oz=oz)
    C.cylinder("capstan", 2.4, 30, (ox + 82, -16, oz + HEAD_Z + 2.6), m["steel"], axis="X", segs=24)
    C.cylinder("pinch", 12.0, 14, (ox + 82, -16, oz + HEAD_Z - 12.2), m["rubber"], axis="X", segs=48, bevel=1.5)
    for s in (-1, 1):  # guide rollers / tension arms
        C.cylinder("guide", 6.0, 22, (ox + s * 132, -14, oz + HEAD_Z + 6), m["steel"], axis="X", segs=32,
                   bevel=0.8)
        C.cylinder("guide_post", 2.0, 30, (ox + s * 132, -10, oz + HEAD_Z + 6), m["steel"], axis="X", segs=16)
    # controls: two small VU meters (faintly lit), a row of knobs, piano-key transport
    vu = glow("vu_face", "#F2C27A", 1.4)
    for s in (-1, 1):
        _y_box("vu_bezel", s * 95 - 46, s * 95 + 46, 70, 128, -6, -2, m["knob"], bevel=2, ox=ox, oz=oz)
        _y_box("vu_face", s * 95 - 40, s * 95 + 40, 75, 123, -6.6, -5.9, vu, ox=ox, oz=oz)
        C.box("vu_needle", (0.8, 0.3, 36), (ox + s * 95 + 6, -6.9, oz + 92), m["knob"], rot=(0, math.radians(-25), 0))
    for k in range(5):
        _knob("deck_knob", ox - 120 + k * 60, oz + 38, 11, 12, m["knob"], y_face=-3)
    for k in range(5):
        _y_box("key", -30 + k * 14, -30 + k * 14 + 11, 22, 52, -16, -3, m["face"], bevel=1.5, ox=ox - 14, oz=oz)
    # reels
    y_mid = None
    for s, pack in ((-1, supply), (1, takeup)):
        if pack is None:
            C.cylinder("spindle_bare", 4.0, 24, (ox + s * REEL_DX, -12, oz + REEL_Z), m["steel"], axis="X", segs=24)
            C.cylinder("turntable", 22, 4, (ox + s * REEL_DX, -5, oz + REEL_Z), m["alu"], axis="X", segs=48, bevel=0.6)
            continue
        y_mid = reel("reel", ox + s * REEL_DX, oz + REEL_Z, -2, REEL_R, pack, 27.0, m["alu"], m)
    return y_mid if y_mid is not None else -16.0


def tape_loop():
    """Two decks on a long table, one tape strung from the left deck's supply
    reel to the right deck's take-up reel. Returns refs."""
    m = mats()
    top = 740.0
    gap = 420.0                         # between the cabinets
    xl, xr = -(DECK_W + gap) / 2, (DECK_W + gap) / 2
    # the table: a long top on four legs
    C.box("table_top", (2100, 760, 32), (0, 200, top - 16), m["table"], bevel=3)
    for sx in (-1, 1):
        for sy in (-1, 1):
            C.box("table_leg", (50, 50, top - 32), (sx * 1000, 200 + sy * 330, (top - 32) / 2), m["table"], bevel=2)
    y_mid = _deck(xl, top, m, supply=72.0, takeup=None)
    _deck(xr, top, m, supply=None, takeup=48.0)
    y_mid = y_mid if y_mid else -16.0
    # tape path, in world x / z, in the plane y = y_mid
    hz = top + HEAD_Z
    sup_c = (xl - REEL_DX, top + REEL_Z)
    take_c = (xr + REEL_DX, top + REEL_Z)
    g1 = (xl - 132, hz + 6)             # left deck's left guide (tape passes under it)
    gr = 6.0 + 0.2
    pts = []
    t0 = _tangent(sup_c, 72.0, (g1[0] - gr, g1[1]), cw=False)
    pts += [t0]
    # round the left guide (outside, left -> under -> right)
    pts += _wrap(g1, gr, math.pi, 1.5 * math.pi, 6)
    # across the heads (in contact with their faces), past the capstan
    pts += [(xl - 50, hz), (xl + 50, hz), (xl + 82, hz), (xl + 132, hz - 1)]
    # the span between the machines
    pts += [(xr - 132, hz - 1), (xr - 82, hz), (xr - 50, hz), (xr + 50, hz)]
    g2 = (xr + 132, hz + 6)
    pts += _wrap(g2, gr, 1.5 * math.pi, 2 * math.pi, 6)
    t1 = _tangent(take_c, 48.0, (g2[0] + gr, g2[1]), cw=True)
    pts += [t1]
    tape_strip("tape_loop", pts, y_mid)
    C.cyclorama(width=9000, front=3500, back=1400, height=3500, radius=900, rgb=(0.05, 0.047, 0.044))
    return dict(center=Vector((0, 60, top + 260)), size=DECK_W * 2 + gap, radius=(DECK_W * 2 + gap) / 2 + 60,
                span=Vector((0, y_mid, hz)), deck_l=Vector((xl, 0, top + 260)), deck_r=Vector((xr, 0, top + 260)),
                head_l=Vector((xl, -20, hz + 10)), top=top)


# ------------------------------------------------------------------ (b) rack reverb

U = 44.45
RACK_W, RACK_BODY = 482.6, 430.0

SEG = {  # seven-segment digits: a b c d e f g
    "0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc",
    "7": "abc", "8": "abcdefg", "9": "abcfgd", "-": "g", " ": ""}


def _digit(ch, x, z, y, h, mat):
    """A seven-segment digit, height h, centred at (x, z) on the face y."""
    w, t = h * 0.5, h * 0.11
    segs = dict(a=(0, h / 2, w, t), d=(0, -h / 2, w, t), g=(0, 0, w, t),
                b=(w / 2, h / 4, t, h / 2), c=(w / 2, -h / 4, t, h / 2),
                f=(-w / 2, h / 4, t, h / 2), e=(-w / 2, -h / 4, t, h / 2))
    for s in SEG[ch]:
        dx, dz, sx, sz = segs[s]
        C.box("seg", (sx * 0.86, 0.3, sz * 0.86), (x + dx, y, z + dz), mat)


def _rack_unit_reverb(z0, m, display="1.20"):
    """1U digital reverb, front at y = 0, bottom at z0."""
    h = U - 0.8
    _y_box("rv_ears", -RACK_W / 2, RACK_W / 2, z0, z0 + h, -3, 0, m["face"], bevel=1.0)
    _y_box("rv_body", -RACK_BODY / 2, RACK_BODY / 2, z0 + 0.5, z0 + h - 0.5, 0, 300, m["body"], bevel=1.0)
    for s in (-1, 1):  # rack ear slots
        C.box("ear_slot", (10, 8, 5.5), (s * 232, -1.5, z0 + h / 2), m["void"], bevel=2.5)
    zc = z0 + h / 2
    # power rocker, left
    _y_box("rv_power", -200, -186, zc - 9, zc + 9, -6, -3, m["knob"], bevel=1.5)
    # level-meter LEDs (two short ladders)
    led_g = glow("rv_led_g", "#7CFF8A", 3.0)
    led_r = glow("rv_led_r", "#FF5A3C", 4.0)
    for row, dz in enumerate((5, -5)):
        for k in range(8):
            mat = led_r if k == 7 else (led_g if k < 5 else m["face"])
            C.cylinder("rv_led", 1.6, 1.4, (-168 + k * 7.5, -3.6, zc + dz), mat, axis="X", segs=16)
    # the display window: dark red acrylic, segment digits glowing behind it
    _y_box("rv_window", -100, -18, zc - 13, zc + 13, -4.5, -3, m["void"], bevel=1.0)
    seg = glow("rv_seg", "#FF3B1F", 9.0)
    xs = [-88, -66, -44, -24]
    dots = []
    chars = display.replace(".", "")
    for i, ch in enumerate(chars[:4]):
        _digit(ch, xs[i] + 0.5, zc, -4.8, 15, seg)
    if "." in display:
        dots.append(display.index(".") - 1)
    for d in dots:
        C.box("dp", (1.8, 0.3, 1.8), (xs[d] + 6.0, -4.8, zc - 7.0), seg)
    # a row of square buttons, two with small lit indicators
    ind = glow("rv_ind", "#FFB02E", 4.0)
    for k in range(8):
        bx = 0 + k * 19
        _y_box("rv_btn", bx - 6.5, bx + 6.5, zc - 9, zc + 4, -7, -3, m["face"], bevel=1.2)
        C.cylinder("rv_ind", 1.2, 1.0, (bx, -3.4, zc + 10), ind if k in (2, 5) else m["face"], axis="X", segs=12)
    # the big data wheel, right
    C.cylinder("rv_wheel", 17, 14, (178, -10, zc), m["knob"], axis="X", segs=64, bevel=1.5)
    C.cylinder("rv_wheel_dimple", 3, 1, (184, -17.2, zc + 6), m["face"], axis="X", segs=16)
    # two small knobs (input / mix)
    for kx in (140, 214):
        _knob("rv_knob", kx, zc, 7, 9, m["knob"], y_face=-3)
    return zc


def _rack_blank(z0, units, m, vented=False):
    h = units * U - 0.8
    _y_box("blank", -RACK_W / 2, RACK_W / 2, z0, z0 + h, -2.5, 0, m["face"], bevel=1.0)
    if vented:
        for k in range(14):
            C.box("vent", (16, 3, h * 0.5), (-140 + k * 21.5, -2.0, z0 + h / 2), m["void"], bevel=1.4)


def _rack_dynamics(z0, m):
    """A generic 2U two-channel compressor: two rows of knobs, two lit meters."""
    h = 2 * U - 0.8
    _y_box("comp", -RACK_W / 2, RACK_W / 2, z0, z0 + h, -3, 0, m["alu"], bevel=1.0)
    _y_box("comp_body", -RACK_BODY / 2, RACK_BODY / 2, z0 + 0.5, z0 + h - 0.5, 0, 260, m["body"], bevel=1.0)
    vu = glow("comp_vu", "#F2C27A", 1.2)
    for row, dz in enumerate((h * 0.7, h * 0.3)):
        for k in range(5):
            _knob("comp_knob", -170 + k * 42, z0 + dz, 8, 10, m["knob"], y_face=-3)
        _y_box("comp_meter", 60, 130, z0 + dz - 12, z0 + dz + 12, -5, -3, vu, bevel=1.0)
        _y_box("comp_meter_frame", 56, 134, z0 + dz - 15, z0 + dz + 15, -4, -2.6, m["knob"], bevel=1.0)
        C.box("comp_needle", (0.6, 0.3, 18), (95, -5.4, z0 + dz - 2), m["knob"], rot=(0, math.radians(-30), 0))
        _y_box("comp_sw", 160, 172, z0 + dz - 7, z0 + dz + 7, -7, -3, m["face"], bevel=1.5)


def _rack_patchbay(z0, m):
    h = U - 0.8
    _y_box("pb", -RACK_W / 2, RACK_W / 2, z0, z0 + h, -2.5, 0, m["face"], bevel=1.0)
    for row in (0.68, 0.32):
        for k in range(24):
            C.cylinder("pb_jack", 3.0, 1.2, (-200 + k * 17.4, -2.8, z0 + h * row), m["alu"], axis="X", segs=20)
            C.cylinder("pb_hole", 1.8, 1.0, (-200 + k * 17.4, -3.1, z0 + h * row), m["void"], axis="X", segs=16)


def rack_reverb():
    """A 12U wooden-sided rack on the floor: patch bay, blank, the reverb (at
    eye height for a seated camera), a vented blank, a compressor, blanks."""
    m = mats()
    n_units = 12
    base = 70.0                                    # plinth
    inner_h = n_units * U
    side_t = 19.0
    xo = RACK_W / 2 + side_t / 2 - 6
    for s in (-1, 1):
        C.box("rack_side", (side_t, 520, inner_h + base + 30), (s * xo, 220, (inner_h + base + 30) / 2), m["wood"],
              bevel=2)
        C.box("rack_rail", (16, 4, inner_h), (s * (RACK_W / 2 - 14), 2.5, base + inner_h / 2), m["steel"], bevel=0.5)
    C.box("rack_top", (2 * xo + side_t, 520, side_t), (0, 220, base + inner_h + 30 + side_t / 2), m["wood"], bevel=2)
    C.box("rack_plinth", (2 * xo - side_t, 500, base), (0, 230, base / 2), m["void"], bevel=1)
    z = base
    _rack_blank(z, 1, m); z += U
    _rack_dynamics(z, m); z += 2 * U
    _rack_blank(z, 1, m, vented=True); z += U
    zc = _rack_unit_reverb(z, m); z += U
    _rack_blank(z, 1, m); z += U
    _rack_patchbay(z, m); z += U
    _rack_dynamics(z, m); z += 2 * U
    _rack_blank(z, 2, m, vented=True); z += 2 * U
    _rack_blank(z, 1, m)
    C.cyclorama(width=6000, front=2500, back=900, height=3000, radius=700, rgb=(0.05, 0.047, 0.044))
    top = base + inner_h + 30 + side_t
    return dict(center=Vector((0, 100, top / 2)), size=top, radius=math.hypot(RACK_W + 40, top) / 2 + 30,
                display=Vector((-59, -5, zc)), wheel=Vector((178, -10, zc)), unit=Vector((0, -5, zc)))


# ------------------------------------------------------------------ (c) plate

PLATE_W, PLATE_H = 2000.0, 1000.0


def _helix(name, a, b, r, turns, wire, mat):
    """A coil spring from a to b."""
    a, b = Vector(a), Vector(b)
    axis = (b - a)
    L = axis.length
    t = axis.normalized()
    u = t.orthogonal().normalized()
    v = t.cross(u)
    n = int(turns * 16)
    pts = []
    for k in range(n + 1):
        f = k / n
        ang = 2 * math.pi * turns * f
        p = a + t * (L * f) + (u * math.cos(ang) + v * math.sin(ang)) * r
        pts.append(tuple(p))
    return C.curve_obj(name, pts, wire, mat, resolution=2, spline="POLY")


def plate():
    """The plate hung in an open wooden frame on legs: eight springs from the
    plate's corners and edges to the frame, the driver near the centre, two
    pickups, a damper panel behind on two threaded rods."""
    m = mats()
    z0 = 380.0                       # plate bottom edge
    beam = 70.0
    gapx, gapz = 150.0, 150.0        # plate to frame
    fx0, fx1 = -PLATE_W / 2 - gapx, PLATE_W / 2 + gapx
    fz0, fz1 = z0 - gapz, z0 + PLATE_H + gapz
    fy = 40.0                        # frame centre plane (the plate hangs at y = 0)
    # frame: top / bottom rails, end posts that run to the floor as legs
    C.box("frame_top", (fx1 - fx0 + beam, beam, beam), (0, fy, fz1 + beam / 2), m["wood"], bevel=3)
    C.box("frame_bot", (fx1 - fx0 + beam, beam, beam), (0, fy, fz0 - beam / 2), m["wood"], bevel=3)
    for s in (-1, 1):
        x = s * (fx1 + beam / 2)
        C.box("frame_post", (beam, beam, fz1 + beam), (x, fy, (fz1 + beam) / 2), m["wood"], bevel=3)
        C.box("frame_foot", (beam * 1.2, 520, 50), (x, fy + 60, 25), m["wood"], bevel=3)
        C.box("frame_post_back", (beam, beam, fz1 + beam), (x, fy + 300, (fz1 + beam) / 2), m["wood"], bevel=3)
    C.box("frame_top_back", (fx1 - fx0 + beam, beam, beam), (0, fy + 300, fz1 + beam / 2), m["wood"], bevel=3)
    # the plate: thin steel, a faint brushed grain
    C.box("plate", (PLATE_W, 1.0, PLATE_H), (0, 0, z0 + PLATE_H / 2), C.clay("st_plate", 0.55, 0.38, metallic=0.85, aniso=0.3))
    # suspension: 8 springs, each from a clamp on the plate to an eye bolt on the frame
    anchors = [(-1, -1), (1, -1), (-1, 1), (1, 1), (0, 1), (0, -1)]
    pts = []
    for sx, sz in anchors:
        px = sx * (PLATE_W / 2 - 20) if sx else 0
        pz = z0 + (PLATE_H - 20 if sz > 0 else 20)
        fx = sx * (fx1 - 10) if sx else 0
        fz = fz1 if sz > 0 else fz0
        if sx:  # corners: diagonal springs
            pts.append(((px, 0, pz), (sx * (fx1 - 40), fy - 20, fz + (-1 if sz > 0 else 1) * 10)))
        else:
            pts.append(((px, 0, pz), (fx, fy - 20, fz + (-1 if sz > 0 else 1) * 10)))
    for sx in (-1, 1):  # mid-height springs on the ends
        pts.append(((sx * (PLATE_W / 2 - 20), 0, z0 + PLATE_H / 2), (sx * (fx1 - 20), fy - 20, z0 + PLATE_H / 2)))
    for k, (a, b) in enumerate(pts):
        a, b = Vector(a), Vector(b)
        d = (b - a).normalized()
        C.box(f"clamp{k}", (26, 12, 26), tuple(a), m["steel"], bevel=2)
        C.cylinder(f"eye{k}", 6, 40, tuple(b + Vector((0, 20, 0))), m["steel"], axis="X", segs=16)
        hook0, hook1 = a + d * 18, b - d * 12
        C.curve_obj(f"hook{k}", [tuple(a), tuple(hook0)], 1.4, m["steel"])
        C.curve_obj(f"hook{k}b", [tuple(hook1), tuple(b)], 1.4, m["steel"])
        _helix(f"spring{k}", hook0, hook1, 7.0, max(8, (hook1 - hook0).length / 6.5), 1.2, m["steel"])
    # driver: a moving-coil transducer on a crossbar from the bottom rail
    dx, dz = -260.0, z0 + 380.0
    C.box("driver_bar", (60, 40, dz - fz0), (dx, fy + 10, (dz + fz0) / 2), m["wood"], bevel=2)
    C.cylinder("driver_mag", 42, 40, (dx, 28, dz), m["body"], axis="X", segs=48, bevel=3)
    C.cylinder("driver_coil", 20, 22, (dx, 4, dz), m["face"], axis="X", segs=48, bevel=1.5)
    # two pickups, contact type, small blocks on the plate's back with leads
    for k, (px, pz) in enumerate(((-720.0, z0 + 760.0), (640.0, z0 + 240.0))):
        C.box(f"pickup{k}", (36, 18, 24), (px, 10, pz), m["body"], bevel=2)
        C.curve_obj(f"lead{k}", [(px, 18, pz), (px, 40, pz + 30), (px + 40 * (1 if px < 0 else -1), fy + 20, fz1)],
                    1.5, m["knob"], spline="NURBS")
    # damper: a felt-faced panel behind the plate on two threaded rods + a crank wheel on top
    C.box("damper", (PLATE_W * 0.9, 25, PLATE_H * 0.86), (0, 120, z0 + PLATE_H / 2), m["felt"], bevel=6)
    for s in (-1, 1):
        C.cylinder("damper_rod", 5, 140, (s * 600, 200, fz1 + beam + 10), m["steel"], axis="Z", segs=16)
        C.cylinder("damper_rod_h", 5, 120, (s * 600, 150, fz1 - 60), m["steel"], axis="X", segs=16)
    C.torus("crank_wheel", 55, 5, (0, fy, fz1 + beam + 70), m["steel"], axis="X")
    C.cylinder("crank_hub", 10, 30, (0, fy, fz1 + beam + 70), m["steel"], axis="X", segs=24)
    C.cylinder("crank_shaft", 4, 60, (0, fy, fz1 + beam + 35), m["steel"], axis="Z", segs=16)
    C.cyclorama(width=12000, front=5000, back=1500, height=5000, radius=1200, rgb=(0.05, 0.047, 0.044))
    top = fz1 + beam
    return dict(center=Vector((0, 60, (z0 + PLATE_H / 2))), size=fx1 - fx0, radius=(fx1 - fx0) / 2 + 80,
                corner=Vector((PLATE_W / 2 - 20, 0, z0 + PLATE_H - 20)), driver=Vector((dx, 0, dz)), top=top)
