"""Clay pass for composition review: renders every variant of every site slot
and writes a contact sheet the owner picks compositions from.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \\
        --python site/renders/render_clay.py -- [--only hero,front_a] [--scale 50] \\
        [--samples 48] [--out DIR] [--index-only]

Default output: renders/site_clay/ in the MAIN checkout (gitignored, served at
http://localhost:8765/site_clay/): one JPEG per variant (1920 px wide; the
front-on variants at the site's exact 2116 x 1190 so the annotated panel's
geometry can be measured), index.html (labelled grid grouped by slot, click
to enlarge) and variants.json (the cameras, for the final pass to reuse).

Ported from Resilio Versio's render_clay.py (the owner's MIT code): same
scene / camera / contact-sheet machinery; new variants, the freeze-blue LEDs,
the story subjects (story.py) and the front-on camera solver + measurement.

Clay = grey matte materials (bare metal on hardware), real studio lighting
(common.studio), the panel print on as a decal, the four LEDs lit in the
freeze blue.

Camera labels: az = azimuth from the front towards the right, el = elevation
above the horizon, h = camera height above the ground, then the focal length
(full-frame equivalent) and the f-stop when depth of field is on.
"""
import argparse
import json
import math
import os
import sys
import time

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import module  # noqa: E402
import props  # noqa: E402
import story  # noqa: E402
import studio  # noqa: E402

MAIN_CHECKOUT = "/Users/jesse/Documents/Sites/immensio-versio"
DEFAULT_OUT = os.path.join(MAIN_CHECKOUT, "renders", "site_clay")
OUT_R2 = os.path.join(MAIN_CHECKOUT, "renders", "site_clay_r2")
# round 2 (owner, 30 Oct 2026): the picks from round 1, shown on the r2 sheet for reference
R1_PICKS = ["front_b", "patched_a", "og_a"]
OUT_R3 = os.path.join(MAIN_CHECKOUT, "renders", "site_clay_r3")
# round 3 (owner, 30 Oct 2026): the round-2 frames the notes build on, shown on the r3 sheet for reference
R2_REFS = ["story_rack2_a", "story_tape2_a"]

# The site's annotated panel (site/src/components/AnnotatedPanel.astro) draws
# its callouts on a 2116 x 1190 stage with the panel at this box: the
# front-on render must put the 50.5 x 128.5 mm panel exactly there.
FRONT_FRAME = (2116, 1190)
FRONT_BOX = dict(x=872, y=121, w=372, h=950)

W169 = (1920, 1080)
W219 = (1920, 823)        # 21:9 (final 2400 x 1028)
OG = (1200, 630)          # share image


# ------------------------------------------------------------------ variants
# Each variant: id, slot, scene (builder + mood), note, a camera recipe and a
# resolution. Targets are reference points from the scene builders.

