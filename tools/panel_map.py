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
ROW1 = [("FX", ["effects;", "+SAVE: keep"]), ("SEL", ["SCALE", "layer"]), ("ENV", ["envelope;", "+SAVE: keep"]), ("LFO", ["(unused)"]),
        ("EDIT", ["CHORD /", "SOUND"]), ("GLO", ["SETTINGS /", "MORE"])]
ROW2 = [("HOME", ["PLAY / SOUND", "page"]), ("SAVE", ["settings +", "recording"]), ("ARP", ["layer; lit:", "PLAY on/off"]),
        ("SEQ", ["RECORD-", "INGS layer"]), ("PLAY", ["play / stop;", "ARP lit: ARP"]), ("REC", ["tap: phrase", "hold: all"])]
LAYER = {"FX", "SEL", "ENV", "EDIT", "GLO", "SEQ", "ARP"}
for i, (n, role) in enumerate(ROW1):
    button(578 + i * 65, 172, 58, 82, n, role, CORAL if n in LAYER else (DIM if n == "LFO" else None))
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


HT = []                                # the same, for the cheat sheet's tables: [title, sub, rows]


def section(title, sub=None):
    global y
    HT.append([title, sub, []])
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
    HT[-1][2].append((name, sub, cells))
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
row("GLO, SOUND page", [["PHONES MAX", "100% to 10%"], ["SPREAD", "MPE glide"], ["LOUD", "OFF +3 DB +6 DB"], [], []], "MORE", CORAL)
row("EDIT, PLAY page", [["VOICING", "CLOSE OPEN", "WIDE PAIRS"], ["VOICES", "3 to 6"], ["BASS", "ON OFF"],
                        ["BANK", "QUALITY / SCALE"], ["the chord type"]], "CHORD", CORAL)
row("EDIT, SOUND page", [["DETUNE / WAVE", "/ ONSET"], ["SUB / PW", "/ ONSET LVL"], ["RES / NOISE", "/ LEVEL"],
                         ["PLUCK / FILTER", "/ -"], ["the next page", "(three); SAVE:", "keep the sound"]],
    "a YARD sound", CORAL)
row("", [["engine 1 / 5"], ["engine 2 / 6"], ["engine 3 / 7"], ["engine 4 / 8"], ["the other page", "SAVE: keep it"]],
    "a Clipper sound", CORAL)
row("ENV", [["ATTACK"], ["DECAY"], ["SUSTAIN", "(YARD: PLUCK)"], ["RELEASE"], ["SAVE: keep", "the sound"]],
    "every sound's envelope", CORAL)
row("FX", [["DRIVE / SPACE", "SPACE: more reverb,", "delay and width"], ["CHORUS / ROOM", "ROOM: the reverb's", "model"],
           ["DELAY / HOUSE", "HOUSE: these under", "every sound"], ["REVERB / -"], ["the other page", "SAVE: keep it"]],
    "the effects (YARD: none yet)", CORAL)
row("SEL", [["thousands", "0 to 4"], ["hundreds"], ["tens"], ["units"], ["the named list;", "SAVE: keep the", "number in yours"]],
    "SCALE: its number", CORAL)
row("SEQ", [["SLOT", "1 to 8"], ["LOOP", "ON OFF"], [], [], ["the slot"]], "RECORDINGS", CORAL)
row("ARP", [["MODE / TEMPO", "... CHORD SCALE"], ["RATE / LATCH"], ["GATE / VELO", "VELO: contrast kept"], ["OCTAVES / FEEL", "FEEL: accents"], ["the other page"]],
    "lit: PLAY on, off", CORAL)

