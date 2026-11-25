#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/yard.py against the host simulator: play a phrase, REC and SAVE, fetch the take as MIDI, send a MIDI file
back, and check that PLAY plays what was sent. ./build.sh --host first."""
import json
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import yard
import ysb  # noqa: E402

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
        assert yard.main(["put", own, "--slot", "5"], link) == 0          # another slot: it becomes the current one
        link.run(50)
        r = link.cmd("expect slot 4"); assert r[-1].strip().startswith("ok"), r
        assert yard.main(["takes", os.path.join(d, "all")], link) == 0
        assert sorted(os.listdir(os.path.join(d, "all"))) == ["take1.mid", "take5.mid"]
        assert yard.main(["settings", os.path.join(d, "settings.bin")], link) == 0
        assert os.path.getsize(os.path.join(d, "settings.bin")) > 0
        # the user's YARD sounds: none yet; two sent as JSON (one without a slot: the first free); back the same
        assert yard.main(["sounds", os.path.join(d, "none.json")], link) == 0
        assert json.load(open(os.path.join(d, "none.json")))["sounds"] == []
        mine = {"format": "mvane-yard-sounds", "version": 1, "sounds": [
            {"slot": 3, "name": "Glass Keys", "mono": False, "tone": 80, "release": 70, "glide": 30, "attack": 2,
             "decay": 50, "res": 30, "detune": 40, "sub": 0, "pluck": 90},
            {"name": "REED", "mono": True, "tone": 45, "res": 10}],
            "clipper": [{"name": "Glassy", "sound": "GLASS PAD", "reverb": "hall",
                         "changes": [{"name": "BRITE", "value": 20}, {"id": 34, "value": 6}]}]}
        json.dump(mine, open(os.path.join(d, "mine.json"), "w"))
        assert yard.main(["sounds", "--put", os.path.join(d, "mine.json")], link) == 0
        link.run(50)
        r = link.cmd("expect nyuser 2"); assert r[-1].strip().startswith("ok"), r
        r = link.cmd("expect ncuser 1"); assert r[-1].strip().startswith("ok"), r
        assert yard.main(["sounds", os.path.join(d, "back.json")], link) == 0
        back = json.load(open(os.path.join(d, "back.json")))["sounds"]
        assert [(x["slot"], x["name"], x["mono"], x["pluck"]) for x in back] == [(1, "REED", True, 0),
                                                                                 (3, "GLASS KEYS", False, 90)], back
        assert back[0]["release"] == ysb.DEFAULT["release"]
        clip = json.load(open(os.path.join(d, "back.json")))["clipper"]
        assert [(c["slot"], c["name"], c["sound"], c["reverb"]) for c in clip] == [(1, "GLASSY", "GLASS PAD", "HALL")], clip
        assert [(c["id"], c["name"], c["value"]) for c in clip[0]["changes"]] == [(122, "M1", 20), (34, "CHO", 6)], clip
        # (0.17) a favourite kept both ways; the same FX on every Clipper sound at once (CHO replaced, REV and SPACE added)
        mine["clipper"][0]["favourite"] = True
        json.dump(mine, open(os.path.join(d, "mine.json"), "w"))
        assert yard.main(["sounds", "--put", os.path.join(d, "mine.json")], link) == 0
        link.run(50)
        assert yard.main(["sounds", "--fx", "CHO=4,REV=8,DLY=2", "--reverb", "plate"], link) == 0
        assert yard.main(["sounds", os.path.join(d, "fx.json")], link) == 0
        clip = json.load(open(os.path.join(d, "fx.json")))["clipper"]
        assert clip[0]["favourite"] and clip[0]["reverb"] == "PLATE", clip
        assert [(c["name"], c["value"]) for c in clip[0]["changes"]] == [("M1", 20), ("CHO", 4), ("REV", 8), ("DLY", 2)], clip
        r = link.cmd("expect ncuser 1"); assert r[-1].strip().startswith("ok"), r
        print("yard: all checks passed (take out as MIDI, a MIDI file in, settings out, YARD and Clipper sounds in and out)")
    finally:
        link.close()


if __name__ == "__main__":
    main()
