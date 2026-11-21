#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A take from the FM-1 (take.mvt: core/rec.h) to a standard MIDI file, and back.

  tools/mvt2mid.py take.mvt [out.mid]      MPE channels kept; 1 ms = 1 tick at 120 bpm (500 ticks a beat)
  tools/mvt2mid.py --info take.mvt         what it holds
"""
import struct, sys

MAGIC = 0x3154564D
TPB = 500  # ticks a beat: at the file's 120 bpm a tick is a millisecond

def read_take(path):
    b = open(path, "rb").read()
    magic, ver, count, dur = struct.unpack_from("<4I", b)
    if magic != MAGIC or ver != 1 or len(b) < 16 + 4 * count:
        raise SystemExit(f"{path}: not an mvane take")
    words = struct.unpack_from(f"<{count}I", b, 16)
    t, events = 0, []
    for w in words:
        st = (w >> 16) & 0xFF
        if st == 0:
            t += (w >> 24) << 16 | (w & 0xFFFF)  # a time word: the gap's top byte in dt
            continue
        t += w >> 24
        events.append((t, st, (w >> 8) & 0x7F, w & 0x7F))
    return events, dur

def vlq(n):
    out = [n & 0x7F]
    while n > 0x7F:
        n >>= 7
        out.insert(0, 0x80 | (n & 0x7F))
    return bytes(out)

def write_mid(events, dur, path):
    trk, last = bytearray(b"\x00\xff\x51\x03\x07\xa1\x20"), 0  # tempo 120 bpm
    for t, st, d1, d2 in events:
        trk += vlq(t - last) + bytes([st, d1] if st & 0xF0 in (0xC0, 0xD0) else [st, d1, d2])
        last = t
    trk += vlq(max(0, dur - last)) + b"\xff\x2f\x00"
    open(path, "wb").write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPB) + b"MTrk" + struct.pack(">I", len(trk)) + trk)

def main(argv):
    if len(argv) >= 2 and argv[0] == "--info":
        ev, dur = read_take(argv[1])
        notes = sum(1 for e in ev if e[1] & 0xF0 == 0x90 and e[3])
        chans = sorted({e[1] & 15 for e in ev})
        print(f"{argv[1]}: {dur / 1000:.1f} s, {len(ev)} events, {notes} notes, channels {[c + 1 for c in chans]}")
        return
    if not argv:
        raise SystemExit(__doc__)
    out = argv[1] if len(argv) > 1 else argv[0].rsplit(".", 1)[0] + ".mid"
    ev, dur = read_take(argv[0])
    write_mid(ev, dur, out)
    print(f"{out}: {len(ev)} events, {dur / 1000:.1f} s")

if __name__ == "__main__":
    main(sys.argv[1:])
