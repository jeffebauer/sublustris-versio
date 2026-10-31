"""Period instruments for the story scene (owner, 31 Oct 2026: the decks need something on
the desk that would be recorded onto them, and it should be of the era). All unbranded.
See STORY_RESEARCH.md: Discreet Music (1975) was an EMS Synthi AKS through a graphic EQ and an
echo unit into two Revox decks; on (No Pussyfooting) (1973) Fripp played electric guitar.

  synthi()      a briefcase synth in the Synthi AKS layout: the base holds the panel (pin
                matrix, knobs, joystick, a small meter), the lid lies flat beside it with the
                flat touch keyboard and the sequencer.
  tape_echo()   a 1970s tape echo in the Echoplex EP-3 manner: a tolex box, the tape
                cartridge under a cover on top, the delay slider, three knobs on the front.
  guitar()      a single-cut solid body in sunburst, two humbuckers, leaning on the desk.

Each builds at the origin (x across, y back, z up; mm) and returns an empty that carries
every part, to be moved and turned into place.
"""
import math

import bmesh
import bpy
from mathutils import Vector

import common as C

PARTS = []


def _track(ob):
    PARTS.append(ob)
    return ob


def box(name, x0, x1, y0, y1, z0, z1, mat, bevel=0.0, rot=None):
    ob = C.box(name, (x1 - x0, y1 - y0, z1 - z0), ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), mat,
               bevel=bevel)
    if rot:
        ob.rotation_euler = rot
    return _track(ob)


def cyl(name, r, h, loc, mat, axis="Z", segs=32, bevel=0.0, r2=None):
    return _track(C.cylinder(name, r, h, loc, mat, axis=axis, segs=segs, bevel=bevel, r2=r2))


def group(name):
    """Parent every part built since the last group to one empty at the origin."""
    e = bpy.data.objects.new(name, None)
    C.link(e)
    for ob in PARTS:
        ob.parent = e
    PARTS.clear()
    return e


def knob(name, x, y, z, r, mat, cap=None, h=None):
    """A small knob standing on a horizontal panel at height z, with a white index line."""
    h = h or r * 1.1
    cyl(name, r, h, (x, y, z + h / 2), mat, segs=32, bevel=min(0.8, r * 0.12), r2=r * 0.9)
    if cap:
        cyl(name + "_cap", r * 0.62, 0.6, (x, y, z + h + 0.3), cap, segs=24)
    box(name + "_line", x - 0.5, x + 0.5, y - r * 0.9, y - r * 0.2, z + h, z + h + 0.4, M["print"])


# ------------------------------------------------------------------ materials

M = {}


def mats(m):
    """Materials, built on the scene's set m (studio.mats3)."""
    import studio
    M.update(
        knob=m["knob3"], alu=m["capalu"], steel=m["steel"], void=m["void"], walnut=m["walnut"],
        rubber=m["rubber3"], cable=m["cable"],
        case=studio.finish(C.clay("synthi_vinyl", rgb=(0.025, 0.024, 0.024), rough=0.55), bump=0.5, scale=1.6,
                           rough_var=0.2),   # black leathercloth
        panel=studio.finish(C.clay("synthi_panel", rgb=(0.02, 0.02, 0.022), rough=0.62), bump=0.02, scale=4.0,
                            rough_var=0.15),
        matrix=studio.finish(C.clay("synthi_matrix", rgb=(0.72, 0.70, 0.64), rough=0.45), bump=0.02, scale=4.0),
        print=C.clay("gear_print", 0.72, 0.5),
        chrome=C.steel("gear_chrome", 0.8, 0.12),
        pin_w=C.clay("pin_white", 0.75, 0.3), pin_r=C.clay("pin_red", rgb=(0.55, 0.05, 0.03), rough=0.3),
        keyplate=C.steel("synthi_keyplate", 0.55, 0.3),
        tolex=studio.finish(C.clay("echo_tolex", rgb=(0.035, 0.034, 0.033), rough=0.6), bump=0.6, scale=1.2,
                            rough_var=0.2),
        echo_panel=studio.finish(C.clay("echo_panel", rgb=(0.20, 0.20, 0.19), rough=0.35, metallic=0.6), bump=0.02,
                                 scale=4.0, rough_var=0.2),
        sunburst=_sunburst(),
        neck=studio.finish(C.clay("guitar_neck", rgb=(0.42, 0.24, 0.10), rough=0.3, coat=0.6), bump=0.02, scale=2),
        board=studio.finish(C.clay("guitar_board", rgb=(0.07, 0.04, 0.025), rough=0.5), bump=0.05, scale=3),
        cream=C.clay("guitar_cream", rgb=(0.66, 0.58, 0.42), rough=0.35),
        amber=C.clay("guitar_amber", rgb=(0.45, 0.22, 0.04), rough=0.25, coat=0.4),
        string=C.steel("guitar_string", 0.8, 0.2),
        fret=C.steel("guitar_fret", 0.75, 0.25),
        meter=C.emissive("synthi_meter", "#E8D8B0", 0.6, base=0.8),
        led=C.emissive("echo_pilot", "#FF4A20", 5.0),
    )
    return M


