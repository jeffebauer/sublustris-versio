"""Props for the module shots: patch cables. No brand names anywhere.

Ported from Resilio Versio's site/renders/props.py (the owner's MIT code):
only the patch cables came over (Resilio's tape, reels and cassette were its
own story). Resilio patched black / red pairs; Sublustris uses black for the
left channel and the freeze blue for the right.

Units mm, module coordinates (see module.py): panel front at y = 0 facing -Y,
centred on x = 0, standing on the floor z = 0.
"""
import bpy

import common as C


def _mat(name, rgb, rough, metallic=0.0, coat=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    return C.clay(name, rgb=rgb, rough=rough, metallic=metallic, coat=coat)


CABLE_RGB = dict(
    black=(0.02, 0.02, 0.02),
    blue=tuple(c * 0.55 for c in C.hex_to_linear(C.FREEZE_BLUE)),  # moulded PVC: a shade under the LED blue
    grey=(0.25, 0.25, 0.26),
)


def patch_cable(ref, hid, colour, route):
    """A 3.5 mm TS cable plugged into jack `hid`: nickel collar, moulded plug
    in `colour`, a cord that leaves the plug forward, sags to the floor and
    follows `route` (list of floor points) out of frame."""
    h = ref["holes"][hid]
    x, z = h["x"], h["z"]
    nickel = bpy.data.materials.get("mod_nickel") or _mat("mod_nickel", (0.88, 0.88, 0.88), 0.12, 1.0)
    body = _mat(f"cable_{colour}", CABLE_RGB[colour], 0.42)
    C.cylinder(f"{hid}_plug_tip", 1.75, 6.0, (x, 1.0, z), nickel, axis="X", segs=32)
    C.cylinder(f"{hid}_plug_collar", 2.6, 2.0, (x, -3.0, z), nickel, axis="X", segs=48, bevel=0.3)
    C.cylinder(f"{hid}_plug_body", 3.4, 16.0, (x, -12.0, z), body, axis="X", segs=48, r2=3.0, bevel=0.8)
    C.cylinder(f"{hid}_plug_relief", 2.2, 8.0, (x, -24.0, z), body, axis="X", segs=32, r2=1.8, bevel=0.4)
    pts = [(x, -27.5, z), (x, -36, z - 1.5)] + [tuple(p) for p in route]
    C.curve_obj(f"cord_{hid}", pts, 1.6, body, resolution=4, spline="NURBS")


def cables_all(ref):
    """IN L, IN R, OUT L, OUT R (J9-J12) patched: black for L, blue for R.
    Inputs leave to the left, outputs to the right; cords sag to the floor in
    front of the module and never cross the knobs."""
    def x(h):
        return ref["holes"][h]["x"]
    z = ref["holes"]["J9"]["z"]
    patch_cable(ref, "J9", "black", [(x("J9") - 2, -55, z - 8), (x("J9") - 14, -85, 1.6),
                                     (-80, -120, 1.6), (-260, -170, 1.6), (-600, -260, 1.6)])
    patch_cable(ref, "J10", "blue", [(x("J10") - 1, -58, z - 9), (x("J10") - 10, -100, 1.6),
                                     (-60, -160, 1.6), (-220, -260, 1.6), (-560, -420, 1.6)])
    patch_cable(ref, "J11", "black", [(x("J11") + 1, -58, z - 9), (x("J11") + 10, -105, 1.6),
                                      (60, -165, 1.6), (220, -250, 1.6), (560, -400, 1.6)])
    patch_cable(ref, "J12", "blue", [(x("J12") + 2, -55, z - 8), (x("J12") + 14, -88, 1.6),
                                     (85, -118, 1.6), (260, -165, 1.6), (600, -250, 1.6)])
