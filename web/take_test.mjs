// SPDX-License-Identifier: GPL-3.0-only
// node web/take_test.mjs: take.js's conversions round-trip (the page's take <-> MIDI)
import { readTake, writeTake, writeMid, readMid, notesOf } from "./take.js";
const ev = [[0, 0xb0, 74, 60], [0, 0x91, 48, 90], [400, 0x92, 60, 100], [600, 0x82, 60, 0], [600, 0xd2, 40, 0],
  [610, 0xe2, 0, 80], [7000, 0x81, 48, 0], [200000, 0x93, 67, 80], [200100, 0x83, 67, 0]];
const t = readTake(writeTake(ev));
if (JSON.stringify(t.events) !== JSON.stringify(ev) || t.duration !== 200100) throw new Error("take round trip");
const back = readMid(writeMid(t));
if (JSON.stringify(back) !== JSON.stringify(ev)) throw new Error("MIDI round trip: " + JSON.stringify(back));
const n = notesOf(t.events, t.duration);
if (n.length !== 3 || n[0].off !== 7000) throw new Error("notes");
console.log("take.js: all checks passed");