def _sunburst():
    """A two-tone sunburst: amber centre, dark brown edge (a spherical gradient on the body)."""
    m = C.clay("guitar_burst", rgb=(0.5, 0.25, 0.05), rough=0.18, coat=0.9)
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    p.inputs["Coat Roughness"].default_value = 0.06
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1 / 175, 1 / 230, 0.0)   # across the top only, not its depth
    mp.inputs["Location"].default_value = (0.0, 0.15, 0.0)
    gr = nt.nodes.new("ShaderNodeTexGradient")
    gr.gradient_type = "SPHERICAL"
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.035, 0.015, 0.008, 1)
    ramp.color_ramp.elements[1].position = 0.55
    ramp.color_ramp.elements[1].color = (0.62, 0.30, 0.06, 1)
    e = ramp.color_ramp.elements.new(0.25)
    e.color = (0.25, 0.07, 0.02, 1)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], gr.inputs["Vector"])
    nt.links.new(gr.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], p.inputs["Base Color"])
    return m


# ------------------------------------------------------------------ the briefcase synth

SY_W, SY_D, SY_H = 460.0, 300.0, 115.0   # the base, closed-case footprint


def synthi():
    """Base with the panel (left: oscillators, filter, envelope knobs in rows; right: the 16 x 16
    pin matrix with a few pins in; a joystick and a small meter), returns (base, lid) empties.
    The lid, with the flat touch keyboard and the sequencer, is built separately so it can lie
    flat beside the base."""
    w2 = SY_W / 2
    # the case: leathercloth-covered sides, chrome corner caps, a carry handle at the back
    top = SY_H - 9.0
    box("sy_case", -w2, w2, 0, SY_D, 0, top - 3, M["case"], bevel=6)
    # a thin chrome trim round the panel's edge (four strips, not a slab)
    for x0, x1, y0, y1 in ((-w2 + 6, w2 - 6, 6, 12), (-w2 + 6, w2 - 6, SY_D - 12, SY_D - 6),
                           (-w2 + 6, -w2 + 12, 6, SY_D - 6), (w2 - 12, w2 - 6, 6, SY_D - 6)):
        box("sy_trim", x0, x1, y0, y1, top - 4, top + 1.5, M["chrome"], bevel=0.8)
    for sx in (-1, 1):
        for sy in (0, 1):
            cyl("sy_corner", 9, SY_H - 20, (sx * (w2 - 3), 3 + sy * (SY_D - 6), (SY_H - 8) / 2), M["chrome"], segs=24,
                bevel=1)
    box("sy_handle", -60, 60, SY_D + 2, SY_D + 14, SY_H - 40, SY_H - 28, M["case"], bevel=4)
    box("sy_panel", -w2 + 12, w2 - 12, 12, SY_D - 12, top - 2, top, M["panel"], bevel=0.5)
    # left two thirds: knob rows, each section boxed by thin printed lines
    x0, x1 = -w2 + 26, 70.0
    rows = (SY_D - 50, SY_D - 100, SY_D - 150, SY_D - 200, 62)
    for r, y in enumerate(rows):
        n = 7 if r < 4 else 6
        for k in range(n):
            x = x0 + 12 + k * (x1 - x0 - 24) / (n - 1)
            knob(f"sy_knob{r}_{k}", x, y, top, 7.5, M["knob"])
        box("sy_rule", x0, x1, y - 22, y - 21.4, top, top + 0.2, M["print"])
    box("sy_rule_v", x1 + 6, x1 + 6.6, 30, SY_D - 26, top, top + 0.2, M["print"])
    # the pin matrix: a cream board with a 16 x 16 grid of holes, a few pins in
    mx0, my0, pitch = 76.0, SY_D - 26 - 16 * 8.4, 8.4
    box("sy_matrix", mx0 - 6, mx0 + 16 * pitch + 6, my0 - 6, my0 + 16 * pitch + 6, top, top + 1.2, M["matrix"],
        bevel=0.5)
    pins = {(2, 3): "pin_w", (5, 9): "pin_w", (7, 1): "pin_r", (9, 12): "pin_w", (11, 4): "pin_r",
            (12, 14): "pin_w", (14, 7): "pin_w", (3, 15): "pin_r"}
    for i in range(16):
        for j in range(16):
            x, y = mx0 + (i + 0.5) * pitch, my0 + (j + 0.5) * pitch
            if (i, j) in pins:
                cyl("sy_pin", 1.0, 6, (x, y, top + 4), M["steel"], segs=12)
                cyl("sy_pinhead", 2.4, 3.2, (x, y, top + 8.5), M[pins[(i, j)]], segs=16, bevel=0.6)
            else:
                box("sy_hole", x - 1.1, x + 1.1, y - 1.1, y + 1.1, top + 1.2, top + 1.35, M["void"])
    # joystick and the small meter, lower right
    cyl("sy_joy_boot", 14, 6, (mx0 + 30, 60, top + 3), M["rubber"], segs=32, bevel=2)
    stick = cyl("sy_joy", 2.6, 44, (mx0 + 30, 60, top + 24), M["steel"], segs=16)
    stick.rotation_euler = (math.radians(-12), math.radians(8), 0)
    _track(C.sphere("sy_joy_ball", 7, (mx0 + 33, 55.5, top + 45), M["knob"]))
    box("sy_meter", mx0 + 62, mx0 + 112, 40, 80, top, top + 3, M["void"], bevel=1)
    box("sy_meter_face", mx0 + 65, mx0 + 109, 44, 76, top + 3, top + 3.3, M["meter"])
    nd = box("sy_needle", mx0 + 86.6, mx0 + 87.4, 46, 72, top + 3.4, top + 3.7, M["void"])
    nd.rotation_euler = (0, 0, math.radians(-18))
    base = group("synthi_base")

    # the lid: the same leathercloth, its inside a metal panel with the flat touch keyboard
    # (two and a half octaves of printed key plates) and the sequencer's buttons above it
    ld = 260.0
    box("lid_case", -w2, w2, 0, ld, 0, 28, M["case"], bevel=6)
    box("lid_panel", -w2 + 12, w2 - 12, 12, ld - 12, 28, 30, M["panel"], bevel=0.5)
    kx0, kw, nwhite = -w2 + 40, 20.5, 18
    for k in range(nwhite):  # white key plates
        x = kx0 + k * kw
        box("lid_white", x + 0.8, x + kw - 0.8, 20, 110, 30, 30.6, M["keyplate"], bevel=0.3)
    for k in range(nwhite - 1):  # black key plates over the gaps (no black after E and B)
        if k % 7 in (2, 6):
            continue
        x = kx0 + (k + 1) * kw
        box("lid_black", x - 6, x + 6, 62, 110, 30.6, 31.2, M["void"], bevel=0.3)
    for k in range(12):  # sequencer buttons and their legends
        x = kx0 + 10 + k * 28
        box("lid_btn", x - 6, x + 6, 150, 162, 30, 33, M["knob"], bevel=1.2)
        box("lid_legend", x - 8, x + 8, 168, 169, 30, 30.2, M["print"])
    for k in range(4):
        knob(f"lid_knob{k}", kx0 + 30 + k * 60, 210, 30, 7, M["knob"])
    lid = group("synthi_lid")
    return base, lid