def module_variants(ref):
    c = ref["center"]
    at = ref["at"]
    ctrl = (at("SW1") + at("SW2") + at("BTN") + at("P6") + at("P7")) / 5
    return [
        # ---- 1. hero still: the monolith, dim, faintly glowing
        dict(id="hero_a", slot="hero", mood="monolith", res=W169,
             note="Monolith, dead centre: camera on the floor, 24 mm looking up; the top edge catches a cool light, "
                  "the four blue LEDs are the brightest thing in frame (Resilio's hero, re-lit).",
             loc=(0, -170, 6), target=(0, 0, 66), f=24, fstop=16, focus=(0, 0, 64)),
        dict(id="hero_b", slot="hero", mood="monolith", res=W169,
             note="Monolith, off-axis: 24 mm, closer and turned 25 deg, stronger convergence.",
             loc=(60, -140, 5), target=(0, 6, 62), f=24, fstop=16, focus=(0, 0, 64)),
        dict(id="hero_c", slot="hero", mood="monolith", res=W169,
             note="Monolith, 35 mm from further back: calmer verticals, more dark floor.",
             loc=(-20, -230, 8), target=(0, 0, 60), f=35, fstop=16, focus=(0, 0, 64)),
        dict(id="hero_d", slot="hero", mood="night", res=W169,
             note="Half-light: darker still, a cold moonlit top light, the LEDs' blue spilling onto the knobs; "
                  "module on the right third, dark space left for the headline.",
             loc=(0, -200, 7), target=(0, 0, 62), f=28, fstop=16, focus=(0, 0, 64), shift=(-0.17, 0.0)),
        # ---- 5. og-hero (1200 x 630): the hero, composed for link previews
        dict(id="og_a", slot="og", mood="monolith", res=OG,
             note="Share image: hero_a pulled back a little so the whole panel fits the wide 1.91:1 frame.",
             loc=(0, -215, 7), target=(0, 0, 63), f=24, fstop=16, focus=(0, 0, 64)),
        dict(id="og_b", slot="og", mood="night", res=OG,
             note="Share image: the half-light version, module right of centre, room on the left for the name.",
             loc=(0, -235, 8), target=(0, 0, 62), f=28, fstop=16, focus=(0, 0, 64), shift=(-0.2, 0.0)),
        # ---- 2. front-on (exact geometry for the annotated panel)
        dict(id="front_a", slot="front", mood="studio", res=FRONT_FRAME, front=True,
             note="Straight-on, 200 mm, camera a touch high (1 deg) so the knob tops read; warm-grey sweep "
                  "(Resilio's front-on camera, solved to the annotated panel's box)."),
        dict(id="front_b", slot="front", mood="studio_cool", res=FRONT_FRAME, front=True,
             note="Same camera, a cooler neutral grey sweep and a slightly lower-key light."),
        dict(id="front_c", slot="front", mood="gloss", res=FRONT_FRAME, front=True,
             note="Same camera, on a semi-gloss floor: a soft reflection and contact shadow."),
        # ---- 3. panel macro (21:9)
        dict(id="macro_a", slot="macro", mood="studio", res=W219,
             note="Across the controls from the right: SPACE / ONSET switches, the FREEZE button and PAINT, "
                  "MOTION and DEPTH; 100 mm f/11, focus on the button.",
             target=ctrl + C.Vector((0, -6, 0)), az=30, el=16, f=100, dist=175, fstop=11, focus=at("BTN", -2)),
        dict(id="macro_b", slot="macro", mood="studio", res=W219,
             note="Low from the left, shallower (f/8): the switches and their labels sharp, FREEZE and DEPTH softening.",
             target=ctrl + C.Vector((-2, -6, 2)), az=-34, el=10, f=100, dist=160, fstop=8, focus=at("SW2", -3)),
        dict(id="macro_c", slot="macro", mood="studio", res=W219,
             note="Higher and raking from the right, 100 mm f/8: MOTION's cap and the button, the print raking.",
             target=ctrl + C.Vector((2, -6, 4)), az=48, el=30, f=100, dist=190, fstop=8, focus=at("P6", -6)),
        # ---- 4. patched: all four audio jacks, black (L) and freeze-blue (R)
        dict(id="patched_a", slot="patched", mood="cables4", res=W169,
             note="IN L/R and OUT L/R patched, black left and blue right, cords leaving both sides "
                  "(Resilio's patched camera): three-quarter from the right, 60 mm f/16.",
             target=c + C.Vector((0, -30, -6)), az=20, el=16, f=60, fit=0.78, fstop=16, focus=c + C.Vector((0, -8, 0))),
        dict(id="patched_b", slot="patched", mood="cables4", res=W169,
             note="Low from the front-left, 85 mm: the plugs stacked in a row, the cords sweeping out of frame.",
             target=c + C.Vector((0, -30, -18)), az=-18, el=7, f=85, fit=0.92, fstop=16, focus=at("J10", -12)),
        dict(id="patched_c", slot="patched", mood="cables4", res=W169,
             note="High three-quarter, 50 mm: looking down on the cords fanning out across the floor.",
             target=c + C.Vector((0, -80, -30)), az=28, el=38, f=50, fit=0.6, fstop=16, focus=c + C.Vector((0, -20, -10))),
        # ---- round 2 (owner's notes, 30 Oct 2026)
        dict(id="hero_e", slot="hero", mood="night", res=W169, rnd=2,
             note="hero_d's half-light, centred and square like Resilio's hero still (and the film's last frame): "
                  "level with the panel's centre, 100 mm, the panel 72 % of the frame height.",
             loc=(0, -HERO_DIST, module.PH / 2), target=(0, 0, module.PH / 2), f=100, fstop=16,
             focus=(0, 0, module.PH / 2)),
        dict(id="hero_f", slot="hero", mood="night", res=W169, rnd=2,
             note="The half-light, centred, from a little below: 40 mm, the camera low, looking up, so the "
                  "module towers a touch while staying square.",
             loc=(0, -330, 22), target=(0, 0, 70), f=40, fstop=11, focus=(0, 0, 64)),
        dict(id="macro_d", slot="macro", mood="studio", res=W219, rnd=2,
             note="macro_a's camera brought down to 6 deg (at macro_a's 16 deg the WEAR cap hides PLT), focus on "
                  "the SPACE switch's labels (PLT / HAL / MAS); f/11.",
             target=ctrl + C.Vector((0, -6, 0)), az=30, el=6, f=100, dist=175, fstop=11, focus=SPACE_LABELS),
        dict(id="macro_e", slot="macro", mood="studio", res=W219, rnd=2,
             note="Same, shallower (f/4): only the SPACE labels and the switch sharp, the rest melting away.",
             target=ctrl + C.Vector((0, -6, 0)), az=30, el=6, f=100, dist=175, fstop=4, focus=SPACE_LABELS),
    ]


# the hero still's camera: Resilio's (render_hero.py), square to the panel
HERO_DIST = (128.5 / 0.72) * 100.0 / (36.0 / (16 / 9))
# the SPACE switch's labels (PLT / HAL / MAS) on the panel face, from the art
SPACE_LABELS = C.Vector((13.2 - 25.25, 0.0, 128.5 - 59.0))


