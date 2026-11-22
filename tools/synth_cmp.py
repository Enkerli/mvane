#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare the saw's output with its filter tables (A) and computed exactly (B): tools/synth_cmp.c's two renders.
Prints the difference's level under the signal, overall and per part (the tone sweep, the bends, the top), and fails
below a bound (an audible change). Usage: synth_cmp.py A.raw B.raw [MIN_DB]"""
import math
import struct
import sys

SR = 44100
PARTS = [('tone sweep', 0, 3), ('bends, slide, pressure', 3, 6), ('top of the cutoff', 6, 8)]


def load(p):
    d = open(p, 'rb').read()
    return struct.unpack(f'<{len(d) // 4}i', d)


def snr(a, b):
    s = sum(x * x for x in b)
    e = sum((x - y) ** 2 for x, y in zip(a, b))
    return float('inf') if e == 0 else 10 * math.log10(s / e) if s else float('-inf')


def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    bound = float(sys.argv[3]) if len(sys.argv) > 3 else 40.0
    worst = snr(a, b)
    print(f'all: the difference {worst:.1f} dB under the signal')
    for name, t0, t1 in PARTS:
        i0, i1 = t0 * SR, min(t1 * SR, len(a))
        v = snr(a[i0:i1], b[i0:i1])
        worst = min(worst, v)
        print(f'{name} ({t0}-{t1} s): {v:.1f} dB')
    if worst < bound:
        print(f'FAIL: under {bound} dB')
        return 1
    print('synth_cmp: the tables change nothing audible (by this bound; listen too)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
