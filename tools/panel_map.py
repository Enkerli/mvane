#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""mvane yard's controls on one picture: docs/media/panel-map.svg (the manual and the page show it).

The FM-1's panel as fm1-base's emulator lays it out (web/emu/index.html fm1Panel), each control named by what it does
in mvane yard; below it the two knob pages, the layers (a button held, or tapped open), and the usual errands. Made
for Alex's question (2026-11-23): "it's unclear to me how to move between global settings, how to switch the USB audio
and go through poly+br". Run after a control changes:  python3 tools/panel_map.py
"""
import os
from xml.sax.saxutils import escape

W = 1000
BODY, TRAY, KEYW, KEYB, BTN, INK, DIM = "#3a3d42", "#26282c", "#d9dadc", "#c9cacd", "#2e3135", "#f1f1f3", "#a9adb4"
TEAL, CORAL, PAPER, TXT, LINE, SOFT = "#2f8f78", "#e07a4f", "#fbfaf7", "#1d1f22", "#d6d3cc", "#5a5f66"
FONT = "font-family=\"'Barlow Semi Condensed', 'Arial Narrow', Arial, sans-serif\""
out = []


def t(x, y, s, size=13, fill=TXT, weight=500, anchor="start", italic=False):
    st = ' font-style="italic"' if italic else ""
    out.append(f'<text x="{x}" y="{y}" {FONT} font-size="{size}" font-weight="{weight}" fill="{fill}" '
               f'text-anchor="{anchor}"{st}>{escape(s)}</text>')


def r(x, y, w, h, fill, rx=8, stroke=None):
    s = f' stroke="{stroke}" stroke-width="1.2"' if stroke else ""
    out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"{s}/>')


def knob(x, y, rad, name, role):
    out.append(f'<circle cx="{x}" cy="{y}" r="{rad + 5}" fill="{TRAY}"/><circle cx="{x}" cy="{y}" r="{rad}" fill="#1f2124"/>')
    t(x, y - rad - 12, name, 12, DIM, 600, "middle")
    for i, line in enumerate(role):
        t(x, y + rad + 20 + i * 15, line, 13, INK if i == 0 else DIM, 700 if i == 0 else 500, "middle")


def button(x, y, w, h, name, role, accent=None):
    r(x, y, w, h, BTN, 9, "#4a4d52")
    t(x + w / 2, y + 17, name, 12, accent or INK, 700, "middle")
    for i, line in enumerate(role):
        t(x + w / 2, y + 31 + i * 11, line, 9.5, DIM, 500, "middle")


# ---- the panel (the emulator's coordinates, a little taller for the words) ----
PH = 700
r(4, 4, W - 8, PH - 8, BODY, 44)
knob(105, 100, 24, "MASTER", ["volume"])
knob(215, 100, 24, "SELECT", ["what the", "layer lists"])
knob(105, 215, 24, "PRESETS", ["the sound", "fast: family"])
knob(215, 215, 24, "ALGORITHM", ["NOTES / CHORDS"])
r(58, 290, 210, 58, TRAY, 14)
button(72, 300, 82, 38, "OCT-", ["octave"])
button(170, 300, 82, 38, "OCT+", ["both: PANIC"])
r(300, 44, 236, 236, "#111214", 26)
t(418, 120, "the screen", 15, DIM, 600, "middle")
t(418, 142, "names the four knobs", 12, DIM, 500, "middle")
t(418, 158, "above them, and says", 12, DIM, 500, "middle")
t(418, 174, "what just changed", 12, DIM, 500, "middle")
t(418, 214, "a layer open: its name", 12, CORAL, 600, "middle")
t(418, 230, "and its four knobs", 12, CORAL, 600, "middle")
for i in range(4):
    knob(610 + i * 104, 92, 22, f"K{i + 1}", ["as the", "screen says"] if i == 0 else [])
r(568, 160, 404, 196, TRAY, 18)
ROW1 = [("FX", ["sends /", "+SAVE: keep"]), ("SEL", ["SCALE", "layer"]), ("ENV", ["(unused)"]), ("LFO", ["(unused)"]),
        ("EDIT", ["CHORD /", "SOUND"]), ("GLO", ["SETTINGS /", "MORE"])]
ROW2 = [("HOME", ["PLAY / SOUND", "page"]), ("SAVE", ["settings +", "recording"]), ("ARP", ["tap on/off", "hold: layer"]),
        ("SEQ", ["RECORD-", "INGS layer"]), ("PLAY", ["play / stop"]), ("REC", ["tap: phrase", "hold: all"])]
LAYER = {"FX", "SEL", "EDIT", "GLO", "SEQ", "ARP"}
for i, (n, role) in enumerate(ROW1):
    button(578 + i * 65, 172, 58, 82, n, role, CORAL if n in LAYER else (DIM if n in ("ENV", "LFO") else None))
for i, (n, role) in enumerate(ROW2):
    button(578 + i * 65, 264, 58, 82, n, role, CORAL if n in LAYER else (TEAL if n == "HOME" else None))
t(770, 376, "coral: a layer button (hold it and turn, or tap to open, tap to close)", 11.5, CORAL, 600, "middle")
t(770, 392, "teal: HOME, the knobs' page; lit on SOUND", 11.5, "#7fd1bf", 600, "middle")

# the keyboard: 16 white, 11 black
X0, Y0, KW = 40, 420, 920
r(X0, Y0, KW, 260, TRAY, 18)
pitch = (KW - 24) / 16
NAMES = ["F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5"]
for i in range(16):
    x = X0 + 12 + i * pitch + 3
    out.append(f'<rect x="{x:.1f}" y="{Y0 + 120}" width="{pitch - 6:.1f}" height="128" rx="12" fill="{KEYW}"/>')
    t(x + (pitch - 6) / 2, Y0 + 238, NAMES[i], 10, SOFT, 600, "middle")
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
QUAL = ["M6", "M7", "M9", "S2", "S4", "m6", "m7", "m9", "M", "m", "HOLD"]
for j, a in enumerate(BLACK_AFTER):
    x = X0 + 12 + (a + 1) * pitch - (pitch - 10) / 2
    out.append(f'<rect x="{x:.1f}" y="{Y0 + 16}" width="{pitch - 10:.1f}" height="92" rx="12" fill="{KEYB}"/>')
    t(x + (pitch - 10) / 2, Y0 + 70, QUAL[j], 13, CORAL if QUAL[j] == "HOLD" else TXT, 700, "middle")
# ---- the pages and the layers ----
y = PH + 30
r(4, PH + 6, W - 8, 10, PAPER, 0)


def section(title, sub=None):
    global y
    t(24, y + 20, title, 20, TXT, 700)
    if sub:
        t(24, y + 40, sub, 13, SOFT)
    y += 56 if sub else 38


COLS = [24, 190, 330, 470, 610, 750]   # the row's name, K1..K4, SELECT


def head():
    global y
    for c, s in zip(COLS, ["", "K1", "K2", "K3", "K4", "SELECT"]):
        t(c, y, s, 12, SOFT, 700)
    y += 10


def row(name, cells, sub=None, accent=TEAL, h=None):
    global y
    lines = max(len(c) for c in cells)
    hh = h or (12 + 16 * max(lines, 2 if sub else 1))
    r(16, y, W - 32, hh, "#ffffff", 8, LINE)
    t(COLS[0] + 4, y + 20, name, 14, accent, 700)
    if sub:
        t(COLS[0] + 4, y + 36, sub, 11.5, SOFT)
    for c, cell in zip(COLS[1:], cells):
        for i, line in enumerate(cell):
            t(c, y + 20 + i * 16, line, 12.5, TXT if i == 0 else SOFT, 600 if i == 0 else 500)
    y += hh + 6


section("The keys")
r(16, y, W - 32, 78, "#ffffff", 8, LINE)
t(30, y + 20, "White keys", 14, TEAL, 700)
t(170, y + 20, "NOTES: notes, each on its own MPE channel with its own pressure.", 12.5, TXT)
t(170, y + 37, "CHORDS: the scale's degrees, climbing the keyboard; higher keys, higher voicings.", 12.5, TXT)
t(170, y + 54, "Pressure: F3 to C5, each key its own (how hard, not where); the top row, C#5 to G5, has velocity only.", 12.5, SOFT)
t(170, y + 70, "In CHORDS the chord key's pressure moves every voice (channel 1).", 12.5, SOFT)
y += 84
r(16, y, W - 32, 62, "#ffffff", 8, LINE)
t(30, y + 20, "Black keys", 14, TEAL, 700)
t(170, y + 20, "CHORDS: the chord's quality, latched (shown: the qualities bank; EDIT's BANK: types from the scale).", 12.5, TXT)
t(170, y + 37, "Several pressed together combine (M7 + S4); the only latched one tapped again: single notes. NOTES: notes.", 12.5, SOFT)
t(170, y + 54, "F#5 is HOLD: a chord keeps sounding after you let go; the next white key changes it.", 12.5, SOFT)
y += 74

section("The knobs' two pages", "HOME switches them (lit: SOUND). Every turn on them is recorded with what you play.")
head()
row("PLAY page", [["QUANT", "OFF BASIC FULL"], ["KEY"], ["SCALE", "the named list"], ["GLIDE"], ["the next family's", "first sound"]])
row("SOUND page", [["TONE / BRITE"], ["ATTACK / TIMBR"], ["DECAY / MOTN"], ["RELEASE / SPACE"], ["as on PLAY"]],
    "YARD sound / Clipper's")

section("Layers: hold a button and turn, or tap it to open (tap again to close)",
        "GLO and EDIT follow the page: with one open, HOME flips it to its other half. The screen names the layer.")
head()
row("GLO, PLAY page", [["PLAY", "POLY MONO", "POLY+BR MONO+BR"], ["USB AUDIO", "MASTER: as MASTER", "FULL: full level"],
                       ["MPE OUT", "ON OFF"], ["BEND", "12 24 48 96"], ["USB: right, host", "a MIDI device;", "left, a computer"]],
    "SETTINGS", CORAL)
row("GLO, SOUND page", [["PHONES MAX", "100% to 10%"], ["SPREAD", "MPE glide"], [], [], []], "MORE", CORAL)
row("EDIT, PLAY page", [["VOICING", "CLOSE OPEN", "WIDE PAIRS"], ["VOICES", "3 to 6"], ["BASS", "ON OFF"],
                        ["BANK", "QUALITY / SCALE"], ["the chord type"]], "CHORD", CORAL)
row("EDIT, SOUND page", [["DETUNE / WAVE", "/ ONSET"], ["SUB / PW", "/ ONSET LVL"], ["RES / NOISE", "/ LEVEL"],
                         ["PLUCK / FILTER", "/ -"], ["the next page", "(three); SAVE:", "keep the sound"]],
    "a YARD sound", CORAL)
row("", [["engine 1-4 / 5-8", "/ ATTACK"], ["... / DECAY"], ["... / SUSTAIN"], ["... / RELEASE"], ["the next page", "SAVE: keep it"]],
    "a Clipper sound", CORAL)
row("FX", [["DRIVE"], ["CHORUS"], ["DELAY"], ["REVERB"], ["the reverb's model", "(YARD: as EDIT)"]],
    "Clipper's sends", CORAL)
row("SEL", [["thousands", "0 to 4"], ["hundreds"], ["tens"], ["units"], ["the named list;", "SAVE: keep the", "number in yours"]],
    "SCALE: its number", CORAL)
row("SEQ", [["SLOT", "1 to 8"], ["LOOP", "ON OFF"], [], [], ["the slot"]], "RECORDINGS", CORAL)
row("ARP (held)", [["MODE / TEMPO"], ["RATE / LATCH"], ["GATE"], ["OCTAVES"], ["the other page"]],
    "tap ARP: on, off", CORAL)

section("Errands")
ERR = [
    ("POLY+BREATH (a wind controller)", "PLAY page (HOME unlit) - tap GLO - turn K1 to POLY+BR - tap GLO"),
    ("USB audio at full level", "PLAY page - tap GLO - turn K2 to FULL - tap GLO"),
    ("Headphones quieter at the top", "HOME to SOUND (lit) - tap GLO (MORE) - turn K1 (PHONES MAX)"),
    ("A Diosynth on the USB port", "GLO - turn SELECT right (left: back to the computer)"),
    ("A scale by its number", "tap SEL - K1 to K4 the digits (2741 C major, 1723 PCO) - SAVE keeps it in your list"),
    ("Keep what you just played", "tap REC (the last phrase; held: the whole history) - PLAY, PLAY to stop - SAVE into the slot"),
    ("Loop and overdub", "tap SEQ - K2 LOOP ON - tap SEQ - PLAY - tap REC: a layer at the next pass; hold REC: undo"),
    ("Edit and keep a sound", "HOME to SOUND - tap EDIT - turn (SELECT: the next page) - SAVE - tap EDIT"),
    ("Everything off", "OCT- and OCT+ together"),
]
for a, b in ERR:
    r(16, y, W - 32, 30, "#ffffff", 8, LINE)
    t(30, y + 20, a, 13, TEAL, 700)
    t(300, y + 20, b, 13, TXT)
    y += 36
y += 10
t(W / 2, y, "mvane yard 0.16 - tools/panel_map.py", 11, SOFT, 500, "middle", True)
H = y + 20

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
       f'aria-label="mvane yard on the FM-1: what each control does, the knob pages and the layers">'
       f'<rect width="{W}" height="{H}" fill="{PAPER}"/>' + "".join(out) + "</svg>\n")
here = os.path.dirname(os.path.abspath(__file__))
for p in (os.path.join(here, "..", "docs", "media", "panel-map.svg"), os.path.join(here, "..", "web", "panel-map.svg")):
    with open(p, "w") as f:
        f.write(svg)
print(f"panel-map.svg: {W} x {H}")