def story_variants(refs):
    t, r, p = refs.get("tape"), refs.get("rack"), refs.get("plate")
    t2, r2 = refs.get("tape2"), refs.get("rack2")
    t3, r3 = refs.get("tape3"), refs.get("rack3")
    v = []
    if r3:
        v += [
            dict(id="story_rack3_a", slot="story", mood="rack3", res=W169, rnd=3,
                 note="(b) rack2_a's room and camera, now with the two tape decks on the desk (the round-3 deck: "
                      "A77 Mk III/IV layout, unbranded), each with both reels loaded and running its own tape, "
                      "cross-patched (the phono leads hang in the gap between them); 35 mm from the room's corner.",
                 target=r3["center"] + C.Vector((100, 0, 0)), az=-18, el=14, f=35, dist=4300),
        ]
    if t3:
        v += [
            dict(id="story_tape3_a", slot="story", mood="tape3", res=W169, rnd=3,
                 note="(a) tape2_a pulled back: the same three-quarter view and 40 mm lens, framed so neither deck "
                      "touches an edge. The round-3 deck: brushed stainless transport plate, spun-aluminium 7-inch "
                      "reels, head block with cover and smoked shield, capstan and pinch roller, tension levers, "
                      "lit VU meters; cross-patched, leads hanging in the gap.",
                 target=t3["center"] + C.Vector((-90, 0, 10)), az=-22, el=16, f=40, fit=0.70, by="width"),
            dict(id="story_tape3_b", slot="story", mood="tape3", res=W169, rnd=3,
                 note="(a) Front-on: square to both decks' faces at seated eye height, 50 mm; the cross-patch "
                      "leads hang in the gap between the cases.",
                 target=t3["decks_center"] + C.Vector((0, 0, 0)), az=0, el=4, f=50, fit=0.76, by="width",
                 fstop=4.0, focus=t3["decks_center"]),  # a photographer's depth of field (owner, 31 Oct 2026)
            dict(id="story_tape3_c", slot="story", mood="tape3", res=W169, rnd=3,
                 note="(a) Optional detail: close on the left deck's head block, capstan and pinch roller, the "
                      "meters soft beyond; 85 mm f/5.6.",
                 target=t3["heads_a"] + C.Vector((40, 0, -20)), az=-24, el=12, f=85, dist=900, fstop=5.6,
                 focus=t3["heads_a"]),
        ]
    if t2:
        v += [
            dict(id="story_tape2_a", slot="story", mood="tape2", res=W169, rnd=2,
                 note="(a) Two 1970s Swiss-style decks (the Revox A77 layout, unbranded), each running its own tape, "
                      "on a desk in a lamp-lit room; three-quarter from the front, 40 mm. Their outs and ins are "
                      "cross-patched at the back (the cables loop across the gap).",
                 target=t2["center"] + C.Vector((0, 0, 10)), az=-22, el=16, f=40, fit=0.95, by="width"),
            dict(id="story_tape2_b", slot="story", mood="tape2", res=W169, rnd=2,
                 note="(a) From behind and above: the cross-patch itself, A's outputs into B's inputs (black) and "
                      "B's outputs back into A's (grey), the reels standing up beyond; 50 mm.",
                 loc=(420, 690, 1520), target=t2["patch"] + C.Vector((0, -90, 60)), f=24, fstop=8,
                 focus=t2["patch"]),
        ]
    if r2:
        v += [
            dict(id="story_rack2_a", slot="story", mood="rack2", res=W169, rnd=2,
                 note="(b) A cosy late-70s/80s home studio: oak desk, lamp, small mixer, the reverb's remote on the "
                      "desk, a walnut rack of period units (reverb mainframe, delay, pitch shifter, EQ, patch bay), "
                      "records and tape boxes, a plant; 35 mm from the room's corner.",
                 target=r2["center"] + C.Vector((100, 0, 0)), az=-18, el=14, f=35, dist=4300),
            dict(id="story_rack2_a2", slot="story", mood="rack2", res=W169, rnd=2,
                 note="(b) The same room, nearer the rack: the units at eye level, the desk and lamp to the left; 50 mm.",
                 target=r2["rack"] + C.Vector((-450, 0, 200)), az=-32, el=8, f=50, dist=2700, fstop=8,
                 focus=r2["reverb"]),
            dict(id="story_rack2_b", slot="story", mood="rack2", res=W169, rnd=2,
                 note="(b) Close on the reverb mainframe: the red display reading 99.9, program buttons and their "
                      "LEDs, headroom ladders, level pots, rack ears and screws, rubbed edges; 100 mm f/5.6.",
                 target=r2["reverb"] + C.Vector((-40, 0, 0)), az=-24, el=8, f=100, dist=900, fstop=5.6,
                 focus=r2["reverb"] + C.Vector((-110, 0, 20))),
            dict(id="story_rack2_b2", slot="story", mood="rack2", res=W169, rnd=2,
                 note="(b) Close on the remote on the desk: the six sliders, the red display, the buttons; the rack "
                      "soft behind; 85 mm f/4.",
                 target=r2["remote"] + C.Vector((0, -40, -10)), az=-28, el=30, f=85, dist=800, fstop=4,
                 focus=r2["remote"] + C.Vector((0, -40, -10))),
        ]
    if t:
        v += [
            dict(id="story_tape_a", slot="story", mood="tape", res=W169,
                 note="(a) Two-machine tape loop: both decks on a table, three-quarter from the left, 50 mm. One tape "
                      "runs from the left deck's supply reel past its heads, across the gap, to the right deck's take-up.",
                 target=t["center"] + C.Vector((0, 0, -30)), az=-24, el=18, f=50, fit=1.0, by="width"),
            dict(id="story_tape_b", slot="story", mood="tape", res=W169,
                 note="(a) Close on the left deck's heads, the strand leaving them and crossing the gap to the "
                      "second machine, soft behind; a little from above so the tape's face catches the light (70 mm f/4).",
                 target=t["head_l"] + C.Vector((190, 0, 0)), az=-30, el=20, f=70, dist=900, fstop=4,
                 focus=t["head_l"] + C.Vector((80, -16, -10))),
        ]
    if r:
        v += [
            dict(id="story_rack_a", slot="story", mood="rack", res=W169,
                 note="(b) A 1980s digital reverb (1U: red segment display, square buttons, a big data wheel) in a "
                      "small rack among generic units; three-quarter from the right, seated eye height, 50 mm.",
                 target=r["center"] + C.Vector((0, 0, 20)), az=28, el=6, f=50, fit=0.92),
            dict(id="story_rack_b", slot="story", mood="rack", res=W169,
                 note="(b) Close on the reverb: the glowing display sharp, the wheel and the rack falling soft "
                      "(100 mm f/4).",
                 target=r["unit"] + C.Vector((-5, 0, 0)), az=-26, el=10, f=100, dist=560, fstop=4,
                 focus=r["display"]),
        ]
    if p:
        v += [
            dict(id="story_plate_a", slot="story", mood="plate", res=W169,
                 note="(c) A steel reverb plate hung on springs in an open wooden frame (EMT 140-like, unbranded): "
                      "driver, two pickups, damper panel behind; three-quarter, 35 mm.",
                 target=p["center"] + C.Vector((0, 0, 60)), az=28, el=12, f=35, fit=0.72, by="width"),
            dict(id="story_plate_b", slot="story", mood="plate", res=W169,
                 note="(c) Close on a top corner: the suspension spring, the clamp on the plate's edge, the frame; "
                      "85 mm f/4.",
                 target=p["corner"] + C.Vector((-40, 10, 60)), az=-28, el=10, f=85, dist=1300, fstop=5.6,
                 focus=p["corner"] + C.Vector((65, 10, 80))),
        ]
    return v


