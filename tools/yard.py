#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""mvane yard's files over USB, through fm1-base's companion services (base/docs/COMPANION.md).

    tools/yard.py take [OUT.mid]          slot 1's take (--slot N for another) as a MIDI file (default takeN.mid): one channel, so a DAW
                                          makes one clip of it (pressure per note as poly aftertouch); --mpe keeps the
                                          MPE channels instead; --mvt keeps the .mvt too
    tools/yard.py takes [DIR]             every slot's take as MIDI files (takeN.mid), into DIR (default .)
    tools/yard.py put FILE                a take into slot 1 (--slot N): a .mvt, or a MIDI file (its notes, controllers, pressure
                                          and bend, all tracks merged; at most 30,000 events)
    tools/yard.py settings [OUT]          the settings as a file (default settings.bin); --put FILE sends one back
    tools/yard.py sounds [OUT.json]       your own sounds, YARD's and Clipper's (FX and SAVE keep them on the FM-1) as JSON (default
                                          yard-sounds.json; .ysb: the bank as it is); --put FILE (.json or .ysb) sends
                                          them, replacing the FM-1's; --fx "CHO=4,REV=8,SPACE=12" (0.17) sets those
                                          on every Clipper sound of yours on the FM-1 (--favourites: only those;
                                          --reverb MODEL as well), and sends them back
    tools/yard.py ls                      what the FM-1 has

Options: --port NAME (part of the FM-1's MIDI port name), --serve-cmd CMD (a host simulator instead of an FM-1:
"app/build/host/mvane-yard_host --serve"). The FM-1 keeps eight takes (SEQ and HOME pick the slot, SAVE keeps
the take there). Needs mido with
python-rtmidi for an FM-1. Stop PLAY first: the FM-1 refuses files while a take plays.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "base", "tools"))
sys.path.insert(0, HERE)
import fm1_companion as fc  # noqa: E402
import json  # noqa: E402
import mvt2mid  # noqa: E402
import ysb  # noqa: E402

SETTINGS, SLOTS, SOUNDS = "settings", 8, "yard.ysb"


def take_name(slot):
    return f"take{slot}.mvt"
MAX_WORDS = 30000


def connect(a, link=None):
    link = link or (fc.ServeLink(a.serve_cmd) if a.serve_cmd else fc.find(None, a.port))
    c = fc.Client(link)
    c.hello()
    return c, link


def read_file(c, name):
    return bytes(c.read(c.index_of(name)["index"]))


# ------------------------------------------------------------ MIDI file -> take ---
def read_mid(path):
    """a standard MIDI file -> [(ms, status, d1, d2)], all tracks merged, tempo changes honoured"""
    b = open(path, "rb").read()
    if b[:4] != b"MThd":
        raise SystemExit(f"{path}: not a MIDI file")
    _, fmt, ntrk, div = struct.unpack(">IHHH", b[4:14])
    if div & 0x8000:
        raise SystemExit(f"{path}: SMPTE time is not supported")
    pos, tracks = 8 + struct.unpack(">I", b[4:8])[0], []
    for _ in range(ntrk):
        if b[pos:pos + 4] != b"MTrk":
            break
        n = struct.unpack(">I", b[pos + 4:pos + 8])[0]
        tracks.append(b[pos + 8:pos + 8 + n])
        pos += 8 + n
    events, tempos = [], [(0, 500000)]
    for trk in tracks:
        i = tick = 0
        run = 0
        while i < len(trk):
            d = 0
            while True:
                x = trk[i]; i += 1
                d = d << 7 | (x & 0x7F)
                if x < 0x80:
                    break
            tick += d
            st = trk[i]
            if st < 0x80:
                st = run
            else:
                i += 1
            if st == 0xFF:
                typ = trk[i]; i += 1
                ln = 0
                while True:
                    x = trk[i]; i += 1
                    ln = ln << 7 | (x & 0x7F)
                    if x < 0x80:
                        break
                if typ == 0x51:
                    tempos.append((tick, int.from_bytes(trk[i:i + 3], "big")))
                i += ln
                continue
            if st in (0xF0, 0xF7):
                ln = 0
                while True:
                    x = trk[i]; i += 1
                    ln = ln << 7 | (x & 0x7F)
                    if x < 0x80:
                        break
                i += ln
                continue
            run = st
            k = st & 0xF0
            if k in (0xC0, 0xD0):
                events.append((tick, st, trk[i], 0)); i += 1
            else:
                events.append((tick, st, trk[i], trk[i + 1])); i += 2
    tempos.sort()

    def ms(t):  # ticks -> ms through the tempo map
        out, last_t, us = 0.0, 0, 500000
        for tt, u in tempos:
            if tt >= t:
                break
            out += (tt - last_t) * us / div / 1000.0
            last_t, us = tt, u
        return out + (t - last_t) * us / div / 1000.0

    return sorted(((round(ms(t)), st, d1, d2) for t, st, d1, d2 in events if st & 0xF0 != 0xC0),
                  key=lambda e: e[0])