# ------------------------------------------------------------------ the tape echo

EC_W, EC_D, EC_H = 340.0, 220.0, 175.0


def tape_echo():
    """Front faces -y: a dark panel with three knobs and a pilot light; on top the tape
    cartridge under a smoked cover and the delay slider in its slot. Tolex, chrome corners."""
    w2 = EC_W / 2
    box("ec_case", -w2, w2, 0, EC_D, 0, EC_H, M["tolex"], bevel=5)
    for sx in (-1, 1):
        for sz in (0, 1):
            cyl("ec_corner", 8, 22, (sx * (w2 - 4), 4, 6 + sz * (EC_H - 12)), M["chrome"], axis="X", segs=20,
                bevel=1)
    # front control panel, slightly recessed
    box("ec_front", -w2 + 14, w2 - 14, -1.5, 2, 18, 92, M["echo_panel"], bevel=1)
    for k in range(3):
        x = -90 + k * 70
        cyl(f"ec_knob{k}", 13, 16, (x, -9, 55), M["knob"], axis="X", segs=40, bevel=1.5, r2=12)
        cyl(f"ec_skirt{k}", 17, 2, (x, -2.5, 55), M["alu"], axis="X", segs=40, bevel=0.4)
        box(f"ec_legend{k}", x - 14, x + 14, -2, -1.6, 28, 30, M["print"])
    cyl("ec_pilot", 4, 4, (120, -3, 70), M["led"], axis="X", segs=20)
    cyl("ec_jack", 5, 6, (120, -3, 40), M["chrome"], axis="X", segs=20)
    # top: a metal deck plate, the cartridge under a smoked cover, the delay slider
    box("ec_top", -w2 + 12, w2 - 12, 12, EC_D - 12, EC_H, EC_H + 2, M["echo_panel"], bevel=1)
    box("ec_cart", -120, 60, 40, 180, EC_H + 2, EC_H + 26, M["void"], bevel=3)
    box("ec_cover", -124, 64, 36, 184, EC_H + 26, EC_H + 29, M["chrome"], bevel=1.5)
    box("ec_slot", 85, 95, 40, 190, EC_H + 2, EC_H + 2.4, M["void"])
    box("ec_slider", 80, 100, 120, 134, EC_H + 2, EC_H + 22, M["knob"], bevel=2)
    return group("tape_echo")