# ------------------------------------------------------------------ scenes

DARK_RGB = (0.035, 0.033, 0.031)
COOL_GREY = (0.38, 0.385, 0.39)   # neutral studio grey, a hair cool


def build_module(mood):
    """Module moods: studio (warm-grey sweep), studio_cool (neutral grey, lower
    key), gloss (semi-gloss floor), monolith (dark backdrop, strong top/back
    light, the face falls into shadow: Resilio's R7, re-lit cooler), night
    (darker, a cold moonlit top light, LED spill up), cables4 (studio + the
    four patch cables)."""
    C.reset_scene()
    dark = mood in ("monolith", "night")
    spill = {"monolith": 1.0, "night": 2.2}.get(mood, 0.25)
    ref = module.build(dict(led_spill=spill))
    ground = DARK_RGB if dark else (COOL_GREY if mood == "studio_cool" else C.GROUND_RGB)
    cyc = C.cyclorama(width=2400 if not dark else 6000, front=1200 if not dark else 3000, back=260,
                      height=1600, radius=380, rgb=ground)
    c = ref["center"]
    if mood == "gloss":
        p = cyc.data.materials[0].node_tree.nodes["Principled BSDF"]
        p.inputs["Roughness"].default_value = 0.22
    if mood in ("studio", "gloss", "cables4"):
        C.world(0.32)
        C.studio(c, ref["size"] * 1.6, key=1.0, fill=0.25, rim=0.6)
    elif mood == "studio_cool":
        C.world(0.26, rgb=(0.48, 0.50, 0.52))
        L = C.studio(c, ref["size"] * 1.6, key=0.85, fill=0.18, rim=0.7)
        L["key"].data.color = (0.98, 0.98, 1.0)
    elif mood in ("monolith", "night"):
        night = mood == "night"
        C.world(0.006 if night else 0.012, rgb=(0.40, 0.44, 0.52))
        L = C.studio(c, ref["size"] * 1.6, key=0.05 if night else 0.10, fill=0.0 if night else 0.02,
                     rim=1.1 if night else 1.6)
        L["rim"].data.color = (0.80, 0.86, 1.0)
        # the top/back light: just behind and above the top edge, raking forward;
        # cool, like a clouded night sky (Resilio's was neutral)
        top = C.Vector((0, 60, module.PH + 120))
        lt = C.area_light("top", top, C.Vector((0, 0, module.PH - 10)), size=120,
                          power=C.watts(C.KEY_W * (5.0 if night else 8.0)) * (0.15) ** 2,
                          rgb=(0.78, 0.85, 1.0) if night else (0.90, 0.94, 1.0))
        L["key"].location = C.Vector((-60, -260, 420))
        C.aim(L["key"], c + C.Vector((0, 0, 50)))
        # a big soft fill low in front, well off to the left so its reflection
        # in the semi-gloss paint misses the camera (Resilio R7 v2)
        C.area_light("front_fill", C.Vector((-330, -300, 25)), c + C.Vector((0, 0, -10)), size=200,
                     power=C.watts(C.KEY_W * (0.07 if night else 0.16)) * (0.45) ** 2, spread=45,
                     rgb=(0.85, 0.9, 1.0))
    if mood == "cables4":
        props.cables_all(ref)
    return ref