def take_words(events):
    """[(ms, status, d1, d2)] -> core/rec.h words: dt in the top byte, time words for gaps of 255 ms and more"""
    words, last = [], 0
    for t, st, d1, d2 in events:
        gap = max(0, t - last)
        last = t
        while gap >= 255:
            g = min(gap, 0xFFFFFF)
            words.append((g >> 16) << 24 | (g & 0xFFFF))
            gap -= g
        words.append(gap << 24 | st << 16 | (d1 & 0x7F) << 8 | (d2 & 0x7F))
    return words, last


def take_file(words, duration):
    if len(words) > MAX_WORDS:
        raise SystemExit(f"{len(words)} events: the FM-1 keeps {MAX_WORDS} at most")
    return struct.pack(f"<4I{len(words)}I", 0x3154564D, 1, len(words), duration, *words)


# ------------------------------------------------------------------- commands ---
def main(argv=None, link=None):
    """link: an open link to use (a test's simulator), left open"""
    own = link is None
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                epilog=__doc__.split("\n", 2)[2])
    p.add_argument("cmd", choices=["take", "takes", "put", "settings", "sounds", "ls"])
    p.add_argument("--slot", type=int, default=1, choices=range(1, 9), metavar="1..8")
    p.add_argument("file", nargs="?")
    p.add_argument("--mvt", action="store_true", help="take: keep the .mvt beside the MIDI file")
    p.add_argument("--mpe", action="store_true", help="take: the MPE channels kept (a DAW may split them into clips)")
    p.add_argument("--put", metavar="FILE", help="settings, sounds: send this file to the FM-1")
    p.add_argument("--fx", metavar="NAME=V,...", help="sounds: these changes on your Clipper sounds, sent back")
    p.add_argument("--reverb", metavar="MODEL", help="sounds --fx: the reverb's model too (ROOM PLATE HALL SHIMR)")
    p.add_argument("--favourites", action="store_true", help="sounds --fx: only your favourites")
    p.add_argument("--port")
    p.add_argument("--serve-cmd")
    a = p.parse_args(argv)
    try:
        c, link = connect(a, link)
    except fc.CompanionError as e:
        print(f"yard: {e}", file=sys.stderr)
        return fc.EXIT.get(e.code, 1)
    try:
        if a.cmd == "ls":
            for f in c.files():
                print(f"{f['name']:<12} {f['size']:>7} B  (room {f['room']})")
        elif a.cmd in ("take", "takes"):
            slots = [a.slot] if a.cmd == "take" else range(1, SLOTS + 1)
            got = 0
            for slot in slots:
                data = read_file(c, take_name(slot)) if c.index_of(take_name(slot))["size"] >= 16 else b""
                if len(data) < 16:
                    if a.cmd == "take":
                        print(f"yard: slot {slot} is empty (REC, then SAVE)", file=sys.stderr)
                        return 6
                    continue
                if a.cmd == "take":
                    out = a.file or f"take{slot}.mid"
                else:
                    os.makedirs(a.file or ".", exist_ok=True)
                    out = os.path.join(a.file or ".", f"take{slot}.mid")
                mvt = out.rsplit(".", 1)[0] + ".mvt"
                open(mvt, "wb").write(data)
                ev, dur = mvt2mid.read_take(mvt)
                mvt2mid.write_mid(ev if a.mpe else mvt2mid.flatten(ev), dur, out)
                if not a.mvt:
                    os.remove(mvt)
                notes = sum(1 for e in ev if e[1] & 0xF0 == 0x90 and e[3])
                print(f"{out}: {dur / 1000:.1f} s, {notes} notes, {len(ev)} events" + (f" (and {mvt})" if a.mvt else ""))
                got += 1
            if a.cmd == "takes" and not got:
                print("yard: every slot is empty", file=sys.stderr)
        elif a.cmd == "put":
            if not a.file:
                raise SystemExit("yard put FILE")
            raw = open(a.file, "rb").read()
            if raw[:4] == b"MThd":
                ev = read_mid(a.file)
                words, last = take_words(ev)
                raw = take_file(words, last)
            elif raw[:4] != struct.pack("<I", 0x3154564D):
                raise SystemExit(f"{a.file}: neither a take (.mvt) nor a MIDI file")
            c.write(c.index_of(take_name(a.slot))["index"], raw)
            print(f"{a.file} -> slot {a.slot} ({len(raw)} B): PLAY plays it there")
        elif a.cmd == "settings":
            i = c.index_of(SETTINGS)["index"]
            if a.put:
                c.write(i, open(a.put, "rb").read())
                print(f"{a.put} -> the FM-1's settings")
            else:
                out = a.file or "settings.bin"
                open(out, "wb").write(bytes(c.read(i)))
                print(f"settings -> {out}")
        elif a.cmd == "sounds":
            i = c.index_of(SOUNDS)["index"]
            if a.fx:                         # (0.17) the same changes on many sounds: read, changed, sent back
                try:
                    vals = {k.strip(): int(v) for k, v in (x.split("=", 1) for x in a.fx.split(",") if x.strip())}
                    j = ysb.to_json(bytes(c.read(i)))
                    n, skipped = ysb.fx(j, vals, a.reverb, a.favourites)
                    raw = ysb.from_json(j)
                except ValueError as e:
                    raise SystemExit(f"--fx: {e}")
                c.write(i, raw)
                print(f"{n} Clipper sound{'' if n == 1 else 's'} changed on the FM-1 ({a.fx})")
                for name, why in skipped:
                    print(f"  {name}: left as it was ({why})")
            elif a.put:
                data = open(a.put, "rb").read()
                try:
                    raw = data if a.put.endswith(".ysb") else ysb.from_json(json.loads(data))
                except ValueError as e:
                    raise SystemExit(f"{a.put}: {e}")
                c.write(i, raw)
                j = ysb.to_json(raw)
                print(f"{a.put} -> the FM-1's own sounds ({len(j['sounds'])} YARD, {len(j['clipper'])} Clipper)")
            else:
                out = a.file or "yard-sounds.json"
                raw = bytes(c.read(i))
                if out.endswith(".ysb"):
                    open(out, "wb").write(raw)
                else:
                    open(out, "w").write(json.dumps(ysb.to_json(raw), indent=2) + "\n")
                j = ysb.to_json(raw)
                print(f"{len(j['sounds'])} YARD and {len(j['clipper'])} Clipper sounds -> {out}")
    except fc.CompanionError as e:
        print(f"yard: {e}", file=sys.stderr)
        return fc.EXIT.get(e.code, 1)
    finally:
        close = getattr(link, "close", None)
        if close and own:
            close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
