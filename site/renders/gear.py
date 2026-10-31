"""Period instruments for the story scene (owner, 31 Oct 2026: the decks need something on
the desk that would be recorded onto them, and it should be of the era). All unbranded.
See STORY_RESEARCH.md: Discreet Music (1975) was an EMS Synthi AKS through a graphic EQ and an
echo unit into two Revox decks; on (No Pussyfooting) (1973) Fripp played electric guitar.

  synthi()      a briefcase synth in the Synthi AKS layout: the base holds the panel (pin
                matrix, knobs, joystick, a small meter), the lid lies flat beside it with the
                flat touch keyboard and the sequencer.
  tape_echo()   a 1970s tape echo in the Echoplex EP-3 manner: a tolex box, the tape
                cartridge under a cover on top, the delay slider, three knobs on the front.
  guitar()      a black single-cut Custom, three gold humbuckers, leaning on the desk.

Each builds at the origin (x across, y back, z up; mm) and returns an empty that carries
every part, to be moved and turned into place.
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

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
        rubber=m["rubber3"], cable=m["cable"], cable_grey=m["cable_grey"],
        case=studio.finish(C.clay("synthi_case", rgb=(0.02, 0.02, 0.02), rough=0.5), bump=0.25, scale=1.6,
                           rough_var=0.2),   # black moulded plastic, a fine pebble grain
        panel=studio.finish(C.clay("synthi_panel", rgb=(0.66, 0.66, 0.64), rough=0.42, metallic=1.0), bump=0.01,
                            scale=4.0, rough_var=0.12),   # satin silver aluminium
        lidblack=studio.finish(C.clay("synthi_lid", rgb=(0.025, 0.025, 0.027), rough=0.55), bump=0.03, scale=3.0),
        ink=C.clay("gear_ink", 0.01, 0.6),
        label=C.clay("gear_label", 0.78, 0.6),
        print=C.clay("gear_print", 0.78, 0.5),
        chrome=C.steel("gear_chrome", 0.8, 0.12),
        brass=C.steel("gear_brass_dark", 0.35, 0.4),
        pin_w=C.clay("pin_white", 0.78, 0.3), pin_r=C.clay("pin_red", rgb=(0.6, 0.04, 0.03), rough=0.3),
        pin_g=C.clay("pin_green", rgb=(0.04, 0.4, 0.1), rough=0.3),
        rack_blue=C.clay("synthi_rack", rgb=(0.04, 0.12, 0.45), rough=0.4),
        blue_plate=C.clay("synthi_blue", rgb=(0.10, 0.30, 0.65), rough=0.35),
        blue_key=studio.finish(C.clay("synthi_key", rgb=(0.10, 0.28, 0.66), rough=0.3, coat=0.3), bump=0.0,
                               rough_var=0.2),
        meter=C.emissive("synthi_meter", "#EDE4CC", 0.5, base=0.85),
        led=C.emissive("gear_red_lamp", "#FF3A1A", 4.0),
        tolex=studio.finish(C.clay("echo_tolex", rgb=(0.03, 0.03, 0.03), rough=0.6), bump=0.7, scale=1.4,
                            rough_var=0.2),
        maroon=studio.finish(C.clay("echo_maroon", rgb=(0.32, 0.02, 0.04), rough=0.3, metallic=0.8), bump=0.01,
                             scale=4, rough_var=0.15),
        gloss_black=C.clay("echo_cover", 0.01, 0.08, coat=0.6),
        rosewood=studio.finish(C.clay("sh_rosewood", rgb=(0.20, 0.07, 0.035), rough=0.45, coat=0.2), bump=0.15,
                               scale=2.0),
        sh_key_white=C.clay("sh_key_white", rgb=(0.80, 0.74, 0.60), rough=0.3, coat=0.3),
        tab_green=C.clay("sh_tab_green", rgb=(0.03, 0.30, 0.07), rough=0.3, coat=0.4),
        tab_grey=C.clay("sh_tab_grey", rgb=(0.50, 0.50, 0.48), rough=0.3, coat=0.4),
        tab_amber=C.clay("sh_tab_amber", rgb=(0.86, 0.36, 0.02), rough=0.3, coat=0.4),
        tab_red=C.clay("sh_tab_red", rgb=(0.45, 0.015, 0.02), rough=0.3, coat=0.4),
        tab_cream=C.clay("sh_tab_cream", rgb=(0.80, 0.74, 0.60), rough=0.3, coat=0.4),
        sheet=C.clay("echo_sheet", rgb=(0.72, 0.68, 0.55), rough=0.7),
        black_lacquer=C.clay("lp_black", 0.008, 0.1, coat=1.0),
        binding=C.clay("lp_binding", rgb=(0.72, 0.64, 0.46), rough=0.3, coat=0.5),
        guard=C.clay("lp_guard", 0.01, 0.25, coat=0.3),
        board=studio.finish(C.clay("lp_ebony", 0.012, 0.45), bump=0.04, scale=3),
        pearl=C.clay("lp_pearl", rgb=(0.82, 0.80, 0.74), rough=0.15, coat=0.6),
        gold=C.clay("lp_gold", rgb=(0.80, 0.60, 0.28), rough=0.18, metallic=1.0),
        string=C.steel("guitar_string", 0.8, 0.2),
        fret=C.clay("lp_fret", rgb=(0.78, 0.70, 0.5), rough=0.25, metallic=1.0),
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


# ------------------------------------------------------------------ the Synthi AKS

# Sizes and layout: the V&A's description of Eno's own AKS (485 x 432 x 137 mm closed, 802 x 485 x
# 105 mm open), positions read off the Commons photo "EMS Synthi AKS (opened)" (STORY_RESEARCH.md,
# round 4). The photo's pixels map to the panel half by _px / _py; the lid by _ly.
SY_W, SY_D, SY_H, LID_H = 485.0, 400.0, 105.0, 32.0


def _px(px):
    return (px - 470) / 880 * SY_W


def _py(py):   # panel half: y = 0 at the hinge (towards the player), 400 at the far edge
    return (660 - py) / 610 * SY_D


def _ly(py):   # lid, lying flat in front: y = 0 at the hinge, -400 at its front edge
    return (1340 - py) / 660 * SY_D - SY_D


CAPS = {}


def sknob(name, px, py, colour, z, big=False):
    """A Synthi knob: black skirt and body, a domed coloured cap (blue frequency, white level,
    green shape, yellow filter, red envelope); the big frequency knobs sit in chrome bezels."""
    x, y = _px(px), _py(py)
    r = 8.5 if big else 7.0
    if big:
        cyl(name + "_bezel", 15, 4, (x, y, z + 2), M["chrome"], segs=40, bevel=1.2)
        z += 4
    cyl(name + "_skirt", r * 1.25, 2.4, (x, y, z + 1.2), M["knob"], segs=36, bevel=0.6)
    cyl(name, r, 8, (x, y, z + 6), M["knob"], segs=36, bevel=0.8, r2=r * 0.95)
    cap = _track(C.sphere(name + "_cap", 1.0, (x, y, z + 10), CAPS[colour]))
    cap.scale = (r * 0.9, r * 0.9, 3.2)


def jack(name, x, y, z):
    cyl(name, 5.2, 2, (x, y, z + 1), M["chrome"], segs=24, bevel=0.5)
    cyl(name + "_hole", 3.0, 0.4, (x, y, z + 2.1), M["void"], segs=16)


def rule(x0, x1, y0, y1, z, mat=None):
    box("rule", x0, x1, y0, y1, z, z + 0.15, mat or M["ink"])


def _shell(name, x0, x1, y0, y1, h, floor, rim):
    """An open case: solid up to `floor`, then a rim `rim` wide up to `h`, so whatever sits on
    the floor shows (a solid box hid the panel)."""
    box(name, x0, x1, y0, y1, 0, floor, M["case"], bevel=8)
    for a, b, c, d in ((x0, x1, y0, y0 + rim), (x0, x1, y1 - rim, y1), (x0, x0 + rim, y0, y1), (x1 - rim, x1, y0, y1)):
        box(name + "_rim", a, b, c, d, floor - 4, h, M["case"], bevel=3)


def synthi():
    """The AKS open flat: the panel half behind (silver panel in a black moulded case), the lid
    in front with the KS touch keyboard along its front edge. One empty, y = 0 at the hinge."""
    for k, rgb in dict(blue=(0.03, 0.12, 0.55), white=(0.80, 0.80, 0.77), green=(0.03, 0.38, 0.10),
                       yellow=(0.72, 0.58, 0.05), red=(0.62, 0.03, 0.02)).items():
        CAPS[k] = C.clay(f"synthi_cap_{k}", rgb=rgb, rough=0.22, coat=0.4)
    w2 = SY_W / 2
    # ---- the panel half: black moulded case, silver panel recessed 4 mm below its rim
    top = SY_H - 4.0
    _shell("sy_case", -w2, w2, 0, SY_D, SY_H, top - 3, 10.0)
    box("sy_panel", -w2 + 10, w2 - 10, 10, SY_D - 10, top - 3, top, M["panel"], bevel=0.6)
    z = top
    for px in (115, 750):   # the built-in speakers' grilles, top corners
        for k in range(3):
            box("sy_grille", _px(px), _px(px + 78), _py(100 + k * 16) - 1.6, _py(100 + k * 16) + 1.6, z - 0.4, z + 0.1,
                M["void"])
    for px in (225, 262, 302, 338, 378, 414, 515, 552, 590, 628):   # jacks in pairs along the top
        jack("sy_jack", _px(px), _py(92), z)
    box("sy_ks_socket", _px(450), _px(482), _py(60), _py(88), z, z + 9, M["chrome"], bevel=1.5)
    cyl("sy_power_socket", 9, 3, (_px(685), _py(102), z + 1.5), M["void"], segs=24, bevel=0.8)
    # the meter: black bezel, white face, needle
    box("sy_meter_bezel", _px(208), _px(302), _py(204), _py(122), z, z + 2.5, M["void"], bevel=1)
    box("sy_meter_face", _px(214), _px(296), _py(198), _py(128), z + 2.5, z + 2.8, M["meter"])
    nd = box("sy_needle", _px(254) - 0.4, _px(254) + 0.4, _py(195), _py(140), z + 2.9, z + 3.1, M["void"])
    nd.rotation_euler = (0, 0, math.radians(20))
    cyl("sy_led", 2.2, 2, (_px(330), _py(165), z + 1), M["led"], segs=16)
    for px in (145, 785):
        cyl("sy_toggle", 2.4, 6, (_px(px), _py(185), z + 3), M["chrome"], segs=12)
    cyl("sy_power_tog", 2.6, 9, (_px(688), _py(140), z + 4.5), M["chrome"], segs=12)
    cyl("sy_pilot", 4.5, 3, (_px(688), _py(188), z + 1.5), M["led"], segs=20, bevel=1)
    # knobs, as photographed (blue frequency, white level, green shape, yellow filter, red envelope)
    for px, col in ((398, "blue"), (465, "yellow"), (532, "white"), (608, "white")):
        sknob("sy_k_filter", px, 170, col, z)
    for py in (252, 330, 410):   # oscillators 1-3
        sknob("sy_k_osc", 115, py, "blue", z, big=True)
        for px, col in ((192, "green"), (268, "white"), (345, "white")):
            sknob("sy_k_osc", px, py, col, z)
    for px, col in ((425, "red"), (502, "red"), (578, "red"), (655, "red"), (735, "white"), (812, "white")):
        sknob("sy_k_env", px, 252, col, z)
    cyl("sy_env_led", 2.2, 2, (_px(465), _py(222), z + 1), M["led"], segs=16)
    for px, py, col in ((735, 334, "green"), (815, 334, "white"), (735, 410, "white"), (722, 497, "green"),
                        (827, 497, "green"), (108, 500, "blue"), (185, 500, "white"), (262, 497, "yellow"),
                        (340, 497, "yellow"), (102, 582, "white"), (182, 582, "green"), (258, 582, "green"),
                        (340, 582, "white")):
        sknob("sy_k", px, py, col, z)
    cyl("sy_trim", 5, 4, (_px(820), _py(410), z + 2), M["chrome"], segs=24, bevel=0.8)
    # the module boxes: thin black rules, and a short black bar for each printed label
    L, R, X1, X2 = _px(55), _px(888), _px(388), _px(685)
    for py in (125, 210, 290, 370, 450, 540):
        rule(L, X1 if py >= 290 else R, _py(py) - 0.35, _py(py) + 0.35, z)
    for py in (370, 450, 540):
        rule(X2, R, _py(py) - 0.35, _py(py) + 0.35, z)
    rule(X1 - 0.35, X1 + 0.35, _py(640), _py(210), z)
    rule(X2 - 0.35, X2 + 0.35, _py(640), _py(290), z)
    for px in (205, 360, 440, 572, 650):
        rule(_px(px) - 0.35, _px(px) + 0.35, _py(125), _py(65), z)
    # the pin matrix: a black square, 16 x 16 sockets in 4 x 4 blocks split by white rules,
    # white legend tables to its left and above, Eno-style patch pins in, the pin rack below
    mx0, mx1, my0, my1 = _px(478), _px(665), _py(532), _py(355)
    box("sy_matrix", mx0, mx1, my0, my1, z, z + 1.0, M["void"], bevel=0.4)
    box("sy_legend_l", _px(400), _px(474), _py(540), _py(300), z, z + 0.4, M["label"])
    box("sy_legend_t", mx0, mx1, _py(350), _py(296), z, z + 0.4, M["label"])
    for k in range(1, 12):
        rule(_px(400), _px(474), _py(300 + k * 20) - 0.25, _py(300 + k * 20) + 0.25, z + 0.4)
        rule(mx0 + k * (mx1 - mx0) / 12 - 0.25, mx0 + k * (mx1 - mx0) / 12 + 0.25, _py(350), _py(296), z + 0.4)
    pw, ph = (mx1 - mx0) / 16, (my1 - my0) / 16
    for k in (4, 8, 12):
        rule(mx0 + k * pw - 0.3, mx0 + k * pw + 0.3, my0, my1, z + 1.0, M["print"])
        rule(mx0, mx1, my0 + k * ph - 0.3, my0 + k * ph + 0.3, z + 1.0, M["print"])
    pins = {(0, 2): "pin_w", (1, 9): "pin_w", (2, 5): "pin_r", (4, 12): "pin_w", (6, 1): "pin_w",
            (7, 14): "pin_r", (9, 6): "pin_w", (10, 10): "pin_g", (12, 3): "pin_w", (13, 13): "pin_w",
            (15, 8): "pin_r"}
    for i in range(16):
        for j in range(16):
            x, y = mx0 + (i + 0.5) * pw, my1 - (j + 0.5) * ph
            if (i, j) in pins:
                cyl("sy_pin", 0.8, 5, (x, y, z + 3), M["steel"], segs=10)
                cyl("sy_pinhead", 2.3, 3.5, (x, y, z + 6.5), M[pins[(i, j)]], segs=14, bevel=0.6)
            else:
                cyl("sy_socket", 1.1, 0.2, (x, y, z + 1.05), M["brass"], segs=8)
    box("sy_rack", _px(405), _px(665), _py(598), _py(562), z, z + 3, M["rack_blue"], bevel=0.8)
    for k in range(20):
        x = _px(412) + k * (_px(660) - _px(412)) / 19
        col = "pin_r" if k % 5 in (1, 2) else ("pin_g" if k % 7 == 3 else "pin_w")
        cyl("sy_rackpin", 2.2, 4, (x, _py(580), z + 5), M[col], segs=14, bevel=0.6)
    box("sy_nameplate", _px(480), _px(660), _py(622), _py(600), z, z + 0.8, M["void"], bevel=0.3)   # no lettering
    # the joystick in its round mount, the red attack button beside it
    cyl("sy_joy_mount", 25, 6, (_px(775), _py(580), z + 3), M["knob"], segs=48, bevel=2)
    stick = cyl("sy_joy", 3.0, 40, (_px(775), _py(580), z + 24), M["knob"], segs=16)
    stick.rotation_euler = (math.radians(-10), math.radians(12), 0)
    _track(C.sphere("sy_joy_ball", 6.5, (_px(775) + 4.5, _py(580) - 3.8, z + 43), M["knob"]))
    cyl("sy_attack", 5.5, 5, (_px(848), _py(573), z + 2.5), CAPS["red"], segs=24, bevel=1.5)
    # chrome latches on the far edge, lift-off hinges on the near edge
    for px in (240, 700):
        box("sy_latch", _px(px) - 16, _px(px) + 16, SY_D - 2, SY_D + 6, SY_H - 34, SY_H - 4, M["chrome"], bevel=2)
    for px in (185, 750):
        box("sy_hinge", _px(px) - 22, _px(px) + 22, -6, 6, SY_H - 10, SY_H - 2, M["chrome"], bevel=1.5)

    # ---- the lid, flat in front: black interior, a tray with the sequencer's three program
    # cards, the control strip, and the 30-note touch keyboard along the front edge
    lz = LID_H - 3
    _shell("lid_case", -w2, w2, -SY_D, 0, LID_H + 4, lz - 2, 10.0)
    box("lid_floor", -w2 + 10, w2 - 10, -SY_D + 10, -10, lz - 2, lz, M["lidblack"], bevel=0.5)
    box("lid_tray_lip", _px(80), _px(860), _ly(1006), _ly(1000), lz, lz + 5, M["lidblack"], bevel=1)
    for px0, px1, py0 in ((370, 560, 750), (595, 785, 752), (622, 815, 838)):
        box("lid_card", _px(px0), _px(px1), _ly(py0 + 55), _ly(py0), lz, lz + 2, M["void"], bevel=0.8)
        box("lid_card_label", _px(px0 + 45), _px(px1 - 45), _ly(py0 + 35), _ly(py0 + 10), lz + 2, lz + 2.2,
            M["blue_plate"])
        box("lid_card_pins", _px(px0 + 20), _px(px1 - 20), _ly(py0 + 70), _ly(py0 + 55), lz, lz + 1.2, M["brass"])
    box("lid_badge", _px(335), _px(605), _ly(915), _ly(893), lz, lz + 1.0, M["blue_plate"], bevel=0.4)   # blank
    # control strip: a raised black bar with blue touch pads and the angled random pad
    box("lid_strip", _px(80), _px(860), _ly(1125), _ly(1020), lz, lz + 6, M["lidblack"], bevel=1.5)
    sz = lz + 6
    for px, py in ((100, 1068), (140, 1068), (180, 1068), (218, 1068), (375, 1068), (333, 1100)):
        box("lid_pad", _px(px) - 8, _px(px) + 8, _ly(py) - 6, _ly(py) + 6, sz, sz + 1.5, M["blue_plate"], bevel=1)
    rp = box("lid_random", _px(770), _px(845), _ly(1100), _ly(1058), sz, sz + 1.5, M["blue_plate"], bevel=2)
    rp.rotation_euler = (0, 0, math.radians(-8))
    for px0, px1 in ((145, 210), (635, 850)):
        box("lid_strip_label", _px(px0), _px(px1), _ly(1030), _ly(1022), sz, sz + 0.3, M["blue_plate"])
    for px in (655, 720, 790):
        cyl("lid_knob", 5, 5, (_px(px), _ly(1008), lz + 2.5), M["chrome"], segs=24, bevel=1)
    # the keyboard: 18 naturals as flat blue plates, 12 black sharps, set in a black bed
    box("lid_kbed", _px(75), _px(865), _ly(1318), _ly(1135), lz, lz + 1, M["lidblack"], bevel=1)
    kx0, kx1 = _px(82), _px(858)
    kw = (kx1 - kx0) / 18
    for k in range(18):
        x = kx0 + k * kw
        box("lid_white", x + 0.7, x + kw - 0.7, _ly(1310), _ly(1142), lz + 1, lz + 1.6, M["blue_key"], bevel=0.4)
    for k in range(17):
        if k % 7 in (2, 6):   # no sharp after E or B
            continue
        x = kx0 + (k + 1) * kw
        box("lid_black", x - 6.5, x + 6.5, _ly(1225), _ly(1145), lz + 1.6, lz + 2.4, M["void"], bevel=0.4)
    return group("synthi_aks")


# ------------------------------------------------------------------ the Echoplex EP-3

# About 370 x 240 mm, the walls 150 mm high (sources disagree: STORY_RESEARCH.md); the layout from the
# Met's photo of Steve Miller's c.1971 EP-3 (round 4, redone 31 Oct 2026 after "the tape echo looks
# odd"): a low front wall with the latch, the maroon panel sloping up from its top edge across the
# full width, the glossy black cartridge cover standing proud behind it, a cable well at the left,
# and the lid open behind, hinged at the back, its cream instruction sheet facing the player.
EC_W, EC_D, EC_H = 370.0, 240.0, 150.0
EC_FRONT = 112.0     # the front wall: the panel's front edge sits on it
EC_PANEL_D = 88.0    # the panel's depth, front to back
EC_SLOPE = 16.0      # degrees


def _pivot(name, n_before, loc, rot_deg):
    """Hang every part built since PARTS[n_before] on an empty at loc, turned by rot_deg."""
    e = bpy.data.objects.new(name, None)
    C.link(e)
    e.location = Vector(loc)
    for ob in PARTS[n_before:]:
        ob.location -= Vector(loc)
        ob.parent = e
    del PARTS[n_before:]
    _track(e)
    e.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    return e


def tape_echo():
    w2 = EC_W / 2
    t = 9.0
    # the box: tolex walls round a raised dark floor
    box("ec_floor", -w2, w2, 0, EC_D, 0, 12, M["tolex"], bevel=3)
    box("ec_inner", -w2 + t, w2 - t, t, EC_D - t, 0, 90, M["void"])
    box("ec_wall_f", -w2, w2, 0, t, 0, EC_FRONT, M["tolex"], bevel=3)
    box("ec_wall_b", -w2, w2, EC_D - t, EC_D, 0, EC_H, M["tolex"], bevel=3)
    for sx in (-1, 1):
        box("ec_wall_s", sx * w2 - t / 2 - sx * t / 2, sx * w2 + t / 2 - sx * t / 2, 0, EC_D, 0, EC_H, M["tolex"],
            bevel=3)
    box("ec_latch", -15, 15, -4, 1, EC_FRONT - 32, EC_FRONT - 4, M["chrome"], bevel=1.5)
    box("ec_latch_hasp", -6, 6, -6, -3, EC_FRONT - 26, EC_FRONT - 12, M["chrome"], bevel=1)

    # the maroon panel, built flat and then sloped up from the front wall's top edge
    n0 = len(PARTS)
    pw2 = w2 - t
    box("ec_panel", -pw2, pw2, 0, EC_PANEL_D, -2.5, 0, M["maroon"], bevel=0.6)
    def at(f):
        return -pw2 + f * 2 * pw2
    py = 30.0          # the row of controls, from the panel's front edge
    box("ec_bat_base", at(0.05) - 6, at(0.05) + 6, py - 9, py + 9, 0, 2, M["chrome"], bevel=1)
    bat = cyl("ec_bat", 1.7, 15, (at(0.05), py - 3, 8), M["chrome"], segs=12)
    bat.rotation_euler = (math.radians(-20), 0, 0)
    for f in (0.19, 0.33):   # sustain and echo volume: black skirted pointer knobs
        cyl("ec_knob_skirt", 12, 4, (at(f), py, 2), M["knob"], segs=32, bevel=1)
        cyl("ec_knob", 8.5, 13, (at(f), py, 8.5), M["knob"], segs=32, bevel=1.2, r2=7.5)
        box("ec_pointer", at(f) - 0.8, at(f) + 0.8, py - 12, py - 4, 4, 4.3, M["print"])
    cyl("ec_trim", 3.5, 2, (at(0.26), py + 4, 1), M["void"], segs=16)
    for f in (0.48, 0.59, 0.70, 0.93):
        jack("ec_jack", at(f), py + 2, 0)
    cyl("ec_rec_bezel", 9, 3, (at(0.815), py + 2, 1.5), M["chrome"], segs=32, bevel=1)
    _track(C.sphere("ec_rec_lamp", 7, (at(0.815), py + 2, 3), M["led"]))
    box("ec_scale_rule", at(0.42), at(0.9), 66, 66.6, 0, 0.2, M["print"])
    for k in range(36):   # the 0-35 echo-delay scale along the panel's back edge
        x = at(0.42) + k * (at(0.9) - at(0.42)) / 35
        box("ec_tick", x - 0.35, x + 0.35, 66.6, 66.6 + (6 if k % 5 == 0 else 3), 0, 0.2, M["print"])
    for f in (0.05, 0.19, 0.33):   # small white legends under the controls (unlettered bars)
        box("ec_legend", at(f) - 9, at(f) + 9, py - 22, py - 19, 0, 0.2, M["print"])
    box("ec_footsw_rule", at(0.62), at(0.97), 52, 52.5, 0, 0.2, M["print"])
    for ob in PARTS[n0:]:   # built flat at z = 0; lift onto the front wall, then slope it
        ob.location.z += EC_FRONT
    _pivot("ec_panel_tilt", n0, (0, 0, EC_FRONT), (EC_SLOPE, 0, 0))
    pz_back = EC_FRONT + EC_PANEL_D * math.sin(math.radians(EC_SLOPE))
    py_back = EC_PANEL_D * math.cos(math.radians(EC_SLOPE))

    # the cartridge housing behind the panel, its glossy cover standing proud of the walls
    cx0, cx1 = -w2 + t + 0.26 * (EC_W - 2 * t), w2 - t - 4
    cy0, cy1 = py_back + 4, EC_D - t - 6
    ctop = EC_H + 24
    box("ec_housing", cx0, cx1, cy0, cy1, 0, ctop - 4, M["void"], bevel=1)
    box("ec_cover", cx0, cx1, cy0, cy1, ctop - 4, ctop, M["gloss_black"], bevel=1.5)
    ccx, ccy = (cx0 + cx1) / 2, (cy0 + cy1) / 2 + 12
    for r in (26, 40, 54):
        ring = _track(C.torus("ec_ring", r, 0.5, (ccx, ccy, ctop + 0.1), M["print"], major=96, minor=6))
        ring.scale = (1.5, 0.8, 0.25)
    sx0, sx1 = cx0 + 40, cx1 - 30
    box("ec_slot", sx0, sx1, cy0 + 12, cy0 + 18, ctop, ctop + 0.3, M["void"])
    cyl("ec_slider", 3.2, 16, (sx0 + 0.55 * (sx1 - sx0), cy0 + 15, ctop + 8), M["chrome"], segs=16, bevel=0.8)
    for x, y in ((cx0 + 14, cy0 + 12), (cx1 - 14, cy1 - 14)):
        cyl("ec_post", 5, 10, (x, y, ctop + 5), M["chrome"], segs=20, bevel=1)
    # the cable well at the left: a coiled grey lead on the dark floor
    box("ec_well_floor", -w2 + t, cx0, cy0, EC_D - t, 90, 92, M["void"])
    for k in range(4):
        _track(C.torus("ec_coil", 30 - k * 2, 2.3, ((-w2 + t + cx0) / 2 + k * 2, (cy0 + EC_D) / 2, 96 + k * 4.5),
                       M["cable_grey"], major=40, minor=8))

    # the lid, open behind: a shallow tolex tray hinged at the back top edge, tipped back past
    # upright, the cream instruction sheet with its printed diagram inside
    n1 = len(PARTS)
    LD = 52.0   # the lid's depth
    box("ec_lid_top", -w2, w2, 0, EC_D, LD - t, LD, M["tolex"], bevel=3)
    box("ec_lid_rim_hinge", -w2, w2, EC_D - t, EC_D, 0, LD, M["tolex"], bevel=3)
    box("ec_lid_rim_front", -w2, w2, 0, t, 0, LD, M["tolex"], bevel=3)
    for sx in (-1, 1):
        box("ec_lid_rim_s", sx * w2 - t / 2 - sx * t / 2, sx * w2 + t / 2 - sx * t / 2, 0, EC_D, 0, LD, M["tolex"],
            bevel=3)
    sz = LD - t - 0.4
    box("ec_sheet", -w2 + t + 6, w2 - t - 6, t + 6, EC_D - t - 6, sz - 0.4, sz, M["sheet"])
    # the printed diagram: a window for the delay scale, the controls' boxes, the leader lines
    zl = sz - 0.6
    def ink(x0, x1, y0, y1):
        box("ec_sheet_ink", x0, x1, y0, y1, zl, zl + 0.2, M["ink"])
    ink(-40, 110, EC_D - 52, EC_D - 50); ink(-40, 110, EC_D - 30, EC_D - 28)
    ink(-40, -38, EC_D - 52, EC_D - 28); ink(108, 110, EC_D - 52, EC_D - 28)
    ink(-150, 150, EC_D - 75, EC_D - 74)
    for x in (-130, -95, -55, -15, 25, 65, 105, 140):
        ink(x - 12, x + 12, EC_D - 112, EC_D - 111); ink(x - 12, x + 12, EC_D - 88, EC_D - 87)
        ink(x - 12, x - 11, EC_D - 112, EC_D - 87); ink(x + 11, x + 12, EC_D - 112, EC_D - 87)
        ink(x - 0.6, x + 0.6, EC_D - 150 + (x % 3) * 8, EC_D - 112)
    for k in range(5):
        ink(-150, -60, 40 + k * 9, 41.2 + k * 9)
    for x in (-w2 + 40, w2 - 40):
        box("ec_hinge", x - 14, x + 14, EC_D - 3, EC_D + 3, 0, 18, M["chrome"], bevel=1)
    # built as if lying closed on the box (inside facing down), then swung open about the back edge
    for ob in PARTS[n1:]:
        ob.location.z += EC_H
    _pivot("ec_lid_open", n1, (0, EC_D, EC_H), (-100, 0, 0))
    return group("tape_echo")


# ------------------------------------------------------------------ the Les Paul Custom

# Fripp's 1959 Les Paul Custom (Guitar Player; Premier Guitar's 1959 spec): black, multi-ply cream
# binding, ebony board with pearl blocks, three gold humbuckers, ABR-1 bridge and stop tailpiece,
# four gold bell knobs, the switch on the upper bass horn, a bound open-book headstock with the
# split-diamond inlay (no lettering). Body about 440 x 330 x 55 mm, scale 628 mm.

# The single-cut outline in mm (neck along +y, the bridge near y = 0, the cutaway at +x): 435 long,
# 330 across the lower bout, a waist about 215 across, the rounded horn reaching up beside the neck
# (round 4, redone 31 Oct 2026: the circle-built body read as a blob).
_LP_OUTLINE = [
    (0, -236), (-62, -231), (-112, -212), (-148, -178), (-164, -128), (-163, -78), (-148, -32),
    (-122, 8), (-107, 42), (-108, 78), (-118, 118), (-119, 152), (-108, 182), (-84, 202),
    (-50, 210), (-22, 204),                                  # the bass upper bout to the neck
    (22, 196), (28, 178), (40, 165), (53, 166), (62, 180), (66, 198),   # the cutaway
    (78, 206), (96, 196), (111, 170), (118, 132), (115, 92), (106, 54),   # the horn and treble bout
    (122, 10), (148, -32), (163, -78), (164, -128), (148, -178), (112, -212), (62, -231),
]


def _body_outline(sub=6):
    """_LP_OUTLINE smoothed (closed Catmull-Rom), sub points per span."""
    P = [Vector(p) for p in _LP_OUTLINE]
    n = len(P)
    pts = []
    for i in range(n):
        p0, p1, p2, p3 = P[i - 1], P[i], P[(i + 1) % n], P[(i + 2) % n]
        for k in range(sub):
            t = k / sub
            pts.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    return pts


def _prism(name, pts, z0, z1, mat):
    bm = bmesh.new()
    vs = [bm.verts.new((x, y, z0)) for x, y in pts]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    bmesh.ops.translate(bm, vec=Vector((0, 0, z1 - z0)), verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _track(C.mesh_obj(name, bm, mat))


def _ribbon(name, pts, z0, z1, mat, out=0.8):
    """A thin band round an outline (binding), pushed out along each point's outward normal."""
    bm = bmesh.new()
    n = len(pts)
    area = sum(pts[i].x * pts[(i + 1) % n].y - pts[(i + 1) % n].x * pts[i].y for i in range(n))
    sgn = 1.0 if area > 0 else -1.0   # counter-clockwise: the outward normal is (dy, -dx)
    ring = []
    for i, p in enumerate(pts):
        d = (pts[(i + 1) % n] - pts[i - 1]).normalized()
        q = p + Vector((d.y, -d.x)) * sgn * out
        ring.append((bm.verts.new((q.x, q.y, z0)), bm.verts.new((q.x, q.y, z1))))
    for i in range(len(ring)):
        a, b = ring[i], ring[(i + 1) % len(ring)]
        bm.faces.new((a[0], b[0], b[1], a[1]))
    return _track(C.mesh_obj(name, bm, mat))