def build_story(mood):
    """Story subjects in a dim studio: dark sweep, a warm soft key from the
    upper left, a cool rim from behind, very little world light."""
    C.reset_scene()
    if mood in ("tape2", "rack2"):  # round 2: the room brings its own lamp-lit lighting
        return {"tape2": studio.tape_cross, "rack2": studio.studio_rack}[mood]()
    if mood == "tape3":  # round 3: the new deck
        return studio.tape_cross3()
    if mood == "rack3":
        return studio.studio_rack(decks=True)
    ref = {"tape": story.tape_loop, "rack": story.rack_reverb, "plate": story.plate}[mood]()
    C.world(0.02, rgb=(0.45, 0.45, 0.5))
    L = C.studio(ref["center"], ref["size"] * 1.2, key=0.55, fill=0.06, rim=0.9)
    L["key"].data.color = (1.0, 0.90, 0.78)
    L["rim"].data.color = (0.80, 0.86, 1.0)
    return ref


# ------------------------------------------------------------------ cameras

def camera_for(v, ref):
    if v.get("front"):
        return front_camera(v)
    if v.get("loc") is not None:
        return _camera_at(v)
    aspect = v["res"][0] / v["res"][1]
    dist = v.get("dist") or C.fit_distance(ref["radius"], v["f"], v["fit"], aspect=aspect, by=v.get("by", "height"))
    cam = C.camera(v["id"], v["target"], v["az"], v["el"], v["f"], dist,
                   fstop=v.get("fstop"), focus=v.get("focus"), shift=v.get("shift", (0.0, 0.0)))
    v["dist_mm"] = round(dist)
    v["height_mm"] = round(cam.location.z)
    v["label"] = (f"h {v['height_mm']} mm · el {v['el']}° · az {v['az']}° · {v['f']} mm"
                  + (f" · f/{v['fstop']}" if v.get("fstop") else "") + f" · {v['dist_mm']} mm away")
    return cam


def _camera_at(v):
    """Camera at an explicit position (floor-level shots), aimed at target."""
    loc, tgt = C.Vector(v["loc"]), C.Vector(v["target"])
    d = tgt - loc
    tilt = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    az = math.degrees(math.atan2(loc.x - tgt.x, tgt.y - loc.y))
    cam = C.camera(v["id"], tgt, 0, 0, v["f"], 1.0, fstop=v.get("fstop"), focus=v.get("focus"),
                   shift=v.get("shift", (0.0, 0.0)))
    cam.location = loc
    C.aim(cam, tgt)
    if v.get("fstop"):
        cam.data.dof.focus_distance = ((C.Vector(v["focus"]) if v.get("focus") is not None else tgt) - loc).length
    v["az"], v["el"] = round(az), round(tilt)
    v["dist_mm"] = round(d.length)
    v["height_mm"] = round(loc.z)
    v["label"] = (f"h {v['height_mm']} mm · tilt {round(tilt)}° · az {round(az)}° · {v['f']} mm"
                  + (f" · f/{v['fstop']}" if v.get("fstop") else "") + f" · {v['dist_mm']} mm to target"
                  + (f" · shift {v['shift'][0]:+.2f}" if v.get("shift") else ""))
    return cam


PANEL_CORNERS = [C.Vector((sx * module.PW / 2, 0.0, z)) for sx in (-1, 1) for z in (0.0, module.PH)]


def _project(cam, pts):
    from bpy_extras.object_utils import world_to_camera_view
    sc = bpy.context.scene
    W, H = sc.render.resolution_x, sc.render.resolution_y
    bpy.context.view_layer.update()
    out = []
    for p in pts:
        q = world_to_camera_view(sc, cam, p)
        out.append((q.x * W, (1 - q.y) * H))
    return out


def projected_box(cam):
    pp = _project(cam, PANEL_CORNERS)
    xs, ys = [p[0] for p in pp], [p[1] for p in pp]
    return dict(x=min(xs), y=min(ys), w=max(xs) - min(xs), h=max(ys) - min(ys))


def front_camera(v, f=200.0, rise_deg=1.0):
    """Resilio's front-on camera (200 mm, a hair above the panel's centre so
    the knob tops read), solved so the panel's face lands on FRONT_BOX: the
    camera stays square to the panel (no tilt: no keystoning) and is raised
    1 deg of view, with lens shift recentring the frame. Height (950 px) and
    the box's top-left are matched exactly; the width then follows from the
    panel's true proportions (50.5 / 128.5 x 950 = 373.3 px vs the box's 372)."""
    sc = bpy.context.scene
    W, H = v["res"]
    sc.render.resolution_x, sc.render.resolution_y = W, H
    cd = bpy.data.cameras.new(v["id"])
    cd.lens = f
    cd.sensor_fit = "HORIZONTAL"
    cd.sensor_width = 36.0
    cd.clip_start, cd.clip_end = 1.0, 100000.0
    cam = bpy.data.objects.new(v["id"], cd)
    C.link(cam)
    cam.rotation_euler = (math.pi / 2, 0, 0)  # looking along +Y, level
    box = FRONT_BOX
    sensor_v = 36.0 * H / W
    D = f * module.PH / (sensor_v * box["h"] / H)
    for _ in range(8):
        cam.location = (0.0, -D, module.PH / 2 + D * math.tan(math.radians(rise_deg)))
        b = projected_box(cam)
        D *= b["h"] / box["h"]
        cx_err = (b["x"] + b["w"] / 2) - (box["x"] + box["w"] / 2)
        cy_err = b["y"] - box["y"]
        cd.shift_x += cx_err / max(W, H)
        cd.shift_y -= cy_err / max(W, H)
    cam.location = (0.0, -D, module.PH / 2 + D * math.tan(math.radians(rise_deg)))
    b = projected_box(cam)
    v["projected_box"] = {k: round(x, 2) for k, x in b.items()}
    v["dist_mm"] = round(D)
    v["height_mm"] = round(cam.location.z)
    v["az"], v["el"], v["f"] = 0, rise_deg, f
    v["label"] = (f"h {v['height_mm']} mm · level, {rise_deg}° above the panel centre · az 0° · {f:.0f} mm · "
                  f"{v['dist_mm']} mm away · shift {cd.shift_x:+.4f}/{cd.shift_y:+.4f}")
    return cam


