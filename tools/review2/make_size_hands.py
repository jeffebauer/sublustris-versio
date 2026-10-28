#!/usr/bin/env python3
"""SIZE hands for the desktop, after firmware/main.cpp SizeHand(): automation JSON (iv_render --auto) that turns
SIZE the way the profile lap's hand does after release's knob path (a 0.003 deadband: SIZE moves in 0.003 steps).
6 s with SIZE still at 0.7 first (the lap's untimed play), then 6 s of the hand. review2/cpu, 28 Oct 2026."""
import json, sys

def ease(a, b, u):
    u = min(1.0, max(0.0, u))
    return a + (b - a) * u * u * (3 - 2 * u)

def hand(kind, t):
    if kind == "sweep":
        u = t % 5.0
        return ease(0.2, 1.0, (u - 0.5) / 2.0) if u < 2.5 else ease(1.0, 0.2, (u - 3.0) / 2.0)
    if kind == "twists":
        u = t % 0.7
        return ease(0.5, 0.9, u / 0.15) if u < 0.35 else ease(0.9, 0.5, (u - 0.35) / 0.15)
    if kind == "nudge":
        k = int(t / 1.5); u = t - 1.5 * k
        return ease(0.706, 0.7, (u - 0.2) / 0.05) if k % 2 else ease(0.7, 0.706, (u - 0.2) / 0.05)
    if kind == "still":
        return 0.7
    return 0.7 + 0.3 * min(1.0, t / 6.0)  # creep

def main():
    kind, out = sys.argv[1], sys.argv[2]
    pre, dur, ms = 6.0, 6.0, 0.001
    pts, sent = [{"t": 0.0, "key": "size", "value": 0.7}], 0.7
    n = int(dur / ms)
    for b in range(n):
        v = hand(kind, b * ms)
        if abs(v - sent) > 0.003 or (v in (0.0, 1.0) and v != sent):  # release's deadband (and its end stops)
            t = pre + b * ms
            pts.append({"t": t - 1e-6, "key": "size", "value": sent})
            pts.append({"t": t, "key": "size", "value": v})
            sent = v
    json.dump({"breakpoints": pts}, open(out, "w"))
    print(f"{kind}: {len(pts)} points")

if __name__ == "__main__":
    main()