section("Errands")
ERR = [   # (the errand's name, the task as a user test asks it, the answer)
    ("POLY+BREATH (a wind controller)", "Set the play mode for a wind controller's breath, then back to POLY.",
     "PLAY page (HOME unlit) - tap GLO - turn K1 to POLY+BR - tap GLO"),
    ("USB audio at full level", "Make the computer record the FM-1 at full level, whatever the volume.",
     "PLAY page - tap GLO - turn K2 to FULL - tap GLO"),
    ("Headphones quieter at the top", "Lower PHONES MAX to 50%.",
     "HOME to SOUND (lit) - tap GLO (MORE) - turn K1 (PHONES MAX)"),
    ("A Diosynth on the USB port", "Let the FM-1 host a USB MIDI device, then go back to the computer.",
     "GLO - turn SELECT right (left: back to the computer)"),
    ("Chords from a minor scale", "Play C minor's own chords (Cm7, Fm7, Gm7) on the white keys.",
     "PLAY page - K3 SCALE to MINOR - K1 QUANT to FULL (or EDIT, K4 BANK to SCALE) - the 1st, 4th, 5th white keys"),
    ("Two qualities at once", "Make the chords maj7 add4.", "press the M7 and S4 black keys together (they latch together)"),
    ("A scale by its number", "Play in the pitch-class set 1723 (PCO), and keep it in your list.",
     "tap SEL - K1 to K4 the digits (2741 C major, 1723 PCO) - SAVE keeps it in your list"),
    ("Keep what you just played", "Keep the last phrase you played as a recording in slot 3.",
     "tap SEQ, SELECT to 3, tap SEQ - play - tap REC (held: the whole history) - SAVE"),
    ("Loop and overdub", "Loop a recording, add a layer, then undo it.",
     "tap SEQ - K2 LOOP ON - tap SEQ - PLAY - tap REC: overdub from now, a layer each pass; hold REC: undo"),
    ("The arpeggiator", "Turn the arpeggiator on, latched, at 1/16.",
     "tap ARP - K2 RATE to 1/16 - SELECT, K2 LATCH ON - PLAY (the ARP on) - tap ARP"),
    ("Edit and keep a sound", "Make CLAV 3 dB quieter and keep it as your own.",
     "PRESETS to CLAV - HOME to SOUND - tap EDIT - SELECT to page 3/3 - K3 LEVEL down 6 steps - SAVE - tap EDIT"),
    ("Everything off", "Silence every note at once.", "OCT- and OCT+ together"),
]
for a, _, b in ERR:
    r(16, y, W - 32, 30, "#ffffff", 8, LINE)
    t(30, y + 20, a, 13, TEAL, 700)
    t(300, y + 20, b, 13, TXT)
    y += 36
y += 10
t(W / 2, y, "mvane yard 0.17 - tools/panel_map.py", 11, SOFT, 500, "middle", True)
H = y + 20

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
       f'aria-label="mvane yard on the FM-1: what each control does, the knob pages and the layers">'
       f'<rect width="{W}" height="{H}" fill="{PAPER}"/>' + "".join(out) + "</svg>\n")
here = os.path.dirname(os.path.abspath(__file__))
for p in (os.path.join(here, "..", "docs", "media", "panel-map.svg"), os.path.join(here, "..", "web", "panel-map.svg")):
    with open(p, "w") as f:
        f.write(svg)
print(f"panel-map.svg: {W} x {H}")

# ---- the cheat sheet (Alex, 2026-11-23: the map's top part, on the landing page): the knob pages and the layers in
# one accessible table, written into web/index.html (between its cheat markers) and web/cheatsheet.html (the Hub's
# cheat link); the errands as tasks, their answers hidden, on a page of their own (web/tasks.html) ----
def cell_html(c):                         # the name, then its values in small print
    if not c:
        return '<span class="n">-</span>'
    return f"<b>{escape(c[0])}</b>" + (f'<span class="sub">{escape(" ".join(c[1:]))}</span>' if c[1:] else "")


body = []
for title, sub, rows in HT:
    if not rows:
        continue
    body.append(f'<tr class="grp"><th scope="rowgroup" colspan="6">{escape(title.split(":")[0])}</th></tr>')
    for name, rsub, cells in rows:
        head_ = escape(name or "") + (f'<span class="sub">{escape(rsub)}</span>' if rsub else "")
        body.append(f'<tr><th scope="row">{head_}</th>' + "".join(f"<td>{cell_html(c)}</td>" for c in cells) + "</tr>")
CHEAT_CSS = """.cheat { overflow-x: auto; }
.cheat table { border-collapse: collapse; width: 100%; font-size: 13px; line-height: 1.3; }
.cheat th, .cheat td { border: 1px solid var(--line); padding: 4px 6px; text-align: left; vertical-align: top; }
.cheat b { font-weight: 600; }
.cheat thead th { color: var(--dim); font-weight: 600; }
.cheat tbody th[scope=row] { color: var(--accent); min-width: 7em; }
.cheat tr.grp th { background: var(--bg); font-size: 15px; padding-top: 10px; }
.cheat .sub { display: block; color: var(--dim); font-weight: 400; font-size: 11.5px; }
.cheat .n { color: var(--dim); }"""
frag = ('<div class="cheat"><table><caption class="sr">mvane yard\'s controls: what K1 to K4 and SELECT do on each '
        'page and in each layer</caption><thead><tr><th scope="col">Where</th><th scope="col">K1</th>'
        '<th scope="col">K2</th><th scope="col">K3</th><th scope="col">K4</th><th scope="col">SELECT</th></tr></thead>'
        "<tbody>" + "".join(body) + "</tbody></table></div>")

