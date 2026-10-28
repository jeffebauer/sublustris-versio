#!/usr/bin/env python3
"""Bit-identity check between two renderers: the proof behind every "sound-neutral" change.

    tools/same_render.py <ref_iv_render> <candidate_iv_render> <out_dir> [--jobs N] [--keep]

Renders a fixed set of ~55 cases (every SPACE; SIZE, ERA, DECAY, DRIFT, ATTACK corners; SIZE and
TIME moves and flicks; SPACE flips; freezes and paints; an ERA sweep; an odd block size) through both
renderers as 32-bit float WAVs and compares them byte for byte (cmp). The stimulus is generated here,
deterministically (a decaying chord every second for 7 s, a faint noise floor, then 5 s of tail), so
the check needs nothing that isn't in git.

Prints one line per case and ends with "SAME n/n" (exit 0) or "DIFFER k of n" (exit 1), listing the
cases that differ with the first differing sample and the largest difference, so a change that is
"identical except the last bit" reads differently from one that moved the sound.

The reference renderer is built from the parent commit (or any commit the change claims to equal):
    git worktree add /tmp/ref <commit> && cmake -S /tmp/ref -B /tmp/ref/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DIV_BUILD_PLUGIN=OFF && cmake --build /tmp/ref/build --target iv_render
Two renderers from the same source must come out SAME, so run it on itself once to trust it.
(25 Oct 2026, the overnight review: the 66-render scripts it replaces lived in a scratchpad and were lost.)
"""
import argparse
import concurrent.futures as cf
import json
import math
import os
import random
import struct
import subprocess
import sys

SR = 48000
SECONDS = 12.0


def write_stimulus(path):
    random.seed(7)
    n = int(SR * SECONDS)
    data = bytearray()
    for i in range(n):
        t = i / SR
        env = 0.0
        if t < 7.0:
            ph = t % 1.0
            if ph < 0.4:
                env = math.exp(-ph * 9.0)
        s = 0.0
        if env > 0.0:
            s = env * (0.4 * math.sin(2 * math.pi * 220 * t) + 0.3 * math.sin(2 * math.pi * 277.2 * t)
                       + 0.25 * math.sin(2 * math.pi * 329.6 * t) + 0.15 * math.sin(2 * math.pi * 1760 * t))
        nz = (random.random() - 0.5) * 2e-4
        data += struct.pack('<ff', s + nz, 0.8 * s - nz)
    hdr = (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt '
           + struct.pack('<IHHIIHH', 16, 3, 2, SR, SR * 8, 8, 32) + b'data' + struct.pack('<I', len(data)))
    with open(path, 'wb') as f:
        f.write(hdr + data)


SPACES = [('plate', 'PLATE'), ('hall', 'HALL'), ('massive', 'MASSIVE')]  # --set takes a switch's position by name


def cases():
    """(name, sets, automation-or-None, block)"""
    out = []
    for sp, v in SPACES:
        base = {'space': v, 'mix': 1.0}
        for size in (0.0, 0.5, 1.0):
            out.append((f'{sp}_size{size}', {**base, 'size': size}, None, 48))
        for era in (0.3, 0.5, 0.7, 0.9):
            out.append((f'{sp}_era{era}', {**base, 'size': 0.6, 'era': era}, None, 48))
        out.append((f'{sp}_bloom', {**base, 'attack': 'BLOOM', 'time': 0.5}, None, 48))
        out.append((f'{sp}_shimmer', {**base, 'attack': 'SHIMMER', 'time': 0.7}, None, 48))
        out.append((f'{sp}_infinite', {**base, 'decay': 1.0}, None, 48))
        out.append((f'{sp}_drift0', {**base, 'drift': 0.0}, None, 48))
        out.append((f'{sp}_drift1', {**base, 'drift': 1.0}, None, 48))
        out.append((f'{sp}_block37', {**base, 'size': 0.7}, None, 37))
        # SIZE: a slow move, then flicks (the crossfade and its rest, ADR 0025).
        out.append((f'{sp}_sizemove', base, {'breakpoints': [
            {'t': 0.0, 'key': 'size', 'value': 0.2}, {'t': 2.0, 'key': 'size', 'value': 0.2},
            {'t': 4.0, 'key': 'size', 'value': 0.9}, {'t': 5.0, 'key': 'size', 'value': 0.9},
            {'t': 5.01, 'key': 'size', 'value': 0.3}, {'t': 6.5, 'key': 'size', 'value': 0.3},
            {'t': 6.51, 'key': 'size', 'value': 0.8}]}, 48))
        out.append((f'{sp}_timemove', {**base, 'attack': 'NORMAL'}, {'breakpoints': [
            {'t': 0.0, 'key': 'time', 'value': 0.0}, {'t': 3.0, 'key': 'time', 'value': 0.0},
            {'t': 3.5, 'key': 'time', 'value': 0.8}, {'t': 6.0, 'key': 'time', 'value': 0.8},
            {'t': 6.01, 'key': 'time', 'value': 0.1}]}, 48))
        # Freeze by a tap at 3 s, unfreeze at 8 s; a paint (long hold) at 5 s.
        out.append((f'{sp}_freeze', base, {'buttons': [[3.0, 3.1], [8.0, 8.1]]}, 48))
        out.append((f'{sp}_paint', base, {'buttons': [[3.0, 3.1], [5.0, 7.0], [9.0, 9.1]]}, 48))
        out.append((f'{sp}_erasweep', base, {'breakpoints': [
            {'t': 0.0, 'key': 'era', 'value': 0.0}, {'t': 10.0, 'key': 'era', 'value': 1.0}]}, 48))
    # The CPU hunt's overload settings (28 Oct 2026): SHIMMER with WEAR half and MOTION full left (tape), SIZE swept
    # across noon (a small HALL's in-loop diffusion joining and leaving) and crept near the top, DECAY 0.9.
    hunt = {'mix': 1.0, 'attack': 'SHIMMER', 'time': 0.5, 'era': 0.5, 'drift': 0.0, 'decay': 0.9}
    for sp, v in SPACES:
        out.append((f'{sp}_hunt_sweep', {**hunt, 'space': v}, {'breakpoints': [
            {'t': 0.0, 'key': 'size', 'value': 0.2}, {'t': 3.0, 'key': 'size', 'value': 0.2},
            {'t': 3.01, 'key': 'size', 'value': 1.0}, {'t': 6.0, 'key': 'size', 'value': 1.0},
            {'t': 6.01, 'key': 'size', 'value': 0.2}, {'t': 9.0, 'key': 'size', 'value': 0.2},
            {'t': 9.01, 'key': 'size', 'value': 0.9}]}, 48))
    out.append(('massive_hunt_creep', {**hunt, 'space': 'MASSIVE'}, {'breakpoints': [
        {'t': 0.0, 'key': 'size', 'value': 0.7}, {'t': 2.0, 'key': 'size', 'value': 0.7},
        {'t': 11.0, 'key': 'size', 'value': 1.0}]}, 48))
    out.append(('hall_hunt_still', {**hunt, 'space': 'HALL', 'size': 0.3}, None, 48))
    out.append(('flips', {'mix': 1.0}, {'breakpoints': [
        {'t': 0.0, 'key': 'space', 'value': 0.0}, {'t': 2.5, 'key': 'space', 'value': 0.0},
        {'t': 2.51, 'key': 'space', 'value': 0.5}, {'t': 5.0, 'key': 'space', 'value': 0.5},
        {'t': 5.01, 'key': 'space', 'value': 1.0}, {'t': 7.5, 'key': 'space', 'value': 1.0},
        {'t': 7.51, 'key': 'space', 'value': 0.0}]}, 48))
    return out


