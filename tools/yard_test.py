#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/yard.py against the host simulator: play a phrase, REC and SAVE, fetch the take as MIDI, send a MIDI file
back, and check that PLAY plays what was sent. ./build.sh --host first."""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import yard  # noqa: E402

HOST = os.path.join(HERE, "..", "app", "build", "host", "mvane-yard_host")
REC, SAVE, PLAY = 11, 7, 10   # plat.h B_* bits


def tap(link, bit):
    link.cmd(f"buttons {1 << bit}")
    link.run(60)
    link.cmd("buttons 0")
    link.run(60)


def main():
    link = yard.fc.ServeLink([HOST, "--serve"])
    d = tempfile.mkdtemp()
    try:
        link.run(700)
        for k in (0, 9, 2, 7):               # four chords, legato
            link.cmd(f"key {k} 100")
            link.run(400)
        for k in (0, 9, 2, 7):
            link.cmd(f"key {k} 0")
        link.run(300)
        tap(link, REC)
        tap(link, SAVE)
        link.run(300)
        mid = os.path.join(d, "take.mid")
        assert yard.main(["take", mid, "--mvt", "--mpe"], link) == 0
        ev, dur = yard.mvt2mid.read_take(mid[:-4] + ".mvt")
        notes = [e for e in ev if e[1] & 0xF0 == 0x90 and e[3]]
        assert len(notes) >= 12, notes
        assert open(mid, "rb").read(4) == b"MThd"
        back = yard.read_mid(mid)                  # the MIDI file reads back as the same events
        assert [e[1:] for e in back] == [e[1:] for e in ev], "MIDI round trip"
        assert all(abs(a[0] - b[0]) <= 1 for a, b in zip(back, ev))
        flat = os.path.join(d, "flat.mid")
        assert yard.main(["take", flat], link) == 0
        fev = yard.read_mid(flat)
        assert {e[1] & 15 for e in fev} == {0}, "one channel"
        assert sum(1 for e in fev if e[1] & 0xF0 == 0x90 and e[3]) <= len(notes), "a note held twice sounds once"
        # a MIDI file of our own to the FM-1: three notes; PLAY plays them
        trk = b"".join([b"\x00\x90\x3c\x64", b"\x83\x60\x80\x3c\x00", b"\x00\x90\x40\x64", b"\x83\x60\x80\x40\x00",
                        b"\x00\x90\x43\x64", b"\x83\x60\x80\x43\x00", b"\x00\xff\x2f\x00"])
        own = os.path.join(d, "three.mid")
        open(own, "wb").write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(trk)) + trk)
        assert yard.main(["put", own], link) == 0
        link.run(100)
        r = link.cmd("expect take 9"); assert r[-1].strip().startswith("ok"), r   # six events, three time words (slot 1: the current)
        r = link.cmd("expect take_ms 1500"); assert r[-1].strip().startswith("ok"), r   # the last note-off at 1.5 s
        link.cmd(f"buttons {1 << PLAY}")
        link.run(100)
        assert link.cmd("expect voices 1")[-1].strip().startswith("ok"), "PLAY plays the first note"
        link.cmd("buttons 0")
        link.cmd("buttons 0")
        link.run(60)
        tap(link, PLAY)                                                    # stop: no files while a take plays
        assert yard.main(["put", own, "--slot", "5"], link) == 0          # another slot
        assert yard.main(["takes", os.path.join(d, "all")], link) == 0
        assert sorted(os.listdir(os.path.join(d, "all"))) == ["take1.mid", "take5.mid"]
        assert yard.main(["settings", os.path.join(d, "settings.bin")], link) == 0
        assert os.path.getsize(os.path.join(d, "settings.bin")) > 0
        print("yard: all checks passed (take out as MIDI, a MIDI file in, settings out)")
    finally:
        link.close()


if __name__ == "__main__":
    main()