def guitar():
    """Lying face up (+z), neck along +y, the bridge near y = 0."""
    out = _body_outline()
    body = _prism("gt_body", [(p.x, p.y) for p in out], 0, 52, M["black_lacquer"])
    C.add_bevel(body, 3, 3)
    _ribbon("gt_binding_top", out, 45, 52.4, M["binding"], out=1.2)
    _ribbon("gt_binding_back", out, 2, 6, M["binding"], out=0.5)
    # the elevated pickguard on the treble side
    # (beside the pickup rings, under the strings, narrowing toward the cutaway; white ply edge)
    pg = [(50, 8), (80, 0), (100, 28), (105, 80), (96, 128), (76, 156), (56, 158), (49, 120)]
    _prism("gt_guard", pg, 57, 58.6, M["guard"])
    _ribbon("gt_guard_edge", [Vector(p) for p in pg], 57.1, 58.4, M["binding"], out=0.4)
    # neck: mahogany under an ebony board with cream binding and pearl blocks; gold frets
    box("gt_neck", -22, 22, 160, 640, 30, 52, M["black_lacquer"], bevel=4)
    box("gt_board", -21, 21, 160, 630, 52, 58, M["board"], bevel=0.8)
    for sx in (-1, 1):
        box("gt_board_bind", sx * 21 - 0.8, sx * 21 + 0.8, 160, 630, 51, 58, M["binding"])
    scale = 628.0
    fy = [630 - (scale - scale / 2 ** (k / 12)) for k in range(0, 23)]
    for k in range(1, 23):
        if fy[k] < 165:
            break
        box("gt_fret", -21, 21, fy[k] - 0.6, fy[k] + 0.6, 58, 59.2, M["fret"], bevel=0.3)
    for k in (1, 3, 5, 7, 9, 12, 15, 17, 19, 21):
        y0, y1 = fy[k], fy[k - 1]
        if y0 > 168:
            box("gt_block", -15, 15, y0 + (y1 - y0) * 0.22, y1 - (y1 - y0) * 0.22, 58, 58.25, M["pearl"])
    box("gt_nut", -22, 22, 630, 635, 52, 60, M["binding"], bevel=0.6)
    # the bound open-book headstock with the split-diamond inlay (no lettering)
    n_before = len(PARTS)
    hp = [(-22, 635), (-40, 690), (-47, 785), (-30, 820), (0, 808), (30, 820), (47, 785), (40, 690), (22, 635)]
    head = _prism("gt_head", hp, 40, 55, M["black_lacquer"])
    C.add_bevel(head, 2, 2)
    _ribbon("gt_head_bind", [Vector(p) for p in hp], 51, 55, M["binding"], out=0.5)
    for sx in (-1, 1):
        _prism("gt_diamond", [(sx * 2, 735), (sx * 13, 752), (sx * 2, 772)], 55, 55.3, M["pearl"])
    posts = []
    for k in range(3):
        y = 690 + k * 40
        for sx in (-1, 1):
            x = sx * (33 + k * 2)
            cyl("gt_post", 3.2, 9, (x, y, 59), M["gold"], segs=16)
            sh = cyl("gt_tuner_shaft", 2, 22, (x + sx * 12, y, 46), M["gold"], axis="Y", segs=12)
            sh.rotation_euler = (0, math.pi / 2, 0)
            tb = cyl("gt_tuner_btn", 7, 5, (x + sx * 25, y, 46), M["gold"], segs=16, bevel=1.5)
            tb.rotation_euler = (0, math.pi / 2, 0)
            posts.append((x, y))
    # the headstock is pitched back 17 degrees at the nut, as on the real neck
    pivot = Vector((0, 635, 55))
    pitch = bpy.data.objects.new("gt_head_pitch", None)
    C.link(pitch)
    pitch.location = pivot
    for ob in PARTS[n_before:]:
        ob.location -= pivot
        ob.parent = pitch
    del PARTS[n_before:]
    _track(pitch)
    pitch.rotation_euler = (math.radians(-17), 0, 0)
    rot = Matrix.Rotation(math.radians(-17), 4, "X")
    posts = [tuple((rot @ (Vector((x, y, 62)) - pivot)) + pivot) for (x, y) in posts]
    # three gold-covered humbuckers in cream rings, the ABR-1 bridge, the stop tailpiece
    for y in (28.0, 88.0, 150.0):
        box("gt_pu_ring", -45, 45, y - 23, y + 23, 52, 56, M["binding"], bevel=2)
        box("gt_pu", -36, 36, y - 17, y + 17, 56, 61, M["gold"], bevel=2)
    box("gt_bridge", -38, 38, -6, 4, 52, 63, M["gold"], bevel=1.5)
    box("gt_tail", -42, 42, -52, -40, 52, 61, M["gold"], bevel=4)
    for (x, y) in ((72, -48), (108, -18), (72, -108), (112, -80)):   # four gold bell knobs, treble side
        cyl("gt_knob", 10, 15, (x, y, 59.5), M["gold"], segs=32, bevel=2, r2=8)
    cyl("gt_switch", 2.6, 22, (-92, 170, 62), M["gold"], segs=16)
    _track(C.sphere("gt_switch_tip", 4, (-92, 170, 73), M["binding"]))
    # strings: tailpiece over the bridge to the nut, then each to its post
    # low E (left, thickest) to the bass-side post nearest the nut, A and D above it; G, B and
    # high e back down the treble side, so D and G run to the top posts and nothing crosses
    # (posts: [L near, R near, L mid, R mid, L far, R far])
    order = [posts[0], posts[2], posts[4], posts[5], posts[3], posts[1]]
    for k in range(6):
        x0 = -22 + k * 8.8
        x1 = -18 + k * 7.2
        r = 0.35 + 0.12 * (5 - k)
        _track(C.curve_obj("gt_string", [(x0, -46, 61), (x0, 0, 63.5), (x1, 633, 60.5), order[k]], r,
                           M["string"]))
    return group("guitar")