# ------------------------------------------------------------------ the guitar

def _body_outline(n=120):
    """A single-cut body outline (mm), star-shaped about the origin: a lower bout and a
    smaller upper bout, the treble horn cut away."""
    circles = ((Vector((0, -95)), 172.0), (Vector((0, 120)), 128.0))
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        d = Vector((math.cos(a), math.sin(a)))
        best = 0.0
        for c, r in circles:
            b = d.dot(c)
            disc = b * b - c.length_squared + r * r
            if disc >= 0:
                best = max(best, b + math.sqrt(disc))
        p = d * best
        if p.x > 30 and p.y > 150:   # the cutaway on the treble side
            p = Vector((30 + (p.x - 30) * 0.35, min(p.y, 175 + (p.x - 30) * 0.2)))
        pts.append(p)
    return pts


def guitar():
    """Lying in the XY plane, face up (+z), the neck along +y; the body's bottom edge at y = -267."""
    bm = bmesh.new()
    vs = [bm.verts.new((p.x, p.y, 0)) for p in _body_outline()]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    bmesh.ops.translate(bm, vec=Vector((0, 0, 50)), verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = _track(C.mesh_obj("gt_body", bm, M["sunburst"]))
    C.add_bevel(body, 6, 4)
    body.data.shade_smooth()
    body.data.set_sharp_from_angle(angle=math.radians(50))
    # cream binding round the top edge
    # neck, fretboard, frets, headstock
    box("gt_neck", -22, 22, 150, 640, 30, 50, M["neck"], bevel=4)
    box("gt_board", -21, 21, 150, 630, 50, 56, M["board"], bevel=1)
    scale = 628.0
    for k in range(1, 23):
        y = 630 - (scale - scale / 2 ** (k / 12)) * 0.78
        if y < 160:
            break
        box("gt_fret", -21, 21, y - 0.6, y + 0.6, 56, 57.3, M["fret"], bevel=0.3)
    for k in (3, 5, 7, 9, 12, 15, 17):  # dot inlays
        y = 630 - (scale - scale / 2 ** ((k - 0.5) / 12)) * 0.78
        if y > 160:
            cyl("gt_dot", 3, 0.4, (0, y, 56.2), M["cream"], segs=16)
    box("gt_nut", -22, 22, 630, 636, 50, 58, M["cream"], bevel=0.6)
    # the headstock: an open-book outline, set a little below the fretboard, six tuner posts on
    # its face (three a side) with the buttons out to the sides
    bm = bmesh.new()
    hp = [(-22, 636), (-40, 690), (-46, 780), (-30, 815), (0, 805), (30, 815), (46, 780), (40, 690), (22, 636)]
    vs = [bm.verts.new((x, y, 40)) for x, y in hp]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    bmesh.ops.translate(bm, vec=Vector((0, 0, 15)), verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    head = _track(C.mesh_obj("gt_head", bm, M["board"]))
    C.add_bevel(head, 2, 2)
    posts = []
    for k in range(3):
        y = 690 + k * 42
        for sx in (-1, 1):
            x = sx * (33 + k * 2)
            cyl("gt_post", 3.2, 10, (x, y, 59), M["chrome"], segs=16)
            tb = cyl("gt_tuner_btn", 8, 4, (x + sx * 24, y, 47), M["cream"], segs=16, bevel=1)
            tb.rotation_euler = (0, math.pi / 2, 0)
            cyl("gt_tuner_shaft", 2, 20, (x + sx * 12, y, 47), M["chrome"], axis="Y", segs=12).rotation_euler = (0, math.pi / 2, 0)
            posts.append((x, y))
    # pickups, bridge, tailpiece, knobs, switch
    for y in (35.0, 125.0):
        box("gt_pu_ring", -44, 44, y - 24, y + 24, 50, 54, M["cream"], bevel=2)
        box("gt_pu", -36, 36, y - 18, y + 18, 54, 60, M["chrome"], bevel=2)
    box("gt_bridge", -36, 36, -15, -5, 50, 60, M["chrome"], bevel=1.5)
    box("gt_tail", -40, 40, -62, -50, 50, 58, M["chrome"], bevel=3)
    for (x, y) in ((70, -60), (110, -30), (70, -125), (110, -95)):
        cyl("gt_knob", 10, 14, (x, y, 57), M["amber"], segs=32, bevel=1.5, r2=9)
    cyl("gt_switch", 3, 18, (-95, 175, 59), M["cream"], segs=16)
    # strings from the tailpiece over the bridge to the nut and on to the tuners
    # bass E on the left (k = 0) to treble E; each string to its post (bass side posts on the left)
    order = [posts[4], posts[2], posts[0], posts[1], posts[3], posts[5]]
    for k in range(6):
        x0 = -22 + k * 8.8
        x1 = -18 + k * 7.2
        r = 0.35 + 0.12 * (5 - k)
        px, py = order[k]
        _track(C.curve_obj("gt_string", [(x0, -56, 58), (x0, -10, 61), (x1, 633, 58.5), (px, py, 62)], r,
                           M["string"]))
    return group("guitar")


# ------------------------------------------------------------------ placement

def place(e, loc, rot_deg=(0, 0, 0), mirror=False):
    """mirror: flip left for right (the guitar is modelled as seen from behind, so a
    right-handed one faces the camera mirrored)."""
    e.location = Vector(loc)
    if mirror:
        e.scale = (-1, 1, 1)
    e.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    return e
