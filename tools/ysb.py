#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The user's sounds, YARD's and Clipper's: the FM-1's bank (yard.ysb, app.c yard_bank_t) to JSON and back.

The bank, version 3 (0.12: the user's scales; version 2, 0.11's, 3088 bytes, without them; version 1, 0.10's, had the
YARD part only, 1032 bytes):
  "MVYB" (little-endian 0x4259564D), version, 32, two zeros; 32 YARD sounds of 32 bytes: a name (12, NUL-padded,
  printable ASCII), flags (bit 0 used, bit 1 mono, bit 2 a favourite: 0.17), the thirteen parameters 0..127 in synth.h's order (0.10 wrote
  nine: the other four at 0), six zeros;
  "MVCB" (0x4243564D), 32, three zeros; 32 Clipper sounds of 64 bytes: a name (12), flags (bit 0 used, bit 2 a
  favourite), the factory
  sound it starts from (engine, preset), the reverb's model (signed: -1 as it is), how many changes (up to 14), three
  zeros, 14 changes of 3 bytes (Clipper's parameter P_*, a signed 16-bit value, little-endian), two zeros;
  "MVSB" (0x4253564D), 16, three zeros; 16 scales of 16 bytes: a name (12; empty: the screen shows its number),
  0x8000 | the pitch-class set's number (0..4095, bit 0 the tonic: C major 2741; 0: an empty slot), two zeros.
The JSON: {"format": "mvane-yard-sounds", "version": 3, "scales": [{"slot", "name", "pcs"}], "sounds": [YARD's: {"slot", "name", "mono", "tone",
"release", "glide", "attack", "decay", "res", "detune", "sub", "pluck", "wave" (0..42 SAW, 43..84 PULSE, 85..127
TRIANGLE), "pw", "noise", "filter" (0..42 LOW, 43..84 BAND, 85..127 HIGH)}], "clipper": [{"slot", "name", "engine",
"preset", "sound" (its name, to read), "reverb" (ROOM, PLATE, HALL, SHIMR or null), "changes": [{"id", "name" (to
read), "value"}]}]}; a sound without a slot takes the first free one; a Clipper sound may name its sound ("sound")
instead of its engine and preset, a change its parameter ("name", when one has that label) instead of its id: BRITE,
TIMBR, MOTN and SPACE are a HYBRID sound's M1..M4, as on the panel. Either kind may have "favourite": true (0.17:
first in PRESETS, in FAVOURITES; SAVE held on the FM-1 marks one).

Batch changes (0.17, Alex: "applying some similar FX across the board"): fx() sets the same changes on many Clipper
sounds at once (all of them, or the favourites), each change replacing the sound's own of that parameter:
    tools/ysb.py fx IN OUT CHO=4 REV=8 SPACE=12 [--reverb HALL] [--favourites]