def measure_panel(cam, out_dir, vid):
    """Render a mask of the panel alone (white emission, everything else hidden,
    box pixel filter = exact coverage) and measure its edges with sub-pixel
    precision on rows/columns clear of holes. Returns the measured box."""
    import numpy as np
    sc = bpy.context.scene
    hidden = []
    for ob in sc.objects:
        if ob.type in ("MESH", "CURVE") and ob.name != "panel" and not ob.hide_render:
            ob.hide_render = True
            hidden.append(ob)
    pan = bpy.data.objects["panel"]
    keep = list(pan.data.materials)
    white = bpy.data.materials.new("mask_white")
    white.use_nodes = True
    nt = white.node_tree
    nt.nodes.clear()
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Strength"].default_value = 1.0
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], o.inputs["Surface"])
    pan.data.materials.clear()
    pan.data.materials.append(white)
    bg = sc.world.node_tree.nodes["Background"].inputs["Strength"]
    bg_keep = bg.default_value
    bg.default_value = 0.0
    saved = (sc.cycles.samples, sc.cycles.use_denoising, sc.view_settings.view_transform, sc.view_settings.look,
             sc.render.image_settings.file_format, sc.cycles.use_adaptive_sampling, sc.cycles.pixel_filter_type,
             sc.render.filter_size)
    sc.cycles.samples, sc.cycles.use_denoising = 256, False
    sc.cycles.use_adaptive_sampling = False
    sc.cycles.pixel_filter_type = "BOX"
    sc.render.filter_size = 1.0
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.render.image_settings.file_format = "PNG"
    dof = cam.data.dof.use_dof
    cam.data.dof.use_dof = False
    lights = [ob for ob in sc.objects if ob.type == "LIGHT" and not ob.hide_render]
    for ob in lights:
        ob.hide_render = True
    path = os.path.join(out_dir, f"_mask_{vid}.png")
    C.render(cam, path)
    img = bpy.data.images.load(path)
    W, H = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, 4)[::-1, :, 0]  # top row first
    bpy.data.images.remove(img)
    os.remove(path)  # intermediate
    # restore
    for ob in hidden + lights:
        ob.hide_render = False
    pan.data.materials.clear()
    for m in keep:
        pan.data.materials.append(m)
    bg.default_value = bg_keep
    (sc.cycles.samples, sc.cycles.use_denoising, sc.view_settings.view_transform, sc.view_settings.look,
     sc.render.image_settings.file_format, sc.cycles.use_adaptive_sampling, sc.cycles.pixel_filter_type,
     sc.render.filter_size) = saved
    cam.data.dof.use_dof = dof
    a = np.clip(a, 0, 1)
    rows = np.where(a.max(axis=1) > 0.5)[0]
    cols = np.where(a.max(axis=0) > 0.5)[0]
    # a row 8 mm below the top (between the screw holes and the first knob)
    # and a column 1 mm in from the left edge are clear of every hole
    y_row = int(rows[0] + (8.0 / module.PH) * (rows[-1] - rows[0]))
    x_col = int(cols[0] + (1.0 / module.PW) * (cols[-1] - cols[0]))
    row, col = a[y_row], a[:, x_col]
    cx, cy = int((cols[0] + cols[-1]) / 2), int((rows[0] + rows[-1]) / 2)
    left, right = cx - row[:cx].sum(), cx + row[cx:].sum()
    top, bottom = cy - col[:cy].sum(), cy + col[cy:].sum()
    return dict(x=round(float(left), 2), y=round(float(top), 2), w=round(float(right - left), 2),
                h=round(float(bottom - top), 2))


# ------------------------------------------------------------------ contact sheet

SLOTS = {
    "ref2": "Round-2 frames the owner's round-3 notes build on (for reference)",
    "picks": "Owner's picks from round 1 (for reference: already chosen, final render waits for the blue panel art)",
    "hero": "1 · Hero still (16:9, final 2400 × 1350): the monolith, dim, faintly glowing",
    "anim": "1b · Hero film, LOW-RES PREVIEW (960 px, 12 samples): the reveal plays once, then the hold loop",
    "front": "2 · Front-on (16:9, final 2116 × 1190): the annotated panel's photo, exact geometry",
    "macro": "3 · Panel macro (21:9, final 2400 × 1028): across the controls, shallow depth",
    "patched": "4 · Patched (16:9, final 2400 × 1350): both inputs and both outputs, black L, blue R",
    "og": "5 · Share image (1200 × 630)",
    "story": "6 · Story candidates (16:9): a short history of the endless hold — (a) two tape machines, "
             "(b) the rack reverb's infinite hold, (c) the plate",
}


