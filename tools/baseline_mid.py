#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The baseline phrase (docs/OPTIMIZATION.md): a fixed MPE MIDI file to play into the FM-1 from a DAW while reading
CPU, XR and Q on its screen, so that readings before and after a change compare. 30 seconds at 120 BPM, three parts:

  0-10 s   chords: six notes, each on its own MPE member channel (2-7), held two seconds, no expression
  10-20 s  the same chords with dense expression: each note's bend, pressure and slide (CC74) every 10 ms
  20-30 s  a burst: sixteenth-note arpeggios over a held chord, expression on eight channels every 20 ms, then 600 pressure
           messages at one instant (the render's queue at its busiest)

Usage: tools/baseline_mid.py [out.mid]   (default tests/baseline.mid)
"""
import math
import struct
import sys

TPQ = 480                      # ticks per quarter note; 120 BPM: 960 ticks a second
TPS = 960

CHORDS = [                     # C major's I vi ii V, voiced around middle C (bass first)
    [36, 55, 60, 64, 67, 71],
    [45, 57, 60, 64, 69, 72],
    [38, 57, 60, 62, 65, 69],
    [43, 55, 59, 62, 65, 71],
    [36, 55, 60, 64, 67, 72],
]


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append(0x80 | (n & 0x7F))
        n >>= 7
    return bytes(reversed(out))


def build():
    ev = []                    # (tick, order, bytes): order puts note-offs before note-ons at the same tick

    def at(sec, msg, order=1):
        ev.append((int(round(sec * TPS)), order, bytes(msg)))

    # MPE configuration: lower zone, 15 members (RPN 6 on the manager channel); members' bend range 48
    for b in ([0xB0, 101, 0], [0xB0, 100, 6], [0xB0, 6, 15]):
        at(0, b, 0)
    for ch in range(1, 16):
        for b in ([0xB0 | ch, 101, 0], [0xB0 | ch, 100, 0], [0xB0 | ch, 6, 48]):
            at(0, b, 0)

    def chord(t0, dur, notes, vel=96):
        for i, n in enumerate(notes):
            ch = 1 + i
            at(t0, [0x90 | ch, n, vel])
            at(t0 + dur, [0x80 | ch, n, 0], 0)

    # part 1: chords, no expression
    for k in range(5):
        chord(0.5 + 2 * k, 1.9, CHORDS[k])

    # part 2: the same, with bend, pressure and slide on every note every 10 ms
    for k in range(5):
        t0 = 10.0 + 2 * k
        chord(t0, 1.9, CHORDS[k])
        for s in range(190):
            t = t0 + 0.01 * s
            for i in range(6):
                ch = 1 + i
                ph = 2 * math.pi * (s / 50.0 + i / 6.0)
                bend = 8192 + int(170 * math.sin(ph))            # about +-1 semitone at a 48 range
                at(t, [0xE0 | ch, bend & 0x7F, bend >> 7])
                at(t, [0xD0 | ch, 40 + int(40 * (1 + math.sin(ph / 2)))])
                at(t, [0xB0 | ch, 74, 64 + int(50 * math.sin(ph / 3))])

    # part 3: a held chord, sixteenth-note arpeggios (8 a second) on top with expression, then a flood
    chord(20.0, 8.0, CHORDS[0][:4], 80)
    arp = [72, 76, 79, 84, 83, 79, 76, 74]
    for s in range(64):
        t = 20.0 + s * 0.125
        ch = 5 + s % 4
        n = arp[s % len(arp)]
        at(t, [0x90 | ch, n, 100])
        at(t + 0.11, [0x80 | ch, n, 0], 0)
        for q in range(6):                                       # every 20 ms (the file within 30,000 events)
            tq = t + q * 0.02
            for c in range(1, 9):
                ph = 2 * math.pi * (tq * 3 + c / 8.0)
                at(tq, [0xD0 | c, 64 + int(60 * math.sin(ph))])
                at(tq, [0xB0 | c, 74, 64 + int(60 * math.cos(ph))])
    for q in range(600):                                          # 600 pressure messages at one instant
        at(28.5, [0xD1, q % 128])
    at(29.5, [0xB1, 123, 0])
    for ch in range(1, 16):
        at(30.0, [0xB0 | ch, 123, 0])

    ev.sort(key=lambda e: (e[0], e[1]))
    trk = bytearray()
    trk += b'\x00\xff\x51\x03' + (500000).to_bytes(3, 'big')     # 120 BPM
    trk += b'\x00\xff\x03' + vlq(len(b'mvane yard baseline')) + b'mvane yard baseline'
    last = 0
    for tick, _, msg in ev:
        trk += vlq(tick - last) + msg
        last = tick
    trk += b'\x00\xff\x2f\x00'
    return b'MThd' + struct.pack('>IHHH', 6, 0, 1, TPQ) + b'MTrk' + struct.pack('>I', len(trk)) + bytes(trk), len(ev)


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'tests/baseline.mid'
    data, n = build()
    with open(out, 'wb') as f:
        f.write(data)
    print(f'{out}: {n} events, {len(data)} bytes, 30 s')
