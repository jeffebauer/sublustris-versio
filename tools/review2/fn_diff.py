#!/usr/bin/env python3
"""Per-function size difference between two .elf files (review 2 audit). usage: fn_diff.py a.elf b.elf [min]"""
import subprocess, sys, collections
def sizes(elf):
    out = subprocess.run(['arm-none-eabi-nm', '-S', '-C', '--size-sort', elf], capture_output=True, text=True).stdout
    d = collections.Counter()
    for l in out.splitlines():
        p = l.split(None, 3)
        if len(p) == 4 and p[2] in 'tTWw': d[p[3]] += int(p[1], 16)
    return d
a, b = sizes(sys.argv[1]), sizes(sys.argv[2]); m = int(sys.argv[3]) if len(sys.argv) > 3 else 16
rows = sorted(((b[k] - a[k], k) for k in set(a) | set(b) if abs(b[k] - a[k]) >= m))
for d, k in rows: print(f'{d:+6d}  {a[k]:6d} -> {b[k]:6d}  {k[:100]}')
print(f'{sum(b.values()) - sum(a.values()):+6d} total code')