def write_index(out_dir, variants, title="Sublustris Versio site renders — clay pass", ext="jpg"):
    groups = []
    for slot, slot_title in SLOTS.items():
        items = [v for v in variants if v["slot"] == slot]
        if not items:
            continue
        cards = []
        for v in items:
            extra = ""
            if v.get("measured_box"):
                mb, tb = v["measured_box"], FRONT_BOX
                extra = (f'<span class="cam">panel measured x {mb["x"]:.1f}, y {mb["y"]:.1f}, '
                         f'w {mb["w"]:.1f}, h {mb["h"]:.1f} px (target {tb["x"]}, {tb["y"]}, {tb["w"]}, {tb["h"]})</span>')
            f = v["file"]
            if f.endswith(".mp4"):
                cards.append(
                    f'''<figure><video src="{f}" poster="{v.get("poster", "")}" controls muted playsinline preload="metadata"
{"loop" if v.get("loop") else ""}></video>
<figcaption><b>{v["id"]}</b><span class="cam">{v["label"]}</span><span>{v["note"]}</span></figcaption></figure>''')
                continue
            cards.append(
                f'''<figure><a href="{f}" data-full="{f}" data-cap="{v["id"]} — {v["label"]}">
<img src="{f}" loading="lazy" alt="{v["id"]}"></a>
<figcaption><b>{v["id"]}</b><span class="cam">{v["label"]}</span>{extra}<span>{v["note"]}</span></figcaption></figure>''')
        groups.append(f"<section><h2>{slot_title}</h2><div class=grid>{''.join(cards)}</div></section>")
    html = f"""<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clay renders</title>
<style>
:root{{--paper:#f6f5f3;--ink:#111;--ink2:#5c5853;--rule:#d8d4cf}}
body{{margin:0;background:var(--paper);color:var(--ink);font:15px/1.35 -apple-system,system-ui,sans-serif}}
header,section{{padding:16px 24px;border-top:1px solid var(--rule)}}
h1{{font-weight:400;font-size:28px;margin:0 0 4px}} h2{{font-weight:400;font-size:18px;margin:0 0 12px}}
p{{margin:0;color:var(--ink2);max-width:60em}}
.grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(min(420px,100%),1fr));gap:20px}}
figure{{margin:0}} img,video{{width:100%;display:block;border-radius:6px;background:#ddd}}
figcaption{{display:flex;flex-direction:column;gap:2px;padding-top:6px;font-size:13px;color:var(--ink2)}}
figcaption b{{font-weight:500;color:var(--ink);font-family:ui-monospace,Menlo,monospace}}
.cam{{font-family:ui-monospace,Menlo,monospace;font-size:12px}}
#lb{{position:fixed;inset:0;background:rgba(20,18,16,.92);display:none;flex-direction:column;align-items:center;justify-content:center;gap:10px;cursor:zoom-out}}
#lb img{{max-width:96vw;max-height:88vh;width:auto;border-radius:0}} #lb div{{color:#eee;font:13px ui-monospace,Menlo,monospace;padding:0 16px}}
#lb.on{{display:flex}}
</style></head><body>
<header><h1>{title}</h1>
<p>Composition review: pick a composition (or two) per slot; the finals are rendered from the picks. Clay = grey matte
materials and bare metal, real studio lighting, the owner's panel print on as a decal, the four LEDs lit in the freeze
blue (#2F6FE4). Story gear is generic and unbranded. Click a frame to enlarge; ← → to step, Esc to close.
Camera: h = height above the ground, el / tilt = elevation, az = azimuth from the front towards the right, focal length
full-frame equivalent. Generated {time.strftime('%Y-%m-%d %H:%M')} by site/renders/render_clay.py.</p></header>
{''.join(groups)}
<div id=lb><img alt=""><div></div></div>
<script>
const links=[...document.querySelectorAll('a[data-full]')],lb=document.getElementById('lb');let i=0;
function show(k){{i=(k+links.length)%links.length;lb.querySelector('img').src=links[i].dataset.full;
lb.querySelector('div').textContent=links[i].dataset.cap;lb.classList.add('on')}}
links.forEach((a,k)=>a.addEventListener('click',e=>{{e.preventDefault();show(k)}}));
lb.addEventListener('click',()=>lb.classList.remove('on'));
addEventListener('keydown',e=>{{if(!lb.classList.contains('on'))return;
if(e.key==='Escape')lb.classList.remove('on');if(e.key==='ArrowRight')show(i+1);if(e.key==='ArrowLeft')show(i-1)}});
</script></body></html>"""
    with open(os.path.join(out_dir, "index.html"), "w") as f:
        f.write(html)


# ------------------------------------------------------------------ main

def _dummy_module_ref():
    from mathutils import Vector
    return dict(center=Vector(), radius=1, size=1, at=lambda *_a: Vector())


