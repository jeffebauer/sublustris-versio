"""The animated hero: the monolith emerging from the half-light, then holding.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \\
        --python site/renders/render_hero.py -- --frames DIR [--samples 64] [--res 2400x1350] [--only 1,120,240]

then encode with make_hero_video.sh. Ported from Resilio Versio's
render_hero.py (the owner's MIT code): same camera move and light build, the
"night" look (render_clay hero_d / hero_e) instead of Resilio's monolith, and
the LEDs tell a different story (see below).

Timeline (30 fps):
  frames   1-240  hero-reveal (8 s, plays once). The camera starts low and
                  close in front of the module, looking steeply up so it looms;
                  near-black, the LEDs the only light. One continuous jib-like
                  move: it dollies back while rising and tilting down, ending
                  square to the panel on hero_e's composition (100 mm, the panel
                  72 % of the frame height). Quintic ease-in-out, no overshoot.
                  Light in stages: dark -> the cold top and rim lights trace the
                  edges and knob caps (frames 50-150) -> the soft key and front
                  fill come up to the hero_e look (110-225).
  frames 241-360  hero-hold-loop (4 s, loops seamlessly; it starts where the
                  reveal ends): camera and light at rest; only the LEDs move.

LEDs (freeze blue throughout, the module frozen):
  LED3/LED4 (OUT L/R) the held tail: bright, breathing slowly (one breath per
                      loop, L and R a little out of phase, a faint shimmer on top).
  LED1/LED2 (IN L/R)  dim, a soft swell or two per loop (something played over
                      the hold), R trailing L.
  Every level is periodic in the loop length, so frame 361 equals frame 241.
  Each LED also drives its small spill light so the glow falls on the knobs.
"""
import math
import os
import sys
import time

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import module  # noqa: E402
import render_clay  # noqa: E402

FPS = 30
LOOP = 120                # 4 s hold loop
REVEAL_END = 240
HOLD_START, HOLD_END = 241, 360
EDGES_ON = (50, 150)      # rim + top light: the edges and knob caps appear first
LIGHTS_ON = (110, 225)    # then the soft key and the fill
CAM_MOVE = (1, 236)
EDGE_LIGHTS = ("rim", "top")
BLUE = C.hex_to_linear(C.FREEZE_BLUE)


def led_levels(f):
    """Levels for LED1..LED4 at frame f (periodic in LOOP)."""
    t = 2 * math.pi * f / LOOP
    out_l = 0.78 + 0.14 * math.sin(t) + 0.04 * math.sin(3 * t + 1.1)
    out_r = 0.76 + 0.14 * math.sin(t - 0.6) + 0.04 * math.sin(3 * t + 2.3)
    in_l = 0.12 + 0.30 * max(0.0, math.sin(2 * t)) ** 2
    in_r = 0.12 + 0.26 * max(0.0, math.sin(2 * t - 0.5)) ** 2
    return [in_l, in_r, out_l, out_r]


def _ease(f, f0, f1):
    """Smootherstep: zero velocity and acceleration at both ends."""
    k = min(1.0, max(0.0, (f - f0) / (f1 - f0)))
    return k * k * k * (k * (6 * k - 15) + 10)


