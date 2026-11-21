#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A take from the FM-1 (take.mvt: core/rec.h) to a standard MIDI file, and back.

  tools/mvt2mid.py take.mvt [out.mid]      one channel, one clip in a DAW: each note's pressure as poly aftertouch
  tools/mvt2mid.py --mpe take.mvt [out.mid]   the MPE channels kept (a DAW may split them into a clip each)
                                           1 ms = 1 tick at 120 bpm (500 ticks a beat)
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

def flatten(events):
    """MPE on 16 channels -> one channel: notes on channel 1 (a note held twice sounds once), a member channel's
    pressure as poly aftertouch on its notes, its slide (CC74) and bend as channel 1's; the MPE set-up dropped"""
    held = {c: set() for c in range(16)}   # notes on each channel
    count = {}                              # how many channels hold a note
    out = []
    for t, st, d1, d2 in events:
        kind, ch = st & 0xF0, st & 15
        if kind == 0x90 and d2:
            held[ch].add(d1)
            count[d1] = count.get(d1, 0) + 1
            if count[d1] == 1:
                out.append((t, 0x90, d1, d2))
        elif kind in (0x80, 0x90):
            if d1 in held[ch]:
                held[ch].discard(d1)
                count[d1] -= 1
                if count[d1] == 0:
                    out.append((t, 0x80, d1, 0))
        elif kind == 0xD0:
            if ch == 0:
                out.append((t, 0xD0, d1, 0))
            else:
                out += [(t, 0xA0, n, d1) for n in sorted(held[ch])]
        elif kind == 0xA0:
            out.append((t, 0xA0, d1, d2))
        elif kind == 0xB0:
            if d1 not in (100, 101, 6, 38):  # the MPE configuration and bend ranges
                out.append((t, 0xB0, d1, d2))
        elif kind == 0xE0:
            out.append((t, 0xE0, d1, d2))
    return out


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
    mpe = "--mpe" in argv
    argv = [a for a in argv if a != "--mpe"]
    if not argv:
        raise SystemExit(__doc__)
    out = argv[1] if len(argv) > 1 else argv[0].rsplit(".", 1)[0] + ".mid"
    ev, dur = read_take(argv[0])
    write_mid(ev if mpe else flatten(ev), dur, out)
    print(f"{out}: {len(ev)} events, {dur / 1000:.1f} s")

if __name__ == "__main__":
    main(sys.argv[1:])
