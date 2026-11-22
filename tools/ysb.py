#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The user's YARD sounds: the FM-1's bank (yard.ysb, app.c yard_bank_t) to JSON and back.

The bank: "MVYB" (little-endian 0x4259564D), version 1, its size (32), two zeros; then 32 sounds of 32 bytes: a name
(12 bytes, NUL-padded, printable ASCII), flags (bit 0 used, bit 1 mono), the nine parameters 0..127 in synth.h's
order, ten zeros. The JSON: {"format": "mvane-yard-sounds", "version": 1, "sounds": [{"slot": 1..32, "name", "mono",
"tone", "release", "glide", "attack", "decay", "res", "detune", "sub", "pluck"}, ...]}, the used slots only; a sound
without a slot takes the first free one.
"""
import json
import struct
import sys

MAGIC, VERSION, N, SIZE = 0x4259564D, 1, 32, 8 + 32 * 32
PARAMS = ["tone", "release", "glide", "attack", "decay", "res", "detune", "sub", "pluck"]   # synth.h SP_*
DEFAULT = {"tone": 56, "release": 60, "glide": 30, "attack": 24, "decay": 64, "res": 0, "detune": 0, "sub": 0,
           "pluck": 0}


def to_json(raw):
    """yard.ysb's bytes -> the JSON's dict (an empty or foreign bank: no sounds)"""
    out = {"format": "mvane-yard-sounds", "version": VERSION, "sounds": []}
    if len(raw) != SIZE or struct.unpack_from("<I", raw)[0] != MAGIC or raw[4] != VERSION:
        return out
    for i in range(N):
        b = raw[8 + 32 * i: 8 + 32 * (i + 1)]
        if not b[12] & 1:
            continue
        name = b[:12].split(b"\0", 1)[0].decode("ascii", "replace")
        s = {"slot": i + 1, "name": name, "mono": bool(b[12] & 2)}
        s.update({k: min(127, b[13 + j]) for j, k in enumerate(PARAMS)})
        out["sounds"].append(s)
    return out


def from_json(d):
    """the JSON's dict -> yard.ysb's bytes; raises ValueError on a bad sound"""
    if d.get("format") not in (None, "mvane-yard-sounds"):
        raise ValueError(f"not mvane yard's sounds: format {d.get('format')!r}")
    slots = [None] * N
    later = []
    for s in d.get("sounds", []):
        slot = s.get("slot")
        if slot is None:
            later.append(s)
            continue
        if not isinstance(slot, int) or not 1 <= slot <= N:
            raise ValueError(f"{s.get('name', '?')}: slot {slot!r}, not 1..{N}")
        if slots[slot - 1] is not None:
            raise ValueError(f"slot {slot} twice")
        slots[slot - 1] = s
    for s in later:                                 # (no slot: the first free one)
        if None not in slots:
            raise ValueError(f"more than {N} sounds")
        slots[slots.index(None)] = s
    raw = bytearray(SIZE)
    struct.pack_into("<IBB", raw, 0, MAGIC, VERSION, N)
    for i, s in enumerate(slots):
        if s is None:
            continue
        name = str(s.get("name", f"USER {i + 1}")).upper()
        if not all(" " <= ch <= "~" for ch in name):
            raise ValueError(f"{name!r}: the screen shows ASCII letters, digits and signs only")
        b = name.encode("ascii")[:12]
        o = 8 + 32 * i
        raw[o:o + len(b)] = b
        raw[o + 12] = 1 | (2 if s.get("mono") else 0)
        for j, k in enumerate(PARAMS):
            v = s.get(k, DEFAULT[k])
            if not isinstance(v, int) or not 0 <= v <= 127:
                raise ValueError(f"{name}: {k} {v!r}, not 0..127")
            raw[o + 13 + j] = v
    return bytes(raw)


if __name__ == "__main__":                          # ysb.py IN.ysb|IN.json OUT.json|OUT.ysb: one to the other
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    if src.endswith(".json"):
        open(dst, "wb").write(from_json(json.loads(data)))
    else:
        open(dst, "w").write(json.dumps(to_json(data), indent=2) + "\n")