def _dummy_story_refs():
    from mathutils import Vector
    z = Vector()
    return dict(tape=dict(center=z, span=z, deck_l=z, deck_r=z, head_l=z),
                rack=dict(center=z, unit=z, display=z), plate=dict(center=z, corner=z),
                tape2=dict(center=z, patch=z), rack2=dict(center=z, rack=z, reverb=z, remote=z),
                tape3=dict(center=z, decks_center=z, heads_a=z), rack3=dict(center=z))


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="", help="comma list of slot names or variant id prefixes, e.g. hero,front_a")
    ap.add_argument("--exact", action="store_true", help="--only lists exact ids, not prefixes")
    ap.add_argument("--scale", type=int, default=100, help="resolution percentage (tests)")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--round", type=int, default=1, choices=(1, 2, 3),
                    help="which review round's variants (2 / 3 also write to renders/site_clay_r2/ or _r3/)")
    ap.add_argument("--out", default=None)
    ap.add_argument("--fmt", default="JPEG", choices=("JPEG", "PNG"))
    ap.add_argument("--res-scale", type=float, default=1.0,
                    help="multiply every variant's resolution (final pass: 1.25 = 2400 wide)")
    ap.add_argument("--title", default="Sublustris Versio site renders — clay pass")
    ap.add_argument("--index-only", action="store_true", help="rewrite index.html from variants.json")
    a = ap.parse_args(argv)
    a.out = a.out or {2: OUT_R2, 3: OUT_R3}.get(a.round, DEFAULT_OUT)
    only = [s for s in a.only.split(",") if s]

    def want(v):
        if v.get("rnd", 1) != a.round and not a.exact:
            return False
        if not only:
            return True
        if a.exact:
            return v["id"] in only
        return any(v["id"].startswith(s) or v["slot"] == s for s in only)

    os.makedirs(a.out, exist_ok=True)
    manifest_path = os.path.join(a.out, "variants.json")
    done = {}
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            done = {v["id"]: v for v in json.load(f)}
    ext = "jpg" if a.fmt == "JPEG" else "png"

    if not a.index_only:
        jobs = []  # (builder, mood, variant-fn)
        for v in module_variants(_dummy_module_ref()):
            if want(v) and ("module", v["mood"]) not in [(j[0], j[1]) for j in jobs]:
                jobs.append(("module", v["mood"]))
        for v in story_variants(_dummy_story_refs()):
            if want(v) and ("story", v["mood"]) not in jobs:
                jobs.append(("story", v["mood"]))
        for kind, mood in jobs:
            t_build = time.time()
            if kind == "module":
                ref = build_module(mood)
                variants = module_variants(ref)
            else:
                ref = build_story(mood)
                variants = story_variants({mood: ref})
            C.setup_cycles(samples=a.samples, fmt=a.fmt)
            sc = bpy.context.scene
            sc.render.resolution_percentage = a.scale
            print(f"[clay] built {kind}/{mood} in {time.time() - t_build:.1f} s", flush=True)
            for v in variants:
                if v.get("mood") != mood or not want(v):
                    continue
                w, h = (round(n * a.res_scale) for n in v["res"])
                sc.render.resolution_x, sc.render.resolution_y = w, h
                if v.get("front"):
                    v["res"] = (w, h)
                cam = camera_for(v, ref)
                v["file"] = f"{v['id']}.{ext}"
                t0 = time.time()
                C.render(cam, os.path.join(a.out, v["file"]))
                v["render_s"] = round(time.time() - t0, 1)
                if v.get("front") and a.scale == 100 and a.res_scale == 1.0:
                    v["measured_box"] = measure_panel(cam, a.out, v["id"])
                    print(f"[clay] {v['id']} panel measured {v['measured_box']} "
                          f"(projected {v['projected_box']}, target {FRONT_BOX})", flush=True)
                print(f"[clay] {v['id']}: {v['render_s']} s  ({v['label']})", flush=True)
                done[v["id"]] = {k: (list(x) if hasattr(x, "to_tuple") else x) for k, x in v.items()}
                with open(manifest_path, "w") as f:
                    json.dump(sorted(done.values(), key=_order), f, indent=1, default=str)
    if a.round == 2:
        _add_r1_picks(a.out, done)
    if a.round == 3:
        _add_r2_refs(a.out, done)
    ordered = sorted(done.values(), key=_order)
    with open(manifest_path, "w") as f:
        json.dump(ordered, f, indent=1, default=str)
    write_index(a.out, ordered, title=a.title if a.round == 1
                else a.title.replace("clay pass", f"clay pass, round {a.round}"))
    print(f"[clay] contact sheet: {os.path.join(a.out, 'index.html')}")


def _add_r1_picks(out, done):
    """Copy the round-1 picks' frames into the r2 folder and list them."""
    import shutil
    src = os.path.join(DEFAULT_OUT, "variants.json")
    if not os.path.exists(src):
        return
    with open(src) as f:
        r1 = {v["id"]: v for v in json.load(f)}
    for vid in R1_PICKS:
        if vid not in r1:
            continue
        v = dict(r1[vid])
        shutil.copy2(os.path.join(DEFAULT_OUT, v["file"]), os.path.join(out, v["file"]))
        v["slot"] = "picks"
        v["note"] = "PICKED (round 1). " + v["note"]
        done[vid] = v


def _add_r2_refs(out, done):
    """Copy the round-2 frames the round-3 notes refer to into the r3 folder."""
    import shutil
    src = os.path.join(OUT_R2, "variants.json")
    if not os.path.exists(src):
        return
    with open(src) as f:
        r2 = {v["id"]: v for v in json.load(f)}
    for vid in R2_REFS:
        if vid not in r2:
            continue
        v = dict(r2[vid])
        shutil.copy2(os.path.join(OUT_R2, v["file"]), os.path.join(out, v["file"]))
        v["slot"] = "ref2"
        v["note"] = "ROUND 2 (reference for the round-3 notes). " + v["note"]
        done[vid] = v


def _order(v):
    return (list(SLOTS).index(v["slot"]) if v["slot"] in SLOTS else 99, v["id"])


if __name__ == "__main__":
    main()
