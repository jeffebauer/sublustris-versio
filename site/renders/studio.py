"""Story subjects, round 2 (owner's notes, 30 Oct 2026): researched, more
realistic, in a cosy late-70s / early-80s home studio. See STORY_RESEARCH.md
for the sources behind every choice.

  tape_cross()  two Swiss-style 1970s stereo open-reel decks (modelled on the
                Revox A77 Mk III layout: transport deck above with two reels,
                head cover, tension rollers and capstan; a grey control front
                below with transport keys, level knobs and two VU meters; a
                walnut case), each running its own tape between its own two
                reels. Their inputs and outputs are CROSS-PATCHED at the back
                (A out -> B in, B out -> A in), cables visible.
  studio_rack() the room with a wooden rack of period units: a digital reverb
                mainframe (224-era: black, a small LED display, program
                buttons, level pots, rack ears with screws, worn edges), its
                remote on the desk (sliders, LED display, buttons), a delay,
                a pitch shifter, a graphic EQ, a patch bay; a desk with a lamp,
                tape boxes, a small mixer; record shelves, a plant, a rug.

Everything is unbranded: no maker's name, logo or badge, no model numbers;
label strips are blank. Modelled from understanding of the designs (layout
and proportions), never traced from a photograph.

Scene units mm; z = 0 is the floor; the desk top is at DESK_Z; fronts face -Y.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

import common as C
import story

STORY_DIM = 1.0  # 0: the round-3 light; 1 (the final, owner 31 Oct 2026): the room falls away around the decks.
# The site's story-tape.jpg: --round 3 --only story_tape3_b --exact --samples 256 --res-scale 1.25 with IV_VIGNETTE=0.75

DESK_Z = 740.0
DESK = dict(x0=-950.0, x1=950.0, y0=-380.0, y1=380.0)
WALL_Y = 760.0       # back wall
SIDE_X = -2000.0     # left wall


# ------------------------------------------------------------------ materials

def _get(name):
    return bpy.data.materials.get(name)


def wood(name, rgb, rough=0.5, scale=0.012, stretch=(1.0, 8.0, 1.0)):
    """Wood with a soft grain: wave bands distorted by noise, +-12 % value.
    stretch: the grain runs along the axis with the largest factor."""
    m = _get(name)
    if m:
        return m
    m = C.clay(name, rgb=rgb, rough=rough)
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (scale / stretch[0], scale / stretch[1], scale / stretch[2])
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.wave_type = "RINGS"
    wave.inputs["Scale"].default_value = 3.0
    wave.inputs["Distortion"].default_value = 6.0
    wave.inputs["Detail"].default_value = 3.0
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], wave.inputs["Vector"])
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["To Min"].default_value = 0.82
    rng.inputs["To Max"].default_value = 1.1
    nt.links.new(wave.outputs["Fac"], rng.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    mul.inputs["A"].default_value = (*rgb, 1)
    comb = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(rng.outputs[0], comb.inputs[k])
    nt.links.new(comb.outputs[0], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], p.inputs["Base Color"])
    # the grain is felt as well as seen: darker late wood sits a touch lower and is less glossy
    gb = nt.nodes.new("ShaderNodeBump")
    gb.inputs["Strength"].default_value = 0.25
    gb.inputs["Distance"].default_value = 0.15
    nt.links.new(wave.outputs["Fac"], gb.inputs["Height"])
    nt.links.new(gb.outputs["Normal"], p.inputs["Normal"])
    gr = nt.nodes.new("ShaderNodeMapRange")
    gr.inputs["To Min"].default_value = rough * 1.25
    gr.inputs["To Max"].default_value = rough * 0.8
    nt.links.new(wave.outputs["Fac"], gr.inputs["Value"])
    nt.links.new(gr.outputs[0], p.inputs["Roughness"])
    return m


def finish(m, bump=0.15, scale=0.5, rough_var=0.12, coat=0.0, coat_rough=0.25, detail=8.0):
    """Surface realism on top of any material (owner, 31 Oct 2026: "realistic rather than
    clay-like"): a fine noise bump (orange peel, grain, fibre; scale in 1/mm, object space) and
    roughness that varies +-rough_var across the surface, plus an optional lacquer coat."""
    nt = m.node_tree
    p = _principled(m)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = scale
    nz.inputs["Detail"].default_value = detail
    nz.inputs["Roughness"].default_value = 0.6
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    if bump:
        bp = nt.nodes.new("ShaderNodeBump")
        bp.inputs["Strength"].default_value = bump
        bp.inputs["Distance"].default_value = 0.2   # mm
        nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
        nt.links.new(bp.outputs["Normal"], p.inputs["Normal"])
        if coat:
            nt.links.new(bp.outputs["Normal"], p.inputs["Coat Normal"])
    if rough_var:
        src = p.inputs["Roughness"].links[0].from_socket if p.inputs["Roughness"].is_linked else None
        base = p.inputs["Roughness"].default_value
        rv = nt.nodes.new("ShaderNodeMapRange")
        rv.inputs["To Min"].default_value = 1 - rough_var
        rv.inputs["To Max"].default_value = 1 + rough_var
        nt.links.new(nz.outputs["Fac"], rv.inputs["Value"])
        mul = nt.nodes.new("ShaderNodeMath")
        mul.operation = "MULTIPLY"
        mul.use_clamp = True
        if src:
            nt.links.new(src, mul.inputs[0])
        else:
            mul.inputs[0].default_value = base
        nt.links.new(rv.outputs[0], mul.inputs[1])
        nt.links.new(mul.outputs[0], p.inputs["Roughness"])
    if coat:
        p.inputs["Coat Weight"].default_value = coat
        p.inputs["Coat Roughness"].default_value = coat_rough
    return m


def worn(name, rgb, rough=0.45, edge=(0.42, 0.42, 0.42), metallic_edge=True):
    """Painted metal with use: edges rubbed back to bare metal (pointiness),
    a faint mottling of fingerprints and dust (noise on roughness)."""
    m = _get(name)
    if m:
        return m
    m = C.clay(name, rgb=rgb, rough=rough)
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.04    # ~25 mm patches (object space, mm)
    noise.inputs["Detail"].default_value = 6.0
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    # edge mask: high pointiness (convex bevels), broken up by noise
    ramp = nt.nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value = 0.62   # only true convex edges (flat faces sit at 0.5)
    ramp.inputs["From Max"].default_value = 0.72
    nt.links.new(geo.outputs["Pointiness"], ramp.inputs["Value"])
    brk = nt.nodes.new("ShaderNodeMath")
    brk.operation = "MULTIPLY"
    nt.links.new(ramp.outputs[0], brk.inputs[0])
    nt.links.new(noise.outputs["Fac"], brk.inputs[1])
    sharp = nt.nodes.new("ShaderNodeMapRange")
    sharp.inputs["From Min"].default_value = 0.3
    sharp.inputs["From Max"].default_value = 0.5
    sharp.inputs["To Max"].default_value = 0.6
    nt.links.new(brk.outputs[0], sharp.inputs["Value"])
    col = nt.nodes.new("ShaderNodeMix")
    col.data_type = "RGBA"
    col.inputs["A"].default_value = (*rgb, 1)
    col.inputs["B"].default_value = (*edge, 1)
    nt.links.new(sharp.outputs[0], col.inputs["Factor"])
    nt.links.new(col.outputs["Result"], p.inputs["Base Color"])
    if metallic_edge:
        nt.links.new(sharp.outputs[0], p.inputs["Metallic"])
    rr = nt.nodes.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = rough * 0.85
    rr.inputs["To Max"].default_value = min(1.0, rough * 1.15)
    nt.links.new(noise.outputs["Fac"], rr.inputs["Value"])
    nt.links.new(rr.outputs[0], p.inputs["Roughness"])
    return m


def mats():
    m = story.mats()
    m.update(
        walnut=wood("walnut", (0.16, 0.085, 0.045), 0.45),
        oak=wood("oak", (0.36, 0.22, 0.11), 0.55, stretch=(8.0, 1.0, 1.0)),
        floor=wood("floorboards", (0.30, 0.18, 0.09), 0.5, scale=0.006, stretch=(1.0, 10.0, 1.0)),
        deckplate=worn("deck_plate", (0.035, 0.035, 0.035), 0.55, edge=(0.25, 0.25, 0.25)),
        ctrlgrey=worn("deck_ctrl", (0.30, 0.30, 0.29), 0.4, edge=(0.6, 0.6, 0.6)),
        reeltable=C.clay("reel_table", 0.32, 0.35),
        rackblack=worn("rack_black", (0.022, 0.022, 0.024), 0.42, edge=(0.55, 0.55, 0.55)),
        rackgrey=worn("rack_grey", (0.20, 0.20, 0.21), 0.4, edge=(0.6, 0.6, 0.6)),
        cream=C.clay("cream", rgb=(0.62, 0.57, 0.46), rough=0.45),
        cap_black=C.clay("cap_black", 0.02, 0.35),
        cap_grey=C.clay("cap_grey", 0.30, 0.4),
        red_btn=C.clay("red_btn", rgb=(0.45, 0.03, 0.02), rough=0.35),
        wall=C.clay("wall", rgb=(0.40, 0.36, 0.30), rough=0.9),
        rug=C.clay("rug", rgb=(0.22, 0.07, 0.05), rough=0.95),
        cable=C.clay("cable", 0.02, 0.45),
        cable_grey=C.clay("cable_grey", 0.12, 0.45),
        gold=C.steel("gold", 0.7, 0.25),
        paper=C.clay("paper", rgb=(0.70, 0.66, 0.56), rough=0.8),
        leaf=C.clay("leaf", rgb=(0.05, 0.13, 0.04), rough=0.5),
        terracotta=C.clay("terracotta", rgb=(0.35, 0.14, 0.07), rough=0.85),
    )
    return m


# ------------------------------------------------------------------ helpers

def B(name, x0, x1, y0, y1, z0, z1, mat, o=(0, 0, 0), bevel=0.0, rot=None):
    ob = C.box(name, (x1 - x0, y1 - y0, z1 - z0),
               (o[0] + (x0 + x1) / 2, o[1] + (y0 + y1) / 2, o[2] + (z0 + z1) / 2), mat, bevel=bevel)
    if rot:
        ob.rotation_euler = rot
    return ob


def knob(name, x, z, r, h, mat, o=(0, 0, 0), y_face=0.0, skirt=None, pointer=None, ang=0.0):
    """A knob on a vertical face (axis -Y), optional skirt and pointer line."""
    C.cylinder(name, r, h, (o[0] + x, o[1] + y_face - h / 2, o[2] + z), mat, axis="X", segs=48, r2=r * 0.92,
               bevel=min(1.2, r * 0.12))
    if skirt:
        C.cylinder(name + "_skirt", r * 1.3, 1.8, (o[0] + x, o[1] + y_face - 0.9, o[2] + z), skirt, axis="X",
                   segs=48, bevel=0.4)
    if pointer:
        a = math.radians(ang)
        C.box(name + "_ptr", (0.9, 0.3, r * 0.6), (o[0] + x + r * 0.5 * math.sin(a), o[1] + y_face - h - 0.05,
                                                     o[2] + z + r * 0.5 * math.cos(a)), pointer, rot=(0, a, 0))


def screw(name, x, z, o, mat, r=2.6):
    """A pan-head cross screw on a vertical face."""
    head = C.cylinder(name, r, 1.4, (o[0] + x, o[1] - 0.7, o[2] + z), mat, axis="X", segs=24, bevel=0.5)
    C.boolean_cut(head, [C.box("x1", (r * 1.3, 1.0, 0.45), (o[0] + x, o[1] - 1.45, o[2] + z)),
                         C.box("x2", (0.45, 1.0, r * 1.3), (o[0] + x, o[1] - 1.45, o[2] + z))])


def digits(text, x, z, h, o, y, mat, pitch=None):
    """Seven-segment digits (story._digit) left to right from x."""
    pitch = pitch or h * 0.75
    i = 0
    for ch in text:
        if ch == ".":
            C.box("dp", (h * 0.12, 0.3, h * 0.12), (o[0] + x + (i - 1) * pitch + h * 0.36, o[1] + y,
                                                    o[2] + z - h * 0.45), mat)
            continue
        story._digit(ch, o[0] + x + i * pitch, o[2] + z, o[1] + y, h, mat)
        i += 1


def drum_digit(ch, x, z, y, h, m):
    """One wheel of a mechanical counter: a black drum segment with a printed white numeral,
    facing -Y, centred at (x, z)."""
    C.cylinder("counter_drum", 5.0, 7.6, (x, y + 5.0, z), m["void"], axis="Y", segs=32)
    cu = bpy.data.curves.new("counter_num", "FONT")
    cu.body = ch
    cu.size = h
    cu.align_x, cu.align_y = "CENTER", "CENTER"
    ob = bpy.data.objects.new("counter_num", cu)
    C.link(ob)
    ob.location = (x, y - 0.05, z)
    ob.rotation_euler = (math.pi / 2, 0, 0)
    C.assign(ob, C.clay("counter_white", 0.78, 0.55))


def _digit_top(ch, x, y, h, mat, parent):
    """A seven-segment digit lying in a parent's local XY plane (a sloped top)."""
    w, t = h * 0.5, h * 0.11
    segs = dict(a=(0, h / 2, w, t), d=(0, -h / 2, w, t), g=(0, 0, w, t),
                b=(w / 2, h / 4, t, h / 2), c=(w / 2, -h / 4, t, h / 2),
                f=(-w / 2, h / 4, t, h / 2), e=(-w / 2, -h / 4, t, h / 2))
    for sname in story.SEG[ch]:
        dx, dy, sx, sy = segs[sname]
        ob = C.box("seg_top", (sx * 0.86, sy * 0.86, 0.3), (x + dx, y + dy, 1.0), mat)
        ob.parent = parent


def cable(name, pts, r, mat):
    return C.curve_obj(name, [tuple(p) for p in pts], r, mat, resolution=4, spline="NURBS")


# ------------------------------------------------------------------ the room

def room(m, rack=False, lamp_x=-760.0, show_lamp=True, plant=(-1300.0, 300.0)):
    """Floorboards, two walls, a rug, the desk, a desk lamp (the main light),
    a record shelf, a plant. Returns the lamp's bulb position."""
    C.box("floor", (6000, 5000, 20), (0, 0, -10), m["floor"])
    C.box("wall_back", (6000, 40, 3000), (0, WALL_Y + 20, 1500), m["wall"])
    C.box("wall_side", (40, 5000, 3000), (SIDE_X - 20, 0, 1500), m["wall"])
    C.box("skirting", (6000, 16, 90), (0, WALL_Y - 8, 45), m["oak"])
    C.box("rug", (2400, 1700, 8), (0, -200, 4), m["rug"], bevel=2)
    # desk: oak top on two drawer pedestals
    d = DESK
    B("desk_top", d["x0"], d["x1"], d["y0"], d["y1"], DESK_Z - 32, DESK_Z, m["oak"], bevel=3)
    for sx in (-1, 1):
        x = sx * 720
        B("pedestal", x - 220, x + 220, d["y0"] + 30, d["y1"] - 20, 0, DESK_Z - 32, m["oak"], bevel=3)
        for k in range(3):
            z0 = 40 + k * 225
            B("drawer", x - 205, x + 205, d["y0"] + 24, d["y0"] + 32, z0, z0 + 210, m["oak"], bevel=2)
            B("pull", x - 40, x + 40, d["y0"] + 12, d["y0"] + 24, z0 + 170, z0 + 182, m["steel"], bevel=2)
    # desk lamp, back left: weighted base, arm, a cone shade with a warm bulb
    lx, ly = lamp_x, 230.0
    bulb = Vector((lx + 215, ly - 100, DESK_Z + 370))
    if show_lamp:  # (without it the warm light stays, as from a lamp out of frame)
        C.cylinder("lamp_base", 75, 22, (lx, ly, DESK_Z + 11), m["knob"], segs=48, bevel=4)
        arm = [(lx, ly, DESK_Z + 20), (lx + 20, ly - 10, DESK_Z + 300), (lx + 140, ly - 60, DESK_Z + 430)]
        cable("lamp_arm", arm, 5, m["steel"])
        shade = C.cylinder("lamp_shade", 85, 140, (lx + 200, ly - 90, DESK_Z + 400), m["knob"], segs=48, r2=28)
        shade.rotation_euler = (math.radians(-25), math.radians(35), 0)
        C.sphere("bulb", 22, bulb, story.glow("bulb", "#FFC27A", 22.0))
    # record / tape shelf against the back wall, left: records upright, tape boxes stacked
    sx0, sx1 = -1850.0, -1150.0
    for k in range(5):
        z = 30 + k * 360
        B("shelf", sx0, sx1, WALL_Y - 330, WALL_Y, z, z + 22, m["oak"], bevel=2)
    for sx in (sx0, sx1):
        B("shelf_side", sx - 11, sx + 11, WALL_Y - 330, WALL_Y, 0, 30 + 4 * 360 + 22, m["oak"], bevel=2)
    import random
    rng = random.Random(3)
    palette = [(0.55, 0.50, 0.40), (0.15, 0.15, 0.16), (0.42, 0.12, 0.08), (0.62, 0.58, 0.50), (0.12, 0.20, 0.30),
               (0.70, 0.62, 0.30), (0.30, 0.30, 0.28)]
    sleeves = [C.clay(f"sleeve{i}", rgb=c, rough=0.6) for i, c in enumerate(palette)]
    for k in range(4):
        z = 52 + k * 360
        x = sx0 + 20
        while x < sx1 - 30:
            if k % 2 == 0 or x < (sx0 + sx1) / 2:  # records (LP spines)
                t = rng.uniform(3, 6)
                lean = rng.uniform(-0.03, 0.03) if rng.random() < 0.2 else 0
                B("lp", x, x + t, WALL_Y - 320, WALL_Y - 12, z, z + 314, rng.choice(sleeves), rot=None)
                x += t + 0.4
            else:  # reel-tape boxes, flat stacks (7-inch, 185 mm square)
                n = rng.randint(4, 9)
                for j in range(n):
                    B("tapebox", x, x + 185, WALL_Y - 300, WALL_Y - 115, z + j * 24, z + j * 24 + 23,
                      rng.choice(sleeves[:1] + [m["paper"]]), bevel=1.5)
                x += 200
    # a plant on the floor between the shelf and the desk
    px, py = plant
    C.cylinder("pot", 140, 260, (px, py, 130), m["terracotta"], segs=48, r2=170, bevel=4)
    C.cylinder("soil", 160, 10, (px, py, 255), m["rubber"], segs=48)
    for k in range(14):
        a = 2 * math.pi * k / 14 + rng.uniform(-0.2, 0.2)
        h = rng.uniform(380, 820)
        tip = (px + math.cos(a) * rng.uniform(220, 420), py + math.sin(a) * rng.uniform(220, 420), 255 + h)
        mid = (px + math.cos(a) * 60, py + math.sin(a) * 60, 255 + h * 0.6)
        cable(f"stem{k}", [(px, py, 255), mid, tip], 3, m["leaf"])
        leaf = C.sphere(f"leaf{k}", 1.0, tip, m["leaf"])
        leaf.scale = (110, 38, 6)
        leaf.rotation_euler = (rng.uniform(-0.6, 0.6), rng.uniform(-0.5, 0.5), a)
    # a framed picture on the back wall (blank, a soft print colour)
    B("frame", -250, 350, WALL_Y - 25, WALL_Y, 1350, 1800, m["walnut"], bevel=4)
    B("print", -225, 325, WALL_Y - 27, WALL_Y - 24, 1375, 1775, C.clay("print", rgb=(0.48, 0.42, 0.30), rough=0.8))
    return bulb


def room_lights(bulb, size, warm=1.0, window=0.25, dim=0.0):
    """The lamp (a warm point light in the shade), a dim cool window light from
    the left, low warm world bounce. dim (0..1) darkens the room around the lamp's pool."""
    C.world(0.03 * (1 - 0.9 * dim), rgb=(0.55, 0.45, 0.35))
    window *= 1 - 0.9 * dim
    ld = bpy.data.lights.new("lamp", "POINT")
    ld.energy = C.watts(60 * warm * (1 - 0.4 * dim))  # less spill up the wall
    ld.color = (1.0, 0.72, 0.45)
    ld.shadow_soft_size = 20
    ob = bpy.data.objects.new("lamp", ld)
    C.link(ob)
    ob.location = bulb
    C.area_light("window", Vector((SIDE_X + 60, -600, 1700)), Vector((0, 100, DESK_Z)), size=1400,
                 power=C.watts(C.KEY_W * window) * (2.2) ** 2, rgb=(0.75, 0.82, 1.0))
    # a soft warm fill from behind the camera so faces don't go black
    C.area_light("fill", Vector((600, -2600, 1500)), Vector((0, 0, DESK_Z + 150)), size=1200,
                 power=C.watts(C.KEY_W * 0.12 * (1 - 0.85 * dim)) * (3.0) ** 2, rgb=(1.0, 0.85, 0.7))


# ------------------------------------------------------------------ the deck

CASE_W, CASE_H, CASE_D = 440.0, 380.0, 205.0
REEL_DX, REEL_Z, REEL_R = 108.0, 262.0, 89.0   # 7-inch reels
TAPE_Z = 172.0                                  # tape height across the heads (deck-local)


def deck(o, m, name="deck", supply=70.0, takeup=40.0):
    """A 1970s Swiss-style stereo deck in a walnut case, upright, front at
    y = o.y. o = the case's bottom-front centre. Returns (rear socket points
    {out_l, out_r, in_l, in_r} in world, reel centres)."""
    o = Vector(o)
    w2 = CASE_W / 2
    # walnut case: sides, top, bottom, and a recessed front
    B(f"{name}_case", -w2, w2, 0, CASE_D, 0, CASE_H, m["walnut"], o, bevel=4)
    # transport deck (black, textured) and the grey control front
    B(f"{name}_transport", -w2 + 14, w2 - 14, -2, 2, 122, CASE_H - 12, m["deckplate"], o, bevel=1.5)
    B(f"{name}_ctrl", -w2 + 14, w2 - 14, -2, 2, 12, 116, m["ctrlgrey"], o, bevel=1.5)
    # reels: grey reel tables, then the reels on them
    for s in (-1, 1):
        x = s * REEL_DX
        C.cylinder(f"{name}_table", 52, 5, (o.x + x, o.y - 4.5, o.z + REEL_Z), m["reeltable"], axis="X", segs=64,
                   bevel=1.2)
        pack = supply if s < 0 else takeup
        story.reel(f"{name}_reel", o.x + x, o.z + REEL_Z, o.y - 6, REEL_R, pack, 27.0, m["alu"], m)
    yc = o.y - 6 - 14.0                           # tape centre plane (story.reel)
    # head block: a dark cover with a window to the tape, three heads, a lifter
    B(f"{name}_headcover", -58, 58, -40, -2, 132, TAPE_Z - 3, m["deckplate"], o, bevel=5)
    for k, hx in enumerate((-30, 0, 30)):
        B(f"{name}_head{k}", hx - 7, hx + 7, -34, -10, TAPE_Z - 18, TAPE_Z - 0.4, m["steel"], o, bevel=2.5)
    # capstan right of the heads, the rubber pinch roller above the tape
    C.cylinder(f"{name}_capstan", 2.6, 34, (o.x + 82, yc, o.z + TAPE_Z - 2.7), m["steel"], axis="X", segs=24)
    C.cylinder(f"{name}_pinch", 13.0, 14, (o.x + 82, yc, o.z + TAPE_Z + 13.2), m["rubber"], axis="X", segs=48,
               bevel=1.5)
    B(f"{name}_pinch_arm", 70, 130, -12, -4, TAPE_Z + 8, TAPE_Z + 18, m["alu"], o, bevel=2)
    # tension rollers on arms, lower left and right
    rollers = []
    for s in (-1, 1):
        rc = Vector((o.x + s * 152, yc, o.z + TAPE_Z + 9))
        C.cylinder(f"{name}_roller", 8.5, 16, tuple(rc), m["steel"], axis="X", segs=32, bevel=1)
        B(f"{name}_arm", s * 152 - 4, s * 152 + 4, -6, -2, TAPE_Z + 9, TAPE_Z + 60, m["alu"], o, bevel=1.5)
        rollers.append(rc)
    # counter window between the reels, near the top, with its reset key
    B(f"{name}_counter", -20, 20, -4, -1, 322, 340, m["void"], o, bevel=1.2)
    # A77 counters are mechanical: white numerals printed on black drums (not seven-segment)
    for i, ch in enumerate("0127"):
        drum_digit(ch, o.x - 12.75 + i * 8.5, o.z + 331, o.y - 4.2, 8.0, m)
    B(f"{name}_reset", 26, 34, -6, -2, 327, 335, m["cap_grey"], o, bevel=1.5)
    # --- control front: transport keys left, knobs middle, meters right
    keys = [m["cap_black"]] * 4 + [m["red_btn"]]
    for k, km in enumerate(keys):
        x = -190 + k * 23
        B(f"{name}_key{k}", x, x + 19, -9, -2, 66, 84, km, o, bevel=1.8)
    for k in range(2):  # power and speed: small bat toggles
        x = -180 + k * 50
        C.cylinder(f"{name}_tog_bush", 4.5, 5, (o.x + x, o.y - 4.5, o.z + 34), m["steel"], axis="X", segs=24)
        lev = C.cylinder(f"{name}_tog", 1.4, 14, (o.x + x, o.y - 13, o.z + 37), m["steel"], axis="X", segs=16, r2=1.1)
        lev.rotation_euler = (math.pi / 2 - 0.35, 0, 0)
    for k, x in enumerate((-55, -15)):  # input selectors (skirted)
        knob(f"{name}_sel{k}", x, 72, 10, 11, m["cap_black"], o, y_face=-2, skirt=m["alu"], pointer=m["cream"],
             ang=-40 + 50 * k)
    for k, x in enumerate((25, 62)):   # dual concentric level knobs
        knob(f"{name}_lvl{k}", x, 72, 13, 8, m["cap_black"], o, y_face=-2, skirt=m["alu"])
        knob(f"{name}_lvlin{k}", x, 72, 8, 16, m["cap_grey"], o, y_face=-2, pointer=m["cream"], ang=30)
    vu = story.glow("vu_face_warm", "#F0D8A8", 0.8)
    for k, x in enumerate((118, 172)):  # two VU meters, a red record key beside each
        B(f"{name}_vu_bezel", x - 25, x + 25, -6, -2, 52, 98, m["cap_black"], o, bevel=2)
        B(f"{name}_vu", x - 21, x + 21, -6.5, -5.8, 56, 92, vu, o)
        C.box(f"{name}_needle", (0.6, 0.3, 30), (o.x + x - 5, o.y - 6.8, o.z + 70), m["knob"],
              rot=(0, math.radians(-28 + 10 * k), 0))
        B(f"{name}_rec{k}", x - 7, x + 7, -8, -2, 30, 42, m["red_btn"], o, bevel=1.8)
    C.cylinder(f"{name}_phones", 5, 4, (o.x + 82, o.y - 3, o.z + 32), m["steel"], axis="X", segs=24)
    C.cylinder(f"{name}_phones_hole", 3, 1, (o.x + 82, o.y - 5.2, o.z + 32), m["void"], axis="X", segs=16)
    # rear panel with phono sockets (in L/R, out L/R) low on the back
    B(f"{name}_rear", -w2 + 14, w2 - 14, CASE_D - 1, CASE_D + 1, 12, 120, m["ctrlgrey"], o, bevel=1)
    sockets = {}
    for k, key in enumerate(("in_l", "in_r", "out_l", "out_r")):
        x = 60 + k * 26
        p = Vector((o.x + x * (1 if name.endswith("A") else -1), o.y + CASE_D + 2, o.z + 70))
        C.cylinder(f"{name}_sock", 4.5, 6, tuple(p), m["gold"], axis="X", segs=24)
        sockets[key] = p
    # the two tapes: supply -> left roller -> heads -> capstan -> right roller -> take-up
    sup_c = (o.x - REEL_DX, o.z + REEL_Z)
    tak_c = (o.x + REEL_DX, o.z + REEL_Z)
    rl, rr = rollers
    gr = 8.5 + 0.2
    pts = [story._tangent(sup_c, supply, (rl.x - gr, rl.z), cw=False)]
    pts += story._wrap((rl.x, rl.z), gr, math.pi, 1.5 * math.pi, 8)
    pts += [(o.x - 45, o.z + TAPE_Z), (o.x + 45, o.z + TAPE_Z), (o.x + 82, o.z + TAPE_Z)]
    pts += story._wrap((rr.x, rr.z), gr, 1.5 * math.pi, 2 * math.pi, 8)
    pts += [story._tangent(tak_c, takeup, (rr.x + gr, rr.z), cw=True)]
    story.tape_strip(f"{name}_tape", pts, yc)
    return sockets


def rca(name, p, direction, m, mat):
    """A phono plug pushed onto a socket at p, its body pointing along +Y
    (out of the back panel). Returns the cable start point."""
    C.cylinder(f"{name}_shell", 4.4, 10, (p.x, p.y + 5, p.z), m["gold"], axis="X", segs=24)
    C.cylinder(f"{name}_body", 4.2, 22, (p.x, p.y + 21, p.z), mat, axis="X", segs=24, r2=3.6, bevel=1)
    return Vector((p.x, p.y + 32, p.z))


def tape_cross():
    """Two decks side by side on the desk, each running its own tape between
    its own reels; A's outputs patched into B's inputs and B's outputs back
    into A's inputs at the back, the cables looping across the gap."""
    m = mats()
    bulb = room(m)
    gap = 150.0
    xa, xb = -(CASE_W + gap) / 2, (CASE_W + gap) / 2
    y0 = -120.0
    a = deck((xa, y0, DESK_Z), m, "deck_A", supply=74.0, takeup=46.0)
    b = deck((xb, y0, DESK_Z), m, "deck_B", supply=62.0, takeup=56.0)
    # cross-patch: A out L/R -> B in L/R (black), B out L/R -> A in L/R (grey)
    ztab = DESK_Z + 5
    for src, dst, mat, tag, dz in ((a["out_l"], b["in_l"], m["cable"], "AB_L", 0),
                                   (a["out_r"], b["in_r"], m["cable"], "AB_R", 6),
                                   (b["out_l"], a["in_l"], m["cable_grey"], "BA_L", 12),
                                   (b["out_r"], a["in_r"], m["cable_grey"], "BA_R", 18)):
        p0 = rca(f"plug_{tag}_s", src, 1, m, mat)
        p1 = rca(f"plug_{tag}_d", dst, 1, m, mat)
        mid_x = (p0.x + p1.x) / 2
        pts = [p0, p0 + Vector((0, 30, -10)), Vector((p0.x + (mid_x - p0.x) * 0.3, p0.y + 90 + dz, ztab + 3)),
               Vector((mid_x, p0.y + 140 + dz * 2, ztab + 2)),
               Vector((p1.x + (mid_x - p1.x) * 0.3, p1.y + 90 + dz, ztab + 3)), p1 + Vector((0, 30, -10)), p1]
        cable(f"xpatch_{tag}", pts, 2.4, mat)
    # on the desk: stacked tape boxes, a notebook, a pencil
    for j in range(3):
        B("desk_tapebox", 520, 705, -300 + j * 6, -115 + j * 6, DESK_Z + j * 24, DESK_Z + j * 24 + 23,
          m["paper"] if j % 2 else C.clay("box_dark", rgb=(0.18, 0.12, 0.08), rough=0.6), bevel=1.5)
    B("notebook", -720, -520, -330, -190, DESK_Z, DESK_Z + 9, m["paper"], bevel=1.5)
    room_lights(bulb, CASE_W)
    # a soft light low behind the decks (off the back wall) so the patch reads
    C.area_light("back_bounce", Vector((150, 600, DESK_Z + 500)), Vector((0, 150, DESK_Z + 60)), size=500,
                 power=C.watts(C.KEY_W * 0.25) * (0.7) ** 2, rgb=(1.0, 0.85, 0.7))
    return dict(center=Vector((0, y0 + 80, DESK_Z + 190)), size=2 * CASE_W + gap,
                radius=(2 * CASE_W + gap) / 2 + 40, deck_a=Vector((xa, y0, DESK_Z + 190)),
                deck_b=Vector((xb, y0, DESK_Z + 190)), heads_a=Vector((xa, y0 - 20, DESK_Z + TAPE_Z)),
                patch=Vector((0, y0 + CASE_D + 120, DESK_Z + 40)))


# ------------------------------------------------------------------ rack units

U = 44.45
RW, RB = 482.6, 430.0   # panel width with ears, body width


def _panel(name, z0, units, mat, o, m, depth=280):
    """A rack panel at bottom z0 with ears, four screws and a body behind."""
    h = units * U - 0.8
    B(f"{name}_panel", -RW / 2, RW / 2, -3, 0, z0, z0 + h, mat, o, bevel=0.8)
    B(f"{name}_body", -RB / 2, RB / 2, 0, depth, z0 + 0.6, z0 + h - 0.6, m["rackgrey"], o, bevel=1)
    for s in (-1, 1):
        for dz in ((h / 2,) if units == 1 else (U / 2 - 0.4, h - U / 2 + 0.4)):
            screw(f"{name}_screw", s * 232, z0 + dz, Vector(o) + Vector((0, -3, 0)), m["steel"])
    return z0 + h / 2, h


def reverb_unit(z0, o, m):
    """The digital reverb mainframe, 3U, black: power switch with a red pilot,
    a small red LED display, two rows of program buttons with indicator LEDs,
    input/output level pots, a pair of LED headroom ladders; worn edges."""
    zc, h = _panel("rev", z0, 3, m["rackblack"], o, m, depth=380)
    oy = Vector(o) + Vector((0, -3, 0))
    red = story.glow("rev_red", "#FF3A1C", 10.0)
    green = story.glow("rev_green", "#6CFF7E", 4.0)
    amber = story.glow("rev_amber", "#FFB12E", 6.0)
    # power rocker + pilot
    B("rev_power", -205, -188, -5, 0, zc - 14, zc + 14, m["cap_black"], oy, bevel=2)
    C.cylinder("rev_pilot", 2.2, 2, (oy[0] - 196, oy[1] - 1.2, oy[2] + zc + 26), red, axis="X", segs=16)
    # display window with four red digits: the decay time, "99.9" = held
    B("rev_window", -160, -70, -3.5, 0, zc + 4, zc + 36, m["void"], oy, bevel=1.5)
    digits("99.9", -150, zc + 20, 19, oy, -3.8, red, pitch=15)
    # headroom ladders: two rows of 10 small LEDs under the window
    for row, dz in enumerate((-12, -22)):
        for k in range(10):
            mat = red if k >= 9 else (amber if k >= 7 else (green if k < 5 else m["void"]))
            C.box("rev_led", (5, 1.2, 3), (oy[0] - 155 + k * 8.4, oy[1] - 0.6, oy[2] + zc + dz), mat)
    # program buttons: two rows of 8, square, cream and dark, an LED above each
    for row in range(2):
        for k in range(8):
            x = -45 + k * 22
            z = zc + 18 - row * 30
            capm = m["cream"] if (k % 4) < 2 else m["cap_grey"]
            B("rev_btn", x - 7.5, x + 7.5, -8, 0, z - 7, z + 7, capm, oy, bevel=1.6)
            lit = (row == 0 and k == 5) or (row == 1 and k == 2)
            C.cylinder("rev_btn_led", 1.2, 1, (oy[0] + x, oy[1] - 0.6, oy[2] + z + 11), amber if lit else m["void"],
                       axis="X", segs=12)
    # level pots: input L/R, output
    for k, x in enumerate((150, 178, 206)):
        knob("rev_pot", x, zc + 6, 6.5, 9, m["cap_black"], oy, y_face=0, pointer=m["cream"], ang=-60 + 40 * k)
    # a blank label strip along the bottom
    B("rev_strip", -170, 120, -0.6, 0.2, z0 + 8, z0 + 14, m["cap_grey"], oy)
    return zc


def delay_unit(z0, o, m):
    """A 2U digital delay: delay-time digits, a row of small push buttons,
    four knobs, a thin feedback slider."""
    zc, h = _panel("dly", z0, 2, m["rackgrey"], o, m)
    oy = Vector(o) + Vector((0, -3, 0))
    red = story.glow("dly_red", "#FF3A1C", 9.0)
    B("dly_window", -170, -100, -3, 0, zc - 10, zc + 16, m["void"], oy, bevel=1.2)
    digits("400", -160, zc + 3, 16, oy, -3.3, red, pitch=14)
    for k in range(6):
        x = -80 + k * 18
        B("dly_btn", x - 6, x + 6, -6, 0, zc - 6, zc + 6, m["cap_black"], oy, bevel=1.4)
    for k, x in enumerate((60, 100, 140, 180)):
        knob("dly_knob", x, zc, 9, 11, m["cap_black"], oy, y_face=0, skirt=m["alu"], pointer=m["cream"],
             ang=-90 + 50 * k)
    B("dly_slot", 200, 206, -0.8, 0.2, zc - 25, zc + 25, m["void"], oy)
    B("dly_slider", 197, 209, -7, 0, zc + 4, zc + 12, m["cap_grey"], oy, bevel=1.5)
    return zc


def pitch_unit(z0, o, m):
    """A 2U pitch shifter: a large four-digit display, a row of knobs."""
    zc, h = _panel("pit", z0, 2, m["rackblack"], o, m)
    oy = Vector(o) + Vector((0, -3, 0))
    red = story.glow("pit_red", "#FF4422", 8.0)
    B("pit_window", -60, 60, -3, 0, zc - 15, zc + 15, m["void"], oy, bevel=1.5)
    digits("1.000", -50, zc, 20, oy, -3.3, red, pitch=26)
    for k, x in enumerate((-185, -150, -115, 100, 140, 180)):
        knob("pit_knob", x, zc, 8, 10, m["cap_black"], oy, y_face=0, pointer=m["cream"], ang=-30 + 25 * k)
    return zc


def eq_unit(z0, o, m):
    """A 2U stereo graphic EQ: two banks of ten short sliders."""
    zc, h = _panel("eq", z0, 2, m["rackgrey"], o, m)
    oy = Vector(o) + Vector((0, -3, 0))
    import random
    rng = random.Random(5)
    for bank, x0 in enumerate((-195, 15)):
        for k in range(10):
            x = x0 + k * 18
            B("eq_slot", x - 1.2, x + 1.2, -0.8, 0.2, zc - 26, zc + 26, m["void"], oy)
            z = zc + rng.uniform(-14, 14)
            B("eq_cap", x - 5, x + 5, -9, 0, z - 4, z + 4, m["cap_black"], oy, bevel=1.4)
    return zc


def patch_bay(z0, o, m):
    zc, h = _panel("pb", z0, 1, m["rackblack"], o, m, depth=60)
    oy = Vector(o) + Vector((0, -3, 0))
    for row in (0.72, 0.28):
        for k in range(24):
            x = -205 + k * 17.8
            C.cylinder("pb_jack", 3.0, 1.2, (oy[0] + x, oy[1] - 0.6, oy[2] + z0 + h * row), m["alu"], axis="X",
                       segs=20)
            C.cylinder("pb_hole", 1.9, 1.0, (oy[0] + x, oy[1] - 1.3, oy[2] + z0 + h * row), m["void"], axis="X",
                       segs=16)
    B("pb_strip", -212, 212, -0.6, 0.2, zc - 2.5, zc + 2.5, m["paper"], oy)
    # a few patch cords between rows
    for k, (a, b) in enumerate(((3, 9), (5, 6), (12, 20))):
        pa = Vector((oy[0] - 205 + a * 17.8, oy[1] - 2, oy[2] + z0 + h * 0.72))
        pb = Vector((oy[0] - 205 + b * 17.8, oy[1] - 2, oy[2] + z0 + h * 0.28))
        for p in (pa, pb):
            C.cylinder("pb_plug", 3.6, 14, (p.x, p.y - 7, p.z), m["cap_black"], axis="X", segs=16)
        cable(f"pb_cord{k}", [pa + Vector((0, -14, 0)), (pa + pb) / 2 + Vector((0, -45, -30)),
                              pb + Vector((0, -14, 0))], 1.6, m["red_btn"] if k == 1 else m["cable"])


def remote(o, m, yaw=0.0):
    """The reverb's remote on the desk: a wedge, black top with a cream lower
    band, six sliders, a small red display, program buttons. o = bottom
    front-centre on the desk. Returns the display centre."""
    o = Vector(o)
    W, D, h0, h1 = 300.0, 190.0, 28.0, 60.0
    bm = bmesh.new()
    prof = [(0, 0), (D, 0), (D, h1), (0, h0)]
    verts = []
    for x in (-W / 2, W / 2):
        verts.append([bm.verts.new((x, y, z)) for y, z in prof])
    bm.faces.new(verts[0][::-1])
    bm.faces.new(verts[1])
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((verts[0][i], verts[0][j], verts[1][j], verts[1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = C.mesh_obj("remote_body", bm, m["rackblack"])
    C.add_bevel(body, 2.5, 3)
    body.location = o
    body.rotation_euler = (0, 0, yaw)
    # parts in the remote's sloped top frame: an Empty parent carries the slope
    slope = math.atan2(h1 - h0, D)
    fr = bpy.data.objects.new("remote_frame", None)
    C.link(fr)
    fr.location = o + Vector((0, 0, h0))
    fr.rotation_euler = (slope, 0, yaw)

    def top(name, ob):
        ob.parent = fr
        return ob

    red = story.glow("rem_red", "#FF3A1C", 9.0)
    # cream band at the front with six sliders
    top("band", C.box("rem_band", (W - 16, D * 0.5, 0.8), (0, D * 0.27, 0.3), m["cream"]))
    for k in range(6):
        x = -W / 2 + 40 + k * 44
        top("slot", C.box("rem_slot", (2.6, 70, 0.6), (x, D * 0.27, 0.75), m["void"]))
        top("cap", C.box("rem_cap", (14, 9, 9), (x, D * 0.12 + 12 * (k % 3), 5), m["cap_black"], bevel=1.5))
    # display + buttons on the black upper half
    top("win", C.box("rem_win", (76, 22, 0.8), (-80, D * 0.75, 0.4), m["void"], bevel=1))
    for i, ch in enumerate("700"):
        _digit_top(ch, -100 + i * 18, D * 0.75, 15, red, fr)
    for k in range(10):
        x = -10 + (k % 5) * 26
        y = D * 0.68 + (k // 5) * 24
        top("btn", C.box("rem_btn", (17, 12, 5), (x, y, 2.5), m["cream"] if k < 5 else m["cap_grey"], bevel=1.5))
    return o + Vector((-80, D * 0.75, h0 + (h1 - h0) * 0.75))


def studio_rack(decks=False):
    """The room with a wooden rack beside the desk: patch bay, reverb, delay,
    pitch shifter, graphic EQ, blanks; the reverb's remote and a small mixer
    on the desk, its cable running to the rack.
    decks (round 3): the two A77-layout decks stand at the back of the desk,
    cross-patched, reels loaded (the lamp moves 70 mm left to clear them)."""
    m = mats3() if decks else mats()
    bulb = room(m, rack=True, lamp_x=-830.0 if decks else -760.0)
    # rack cabinet: walnut sides on a plinth, right of the desk
    rx, ry = 1340.0, -220.0
    n = 16
    base = 140.0
    inner = n * U
    side_t = 22.0
    xo = RW / 2 + side_t / 2 - 5
    for s in (-1, 1):
        B("rack_side", s * xo - side_t / 2, s * xo + side_t / 2, -10, 520, 0, base + inner + 30, m["walnut"],
          (rx, ry, 0), bevel=3)
        B("rack_rail", s * (RW / 2 - 14) - 8, s * (RW / 2 - 14) + 8, 0.5, 4.5, base, base + inner, m["steel"],
          (rx, ry, 0), bevel=0.5)
    B("rack_top", -xo - side_t / 2, xo + side_t / 2, -10, 520, base + inner + 30, base + inner + 52, m["walnut"],
      (rx, ry, 0), bevel=3)
    B("rack_plinth", -xo + side_t / 2, xo - side_t / 2, 10, 500, 0, base, m["void"], (rx, ry, 0), bevel=1)
    o = (rx, ry, 0)
    z = base
    blank = lambda units, mat: _panel("blank", z, units, mat, o, m, depth=40)  # noqa: E731
    blank(1, m["rackblack"]); z += U
    eq_unit(z, o, m); z += 2 * U
    blank(1, m["rackgrey"]); z += U
    pitch_unit(z, o, m); z += 2 * U
    delay_unit(z, o, m); z += 2 * U
    zc_rev = reverb_unit(z, o, m); z += 3 * U
    patch_bay(z, o, m); z += U
    blank(2, m["rackblack"]); z += 2 * U
    blank(2, m["rackgrey"])
    # on the desk: the remote front-right, a small mixer centre, tape boxes, the lamp (room)
    rem = remote((330, -300, DESK_Z), m, yaw=math.radians(-12))
    # small mixer: a sloped slab with channel strips (knobs + faders)
    mx = Vector((-120, -120, DESK_Z))
    B("mixer", -260, 260, -200, 140, 0, 50, m["rackgrey"], mx, bevel=4)
    for k in range(8):
        x = -225 + k * 62
        for j in range(4):
            C.cylinder("mix_knob", 7, 9, (mx.x + x, mx.y + 60 - j * 34, mx.z + 55), m["cap_black"], segs=24,
                       r2=6, bevel=0.8)
        B("mix_slot", x - 1.3, x + 1.3, -190, -95, 50, 50.8, m["void"], mx)
        fy = -170 + (k * 37 % 60)
        B("mix_fader", x - 7, x + 7, fy - 6, fy + 6, 50, 60, m["cap_black"], mx, bevel=1.5)
    # the remote's cable to the rack, along the desk and down
    cable("remote_cable", [rem + Vector((60, 120, -40)), Vector((700, 150, DESK_Z + 4)),
                           Vector((930, 250, DESK_Z + 2)), Vector((1060, 300, DESK_Z - 300)),
                           Vector((rx - 150, ry + 380, 700)), Vector((rx - 150, ry + 520, zc_rev))], 2.4, m["cable"])
    tb = (560, 745, -340, -155) if decks else (600, 785, 60, 245)
    for j in range(4):
        B("desk_tapebox", tb[0], tb[1], tb[2] + j * 4, tb[3] + j * 4, DESK_Z + j * 24, DESK_Z + j * 24 + 23,
          m["paper"] if j % 2 else C.clay("box_dark", rgb=(0.18, 0.12, 0.08), rough=0.6), bevel=1.5)
    deck_refs = None
    if decks:
        deck_refs = deck_pair(m, (-310.0, 190.0), 120.0)
    room_lights(bulb, 1500)
    # a warm practical on the rack side so the units read (a second lamp, off-frame right)
    ld = bpy.data.lights.new("rack_lamp", "POINT")
    ld.energy = C.watts(18)
    ld.color = (1.0, 0.75, 0.5)
    ld.shadow_soft_size = 60
    lo = bpy.data.objects.new("rack_lamp", ld)
    C.link(lo)
    lo.location = (rx + 500, ry - 500, 1300)
    ref = dict(center=Vector((300, 0, 900)), size=2600, radius=1500,
               rack=Vector((rx, ry, base + inner / 2)), reverb=Vector((rx, ry - 3, zc_rev)),
               remote=rem, desk=Vector((0, 0, DESK_Z)))
    if deck_refs:
        ref.update(deck_refs)
        deck_lights(deck_refs, soft=0.6)
    return ref


# ================================================================== round 3
# Owner's notes (30 Oct 2026): the decks on the desk in the rack room, the
# tape view pulled back plus a front-on view, and the deck itself more
# accurate in detail, materials and light. The deck below follows the A77
# Mk III/IV as described in STORY_RESEARCH.md ("Round 3"): 415 x 375 mm
# front, a brushed stainless upper (transport) plate over a dark grey plastic
# control panel, 7-inch reels on spring-sleeved spindles, a head block with
# erase / record / shielded playback heads under a dark cover and a smoked
# swing-down tape shield, the capstan right of the heads with the rubber pinch
# roller below the tape, tension levers with guide rollers left and right,
# a 4-digit counter between the reels; on the control panel five transport
# keys (record red) top left with two knob/switch pairs below, three jacks,
# two lit VU meters flanked by red record buttons with the record-level
# knob/switch pairs below them, and a bar knob for power and speed at the far
# right with its pilot above. Phono sockets along the top of the rear.
# Unbranded: no name, logo or legend text anywhere.

A77_W, A77_H, A77_D = 420.0, 378.0, 215.0   # walnut case (front 415 x 375 mm per the 1972 profile)
A77_T = 14.0                                # case wall
A77_F = 3.0                                 # faceplates sit 3 mm behind the case's front edge (deck-local y)
A77_SPLIT = 122.0                           # transport plate above, control panel below
A77_REEL_DX, A77_REEL_Z, A77_REEL_R = 104.0, 270.0, 89.0   # 7-inch (178 mm) reels
A77_TAPE_Z = 150.0                          # the tape's lower edge line across the heads
A77_YC = A77_F - 20.0                       # tape centre plane


def _principled(m):
    return m.node_tree.nodes["Principled BSDF"]


def brushed(name, value=0.62, rough=0.26, aniso=0.75, radial=False, rgb=None, streak=1.0):
    """Brushed metal: anisotropic highlights along X (plates) or round the
    part's own axis (radial: reel flanges, knob caps), with fine streaks in
    colour and roughness from stretched noise."""
    m = _get(name)
    if m:
        return m
    m = C.clay(name, value, rough, rgb=rgb, metallic=1.0, aniso=aniso)
    nt = m.node_tree
    p = _principled(m)
    if radial:
        tg = nt.nodes.new("ShaderNodeTangent")
        tg.direction_type = "RADIAL"
        tg.axis = "Z"
        nt.links.new(tg.outputs[0], p.inputs["Tangent"])
    else:
        cx = nt.nodes.new("ShaderNodeCombineXYZ")
        cx.inputs[0].default_value = 1.0
        nt.links.new(cx.outputs[0], p.inputs["Tangent"])
    if streak:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.003, 9.0, 9.0) if not radial else (6.0, 6.0, 0.02)
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.0
        nz.inputs["Detail"].default_value = 3.0
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        rr = nt.nodes.new("ShaderNodeMapRange")
        rr.inputs["To Min"].default_value = rough * (1 - 0.3 * streak)
        rr.inputs["To Max"].default_value = rough * (1 + 0.35 * streak)
        nt.links.new(nz.outputs["Fac"], rr.inputs["Value"])
        nt.links.new(rr.outputs[0], p.inputs["Roughness"])
        cv = nt.nodes.new("ShaderNodeMapRange")
        base = rgb or (value, value, value)
        cv.inputs["To Min"].default_value = 1 - 0.06 * streak
        cv.inputs["To Max"].default_value = 1 + 0.03 * streak
        nt.links.new(nz.outputs["Fac"], cv.inputs["Value"])
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        mul.inputs["A"].default_value = (*base, 1)
        comb = nt.nodes.new("ShaderNodeCombineColor")
        for k in range(3):
            nt.links.new(cv.outputs[0], comb.inputs[k])
        nt.links.new(comb.outputs[0], mul.inputs["B"])
        nt.links.new(mul.outputs["Result"], p.inputs["Base Color"])
    return m


def clear_plastic(name, rgb=(0.92, 0.93, 0.93), rough=0.06):
    m = _get(name)
    if m:
        return m
    m = C.clay(name, rgb=rgb, rough=rough)
    p = _principled(m)
    p.inputs["Transmission Weight"].default_value = 1.0
    p.inputs["IOR"].default_value = 1.49
    return m


def tape_pack_material():
    """The wound pack's edge: satin brown-black, faint rings where the
    winding tension changed."""
    m = _get("tape_pack3")
    if m:
        return m
    m = C.clay("tape_pack3", rgb=(0.05, 0.031, 0.02), rough=0.38)
    nt = m.node_tree
    p = _principled(m)
    p.inputs["Sheen Weight"].default_value = 0.3
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.08, 0.08, 0.08)
    wv = nt.nodes.new("ShaderNodeTexWave")
    wv.wave_type = "RINGS"
    wv.inputs["Distortion"].default_value = 1.5
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], wv.inputs["Vector"])
    rr = nt.nodes.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = 0.30
    rr.inputs["To Max"].default_value = 0.46
    nt.links.new(wv.outputs["Fac"], rr.inputs["Value"])
    nt.links.new(rr.outputs[0], p.inputs["Roughness"])
    return m


def mats3():
    m = mats()
    m.update(
        plate=brushed("a77_plate", 0.62, 0.32, 0.8),                   # brushed stainless transport plate
        reelalu=brushed("a77_reel", 0.62, 0.36, 0.6, radial=True),      # reel flanges, spun aluminium
        capalu=brushed("a77_cap", 0.72, 0.18, 0.8, radial=True),       # knob caps
        ctrl3=worn("a77_ctrl", (0.042, 0.042, 0.045), 0.5, edge=(0.2, 0.2, 0.2), metallic_edge=False),
        cover=C.clay("a77_cover", rgb=(0.03, 0.03, 0.032), rough=0.35, coat=0.2),
        casting=C.clay("a77_casting", rgb=(0.16, 0.16, 0.16), rough=0.55, metallic=0.6),
        headmetal=C.steel("a77_head", 0.75, 0.1),
        rollermetal=C.steel("a77_roller", 0.7, 0.32),   # satin-machined, not mirror (owner, 31 Oct 2026)
        key_grey=worn("a77_key", (0.13, 0.13, 0.135), 0.4, edge=(0.3, 0.3, 0.3), metallic_edge=False),
        knob3=worn("a77_knob", (0.018, 0.018, 0.02), 0.32, edge=(0.12, 0.12, 0.12), metallic_edge=False),
        clear=clear_plastic("a77_clear"),
        smoked=clear_plastic("a77_smoked", rgb=(0.62, 0.55, 0.47), rough=0.1),
        glass=clear_plastic("a77_glass", rgb=(0.97, 0.97, 0.97), rough=0.02),
        pack=tape_pack_material(),
        vu=story.glow("a77_vu_face", "#F4DEB0", 1.6),
        vu_red=C.clay("a77_vu_red", rgb=(0.5, 0.04, 0.02), rough=0.5),
        ink=C.clay("a77_ink", 0.015, 0.6),
        pilot=story.glow("a77_pilot", "#FF5A2A", 6.0),
        rubber3=C.clay("a77_rubber", 0.018, 0.72),
    )
    # surface realism (owner, 31 Oct 2026): lacquered wood, eggshell wall paint, fine texture on
    # paint and plastics, fibre on paper and the rug. Scales in 1/mm.
    for k, kw in dict(walnut=dict(bump=0.04, scale=2.0, rough_var=0.1, coat=0.4, coat_rough=0.16),
                      oak=dict(bump=0.04, scale=2.0, rough_var=0.12, coat=0.3, coat_rough=0.28),
                      floor=dict(bump=0.05, scale=1.0, rough_var=0.2, coat=0.15, coat_rough=0.35),
                      wall=dict(bump=0.3, scale=1.2, rough_var=0.08),
                      ctrl3=dict(bump=0.06, scale=3.0, rough_var=0.2),
                      cover=dict(bump=0.02, scale=4.0, rough_var=0.25),
                      casting=dict(bump=0.25, scale=2.5, rough_var=0.15),
                      knob3=dict(bump=0.02, scale=5.0, rough_var=0.18),
                      key_grey=dict(bump=0.02, scale=5.0, rough_var=0.15),
                      paper=dict(bump=0.35, scale=3.0, rough_var=0.1),
                      rug=dict(bump=0.7, scale=2.0, rough_var=0.05),
                      terracotta=dict(bump=0.4, scale=1.0, rough_var=0.1),
                      cable=dict(bump=0.0, rough_var=0.25),
                      rubber3=dict(bump=0.05, scale=3.0, rough_var=0.15)).items():
        finish(m[k], **kw)
    m["wall"].node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.75   # eggshell, not chalk
    leaf = _principled(m["leaf"])
    leaf.inputs["Subsurface Weight"].default_value = 0.3   # light through the leaves
    leaf.inputs["Roughness"].default_value = 0.4
    return m


def _rod(name, a, b, size, y0, y1, mat, o, bevel=1.0):
    """A flat bar in the XZ plane from a to b ((x, z), deck-local), size wide."""
    a, b = Vector((a[0], 0, a[1])), Vector((b[0], 0, b[1]))
    d = b - a
    th = math.atan2(d.x, d.z)
    mid = (a + b) / 2
    return C.box(name, (size, y1 - y0, d.length + size), (o.x + mid.x, o.y + (y0 + y1) / 2, o.z + mid.z), mat,
                 bevel=bevel, rot=(0, th, 0))


def reel77(name, cx, cz, yc, pack_r, m, spin=0.0):
    """A 7-inch reel: two spun-aluminium flanges with three rounded windows
    and a rolled rim, the hub, the tape pack between them, the spindle's
    knurled sleeve in front. Axis along Y; yc = the tape's centre plane."""
    R, hub = A77_REEL_R, 30.0
    gap = story.TAPE_W + 1.8
    for s in (-1, 1):
        yf = yc + s * (gap / 2 + 0.65)
        fl = C.cylinder(f"{name}_flange", R, 1.3, (cx, yf, cz), m["reelalu"], axis="X", segs=160)
        cuts = []
        for k in range(3):
            a = 2 * math.pi * k / 3 + math.pi / 2 + spin
            w = story._window(hub + 9, R - 10, a - 0.66, a + 0.66, 8, n=24)
            w.location = (cx, yf, cz)
            cuts.append(w)
            hole = C.cylinder("hole_cut", 3.2, 8, (cx + (hub + 3) * math.cos(a + math.pi / 3), yf,
                                                   cz + (hub + 3) * math.sin(a + math.pi / 3)), axis="X", segs=16)
            cuts.append(hole)
        C.boolean_cut(fl, cuts)
        # Flat faces stay flat (owner, 31 Oct 2026: the reels' lighting looked wrong): smooth shading
        # over the whole cut disc bent every face's normals towards its edges, so the flanges and
        # spokes showed smeared, chalky gradients. Smooth only across edges under 30 degrees.
        fl.data.set_sharp_from_angle(angle=math.radians(30))
        C.add_bevel(fl, 0.45, 2)
        C.torus(f"{name}_rim", R - 0.5, 0.95, (cx, yf, cz), m["reelalu"], axis="X", major=160, minor=10)
    hb = C.cylinder(f"{name}_hub", hub, gap + 0.4, (cx, yc, cz), m["reelalu"], axis="X", segs=96, bevel=0.4)
    hb.data.set_sharp_from_angle(angle=math.radians(30))
    C.cylinder(f"{name}_bore", 9.5, gap + 4, (cx, yc - 0.5, cz), m["void"], axis="X", segs=48)
    C.cylinder(f"{name}_spindle", 4.0, 30, (cx, yc, cz), m["steel"], axis="X", segs=24)
    sl = C.cylinder(f"{name}_sleeve", 9.0, 11, (cx, yc - gap / 2 - 7.5, cz), m["knob3"], axis="X", segs=48,
                    bevel=1.2)
    for k in range(3):  # the sleeve's three latch tabs
        a = 2 * math.pi * k / 3 + spin
        C.box(f"{name}_tab", (5, 3, 3), (cx + 10 * math.cos(a), yc - gap / 2 - 3.5, cz + 10 * math.sin(a)),
              m["knob3"], bevel=0.8, rot=(0, -a, 0))
    if pack_r > hub:
        C.cylinder(f"{name}_pack", pack_r, story.TAPE_W, (cx, yc, cz), m["pack"], axis="X", segs=160)
    return sl


def knob77(name, x, z, o, m, r=12.5, disc=True, ang=0.0):
    """The A77's knob: a black skirted body with a brushed aluminium cap and
    a white index line; a clear plastic disc behind it works the concentric
    switch."""
    y = o.y + A77_F
    if disc:
        C.cylinder(f"{name}_disc", r * 1.5, 2.4, (o.x + x, y - 1.3, o.z + z), m["clear"], axis="X", segs=64,
                   bevel=0.6)
        a = math.radians(ang + 40)
        C.box(f"{name}_disc_tick", (1.0, 0.4, 4.0), (o.x + x + r * 1.32 * math.sin(a), y - 2.6,
                                                     o.z + z + r * 1.32 * math.cos(a)), m["cream"], rot=(0, a, 0))
    C.cylinder(f"{name}_body", r, 13, (o.x + x, y - 2.5 - 6.5, o.z + z), m["knob3"], axis="X", segs=64, r2=r * 0.9,
               bevel=1.0)
    C.cylinder(f"{name}_cap", r * 0.74, 1.2, (o.x + x, y - 15.6, o.z + z), m["capalu"], axis="X", segs=64, bevel=0.3)
    a = math.radians(ang)
    C.box(f"{name}_line", (1.1, 0.5, r * 0.55), (o.x + x + r * 0.62 * math.sin(a), y - 15.5,
                                               o.z + z + r * 0.62 * math.cos(a)), m["cream"], rot=(0, a, 0))


def vu_meter(name, cx, o, m, needle=-14.0):
    """A VU meter: black bezel, a warm-lit cream face with an arc of ticks,
    the red zone, a black needle and its pivot housing, glass in front."""
    y = o.y + A77_F
    z0, z1, w = 66.0, 112.0, 27.0
    bw = 3.5  # the bezel is a frame: four bars round the window
    for x0_, x1_, za, zb in ((cx - w, cx - w + bw, z0, z1), (cx + w - bw, cx + w, z0, z1),
                             (cx - w, cx + w, z0, z0 + bw), (cx - w, cx + w, z1 - bw, z1)):
        B(f"{name}_bezel", x0_, x1_, A77_F - 5, A77_F + 1, za, zb, m["cap_black"], o, bevel=1.2)
    B(f"{name}_face", cx - w + 3.5, cx + w - 3.5, A77_F - 1.6, A77_F - 1.2, z0 + 3.5, z1 - 3.5, m["vu"], o)
    B(f"{name}_glass", cx - w + 2.5, cx + w - 2.5, A77_F - 5.6, A77_F - 5.1, z0 + 2.5, z1 - 2.5, m["glass"], o)
    pz = z0 - 4.0
    for k in range(13):
        a = math.radians(-42 + 7 * k)
        red = k >= 9
        ln = 5.0 if k % 3 == 0 else 3.0
        r = 38.0
        C.box(f"{name}_tick", (0.7, 0.2, ln), (o.x + cx + (r + ln / 2) * math.sin(a), y - 1.8,
                                               o.z + pz + (r + ln / 2) * math.cos(a)),
              m["vu_red"] if red else m["ink"], rot=(0, a, 0))
    for k in range(30):  # the scale's arc line, black then red
        a = math.radians(-42 + 84 * (k + 0.5) / 30)
        red = a > math.radians(20)
        C.box(f"{name}_arc", (3.0, 0.2, 0.6 if not red else 2.2), (o.x + cx + 38 * math.sin(a), y - 1.8,
                                                                  o.z + pz + 38 * math.cos(a)),
              m["vu_red"] if red else m["ink"], rot=(0, a, 0))
    a = math.radians(needle)
    C.box(f"{name}_needle", (0.7, 0.3, 44), (o.x + cx + 22 * math.sin(a), y - 2.6, o.z + pz + 22 * math.cos(a)),
          m["ink"], rot=(0, a, 0))
    B(f"{name}_pivot", cx - 11, cx + 11, A77_F - 4.5, A77_F - 1.2, z0 + 3.5, z0 + 9, m["cap_black"], o, bevel=1.5)


def deck77(o, m, name="deck", supply=72.0, takeup=44.0, side=1, spin=0.0, needles=(-14.0, -20.0)):
    """The round-3 deck (A77 Mk III/IV layout, unbranded). o = the case's
    bottom-front centre on the desk; front faces -Y. side: +1 puts the rear
    phono sockets on the right half (towards a partner deck on the right).
    Returns the rear sockets (world) and reference points."""
    o = Vector(o)
    W, H, D, t, F = A77_W, A77_H, A77_D, A77_T, A77_F
    w2, iw = W / 2, W / 2 - A77_T
    # --- walnut case: cheeks, top and bottom (the front edges rounded), the back
    B(f"{name}_cheekL", -w2, -iw, 0, D, 0, H, m["walnut"], o, bevel=3.5)
    B(f"{name}_cheekR", iw, w2, 0, D, 0, H, m["walnut"], o, bevel=3.5)
    B(f"{name}_top", -iw - 0.5, iw + 0.5, 0, D, H - t, H, m["walnut"], o, bevel=3.0)
    B(f"{name}_bottom", -iw - 0.5, iw + 0.5, 0, D, 0, t, m["walnut"], o, bevel=3.0)
    B(f"{name}_back", -iw, iw, D - 8, D - 3, t, H - t, m["ctrl3"], o, bevel=1)
    for sx in (-1, 1):  # felt feet
        for fy in (30, D - 30):
            C.cylinder(f"{name}_foot", 11, 4, (o.x + sx * (w2 - 32), o.y + fy, o.z - 1.0), m["rubber3"], segs=24,
                       bevel=1)
    # --- faceplates: brushed transport plate, dark control panel, a dark seam
    B(f"{name}_plate", -iw, iw, F, F + 6, A77_SPLIT, H - t, m["plate"], o, bevel=1.2)
    B(f"{name}_ctrl", -iw, iw, F, F + 6, t, A77_SPLIT - 1.5, m["ctrl3"], o, bevel=1.2)
    B(f"{name}_seam", -iw, iw, F + 2, F + 6, A77_SPLIT - 1.5, A77_SPLIT, m["void"], o)
    of = o + Vector((0, F, 0))
    for sx in (-1, 1):
        for sz in (A77_SPLIT + 9, H - t - 9):
            screw(f"{name}_screw", sx * (iw - 9), sz, of, m["steel"], r=2.4)
    # --- reels on their turntables
    sleeves = []
    for s in (-1, 1):
        x = s * A77_REEL_DX
        C.cylinder(f"{name}_table", 50, 5, (o.x + x, o.y + F - 2.5, o.z + A77_REEL_Z), m["cap_black"], axis="X",
                   segs=96, bevel=1.2)
        C.torus(f"{name}_table_ring", 47, 0.8, (o.x + x, o.y + F - 5.0, o.z + A77_REEL_Z), m["steel"], axis="X")
        pack = supply if s < 0 else takeup
        sleeves.append(reel77(f"{name}_reel{'LR'[s > 0]}", o.x + x, o.z + A77_REEL_Z, o.y + A77_YC, pack, m,
                              spin=spin + (0.7 if s > 0 else 0.0)))
    # --- counter between the reels, its reset key below
    B(f"{name}_counter", -21, 21, F - 3, F, 212, 229, m["void"], o, bevel=1.5)
    B(f"{name}_counter_rim", -23, 23, F - 1.2, F, 210, 231, m["cap_black"], o, bevel=1.0)
    digits("0127", -14.5, 220.5, 9, o, F - 3.3, C.clay("counter_white", 0.8, 0.5), pitch=9.2)
    B(f"{name}_reset", -4, 4, F - 5, F, 203, 208, m["key_grey"], o, bevel=1.2)
    # --- head block: casting, three heads (the playback head in its shield can), the dark cover
    TZ, YC = A77_TAPE_Z, A77_YC
    B(f"{name}_casting", -62, 62, F - 9, F, TZ - 8, TZ + 50, m["casting"], o, bevel=3)
    for k, (hx, hw) in enumerate(((-28, 5.5), (0, 7.0), (28, 7.0))):
        B(f"{name}_head{k}", hx - hw, hx + hw, YC - 9, YC + 9, TZ + 0.35, TZ + 15, m["headmetal"], o, bevel=3.0)
        B(f"{name}_headmount{k}", hx - hw - 1, hx + hw + 1, YC - 7, F - 9, TZ + 12, TZ + 20, m["casting"], o,
          bevel=1)
    B(f"{name}_can", 28 - 10, 28 + 10, YC - 12, YC + 10, TZ + 6, TZ + 18, m["steel"], o, bevel=1.5)
    B(f"{name}_cover", -50, 50, F - 42, F - 8, TZ + 16, TZ + 54, m["cover"], o, bevel=6)
    B(f"{name}_cover_top", -46, 46, F - 38, F - 12, TZ + 53, TZ + 55.5, m["capalu"], o, bevel=1.2)
    for gx in (-60, 60):  # fixed guide posts each side of the heads
        C.cylinder(f"{name}_guide", 4.0, 22, (o.x + gx, o.y + YC, o.z + TZ + 4.2), m["headmetal"], axis="X", segs=32,
                   bevel=0.6)
        C.cylinder(f"{name}_guide_cap", 6.0, 1.6, (o.x + gx, o.y + YC - 9.5, o.z + TZ + 4.2), m["headmetal"],
                   axis="X", segs=32, bevel=0.4)
    # the smoked swing-down tape shield in front of the head area
    B(f"{name}_shield", -70, 64, F - 46, F - 44, TZ - 12, TZ + 12, m["smoked"], o, bevel=1.0)
    for hx in (-66, 60):
        C.cylinder(f"{name}_shield_pivot", 2.4, 30, (o.x + hx, o.y + F - 30, o.z + TZ - 9), m["steel"], axis="X",
                   segs=16)
    # capstan above the tape, the rubber pinch roller below it on its arm
    cx_ = 80.0
    C.cylinder(f"{name}_capstan", 3.0, 36, (o.x + cx_, o.y + F - 18, o.z + TZ + 3.2), m["headmetal"], axis="X",
               segs=32)
    C.cylinder(f"{name}_capstan_collar", 9.0, 4, (o.x + cx_, o.y + F - 2, o.z + TZ + 3.2), m["steel"], axis="X",
               segs=48, bevel=1)
    C.cylinder(f"{name}_pinch", 11.5, 13, (o.x + cx_, o.y + YC, o.z + TZ - 11.7), m["rubber3"], axis="X", segs=64,
               bevel=1.6)
    C.cylinder(f"{name}_pinch_hub", 4.5, 3, (o.x + cx_, o.y + YC - 7.5, o.z + TZ - 11.7), m["headmetal"], axis="X",
               segs=32, bevel=0.6)
    _rod(f"{name}_pinch_arm", (cx_, TZ - 11.7), (cx_ + 38, TZ - 18), 11, YC + 7, F - 1, m["casting"], o, bevel=2)
    C.cylinder(f"{name}_pinch_pivot", 6.5, 6, (o.x + cx_ + 38, o.y + F - 3, o.z + TZ - 18), m["steel"], axis="X",
               segs=32, bevel=1)
    # tension levers with guide rollers, left and right
    gr = 9.0
    rollers = []
    for s in (-1, 1):
        rc = Vector((o.x + s * 152, o.y + YC, o.z + TZ + gr))
        piv = (s * 178, TZ + 68)
        _rod(f"{name}_lever", (s * 152, TZ + gr), piv, 9, YC + 6, F - 1, m["plate"], o, bevel=1.5)
        C.cylinder(f"{name}_lever_pivot", 8, 6, (o.x + piv[0], o.y + F - 3, o.z + piv[1]), m["steel"], axis="X",
                   segs=32, bevel=1)
        C.cylinder(f"{name}_roller", gr, story.TAPE_W + 1.4, tuple(rc), m["rollermetal"], axis="X", segs=48, bevel=0.5)
        for fy in (-1, 1):
            C.cylinder(f"{name}_roller_fl", gr + 2.2, 1.0, (rc.x, rc.y + fy * (story.TAPE_W / 2 + 1.3), rc.z),
                       m["rollermetal"], axis="X", segs=48, bevel=0.3)
        rollers.append(rc)
    # --- control panel
    keys = [m["key_grey"]] * 4 + [m["red_btn"]]
    for k, km in enumerate(keys):
        x = -184 + k * 23
        B(f"{name}_key{k}", x, x + 20, F - 9, F, 90, 108, km, o, bevel=2.2)
    for k, x in enumerate((-162, -116)):   # playback level and balance, with their switch discs
        knob77(f"{name}_play{k}", x, 46, o, m, ang=-35 + 60 * k)
    for k, x in enumerate((-60, -34, -8)):  # headphones, mic L, mic R
        C.cylinder(f"{name}_jack", 6.5, 3, (o.x + x, o.y + F - 1.5, o.z + 46), m["steel"], axis="X", segs=32,
                   bevel=0.8)
        C.cylinder(f"{name}_jack_hole", 3.4, 2, (o.x + x, o.y + F - 2.6, o.z + 46), m["void"], axis="X", segs=24)
    for k, cx in enumerate((58, 114)):
        vu_meter(f"{name}_vu{k}", cx, o, m, needle=needles[k])
        knob77(f"{name}_rec{k}", cx, 40, o, m, r=11.5, ang=-50 + 25 * k)
    for x in (18, 154):                     # the red record (channel select) buttons flanking the meters
        B(f"{name}_recbtn", x - 6.5, x + 6.5, F - 7, F, 84, 97, m["red_btn"], o, bevel=1.8)
    # power / speed bar knob far right, its pilot above
    C.cylinder(f"{name}_bar_base", 11, 5, (o.x + 180, o.y + F - 2.5, o.z + 40), m["knob3"], axis="X", segs=48,
               bevel=1)
    bar = C.box(f"{name}_bar", (7, 12, 30), (o.x + 180, o.y + F - 9, o.z + 40), m["knob3"], bevel=2.2,
                rot=(0, math.radians(30), 0))
    C.cylinder(f"{name}_pilot", 2.6, 2.5, (o.x + 180, o.y + F - 1.5, o.z + 82), m["pilot"], axis="X", segs=24,
               bevel=0.6)
    C.cylinder(f"{name}_pilot_rim", 4.2, 1.6, (o.x + 180, o.y + F - 0.6, o.z + 82), m["steel"], axis="X", segs=24,
               bevel=0.4)
    for x in (-60, -8):   # two small toggles under the jacks
        C.cylinder(f"{name}_tog_bush", 3.6, 4, (o.x + x, o.y + F - 2, o.z + 24), m["steel"], axis="X", segs=24)
        lev = C.cylinder(f"{name}_tog", 1.2, 11, (o.x + x, o.y + F - 8, o.z + 26), m["headmetal"], axis="X",
                         segs=16, r2=0.9)
        lev.rotation_euler = (math.pi / 2 - 0.35, 0, 0)
    # --- rear: phono sockets along the top, a power cord low
    sockets = {}
    for k, key in enumerate(("out_l", "out_r", "in_l", "in_r")):
        x = side * (70 + k * 26)
        p = Vector((o.x + x, o.y + D - 1.5, o.z + H - 52))
        C.cylinder(f"{name}_sock", 4.6, 6, tuple(p), m["gold"], axis="X", segs=24)
        sockets[key] = p
    # --- the tape: supply -> left roller -> guide -> heads -> capstan -> guide -> right roller -> take-up
    sup_c = (o.x - A77_REEL_DX, o.z + A77_REEL_Z)
    tak_c = (o.x + A77_REEL_DX, o.z + A77_REEL_Z)
    rl, rr = rollers
    g = gr + 0.15
    pts = [story._tangent(sup_c, supply, (rl.x - g, rl.z), cw=False)]
    pts += story._wrap((rl.x, rl.z), g, math.pi, 1.5 * math.pi, 10)
    pts += [(o.x - 60, o.z + TZ), (o.x + 60, o.z + TZ)]
    pts += story._wrap((rr.x, rr.z), g, 1.5 * math.pi, 2 * math.pi, 10)
    pts += [story._tangent(tak_c, takeup, (rr.x + g, rr.z), cw=True)]
    story.tape_strip(f"{name}_tape", pts, o.y + YC)
    return dict(sockets=sockets, heads=Vector((o.x, o.y + YC, o.z + TZ + 10)),
                meters=Vector((o.x + 86, o.y, o.z + 88)), center=Vector((o.x, o.y, o.z + H / 2)))


def deck_pair(m, xs, y0, z0=DESK_Z):
    """Two decks on the desk at x = xs (case centres), fronts at y0, each
    running its own tape; A's outputs into B's inputs, B's outputs back into
    A's. The phono leads leave the rear tops and hang down into the gap
    between the cases, so the cross-patch reads from the front."""
    xa, xb = xs
    a = deck77((xa, y0, z0), m, "deck_A", supply=74.0, takeup=46.0, side=1, spin=0.2,
               needles=(-12.0, -18.0))
    b = deck77((xb, y0, z0), m, "deck_B", supply=60.0, takeup=58.0, side=-1, spin=1.1,
               needles=(-16.0, -10.0))
    sa, sb = a["sockets"], b["sockets"]
    gx = (xa + A77_W / 2 + xb - A77_W / 2) / 2   # middle of the gap
    gap = (xb - A77_W / 2) - (xa + A77_W / 2)
    leads = ((sa["out_l"], sb["in_l"], m["cable"], "AB_L"), (sa["out_r"], sb["in_r"], m["cable_grey"], "AB_R"),
             (sb["out_l"], sa["in_l"], m["cable"], "BA_L"), (sb["out_r"], sa["in_r"], m["cable_grey"], "BA_R"))
    for k, (src, dst, mat, tag) in enumerate(leads):
        p0 = rca(f"plug_{tag}_s", src, 1, m, mat)
        p1 = rca(f"plug_{tag}_d", dst, 1, m, mat)
        sag_z = z0 + 40 + 26 * k               # each lead hangs a little differently
        sag_y = y0 + A77_D - 30 - 34 * k          # inside the gap, front to back
        dx = (k - 1.5) * gap * 0.12
        pts = [p0, p0 + Vector((0, 40, 10)), Vector((gx + dx - 0.35 * (gx - p0.x), p0.y + 30, p0.z - 40)),
               Vector((gx + dx, sag_y, sag_z)),
               Vector((gx + dx - 0.35 * (gx - p1.x), p1.y + 30, p1.z - 40)), p1 + Vector((0, 40, 10)), p1]
        cable(f"xpatch_{tag}", pts, 2.6, mat)
    # power cords off the back of the desk
    for d, x in ((a, xa), (b, xb)):
        c0 = Vector((x - 120 * (1 if x < gx else -1), y0 + A77_D + 2, z0 + 40))
        cable("power_cord", [c0, c0 + Vector((0, 60, -20)), Vector((c0.x, DESK["y1"] - 10, DESK_Z + 4)),
                             Vector((c0.x + 20, DESK["y1"] + 30, DESK_Z - 60)),
                             Vector((c0.x + 40, DESK["y1"] + 50, 300))], 2.8, m["cable"])
    mid = Vector((gx, y0, DESK_Z + A77_H / 2))
    return dict(decks_center=mid, deck_a=a["center"], deck_b=b["center"], heads_a=a["heads"],
                meters_a=a["meters"], decks_width=(xb - xa) + A77_W, gap_x=gx)


def deck_lights(ref, soft=1.0, dim=0.0):
    """Light for the decks: a soft warm key from the front left, a large dim
    card above the camera for the brushed plates to reflect, a cool kicker
    from behind right along the case edges."""
    c = ref["decks_center"]
    s = ref["decks_width"]
    d = 1.7 * s
    key = c + Vector((-0.62, -0.72, 0.55)).normalized() * d
    C.area_light("deck_key", key, c, size=0.7 * s, power=C.watts(C.KEY_W * 0.32 * soft * (1 - 0.1 * dim)) * (d / 1000) ** 2,
                 rgb=(1.0, 0.86, 0.68))
    card = c + Vector((0.1, -0.85, 0.75)).normalized() * d * 1.1
    C.area_light("deck_card", card, c, size=1.3 * s, power=C.watts(C.KEY_W * 0.035 * soft * (1 - 0.1 * dim)) * (d * 1.1 / 1000) ** 2,
                 rgb=(1.0, 0.93, 0.84))
    # a big, dim card behind the camera at the plates' height: a flat steel plate seen square on
    # mirrors what is behind the camera, and in the dim room that was nothing, so it read dark grey
    pc = c + Vector((0.0, -1.0, 0.12)).normalized() * d * 1.6
    C.area_light("plate_card", pc, c, size=1.6 * s, power=C.watts(C.KEY_W * 0.028 * soft) * (d * 1.6 / 1000) ** 2,
                 rgb=(1.0, 0.94, 0.86))
    kick = c + Vector((0.7, 0.75, 0.35)).normalized() * d * 0.8
    C.area_light("deck_kick", kick, c, size=0.3 * s, power=C.watts(C.KEY_W * 0.18 * soft) * (d * 0.8 / 1000) ** 2,
                 rgb=(0.82, 0.88, 1.0))


def tape_cross3():
    """Round 3: the two decks on the desk in the lamp-lit room (the round-2
    tape scene), the new deck model, cross-patched, reels loaded."""
    m = mats3()
    bulb = room(m, lamp_x=-880.0)
    gap = 170.0
    xa, xb = -(A77_W + gap) / 2, (A77_W + gap) / 2
    y0 = -150.0
    ref = deck_pair(m, (xa, xb), y0)
    for j in range(3):
        B("desk_tapebox", 560, 745, -320 + j * 6, -135 + j * 6, DESK_Z + j * 24, DESK_Z + j * 24 + 23,
          m["paper"] if j % 2 else C.clay("box_dark", rgb=(0.18, 0.12, 0.08), rough=0.6), bevel=1.5)
    B("notebook", -760, -560, -350, -210, DESK_Z, DESK_Z + 9, m["paper"], bevel=1.5)
    C.cylinder("pencil", 3.5, 170, (-610, -265, DESK_Z + 12.5), C.clay("pencil", rgb=(0.55, 0.38, 0.08), rough=0.5),
               axis="Y", segs=6).rotation_euler = (0, math.pi / 2, math.radians(18))
    # dim: the owner, 31 Oct 2026, on the final story image: "moodier lighting" (env for previews)
    dim = float(os.environ.get("IV_STORY_DIM", STORY_DIM))
    room_lights(bulb, A77_W, dim=dim)
    deck_lights(ref, dim=dim)
    ref.update(center=ref["decks_center"] + Vector((0, 60, 0)), size=ref["decks_width"],
               radius=ref["decks_width"] / 2 + 30)
    return ref


def floor_lamp70(m, x, y):
    """A 1970s floor lamp: a chrome disc base and stem under a big orange glass mushroom shade,
    glowing from the bulb inside (owner, 31 Oct 2026: "a more 70s lamp: orange glass maybe").
    The glass casts no shadow, so the light leaves it soft and warm."""
    chrome = C.clay("lamp70_chrome", 0.8, 0.12, metallic=1.0)
    C.cylinder("lamp70_base", 170, 18, (x, y, 9), chrome, segs=64, bevel=5, r2=150)
    C.cylinder("lamp70_stem", 10, 1080, (x, y, 550), chrome, segs=24)
    glass = bpy.data.materials.new("lamp70_glass")
    glass.use_nodes = True
    p = glass.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (1.0, 0.30, 0.03, 1)
    p.inputs["Roughness"].default_value = 0.12
    p.inputs["Coat Weight"].default_value = 0.6
    p.inputs["Emission Color"].default_value = (1.0, 0.20, 0.01, 1)
    p.inputs["Emission Strength"].default_value = float(os.environ.get("IV_LAMP70_GLOW", "1.8"))
    dome = C.sphere("lamp70_shade", 1.0, (x, y, 1150), glass)
    dome.scale = (230, 230, 150)
    dome.visible_shadow = False
    C.cylinder("lamp70_cap", 22, 16, (x, y, 1302), chrome, segs=32, bevel=4)
    ld = bpy.data.lights.new("lamp70", "POINT")
    ld.energy = C.watts(float(os.environ.get("IV_LAMP70_W", "100")))
    ld.color = (1.0, 0.55, 0.24)
    ld.shadow_soft_size = 120
    ob = bpy.data.objects.new("lamp70", ld)
    C.link(ob)
    ob.location = (x, y, 1120)
    ob.visible_camera = False
    # the bulb doesn't light its own shade (lit from 15 cm it clipped to white); the glass glows
    # orange by itself, the bulb lights the room
    excl = bpy.data.collections.new("lamp70_shade_only")
    excl.objects.link(dome)
    excl.collection_objects[0].light_linking.link_state = "EXCLUDE"
    ob.light_linking.receiver_collection = excl
    return ob


def _squircle(w, d, a, n=4.0):
    """A point on the rounded-rectangle (superellipse) outline w x d at angle a."""
    c, sn = math.cos(a), math.sin(a)
    return (w / 2 * math.copysign(abs(c) ** (2 / n), c), d / 2 * math.copysign(abs(sn) ** (2 / n), sn))


def _pad(name, w, d, top, bottom, mat, n=4.0, rings=16, segs=96, subsurf=2):
    """A smooth surface over a rounded-rectangle plan: top(x, y, t) gives the height at a point
    (t runs 0 at the centre to 1 at the rim); bottom(...) closes it into a solid (a cushion),
    or None leaves a sheet to thicken (a shell)."""
    bm = bmesh.new()
    def rings_of(fz, last):
        out = []
        for i in range(1, last + 1):
            t = i / rings
            row = []
            for j in range(segs):
                bx, by = _squircle(w, d, 2 * math.pi * j / segs, n)
                row.append(bm.verts.new((t * bx, t * by, fz(t * bx, t * by, t))))
            out.append(row)
        return out
    c_top = bm.verts.new((0, 0, top(0, 0, 0)))
    tr = rings_of(top, rings)
    def skin(c, rs, flip):
        for j in range(segs):
            f = (c, rs[0][j], rs[0][(j + 1) % segs])
            bm.faces.new(f[::-1] if flip else f)
        for i in range(len(rs) - 1):
            for j in range(segs):
                f = (rs[i][j], rs[i + 1][j], rs[i + 1][(j + 1) % segs], rs[i][(j + 1) % segs])
                bm.faces.new(f[::-1] if flip else f)
    skin(c_top, tr, False)
    if bottom:
        c_bot = bm.verts.new((0, 0, bottom(0, 0, 0)))
        br = rings_of(bottom, rings - 1) + [tr[-1]]
        skin(c_bot, br, True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = C.mesh_obj(name, bm, mat, smooth=True)
    if not bottom:
        so = ob.modifiers.new("solidify", "SOLIDIFY")
        so.thickness = 13.0
        so.offset = -1.0
    if subsurf:
        sub = ob.modifiers.new("subsurf", "SUBSURF")
        sub.levels = sub.render_levels = subsurf
    return ob


def leather_mat(name):
    """Aged black leather (owner, 31 Oct 2026: "more textured and distressed; it looks plasticy"):
    a fine pebble grain, creases at two scales, worn patches that go duller and a little brown,
    and a sheen that varies across them. Object space, in mm."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    N = nt.nodes
    L = nt.links.new
    p = N["Principled BSDF"]
    tc = N.new("ShaderNodeTexCoord")
    def noise(scale, detail, rough, distort=0.0):
        n = N.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distort
        L(tc.outputs["Object"], n.inputs["Vector"])
        return n
    def math_node(op, a, b=None, clamp=False):
        n = N.new("ShaderNodeMath")
        n.operation = op
        n.use_clamp = clamp
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                L(v, n.inputs[i])
        return n.outputs[0]
    # wear: big soft patches
    wear_n = noise(0.012, 3, 0.55, 0.4)
    wear = N.new("ShaderNodeMapRange")
    wear.inputs["From Min"].default_value = 0.50
    wear.inputs["From Max"].default_value = 0.76
    L(wear_n.outputs["Fac"], wear.inputs["Value"])
    # colour: deep black, worn toward a dusty brown-grey
    mix = N.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (0.010, 0.009, 0.008, 1)
    mix.inputs["B"].default_value = (0.034, 0.029, 0.024, 1)
    L(wear.outputs["Result"], mix.inputs["Factor"])
    L(mix.outputs["Result"], p.inputs["Base Color"])
    # sheen: worn patches rubbed smoother and shinier, the rest satin, flecked by the grain
    grain = N.new("ShaderNodeTexVoronoi")
    grain.inputs["Scale"].default_value = 1.1
    grain.inputs["Randomness"].default_value = 0.9
    L(tc.outputs["Object"], grain.inputs["Vector"])
    rough = math_node("SUBTRACT", 0.42, math_node("MULTIPLY", wear.outputs["Result"], 0.14))
    rough = math_node("ADD", rough, math_node("MULTIPLY", grain.outputs["Distance"], 0.05), clamp=True)
    L(rough, p.inputs["Roughness"])
    # creases: ridged noise, a fine set and a coarse set
    # (owner, 31 Oct: the crinkled first try read "rough", like bark): broad soft folds only,
    # the grain barely there, so the satin sheen carries it
    def folds(scale):
        n = noise(scale, 1.5, 0.4, 0.6)
        ridge = math_node("ABSOLUTE", math_node("SUBTRACT", n.outputs["Fac"], 0.5))
        return math_node("POWER", math_node("SUBTRACT", 1.0, math_node("MULTIPLY", ridge, 2.0), clamp=True), 3.0)
    soft = folds(0.012)
    height = math_node("ADD", math_node("MULTIPLY", grain.outputs["Distance"], 0.06),
                       math_node("MULTIPLY", soft, -1.0))
    bump = N.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    bump.inputs["Distance"].default_value = 1.5
    L(height, bump.inputs["Height"])
    L(bump.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Specular IOR Level"].default_value = 0.5
    p.inputs["Coat Weight"].default_value = 0.15
    p.inputs["Coat Roughness"].default_value = 0.35
    return m


def lounge_chair(m, x, y, rot_deg):
    """A mid-century lounge chair, unbranded (owner, 31 Oct 2026: peeking in from the right
    edge; then "too blocky and rigid; the antithesis of the natural curves"): three dished
    rosewood plywood shells (seat, back, headrest) curling up round soft, pleated black leather
    cushions that roll over their top edges, curved cast arms with leather pads, a five-star
    swivel base. About 840 wide and high. Built facing -y, then turned by rot_deg about z."""
    ply = finish(wood("lounge_rosewood", (0.085, 0.026, 0.011), rough=0.3, scale=0.018, stretch=(1.0, 7.0, 1.0)),
                 bump=0.05, scale=2.0, coat=0.55)
    leather = leather_mat("lounge_leather")
    alu = C.clay("lounge_alu", 0.05, 0.35, metallic=0.7)
    alu_edge = C.clay("lounge_alu_edge", 0.85, 0.18, metallic=1.0)
    button = C.clay("lounge_button", 0.004, 0.45)
    root = bpy.data.objects.new("lounge_chair", None)
    C.link(root)
    parts = []
    def add(ob):
        parts.append(ob)
        return ob
    def group_at(objs, loc, tilt):
        e = bpy.data.objects.new("lounge_sub", None)
        C.link(e)
        e.location = loc
        e.rotation_euler = (math.radians(tilt), 0, 0)
        for o in objs:
            o.parent = e
            parts.remove(o)
        parts.append(e)
        return e

    def section(name, w, d, h, rim, buttons):
        """A deep moulded rosewood tub and the plump tufted leather cushion that fills it, lying
        flat (cushion up +z, +y toward the shell's top edge). rim = (front, sides, back): how far
        the rim rises at each edge. The cushion's underside follows the tub; its top swells and
        is dimpled at each button (fractions of the cushion), with soft creases between."""
        rf, rs, rb = rim
        def shell_z(px, py, t):
            u, v = px / (w / 2), py / (d / 2)
            edge = rf + (rs - rf) * min(1.0, abs(u) ** 2 * 1.4) + max(0.0, v) ** 2 * (rb - rf)
            return edge * t ** 3.2
        objs = [add(_pad(f"{name}_shell", w, d, shell_z, None, ply, n=3.4, rings=18))]
        cw, cd = w - 50, d - 44
        bpts = [((bu - 0.5) * cw, (bv - 0.5) * cd) for bu, bv in buttons]
        def dimples(px, py):
            dz = 0.0
            for bx, by in bpts:
                r2 = (px - bx) ** 2 + (py - by) ** 2
                dz += math.exp(-r2 / 900.0) + 0.25 * math.exp(-r2 / 9000.0)
            return min(dz, 1.0)
        def cush_bot(px, py, t):
            return 16 + shell_z(px * w / cw, py * d / cd, t * 0.97)
        def cush_top(px, py, t):
            return cush_bot(px, py, t) + 10 + h * (1 - t ** 4) ** 0.55 - 0.45 * h * dimples(px, py) * (1 - t ** 3)
        objs.append(add(_pad(f"{name}_cush", cw, cd, cush_top, cush_bot, leather, n=3.4, rings=22, segs=112)))
        for bx, by in bpts:
            tb = max(abs(2 * bx / cw), abs(2 * by / cd))
            objs.append(add(C.sphere(f"{name}_button", 6, (bx, by, cush_top(bx, by, tb) - 2), button)))
        return objs

    # the five-star base: arched cast legs, dark with polished edges, on glides
    for k in range(5):
        a = 2 * math.pi * k / 5 - math.pi / 2
        ca, sa = math.cos(a), math.sin(a)
        add(cable(f"lounge_leg{k}", [(ca * 30, sa * 30, 90), (ca * 170, sa * 170, 72), (ca * 310, sa * 310, 34),
                                     (ca * 330, sa * 330, 22)], 14, alu))
        add(cable(f"lounge_leg_edge{k}", [(ca * 40, sa * 40, 102), (ca * 170, sa * 170, 84), (ca * 305, sa * 305, 46)],
                  3, alu_edge))
        add(C.cylinder("lounge_glide", 17, 14, (ca * 332, sa * 332, 8), alu, segs=20))
    add(C.cylinder("lounge_column", 34, 120, (0, 0, 150), alu, segs=32, bevel=4))
    add(C.cylinder("lounge_hub", 70, 40, (0, 0, 96), alu, segs=32, bevel=8))
    # the seat tub (its front higher), the back tub and the near-square headrest, reclined;
    # proportions from the owner's reference photo (a 1950s rosewood example, 1stDibs)
    group_at(section("lounge_seat", 720, 650, 120, (40, 175, 150),
                     ((0.3, 0.38), (0.7, 0.38), (0.5, 0.62), (0.3, 0.75), (0.7, 0.75))), (0, -10, 225), -12)
    group_at(section("lounge_back", 730, 380, 105, (60, 150, 90), ((0.3, 0.45), (0.7, 0.45), (0.5, 0.7))),
             (0, 368, 430), 52)
    group_at(section("lounge_head", 690, 320, 100, (50, 120, 70), ((0.32, 0.5), (0.68, 0.5))),
             (0, 571, 744), 64)
    # the spines joining the back tub to the headrest, behind
    for sx in (-1, 1):
        add(cable(f"lounge_spine{sx}", [(sx * 120, 440, 470), (sx * 120, 530, 600), (sx * 120, 575, 690)], 9, alu))
    # the arms: plump leather pads on cast brackets over the seat tub's raised sides
    for sx in (-1, 1):
        ax = sx * 340
        pad = add(_pad(f"lounge_arm_pad{sx}", 112, 420,
                       lambda px, py, t: 46 * (1 - t ** 4) ** 0.5, lambda px, py, t: -40 * (1 - t ** 5) ** 0.5,
                       leather, n=3.0, rings=10, segs=64))
        pad.location = (ax, -70, 470)
        pad.rotation_euler = (math.radians(-6), 0, 0)
        add(cable(f"lounge_arm{sx}", [(ax, -150, 430), (ax * 1.02, 40, 420), (ax * 1.02, 170, 445)], 12, alu))
    for o in parts:
        o.parent = root
    root.location = (x, y, 0)
    root.rotation_euler = (0, 0, math.radians(rot_deg))
    # the lamp beside it would flood the black leather to grey from half a metre: keep the
    # chair out of its light, so it reads dark against the glow
    excl = bpy.data.collections.get("lamp70_shade_only")
    if excl:
        def walk(o):
            for c in o.children:
                if c.type in ("MESH", "CURVE"):
                    excl.objects.link(c)
                    excl.collection_objects[-1].light_linking.link_state = "EXCLUDE"
                walk(c)
        walk(root)
    return root


def leafy(m, name, base, rng, n, h=(400, 900), spread=(150, 380), leaf=(110, 38, 6), trail=False):
    """Stems from `base` ending in leaves; trail=True arcs them out and down (a trailing plant)."""
    bx, by, bz = base
    for k in range(n):
        a = 2 * math.pi * k / n + rng.uniform(-0.25, 0.25)
        ca, sa = math.cos(a), math.sin(a)
        if trail:
            r = rng.uniform(*spread)
            drop = rng.uniform(*h)
            pts = [(bx, by, bz), (bx + ca * r * 0.5, by + sa * r * 0.5, bz + 40),
                   (bx + ca * r, by + sa * r, bz - drop * 0.3), (bx + ca * r * 1.1, by + sa * r * 1.1, bz - drop)]
            cable(f"{name}_stem{k}", pts, 1.6, m["leaf"])
            for j in range(7):
                t = 0.25 + j * 0.12
                q = Vector(pts[1]).lerp(Vector(pts[2]), min(1, t * 1.6)) if t < 0.6 else \
                    Vector(pts[2]).lerp(Vector(pts[3]), (t - 0.6) / 0.6)
                lf = C.sphere(f"{name}_leaf{k}_{j}", 1.0, q, m["leaf"])
                lf.scale = (leaf[0] * rng.uniform(0.7, 1.1), leaf[1], leaf[2])
                lf.rotation_euler = (rng.uniform(-1.0, 1.0), rng.uniform(-0.8, 0.8), a + rng.uniform(-0.8, 0.8))
        else:
            hh = rng.uniform(*h)
            r = rng.uniform(*spread)
            tip = (bx + ca * r, by + sa * r, bz + hh)
            mid = (bx + ca * r * 0.2, by + sa * r * 0.2, bz + hh * 0.6)
            cable(f"{name}_stem{k}", [(bx, by, bz), mid, tip], 3, m["leaf"])
            lf = C.sphere(f"{name}_leaf{k}", 1.0, tip, m["leaf"])
            lf.scale = (leaf[0] * rng.uniform(0.8, 1.15), leaf[1], leaf[2])
            lf.rotation_euler = (rng.uniform(-0.6, 0.6), rng.uniform(-0.5, 0.5), a)


def plants70(m, riser_end, rz):
    """More plants (owner, 31 Oct 2026): a rubber plant on the floor at the right, a trailing
    plant on the shelf's right end, a macrame hanger with a trailing plant at the left."""
    import random
    rng = random.Random(11)
    # snake plant: a terracotta pot, tall upright blades
    px, py = 1020.0, 300.0
    C.cylinder("pot2", 120, 260, (px, py, 130), m["terracotta"], segs=48, r2=150, bevel=4)
    C.cylinder("soil2", 140, 10, (px, py, 255), m["rubber"], segs=48)
    for k in range(16):
        a = 2 * math.pi * k / 16 + rng.uniform(-0.3, 0.3)
        r = rng.uniform(10, 90)
        h = rng.uniform(420, 820)
        bl = C.sphere(f"snake_blade{k}", 1.0, (px + math.cos(a) * r, py + math.sin(a) * r, 255 + h / 2), m["leaf"])
        bl.scale = (rng.uniform(22, 34), 4, h / 2)
        bl.rotation_euler = (math.sin(a) * rng.uniform(0.05, 0.2), -math.cos(a) * rng.uniform(0.05, 0.2), a + 1.57)
    # trailing plant on the shelf
    sx, sy = riser_end - 55, 255.0
    C.cylinder("pot3", 52, 95, (sx, sy, rz + 47), m["terracotta"], segs=40, r2=62, bevel=3)
    C.cylinder("soil3", 58, 6, (sx, sy, rz + 92), m["rubber"], segs=40)
    leafy(m, "trail", (sx, sy, rz + 95), rng, 9, h=(120, 330), spread=(60, 150), leaf=(46, 30, 4), trail=True)
    # macrame hanger between the bookshelf and the desk: cords up out of frame, a round pot
    hx, hy, hz = -1120.0, 470.0, 1210.0
    rope = C.clay("macrame", rgb=(0.62, 0.52, 0.38), rough=0.95)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        cable(f"macrame{k}", [(hx + math.cos(a) * 110, hy + math.sin(a) * 110, hz - 40),
                              (hx + math.cos(a) * 60, hy + math.sin(a) * 60, hz + 260), (hx, hy, hz + 700)], 2.5, rope)
    C.sphere("pot4", 1.0, (hx, hy, hz), m["terracotta"]).scale = (115, 115, 85)
    C.cylinder("soil4", 95, 6, (hx, hy, hz + 60), m["rubber"], segs=40)
    leafy(m, "hang", (hx, hy, hz + 62), rng, 12, h=(250, 650), spread=(110, 230), leaf=(52, 34, 4), trail=True)


def tape_cross4():
    """Round 4 (owner, 31 Oct 2026: something on the desk that would be recorded onto the tape,
    and of the era): the cross-patched decks on a low walnut shelf at the back of the desk; in
    front, a briefcase synth (the Synthi AKS of Discreet Music) with its keyboard lid beside it;
    a tape echo at the right; an electric guitar leaning on the desk. See gear.py."""
    import gear
    global DESK
    m = mats3()
    gear.mats(m)
    desk0 = DESK
    DESK = dict(desk0, y0=-640.0)   # a studio-depth desk (1 m): the AKS opens flat to 802 mm
    # the owner, 31 Oct: no lamp (room for the echo); the plant forward, its leaves clear of the bookshelf
    bulb = room(m, lamp_x=-880.0, show_lamp=False, plant=(-1420.0, -140.0))
    # the shelf: a walnut top on two end blocks
    rz = DESK_Z + 165.0
    sx0 = -230.0   # the shelf's centre, left of the desk's so the echo has room at the right
    B("riser_top", sx0 - 660, sx0 + 660, 130, 375, rz - 22, rz, m["walnut"], bevel=3)
    for sx in (-1, 1):
        B("riser_end", sx0 + sx * 640 - 20, sx0 + sx * 640 + 20, 140, 365, DESK_Z, rz - 22, m["walnut"], bevel=3)
    gap = 170.0
    xa, xb = sx0 - (A77_W + gap) / 2, sx0 + (A77_W + gap) / 2
    ref = deck_pair(m, (xa, xb), 155.0, z0=rz)
    # the AKS open flat, its panel half's far edge just under the shelf
    syn = gear.place(gear.synthi(), (-560, -230, DESK_Z), (0, 0, 3))
    echo = gear.place(gear.tape_echo(), (720, 70, DESK_Z), (0, 0, -16))
    gtr = gear.place(gear.guitar(), (850, -775, 262), (75, 0, -10))
    # the SH-2000 (owner, 31 Oct: the synth beside the EP-3 in the Met's photo), front right
    sh = gear.place(gear.sh2000(), (185, -618, DESK_Z), (0, 0, 0))
    floor_lamp70(m, 1150.0, 620.0)
    lounge_chair(m, 1540.0, 400.0, -65.0)
    plants70(m, sx0 + 660.0, rz)
    # leads (owner, 31 Oct: the synth and keyboard need cables): the keyboard lid's multi-way
    # lead into the synth's side, the synth's two outputs back to deck A, the guitar into the
    # echo's input, the echo's output to deck B, mains leads off the back of the desk
    W = lambda e, p: e.matrix_world @ Vector(p)
    bpy.context.view_layer.update()
    # the keyboard's lead: from the lid's corner socket up into the panel's multipin connector
    k0, k1 = W(syn, (gear._px(830), gear._ly(735), gear.LID_H)), W(syn, (gear._px(466), gear._py(74), gear.SY_H + 4))
    cable("ks_lead", [k0, k0 + Vector((30, 20, 25)), Vector((k0.x + 60, k0.y + 200, DESK_Z + 70)),
                      Vector((k0.x + 20, k1.y - 30, k1.z + 70)), k1 + Vector((0, 0, 30)), k1], 3.0, m["cable_grey"])
    for j, mat in enumerate((m["cable"], m["cable_grey"])):
        o0 = W(syn, (gear._px(225 + 37 * j), gear._py(92), gear.SY_H))
        d_in = Vector((xa + 90 + 20 * j, 130 + A77_D, rz + 60))
        cable(f"synth_out{j}", [o0, o0 + Vector((0, 10, 30)), Vector((o0.x - 10, o0.y + 60, DESK_Z + 120)),
                                Vector((d_in.x, 340, DESK_Z + 5)), Vector((d_in.x, 390, rz - 40)), d_in], 2.6, mat)
    def centre(name):   # world centre of a part (its mesh, not its origin, which may sit elsewhere)
        ob = bpy.data.objects[name]
        return sum((ob.matrix_world @ Vector(c) for c in ob.bound_box), Vector()) / 8
    def plug(name, tip, nrm, length=46):
        """A jack plug standing out of a socket at tip along nrm; returns where its lead leaves."""
        body = C.cylinder(name, 6.5, length, tip + nrm * (length / 2), m["cable"], segs=24, bevel=2)
        body.rotation_euler = nrm.to_track_quat("Z", "Y").to_euler()
        C.cylinder(name + "_sleeve", 4.2, 10, tip + nrm * 4, m["steel"], segs=20).rotation_euler = \
            body.rotation_euler
        return tip + nrm * length
    # the echo's INPUT jack on the sloped panel, and the guitar's output jack on the lower bout's rim
    er = echo.matrix_world.to_3x3()
    e_n = (er @ Vector((0, -math.sin(math.radians(gear.EC_SLOPE)), math.cos(math.radians(gear.EC_SLOPE))))).normalized()
    e_fwd = (er @ Vector((0, -1, 0))).normalized()
    e0 = centre("ec_jack")
    e1 = plug("echo_in_plug", e0, e_n)
    gr = gtr.matrix_world.to_3x3()
    g0 = gtr.matrix_world @ Vector((150, -175, 26))
    g_n = (gr @ Vector((0.8, -0.6, 0))).normalized()
    g1 = plug("guitar_plug", g0, g_n)
    # down from the guitar to the floor, round and up the desk's right end, over the edge, along
    # the top behind the SH-2000, then up over the echo's front wall into the plug
    x1 = DESK["x1"]
    top = DESK_Z + 3.5
    cable("guitar_lead", [g1, g1 + g_n * 40 + Vector((0, 0, -30)), Vector((x1 + 60, -640, 120)),
                          Vector((x1 + 40, -420, 60)), Vector((x1 + 22, -320, DESK_Z - 260)),
                          Vector((x1 + 14, -300, DESK_Z - 60)), Vector((x1 + 9, -300, DESK_Z - 8)),
                          Vector((x1 + 4, -300, top + 2)), Vector((x1 - 20, -300, top)),
                          Vector((x1 - 140, -305, top)), Vector((e0.x + 40, -300, top)),
                          e0 + e_fwd * 170 + Vector((0, 0, top - e0.z)), e0 + e_fwd * 95 + Vector((0, 0, top - e0.z)),
                          e0 + e_fwd * 60 + Vector((0, 0, top - e0.z + 25)), e1 + e_fwd * 35 + e_n * 15,
                          e1 + e_n * 12, e1], 2.8, m["cable"])
    eo = W(echo, (-120, gear.EC_D, 60))
    cable("echo_out", [eo, eo + Vector((0, 40, -40)), Vector((eo.x - 60, 360, DESK_Z + 4)),
                       Vector((xb + 120, 395, rz - 30)), Vector((xb + 110, 130 + A77_D, rz + 60))], 2.6,
          m["cable_grey"])
    so = W(sh, (gear._shx(0.67), 225, 113))   # the SH-2000's output, on top of its rear rail
    cable("sh_out", [so, so + Vector((0, 20, 35)), Vector((so.x - 40, so.y + 120, DESK_Z + 40)),
                     Vector((so.x - 120, 100, DESK_Z + 4)), Vector((xb - 40, 395, rz - 30)),
                     Vector((xb - 60, 130 + A77_D, rz + 60))], 2.6, m["cable"])
    for p0 in (W(syn, (gear._px(685), gear.SY_D, 40)), W(echo, (100, gear.EC_D, 30)), W(sh, (300, gear.SH_D, 40))):
        cable("mains", [p0, p0 + Vector((0, 50, -25)), Vector((p0.x, DESK["y1"] - 15, DESK_Z + 4)),
                        Vector((p0.x + 15, DESK["y1"] + 30, DESK_Z - 60)), Vector((p0.x + 30, DESK["y1"] + 45, 300))],
              2.8, m["cable"])
    dim = float(os.environ.get("IV_STORY_DIM", STORY_DIM))
    room_lights(bulb, A77_W, dim=dim)
    deck_lights(ref, dim=dim)
    # (the gear's own key light is gone: owner, 31 Oct, it glared in deck A's left reel; the
    # 1970s floor lamp at the right lights the desk instead)
    ref.update(center=ref["decks_center"] + Vector((0, 60, 0)), size=ref["decks_width"],
               radius=ref["decks_width"] / 2 + 30, desk_front=Vector((0, -330, DESK_Z + 120)))
    DESK = desk0
    return ref
