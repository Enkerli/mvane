#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""VIBE FM port, step 1's check: the .wasm's render (wasmtest, float WAV) against YARD's on the desktop (vibe_cmp.c),
sample for sample, in 24-bit steps. python3 tools/vibe_cmp.py audio.wav native.raw"""
import math
import struct
import sys

wav, raw = open(sys.argv[1], "rb").read(), open(sys.argv[2], "rb").read()
o = 12
while wav[o:o + 4] != b"data":
    o += 8 + struct.unpack_from("<I", wav, o + 4)[0]
w = struct.unpack_from(f"<{struct.unpack_from('<I', wav, o + 4)[0] // 4}f", wav, o + 8)
n = struct.unpack_from(f"<{len(raw) // 4}i", raw)
m = min(len(w), len(n))
diff = [abs(round(w[i] * 8388608.0) - n[i]) for i in range(m)]
same = sum(1 for d in diff if d == 0)
peak = max(abs(x) for x in n[:m]) or 1
worst = max(diff)
print(f"vibe_cmp: {m // 2} frames, {same} of {m} samples the same, the largest difference {worst} (24-bit steps)"
      + (f", {20 * math.log10(worst / peak):.1f} dB under the peak" if worst else ": bit for bit"))
sys.exit(0 if worst <= 64 else 1)
