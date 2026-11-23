#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The VIBE FM card (vibe/mvane-yard-native/icon.png, 960 x 540, 16:9): rolling hills (the yard) over a keyboard with a
chord lit as the PLAY page lights it, the bass in red; a small wordmark. python3 tools/vibe_icon.py (Pillow)"""
import math
import os
from PIL import Image, ImageDraw, ImageFont

W, H, S = 960, 540, 3                       # drawn at 3x, then scaled down: smooth edges
BG, TEAL, AMBER, RED, INK = (16, 17, 20), (92, 226, 180), (224, 160, 64), (232, 72, 76), (242, 242, 242)
im = Image.new("RGB", (W * S, H * S), BG)
d = ImageDraw.Draw(im)

def hill(base, amp, phase, freq, color):     # a band of hills from the left edge to the right
    pts = [(0, H * S)]
    for x in range(0, W * S + 1, 6):
        t = x / (W * S)
        y = base + amp * math.sin(2 * math.pi * (freq * t + phase)) + 0.35 * amp * math.sin(2 * math.pi * (2.3 * freq * t + phase * 1.7))
        pts.append((x, y * S))
    pts.append((W * S, H * S))
    d.polygon(pts, fill=color)

hill(250, 26, 0.10, 1.2, (24, 46, 42))
hill(285, 22, 0.55, 1.6, (30, 70, 61))
hill(318, 18, 0.30, 2.1, (38, 104, 88))

# the keyboard: two octaves and a C, from C; the chord C6/9 over its bass (C; D E G A above), as the screen lights it
kx, ky, kw, kh = 70, 330, 820, 170
whites = 15
ww = kw / whites
lit = {0: RED, 14: TEAL, 16: TEAL, 19: TEAL, 21: TEAL}   # semitones from the first C: the bass, then D E G A
wpc = [0, 2, 4, 5, 7, 9, 11]
for k in range(whites):
    n = (k // 7) * 12 + wpc[k % 7]
    x0 = kx + k * ww
    d.rounded_rectangle([x0 * S, ky * S, (x0 + ww - 4) * S, (ky + kh) * S], radius=6 * S, fill=lit.get(n, (200, 204, 212)))
for k in range(whites - 1):
    b = {0: 1, 1: 3, 3: 6, 4: 8, 5: 10}.get(k % 7)
    if b is None:
        continue
    n = (k // 7) * 12 + b
    x0 = kx + k * ww + ww * 0.66
    d.rounded_rectangle([x0 * S, ky * S, (x0 + ww * 0.62) * S, (ky + kh * 0.6) * S], radius=4 * S, fill=lit.get(n, (28, 30, 36)))

# the wordmark, small, top left; an amber dot: the PLAY page's colour
font = None
for f in ("/System/Library/Fonts/Avenir Next.ttc", "/System/Library/Fonts/HelveticaNeue.ttc", "/System/Library/Fonts/Helvetica.ttc"):
    if os.path.exists(f):
        font = ImageFont.truetype(f, 64 * S, index=0)
        break
d.ellipse([70 * S, 92 * S, 96 * S, 118 * S], fill=AMBER)
d.text((112 * S, 70 * S), "mvane yard", font=font, fill=INK)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "vibe", "mvane-yard-native", "icon.png")
im.resize((W, H), Image.LANCZOS).save(out, optimize=True)
print(f"vibe_icon: {out} ({os.path.getsize(out) // 1024} KB)")
