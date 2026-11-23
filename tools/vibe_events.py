#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""vibe/cmp.json's notes as tools/vibe_cmp.c reads them: "BLOCK STATUS D1 D2", each on the 128-frame block wasmtest
gives it at 44.1 kHz (the first block at or after its time), velocity as the firmware rounds it."""
import json
import sys

for s in json.load(open(sys.argv[1]))["steps"]:
    if "ev" not in s or s["ev"][0] != 9:
        continue
    b = 0
    while b * 128 / 44100 < s["t"]:
        b += 1
    _, note, down, v = s["ev"][:4]
    print(b, 0x90 if down else 0x80, note, max(1, min(127, int(v * 127 + 0.5))) if down else 0)