# ------------------------------------------------------------------ the SH-2000

# 865 x 266 x 133 mm (soundprogramming.net; the Met gives the same), from Perfect Circuit's photos
# on vintagesynth.com (owner, 31 Oct 2026: "the synthesizer that's in the reference photo of the
# tape echo"; the Met shows Steve Miller's SH-2000 beside his EP-3). Wood end cheeks, a tall black
# rear rail with a chrome seam, the silver panel in the left third, 37 keys (F to F), and the 34
# coloured organ tabs hanging off the front edge. x fractions run from the left cheek's outer face.
SH_W, SH_D, SH_H = 865.0, 266.0, 133.0


def _shx(f):
    return -SH_W / 2 + f * SH_W


def _prism_yz(name, pts, x0, x1, mat):
    """An outline in the y-z plane extruded across x."""
    bm = bmesh.new()
    f = bm.faces.new([bm.verts.new((x0, y, z)) for y, z in pts])
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    bmesh.ops.translate(bm, vec=Vector((x1 - x0, 0, 0)),
                        verts=[e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _track(C.mesh_obj(name, bm, mat))


def sh2000():
    w2 = SH_W / 2
    ck = 22.0
    cheek = [(0, 0), (0, 92), (12, 98), (SH_D, SH_H), (SH_D, 0)]
    for sx in (-1, 1):
        ob = _prism_yz("sh_cheek", cheek, sx * w2 - (ck if sx > 0 else 0), sx * w2 + (ck if sx < 0 else 0),
                       M["rosewood"])
        C.add_bevel(ob, 2.5, 2)
    xi0, xi1 = -w2 + ck, w2 - ck
    box("sh_base", xi0, xi1, 4, SH_D - 4, 12, 50, M["void"])
    # the front lip and its tabs
    box("sh_lip", xi0, xi1, 2, 26, 14, 62, M["lidblack"], bevel=1.5)
    groups = [(0.066, 0.134, "GYA"), (0.153, 0.177, "R"), (0.197, 0.396, "AAAAACCCC"),
              (0.418, 0.574, "GGGRRCC"), (0.596, 0.709, "RRRCC"), (0.730, 0.932, "YCCGGGAAC")]
    cols = dict(G=M["tab_green"], Y=M["tab_grey"], A=M["tab_amber"], R=M["tab_red"], C=M["tab_cream"])
    for f0, f1, cs in groups:
        n = len(cs)
        pitch = (_shx(f1) - _shx(f0)) / n
        for k, c in enumerate(cs):
            x = _shx(f0) + (k + 0.5) * pitch
            tb = box("sh_tab", x - 8.5, x + 8.5, -14, 6, 20, 44, cols[c], bevel=2.5)
            tb.rotation_euler = (math.radians(14), 0, 0)
        box("sh_tab_rule", _shx(f0), _shx(f1), 1.8, 2, 52, 53, M["print"])
    # the keyboard: 22 cream naturals F..F, 15 black sharps
    kx0, kx1 = _shx(0.352), _shx(0.935)
    box("sh_keybed", kx0 - 4, kx1 + 4, 26, 180, 40, 58, M["void"])
    kw = (kx1 - kx0) / 22
    for k in range(22):
        x = kx0 + k * kw
        box("sh_white", x + 0.6, x + kw - 0.6, 28, 168, 58, 76, M["sh_key_white"], bevel=1.2)
    names = "FGABCDE"
    for k in range(21):
        if names[k % 7] in "FGACD":
            x = kx0 + (k + 1) * kw
            box("sh_black", x - 6.5, x + 6.5, 82, 168, 76, 89, M["gloss_black"], bevel=1.2)
    box("sh_key_end", kx1 + 4, xi1, 26, 180, 40, 74, M["lidblack"], bevel=1.5)
    # the panel: a black surround, the brushed silver plate, two rows of controls
    px0, px1 = _shx(0.066), _shx(0.35)
    box("sh_surround", px0, px1, 26, 182, 40, 72, M["lidblack"], bevel=1.5)
    box("sh_plate", _shx(0.075), _shx(0.314), 34, 150, 72, 72.6, M["panel"], bevel=0.4)
    box("sh_header", _shx(0.075), _shx(0.314), 152, 176, 72, 72.6, M["lidblack"])
    _track(C.sphere("sh_pilot", 4, (_shx(0.09), 164, 73), M["led"]))
    zt, yu, yl = 72.6, 122.0, 70.0
    box("sh_touch_frame", _shx(0.093), _shx(0.268), yu - 20, yu + 20, zt, zt + 0.3, M["ink"])
    box("sh_touch_inner", _shx(0.095), _shx(0.266), yu - 18, yu + 18, zt, zt + 0.35, M["panel"])
    box("sh_power", _shx(0.081) - 5, _shx(0.081) + 5, yu - 12, yu + 12, zt, zt + 7, M["void"], bevel=1.5)
    for f in (0.119, 0.288):
        cyl("sh_knob_skirt", 11, 4, (_shx(f), yu, zt + 2), M["chrome"], segs=32, bevel=1)
        cyl("sh_knob", 8, 12, (_shx(f), yu, zt + 9), M["knob"], segs=32, bevel=1.2, r2=7)
    for f in (0.154, 0.178, 0.202, 0.225, 0.252):
        box("sh_window", _shx(f) - 8, _shx(f) + 8, yu - 9, yu + 9, zt, zt + 0.4, M["void"])
        bat = cyl("sh_bat", 1.6, 12, (_shx(f), yu, zt + 6), M["chrome"], segs=12)
        bat.rotation_euler = (math.radians(-15), 0, 0)
    for i, f in enumerate((0.085, 0.099, 0.128, 0.150, 0.183, 0.243)):
        box("sh_slot", _shx(f) - 1.5, _shx(f) + 1.5, yl - 26, yl + 26, zt, zt + 0.4, M["void"])
        for k in range(6):
            box("sh_tick", _shx(f) - 9, _shx(f) - 4, yl - 25 + k * 10, yl - 24 + k * 10, zt, zt + 0.3, M["ink"])
        cyl("sh_cap", 6, 12, (_shx(f), yl + (12, -4, 8, 0, -10, 6)[i], zt + 6), M["knob"], segs=24, bevel=1)
    for f, y in ((0.208, yl), (0.284, yl + 18)):
        cyl("sh_toggle", 1.5, 10, (_shx(f), y, zt + 5), M["chrome"], segs=12)
    box("sh_transpose_slot", _shx(0.268), _shx(0.296), yl - 22, yl - 18, zt, zt + 0.4, M["void"])
    lv = cyl("sh_transpose", 2, 16, (_shx(0.281), yl - 20, zt + 6), M["chrome"], segs=12)
    lv.rotation_euler = (0, math.radians(25), 0)
    # the rear rail: textured black, higher at the back, a chrome seam along its front edge
    _prism_yz("sh_rail", [(178, 14), (178, 98), (SH_D - 2, 128), (SH_D - 2, 14)], xi0, xi1, M["tolex"])
    seam = cyl("sh_seam", 1.6, xi1 - xi0, (0, 179, 98), M["chrome"], segs=12)
    seam.rotation_euler = (0, math.pi / 2, 0)
    for f in (0.32, 0.67):
        jack("sh_jack", _shx(f), 225, 112)
    return group("sh2000")


# ------------------------------------------------------------------ placement

def place(e, loc, rot_deg=(0, 0, 0), mirror=False):
    """mirror: flip left for right (the guitar is modelled as seen from behind, so a
    right-handed one faces the camera mirrored)."""
    e.location = Vector(loc)
    if mirror:
        e.scale = (-1, 1, 1)
    e.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    return e