web = os.path.join(here, "..", "web")
ip = os.path.join(web, "index.html")
with open(ip) as f:
    page = f.read()
A, B = "<!-- cheat:start (tools/panel_map.py) -->", "<!-- cheat:end -->"
if A in page and B in page:
    page = page[:page.index(A) + len(A)] + "\n" + frag + "\n" + page[page.index(B):]
    with open(ip, "w") as f:
        f.write(page)
else:
    print("index.html: no cheat markers; left as it was")

HEAD = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title><meta name="description" content="{desc}">
<style>
:root {{ --bg: #f4f2ed; --card: #fff; --ink: #1d1f22; --dim: #5a5f66; --line: #d6d3cc; --accent: #1f7a64; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg: #16181b; --card: #1f2226; --ink: #ececee; --dim: #a9adb4;
  --line: #353940; --accent: #6fd0b8; }} }}
* {{ box-sizing: border-box; }}
body {{ margin: 0; background: var(--card); color: var(--ink); font: 15px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif; }}
main {{ max-width: 70rem; margin: 0 auto; padding: 1.2rem 16px 2rem; }}
h1 {{ font-size: 1.5rem; margin: 0 0 .3rem; }} p {{ margin: .3rem 0 .8rem; color: var(--dim); }} a {{ color: var(--accent); }}
.sr {{ position: absolute; width: 1px; height: 1px; overflow: hidden; clip: rect(0 0 0 0); white-space: nowrap; }}
ol {{ padding-left: 1.4rem; }} li {{ margin: .7rem 0; }}
details {{ margin: .25rem 0 0 1.6rem; }} summary {{ cursor: pointer; color: var(--accent); }}
input[type=checkbox] {{ width: 1.1rem; height: 1.1rem; vertical-align: -.15rem; }}
:focus-visible {{ outline: 3px solid var(--accent); outline-offset: 2px; }}
@media print {{ body {{ font-size: 11px; }} a {{ color: inherit; }} }}
{css}
</style></head><body><main>
"""
LINKS = ('<p><a href="./">mvane yard</a> · <a href="cheatsheet.html">the cheat sheet</a> · '
         '<a href="tasks.html">try it: tasks</a> · <a href="panel-map.svg">the panel map</a> · '
         '<a href="https://github.com/Enkerli/mvane-yard/blob/main/docs/MANUAL.md">the manual</a></p>')
with open(os.path.join(web, "cheatsheet.html"), "w") as f:
    f.write(HEAD.format(title="mvane yard cheat sheet", desc="What K1 to K4 and SELECT do on each page and layer of mvane yard on the M-VAVE FM-1.", css=CHEAT_CSS)
            + "<h1>mvane yard: the cheat sheet</h1><p>HOME switches the knobs' two pages (lit: SOUND). A layer: hold its "
              "button and turn, or tap it open and tap it closed; EDIT and GLO follow the page. While ARP is lit (open, or "
              "running), PLAY turns the ARP on and off.</p>\n" + frag + LINKS + "</main></body></html>\n")
tasks = "".join(f'<li><label><input type="checkbox" data-k="{i}"> {escape(q)}</label>'
                f'<details><summary>The answer<span class="sr"> to task {i + 1}</span></summary><p><b>{escape(a)}:</b> '
                f"{escape(ans)}</p></details></li>" for i, (a, q, ans) in enumerate(ERR))
with open(os.path.join(web, "tasks.html"), "w") as f:
    f.write(HEAD.format(title="mvane yard: try it", desc="Tasks to learn mvane yard's panel on the M-VAVE FM-1, or to test it, their answers hidden.", css="")
            + "<h1>mvane yard: try it</h1><p>A way to learn the panel, or to test it: do each task on the FM-1, then "
              "open its answer. Your ticks stay in this browser.</p>\n<ol id=\"tasks\">" + tasks + "</ol>" + LINKS +
            """</main><script>
document.querySelectorAll("#tasks input").forEach((b) => {
  const k = "mvane-task-" + b.dataset.k;
  try { b.checked = localStorage.getItem(k) === "1"; } catch (e) {}
  b.onchange = () => { try { localStorage.setItem(k, b.checked ? "1" : "0"); } catch (e) {} };
});
</script></body></html>
""")
print("cheat sheet:", len(body), "rows (index.html, cheatsheet.html);", len(ERR), "tasks (tasks.html)")