(IN and OUT .json or .ysb; then tools/yard.py sounds --put OUT, or the page's Load a file, Send to the FM-1).
"""
import json
import os
import struct
import sys

MAGIC, CMAGIC, SMAGIC, N, CN, SN, OV = 0x4259564D, 0x4243564D, 0x4253564D, 32, 32, 16, 14
FAV = 4                                             # (0.17) a sound's flag: a favourite
HYBRID = 12                                         # (Clipper's HYBRID engine: its macros are its EDIT values M1..M4)
SIZE1 = 8 + 32 * N
SIZE2 = SIZE1 + 8 + 64 * CN
SIZE = SIZE2 + 8 + 16 * SN
PARAMS = ["tone", "release", "glide", "attack", "decay", "res", "detune", "sub", "pluck",   # synth.h SP_*
          "wave", "pw", "noise", "filter",          # (0.11: 0 leaves a sound as 0.10 made it)
          "onset", "onset_level", "level"]   # (0.16: LEVEL, half dB from 64; 0, from before it: 0 dB)                   # (0.15: ONSET, 0..127 as the panel steps it, 0..11 OFF..TICK; 0 none)
DEFAULT = {"tone": 56, "release": 60, "glide": 30, "attack": 24, "decay": 64, "res": 0, "detune": 0, "sub": 0,
           "pluck": 0, "wave": 0, "pw": 0, "noise": 0, "filter": 0, "onset": 0, "onset_level": 0, "level": 0}
NAMES_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web", "clp_names.json")
_names = None


def names():
    """Clipper's names (web/clp_names.json, make clp-names): sounds, labels, each engine's EDIT labels, reverbs"""
    global _names
    if _names is None:
        _names = json.load(open(NAMES_PATH))
    return _names


def label(pid, engine):
    n = names()
    if pid >= n["edit0"]:
        return n["edit"].get(str(engine), [""] * 8)[pid - n["edit0"]] if pid - n["edit0"] < 8 else ""
    return n["labels"][pid] if pid < len(n["labels"]) else ""


def sound_name(engine, preset):
    return next((s["name"] for s in names()["sounds"] if s["engine"] == engine and s["preset"] == preset), "?")


def _yard(raw, out):
    for i in range(N):
        b = raw[8 + 32 * i: 8 + 32 * (i + 1)]
        if not b[12] & 1:
            continue
        s = {"slot": i + 1, "name": b[:12].split(b"\0", 1)[0].decode("ascii", "replace"), "mono": bool(b[12] & 2),
             "favourite": bool(b[12] & FAV)}
        s.update({k: min(127, b[13 + j]) for j, k in enumerate(PARAMS)})
        out["sounds"].append(s)


def to_json(raw):
    """yard.ysb's bytes -> the JSON's dict (an empty or foreign bank: no sounds)"""
    out = {"format": "mvane-yard-sounds", "version": 3, "sounds": [], "clipper": [], "scales": []}
    if len(raw) not in (SIZE1, SIZE2, SIZE) or struct.unpack_from("<I", raw)[0] != MAGIC or raw[4] not in (1, 2, 3):
        return out
    _yard(raw, out)
    if len(raw) == SIZE1 or raw[4] == 1 or struct.unpack_from("<I", raw, SIZE1)[0] != CMAGIC:
        return out
    if len(raw) == SIZE and raw[4] == 3 and struct.unpack_from("<I", raw, SIZE2)[0] == SMAGIC:
        for i in range(SN):
            o = SIZE2 + 8 + 16 * i
            p = struct.unpack_from("<H", raw, o + 12)[0]
            if p & 0x8000:
                out["scales"].append({"slot": i + 1, "name": raw[o:o + 12].split(b"\0", 1)[0].decode("ascii", "replace"),
                                      "pcs": p & 0xFFF})
    rev = names()["reverb"]
    for i in range(CN):
        o = SIZE1 + 8 + 64 * i
        b = raw[o:o + 64]
        if not b[12] & 1:
            continue
        engine, preset, rtype, n = b[13], b[14], struct.unpack_from("<b", b, 15)[0], min(OV, b[16])
        ch = []
        for k in range(n):
            pid, v = b[20 + 3 * k], struct.unpack_from("<h", b, 21 + 3 * k)[0]
            ch.append({"id": pid, "name": label(pid, engine), "value": v})
        r = rtype - rev["min"]
        out["clipper"].append({"slot": i + 1, "name": b[:12].split(b"\0", 1)[0].decode("ascii", "replace"),
                               "engine": engine, "preset": preset, "sound": sound_name(engine, preset),
                               "reverb": rev["names"][r] if 0 <= r < len(rev["names"]) else None,
                               "favourite": bool(b[12] & FAV), "changes": ch})
    return out


def _slots(items, n, what):
    slots, later = [None] * n, []
    for s in items:
        slot = s.get("slot")
        if slot is None:
            later.append(s)
            continue
        if not isinstance(slot, int) or not 1 <= slot <= n:
            raise ValueError(f"{s.get('name', '?')}: slot {slot!r}, not 1..{n}")
        if slots[slot - 1] is not None:
            raise ValueError(f"{what}: slot {slot} twice")
        slots[slot - 1] = s
    for s in later:                                 # (no slot: the first free one)
        if None not in slots:
            raise ValueError(f"more than {n} {what}")
        slots[slots.index(None)] = s
    return slots


def _name(s, default):
    name = str(s.get("name", default)).upper()
    if not all(" " <= ch <= "~" for ch in name):
        raise ValueError(f"{name!r}: the screen shows ASCII letters, digits and signs only")
    return name.encode("ascii")[:12]


def from_json(d):
    """the JSON's dict -> yard.ysb's bytes (version 3); raises ValueError on a bad sound or scale"""
    if d.get("format") not in (None, "mvane-yard-sounds"):
        raise ValueError(f"not mvane yard's sounds: format {d.get('format')!r}")
    raw = bytearray(SIZE)
    struct.pack_into("<IBB", raw, 0, MAGIC, 3, N)
    for i, s in enumerate(_slots(d.get("sounds", []), N, "YARD sounds")):
        if s is None:
            continue
        o, b = 8 + 32 * i, _name(s, f"USER {i + 1}")
        raw[o:o + len(b)] = b
        raw[o + 12] = 1 | (2 if s.get("mono") else 0) | (FAV if s.get("favourite") else 0)
        for j, k in enumerate(PARAMS):
            v = s.get(k, DEFAULT[k])
            if not isinstance(v, int) or not 0 <= v <= 127:
                raise ValueError(f"{b.decode()}: {k} {v!r}, not 0..127")
            raw[o + 13 + j] = v
    struct.pack_into("<IB", raw, SIZE1, CMAGIC, CN)
    clip = d.get("clipper", [])
    if clip:
        n, rev = names(), names()["reverb"]
    for i, s in enumerate(_slots(clip, CN, "Clipper sounds")):
        if s is None:
            continue
        o, b = SIZE1 + 8 + 64 * i, _name(s, f"CLIPPER {i + 1}")
        if "engine" in s and "preset" in s:
            engine, preset = s["engine"], s["preset"]
            if not any(x["engine"] == engine and x["preset"] == preset for x in n["sounds"]):
                raise ValueError(f"{b.decode()}: no Clipper sound at engine {engine}, preset {preset}")
        else:
            m = [x for x in n["sounds"] if x["name"] == str(s.get("sound", "")).upper()]
            if not m:
                raise ValueError(f"{b.decode()}: no Clipper sound named {s.get('sound')!r}")
            engine, preset = m[0]["engine"], m[0]["preset"]
        r = s.get("reverb")
        if r is None:
            rtype = -1
        elif str(r).upper() in rev["names"]:
            rtype = rev["min"] + rev["names"].index(str(r).upper())
        else:
            raise ValueError(f"{b.decode()}: reverb {r!r}, not one of {', '.join(rev['names'])}")
        ch = s.get("changes", [])
        if len(ch) > OV:
            raise ValueError(f"{b.decode()}: {len(ch)} changes, {OV} at most")
        raw[o:o + len(b)] = b
        raw[o + 12], raw[o + 13], raw[o + 14] = 1 | (FAV if s.get("favourite") else 0), engine, preset
        struct.pack_into("<bB", raw, o + 15, rtype, len(ch))
        for k, c in enumerate(ch):
            pid = c.get("id")
            macro = ["BRITE", "TIMBR", "MOTN", "SPACE"].index(str(c.get("name", "")).upper()) \
                if str(c.get("name", "")).upper() in ("BRITE", "TIMBR", "MOTN", "SPACE") else -1
            if pid is None and macro >= 0 and engine == HYBRID:   # (HYBRID's macros are its M1..M4, as the panel's)
                pid = n["edit0"] + 4 + macro
            if pid is None:                         # (by its label, when one parameter has it)
                ids = [p for p in range(n["params"]) if label(p, engine) == str(c.get("name", "")).upper()]
                if len(ids) != 1:
                    raise ValueError(f"{b.decode()}: {c.get('name')!r} names {len(ids)} parameters: give its id")
                pid = ids[0]
            v = c.get("value")
            if not isinstance(pid, int) or not 0 <= pid < n["params"]:
                raise ValueError(f"{b.decode()}: parameter {pid!r}, not 0..{n['params'] - 1}")
            if not isinstance(v, int) or not -32768 <= v <= 32767:
                raise ValueError(f"{b.decode()}: {label(pid, engine) or pid}: value {v!r}")
            struct.pack_into("<Bh", raw, o + 20 + 3 * k, pid, v)
    struct.pack_into("<IB", raw, SIZE2, SMAGIC, SN)
    for i, s in enumerate(_slots(d.get("scales", []), SN, "scales")):
        if s is None:
            continue
        o, p = SIZE2 + 8 + 16 * i, s.get("pcs")
        if not isinstance(p, int) or not 0 <= p <= 4095:
            raise ValueError(f"scale {s.get('name', '')!r}: pcs {p!r}, not 0..4095")
        b = _name(s, "")
        raw[o:o + len(b)] = b
        struct.pack_into("<H", raw, o + 12, 0x8000 | p)
    return bytes(raw)


FX_IDS = {"DRIVE": 33, "DST": 33, "CHORUS": 34, "CHO": 34, "DELAY": 35, "DLY": 35, "REVERB": 36, "REV": 36}
"""(0.17) the effects' sends by name, whatever the engine (an engine's own values may share a label: DST)"""


def _pid(name, engine):
    """a change's parameter by its label (or #id; the sends by FX_IDS), as from_json reads one"""
    n, t = names(), str(name).strip().upper()
    if t.lstrip("#").isdigit():
        return int(t.lstrip("#"))
    if t in FX_IDS:
        return FX_IDS[t]
    if t in ("BRITE", "TIMBR", "MOTN", "SPACE") and engine == HYBRID:
        return n["edit0"] + 4 + ["BRITE", "TIMBR", "MOTN", "SPACE"].index(t)
    ids = [p for p in range(n["params"]) if label(p, engine) == t]
    if len(ids) != 1:
        raise ValueError(f"{name}: {len(ids)} parameters by that name (use #id)")
    return ids[0]


