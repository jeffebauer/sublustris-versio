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

import bmesh
import bpy
from mathutils import Vector

import common as C
import story

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

def room(m, rack=False):
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
    lx, ly = -760.0, 230.0
    C.cylinder("lamp_base", 75, 22, (lx, ly, DESK_Z + 11), m["knob"], segs=48, bevel=4)
    arm = [(lx, ly, DESK_Z + 20), (lx + 20, ly - 10, DESK_Z + 300), (lx + 140, ly - 60, DESK_Z + 430)]
    cable("lamp_arm", arm, 5, m["steel"])
    shade = C.cylinder("lamp_shade", 85, 140, (lx + 200, ly - 90, DESK_Z + 400), m["knob"], segs=48, r2=28)
    shade.rotation_euler = (math.radians(-25), math.radians(35), 0)
    bulb = Vector((lx + 215, ly - 100, DESK_Z + 370))
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
    px, py = -1300.0, 300.0
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


def room_lights(bulb, size, warm=1.0, window=0.25):
    """The lamp (a warm point light in the shade), a dim cool window light from
    the left, low warm world bounce."""
    C.world(0.03, rgb=(0.55, 0.45, 0.35))
    ld = bpy.data.lights.new("lamp", "POINT")
    ld.energy = C.watts(60 * warm)
    ld.color = (1.0, 0.72, 0.45)
    ld.shadow_soft_size = 20
    ob = bpy.data.objects.new("lamp", ld)
    C.link(ob)
    ob.location = bulb
    C.area_light("window", Vector((SIDE_X + 60, -600, 1700)), Vector((0, 100, DESK_Z)), size=1400,
                 power=C.watts(C.KEY_W * window) * (2.2) ** 2, rgb=(0.75, 0.82, 1.0))
    # a soft warm fill from behind the camera so faces don't go black
    C.area_light("fill", Vector((600, -2600, 1500)), Vector((0, 0, DESK_Z + 150)), size=1200,
                 power=C.watts(C.KEY_W * 0.12) * (3.0) ** 2, rgb=(1.0, 0.85, 0.7))


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
    digits("0127", -13, 331, 9, o, -4.3, C.clay("counter_white", 0.8, 0.5), pitch=8.5)
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


def studio_rack():
    """The room with a wooden rack beside the desk: patch bay, reverb, delay,
    pitch shifter, graphic EQ, blanks; the reverb's remote and a small mixer
    on the desk, its cable running to the rack."""
    m = mats()
    bulb = room(m, rack=True)
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
    for j in range(4):
        B("desk_tapebox", 600, 785, 60 + j * 4, 245 + j * 4, DESK_Z + j * 24, DESK_Z + j * 24 + 23,
          m["paper"] if j % 2 else C.clay("box_dark", rgb=(0.18, 0.12, 0.08), rough=0.6), bevel=1.5)
    room_lights(bulb, 1500)
    # a warm practical on the rack side so the units read (a second lamp, off-frame right)
    ld = bpy.data.lights.new("rack_lamp", "POINT")
    ld.energy = C.watts(18)
    ld.color = (1.0, 0.75, 0.5)
    ld.shadow_soft_size = 60
    lo = bpy.data.objects.new("rack_lamp", ld)
    C.link(lo)
    lo.location = (rx + 500, ry - 500, 1300)
    return dict(center=Vector((300, 0, 900)), size=2600, radius=1500,
                rack=Vector((rx, ry, base + inner / 2)), reverb=Vector((rx, ry - 3, zc_rev)),
                remote=rem, desk=Vector((0, 0, DESK_Z)))
