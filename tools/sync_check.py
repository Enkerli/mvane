#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The FM-1 firmware and the native VIBE FM port agree (tests/run_tests.sh runs it, so every release checks it).

Alex (2026-11-24): "we need to make sure versions are in sync by the time we do a new release". Both versions come
from the release's tag (release.yml, vibe/ci.sh); what can drift is what each side lists by hand:
  - the ARP's modes and rates (app/app.c, vibe/mvane-yard-native/mvane_yard.c), in the same order: a settings file
    moves between the two, its numbers mean the same;
  - YARD's values (app/synth.h's SP_* enum): the port's CC table, its DAW ids and names, one each, in order;
  - the port's DAW parameters keep their numbers (a host stores them by number): the order of PR_* as 0.15 had it,
    the later ones after the ARP's.
  python3 tools/sync_check.py
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
APP = open(os.path.join(ROOT, "app", "app.c")).read()
NAT = open(os.path.join(ROOT, "vibe", "mvane-yard-native", "mvane_yard.c")).read()
SYN = open(os.path.join(ROOT, "app", "synth.h")).read()
bad = []


def strings(src, name):
    """The string list of `static const char *const NAME[...] = {...};` (the definition, not a declaration)."""
    m = re.search(r"static const char \*const " + name + r"\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        bad.append(f"{name}: not found")
        return []
    return re.findall(r'"([^"]*)"', m.group(1))


for name in ("ARP_MODES", "ARP_RATES"):
    a, n = strings(APP, name), strings(NAT, name)
    if a != n:
        bad.append(f"{name}: the FM-1 {a}, native {n}")

sp = re.search(r"enum \{ (SP_TONE.*?), SP_N \};", SYN, re.S).group(1)
sp = [x.strip() for x in sp.split(",")]
cc = re.search(r"static const uint8_t SP_CC\[SP_N\] = \{(.*?)\};", NAT, re.S).group(1)
cc = [x.strip() for x in cc.split(",")]
want = ["SYNTH_CC_" + {"SP_ONSET_LVL": "ONSET_LVL", "SP_REL": "REL"}.get(x, x[3:]) for x in sp]
if cc != want:
    bad.append(f"the port's SP_CC: {cc}, synth.h's order wants {want}")
for tab in ("SP_ID", "SP_NM", "SP_GR"):
    if len(strings(NAT, tab)) != len(sp):
        bad.append(f"the port's {tab}: {len(strings(NAT, tab))} names for {len(sp)} values")

# the DAW's numbers: 0.15's order, then what came later (0.16: ONSET, ONSET LVL, LEVEL after the ARP's)
PR_015 = ["PR_SOUND", "PR_PLAY", "PR_SP0", "PR_MODE", "PR_KEY", "PR_QUANT", "PR_PCS", "PR_STYLE", "PR_VOICES",
          "PR_BASS", "PR_BANK", "PR_HOLD", "PR_ARP", "PR_ARP_MODE", "PR_ARP_RATE", "PR_ARP_GATE", "PR_ARP_OCT",
          "PR_ARP_LATCH", "PR_ARP_BPM"]
pr = re.search(r"enum \{ (PR_SOUND.*?) \};", NAT, re.S).group(1)
pr = [x.split("=")[0].strip() for x in pr.replace("\n", " ").split(",")]
if pr[:len(PR_015)] != PR_015:
    bad.append(f"the port's DAW parameters moved: {pr[:len(PR_015)]}")
if "PR_MODE = PR_SP0 + SP_N_SET" not in NAT:
    bad.append("the port's YARD block in the DAW is not 0.15's thirteen (PR_MODE = PR_SP0 + SP_N_SET)")

if bad:
    print("sync_check: the FM-1 and the native port disagree:")
    for b in bad:
        print("  " + b)
    sys.exit(1)
print(f"sync_check: the FM-1 and native agree (ARP {len(strings(APP, 'ARP_MODES'))} modes, YARD {len(sp)} values, "
      f"DAW {len(PR_015)} parameters at 0.15's numbers)")