def render(exe, stim, out_wav, sets, auto_path, block):
    cmd = [exe, stim, out_wav, '--block', str(block)]
    for k, v in sets.items():
        cmd += ['--set', f'{k}={v}']
    if auto_path:
        cmd += ['--auto', auto_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return f'render failed ({r.returncode}): {r.stderr.strip()[:200]}'
    return None


def read_float_wav(path):
    with open(path, 'rb') as f:
        b = f.read()
    # Minimal RIFF walk: find the data chunk.
    pos = 12
    while pos + 8 <= len(b):
        cid = b[pos:pos + 4]
        size = struct.unpack('<I', b[pos + 4:pos + 8])[0]
        if cid == b'data':
            return b[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
    return b''


def compare(a_path, b_path):
    a = read_float_wav(a_path)
    b = read_float_wav(b_path)
    if a == b:
        return 'same', None
    if len(a) != len(b):
        return 'differ', f'lengths {len(a)} vs {len(b)} bytes'
    fa = struct.unpack(f'<{len(a) // 4}f', a)
    fb = struct.unpack(f'<{len(b) // 4}f', b)
    first = next(i for i, (x, y) in enumerate(zip(fa, fb)) if x != y)
    worst = max(abs(x - y) for x, y in zip(fa, fb))
    n = sum(1 for x, y in zip(fa, fb) if x != y)
    return 'differ', f'first at frame {first // 2} ({first / 2 / SR:.3f} s), {n} samples differ, max |diff| {worst:.3g}'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('ref')
    ap.add_argument('cand')
    ap.add_argument('out_dir')
    ap.add_argument('--jobs', type=int, default=max(2, (os.cpu_count() or 4) - 2))
    ap.add_argument('--keep', action='store_true', help='keep the WAVs (default: delete the ones that match)')
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    stim = os.path.join(args.out_dir, 'stim.wav')
    if not os.path.exists(stim):
        write_stimulus(stim)
    all_cases = cases()

    def one(case):
        name, sets, auto, block = case
        auto_path = None
        if auto:
            auto_path = os.path.join(args.out_dir, f'{name}.auto.json')
            with open(auto_path, 'w') as f:
                json.dump(auto, f)
        a = os.path.join(args.out_dir, f'{name}.ref.wav')
        b = os.path.join(args.out_dir, f'{name}.cand.wav')
        ea = render(args.ref, stim, a, sets, auto_path, block)
        eb = render(args.cand, stim, b, sets, auto_path, block)
        if ea or eb:
            return name, 'error', ea or eb
        verdict, detail = compare(a, b)
        if verdict == 'same' and not args.keep:
            os.remove(a)
            os.remove(b)
        return name, verdict, detail

    results = []
    with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for name, verdict, detail in ex.map(one, all_cases):
            results.append((name, verdict, detail))
            print(f'{verdict:6s} {name}' + (f'  {detail}' if detail else ''), flush=True)
    bad = [r for r in results if r[1] != 'same']
    if bad:
        print(f'DIFFER {len(bad)} of {len(results)}')
        return 1
    print(f'SAME {len(results)}/{len(results)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