def fx(d, values, reverb=None, favourites=False):
    """(0.17) the same changes on many Clipper sounds: values {label or #id: value} (CHO, REV, DLY, DST, SPACE, or
    any of the sound's parameters), each replacing that parameter's change or added; reverb, a model's name or None
    (as each has it). favourites: only those. Returns (how many changed, [(name, why not)]): a sound that would
    pass 14 changes keeps its own"""
    if reverb is not None and str(reverb).upper() not in names()["reverb"]["names"]:
        raise ValueError(f"reverb {reverb!r}, not one of {', '.join(names()['reverb']['names'])}")
    done, skipped = 0, []
    for s in d.get("clipper", []):
        if favourites and not s.get("favourite"):
            continue
        engine = s.get("engine")
        if engine is None:
            m = [x for x in names()["sounds"] if x["name"] == str(s.get("sound", "")).upper()]
            if not m:
                skipped.append((s.get("name", "?"), f"no Clipper sound named {s.get('sound')!r}"))
                continue
            engine = m[0]["engine"]
        ch = [dict(c) for c in s.get("changes", [])]
        for c in ch:                                # (each change by its id from here)
            if c.get("id") is None:
                c["id"] = _pid(c.get("name", ""), engine)
        for k, v in values.items():
            pid = _pid(k, engine)
            hit = [c for c in ch if c["id"] == pid]
            if hit:
                hit[0]["value"] = int(v)
            else:
                ch.append({"id": pid, "name": label(pid, engine), "value": int(v)})
        if len(ch) > OV:
            skipped.append((s.get("name", "?"), f"{len(ch)} changes, {OV} at most"))
            continue
        s["changes"] = ch
        if reverb is not None:
            s["reverb"] = str(reverb).upper()
        done += 1
    return done, skipped


