#!/usr/bin/env python3
"""Flash / RAM bytes per input object and per archive, from a GNU ld map file (review 2 audit, 28 Oct 2026).

usage: map_by_object.py <file.map> [--region flash|all] [--top N] [--section-prefix .text]

Parses the 'Linker script and memory map' part: each input-section line (' .text.foo 0xADDR 0xSIZE obj')
is attributed to its object (archive member or .o). Flash = output sections loaded from flash
(.isr_vector, .itcm_text (LMA in flash), .text, .init_array, .fini_array, .data (LMA in flash)).
"""
import re, sys, collections, argparse

ap = argparse.ArgumentParser()
ap.add_argument('map')
ap.add_argument('--top', type=int, default=40)
ap.add_argument('--sections', default='.isr_vector,.itcm_text,.text,.init_array,.fini_array,.data,.ARM.extab,.exidx,.preinit_array')
ap.add_argument('--symbols', action='store_true', help='list input sections (functions) instead of objects')
ap.add_argument('--grep', default=None, help='only objects matching this regex')
a = ap.parse_args()
keep = set(a.sections.split(','))

lines = open(a.map, errors='replace').read().split('\n')
start = next(i for i, l in enumerate(lines) if l.startswith('Linker script and memory map'))
out = None
by_obj = collections.Counter(); by_sec = []
pending = None
osec_re = re.compile(r'^(\.[\w.]+)\s+0x[0-9a-f]+\s+0x[0-9a-f]+')
osec_only = re.compile(r'^(\.[\w.]+)\s*$')
isec_re = re.compile(r'^ (\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$')
isec_name_only = re.compile(r'^ (\S+)\s*$')
isec_cont = re.compile(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*)$')
for l in lines[start:]:
    m = osec_re.match(l) or osec_only.match(l)
    if m and not l.startswith(' '):
        out = m.group(1); pending = None; continue
    if out not in keep: continue
    m = isec_re.match(l)
    if m:
        name, addr, size, obj = m.groups(); pending = None
    else:
        m = isec_name_only.match(l)
        if m and not l.startswith('  '):
            pending = m.group(1); continue
        m = isec_cont.match(l)
        if m and pending:
            addr, size, obj = m.groups(); name = pending; pending = None
        else:
            continue
    size = int(size, 16)
    if size == 0 or name.startswith('*'): continue
    obj = obj.strip()
    if a.grep and not re.search(a.grep, obj + ' ' + name): continue
    by_obj[obj] += size
    by_sec.append((size, out, name, obj))

def short(o):
    o = re.sub(r'.*/', '', o)
    return o
if a.symbols:
    for s, out, n, o in sorted(by_sec, reverse=True)[:a.top]:
        print(f'{s:7d} {out:12s} {n[:70]:70s} {short(o)}')
else:
    by_ar = collections.Counter()
    for o, s in by_obj.items():
        ar = re.sub(r'\(.*\)$', '', short(o)) if '(' in o else 'objects'
        by_ar[ar] += s
    print('# per archive / objects')
    for ar, s in by_ar.most_common(): print(f'{s:8d} {ar}')
    print(f'{sum(by_ar.values()):8d} total\n# per object')
    for o, s in by_obj.most_common(a.top): print(f'{s:8d} {short(o)}')
