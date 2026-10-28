#!/usr/bin/env python3
"""Worst-case stack depth from GCC's -fcallgraph-info=su .ci files (review 2 audit, 28 Oct 2026).

usage: stack_depth.py <root-symbol-substring> file.ci [file.ci ...]
Prints the deepest call path from each matching root. Functions with no .ci node (libDaisy, libm, HAL)
are counted as 0 and listed, so the result is a lower bound for those; indirect calls are not followed.
"""
import re, sys, functools
root, files = sys.argv[1], sys.argv[2:]
node_re = re.compile(r'node: \{ title: "([^"]+)" label: "([^"]*)"')
edge_re = re.compile(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"')
def norm(t): return t.split(':')[-1]
stack, label, edges = {}, {}, {}
for f in files:
    for l in open(f):
        m = node_re.search(l)
        if m:
            t, lab = norm(m.group(1)), m.group(2)
            s = re.search(r'(\d+) bytes \((static|dynamic[^)]*)\)', lab)
            if s:
                stack[t] = max(stack.get(t, 0), int(s.group(1)))
                if 'dynamic' in s.group(2): stack[t] = stack[t]; label[t] = lab.split('\\n')[0] + ' [DYNAMIC]'
                else: label[t] = lab.split('\\n')[0]
            else:
                label.setdefault(t, lab.split('\\n')[0])
            continue
        m = edge_re.search(l)
        if m: edges.setdefault(norm(m.group(1)), set()).add(norm(m.group(2)))
sys.setrecursionlimit(10000)
@functools.lru_cache(None)
def deep(t, depth=0):
    if depth > 60: return (0, (t + ' [RECURSION?]',))
    best = (0, ())
    for c in edges.get(t, ()):
        if c == t: continue
        d = deep(c, depth + 1)
        if d[0] > best[0]: best = d
    return (stack.get(t, 0) + best[0], (t,) + best[1])
roots = [t for t in set(stack) | set(edges) if root in t]
for r in roots:
    tot, path = deep(r)
    print(f'{tot:6d} B  from {label.get(r, r)}')
    for p in path:
        print(f'   {stack.get(p, 0):5d}  {label.get(p, p)[:110]}')