def build():
    ref = render_clay.build_module("night")
    sc = bpy.context.scene
    spills = {f"LED{i}": bpy.data.objects.get(f"LED{i}_spill") for i in range(1, 5)}
    # every light the night mood made ramps from 0 to its full (hero_e) energy;
    # the edge lights first, the key and fill later
    for ob in [o for o in bpy.data.objects if o.type == "LIGHT" and o not in spills.values()]:
        full = ob.data.energy
        span = EDGES_ON if ob.name in EDGE_LIGHTS else LIGHTS_ON
        for f in range(1, REVEAL_END + 1, 3):
            ob.data.energy = full * _ease(f, *span)
            ob.data.keyframe_insert("energy", frame=f)
        ob.data.energy = full
        ob.data.keyframe_insert("energy", frame=REVEAL_END)
    bg = sc.world.node_tree.nodes["Background"].inputs["Strength"]
    full = bg.default_value
    for f in (1, LIGHTS_ON[0]):
        bg.default_value = 0.0
        bg.keyframe_insert("default_value", frame=f)
    bg.default_value = full
    bg.keyframe_insert("default_value", frame=LIGHTS_ON[1])

    # LEDs: one material each (the module shares one), keyed every frame
    leds = []
    for i in range(1, 5):
        lens = bpy.data.objects[f"LED{i}_lens"]
        m = bpy.data.materials.new(f"led_anim_{i}")
        m.use_nodes = True
        p = m.node_tree.nodes["Principled BSDF"]
        p.inputs["Base Color"].default_value = (0.05, 0.05, 0.06, 1)
        p.inputs["Roughness"].default_value = 0.25
        p.inputs["Emission Color"].default_value = (*BLUE, 1)
        C.assign(lens, m)
        leds.append((p, spills[f"LED{i}"].data))
    for f in range(1, HOLD_END + 1):
        for (p, ld), lvl in zip(leds, led_levels(f)):
            p.inputs["Emission Strength"].default_value = 0.1 + 7.0 * lvl
            ld.energy = C.watts(0.0035) * 2.2 * lvl
            p.inputs["Emission Strength"].keyframe_insert("default_value", frame=f)
            ld.keyframe_insert("energy", frame=f)

    # camera: low and close looking steeply up -> square to the panel's centre
    # (zero tilt, zero yaw), 100 mm, the panel 72 % of the frame height
    mid = module.PH / 2
    end_dist = render_clay.HERO_DIST
    start, start_t = C.Vector((0, -62, 2.5)), C.Vector((0, 0, 118))
    end, end_t = C.Vector((0, -end_dist, mid)), C.Vector((0, 0, mid))
    lens0, lens1 = 20.0, 100.0
    tgt = bpy.data.objects.new("cam_target", None)
    C.link(tgt)
    cd = bpy.data.cameras.new("hero")
    cd.sensor_width = 36.0
    cd.clip_start, cd.clip_end = 1.0, 100000.0
    cam = bpy.data.objects.new("hero", cd)
    C.link(cam)
    con = cam.constraints.new("TRACK_TO")
    con.target = tgt
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    cd.dof.use_dof = True
    cd.dof.aperture_fstop = 16 * bpy.context.scene.unit_settings.scale_length
    for f in range(CAM_MOVE[0], CAM_MOVE[1] + 1):
        k = _ease(f, *CAM_MOVE)
        cam.location = start.lerp(end, k)
        tgt.location = start_t.lerp(end_t, k)
        # the lens tracks the dolly so the module's apparent size shrinks
        # steadily (a pure pull-back feel, no zoom-out-then-in)
        dist = (end_t - cam.location).length
        excess0 = lens0 / (lens1 * (start_t - start).length / (end_t - end).length)
        cd.lens = lens1 * dist / (end_t - end).length * excess0 ** (1 - k)
        cd.dof.focus_distance = (C.Vector((0, 0, 64)) - cam.location).length
        cd.dof.keyframe_insert("focus_distance", frame=f)
        cam.keyframe_insert("location", frame=f)
        tgt.keyframe_insert("location", frame=f)
        cd.keyframe_insert("lens", frame=f)
    sc.camera = cam
    sc.frame_start, sc.frame_end = 1, HOLD_END
    sc.render.fps = FPS
    return ref


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = argv[argv.index("--frames") + 1]
    samples = int(argv[argv.index("--samples") + 1]) if "--samples" in argv else 64
    only = [int(x) for x in argv[argv.index("--only") + 1].split(",")] if "--only" in argv else None
    res = argv[argv.index("--res") + 1] if "--res" in argv else "2400x1350"
    w, h = (int(n) for n in res.split("x"))
    build()
    C.setup_cycles(samples=samples, width=w, height=h)
    sc = bpy.context.scene
    sc.render.use_persistent_data = True
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 95
    os.makedirs(out, exist_ok=True)
    frames = only or range(sc.frame_start, sc.frame_end + 1)
    t0 = time.time()
    for f in frames:
        sc.frame_set(f)
        sc.render.filepath = os.path.join(out, f"f_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)
        print(f"[hero] frame {f} done, {time.time() - t0:.0f} s elapsed", flush=True)


if __name__ == "__main__":
    main()