def _read(path):
    data = open(path, "rb").read()
    return json.loads(data) if path.endswith(".json") else to_json(data)


def _write(path, d):
    if path.endswith(".json"):
        open(path, "w").write(json.dumps(d, indent=2) + "\n")
    else:
        open(path, "wb").write(from_json(d))


if __name__ == "__main__":
    # ysb.py IN.ysb|IN.json OUT.json|OUT.ysb: one to the other
    # ysb.py fx IN OUT NAME=VALUE ... [--reverb MODEL] [--favourites]: the same changes on many Clipper sounds
    args = sys.argv[1:]
    if args and args[0] == "fx":
        fav, rv, vals, rest = "--favourites" in args, None, {}, []
        args = [x for x in args[1:] if x != "--favourites"]
        while args:
            x = args.pop(0)
            if x == "--reverb":
                rv = args.pop(0)
            elif "=" in x:
                k, v = x.split("=", 1)
                vals[k] = int(v)
            else:
                rest.append(x)
        if len(rest) != 2:
            raise SystemExit("ysb.py fx IN OUT NAME=VALUE ... [--reverb MODEL] [--favourites]")
        d = _read(rest[0])
        try:
            n, skipped = fx(d, vals, rv, fav)
            _write(rest[1], d)
        except ValueError as e:
            raise SystemExit(f"ysb: {e}")
        print(f"{n} Clipper sound{'' if n == 1 else 's'} changed -> {rest[1]}")
        for name, why in skipped:
            print(f"  {name}: left as it was ({why})")
    else:
        _write(args[1], _read(args[0]))
