// SPDX-License-Identifier: GPL-3.0-only
// web/ysb.js against tools/ysb.py: the same bytes from the same sounds (YARD's and Clipper's), and back
// (node web/ysb_test.mjs)
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fromJson, toJson, SIZE, SIZE1 } from "./ysb.js";

const names = JSON.parse(readFileSync(new URL("./clp_names.json", import.meta.url)));
const d = mkdtempSync(join(tmpdir(), "ysb-"));
const mine = { format: "mvane-yard-sounds", version: 2, sounds: [
  { slot: 3, name: "Glass Keys", mono: false, tone: 80, release: 70, glide: 30, attack: 2, decay: 50, res: 30, detune: 40, sub: 0, pluck: 90 },
  { name: "reed", mono: true, tone: 45, res: 10 }],
  clipper: [{ name: "Glassy", sound: "GLASS PAD", reverb: "hall", changes: [{ name: "BRITE", value: 84 }, { id: 34, value: 6 }] },
            { slot: 5, name: "Organ+", sound: "SOUL ORGAN", changes: [] }] };
const js = fromJson(mine, names);
if (js.length !== SIZE) throw new Error(`size ${js.length}`);
writeFileSync(join(d, "mine.json"), JSON.stringify(mine));
execFileSync("python3", [new URL("../tools/ysb.py", import.meta.url).pathname, join(d, "mine.json"), join(d, "py.ysb")]);
const py = new Uint8Array(readFileSync(join(d, "py.ysb")));
if (Buffer.compare(Buffer.from(js), Buffer.from(py))) throw new Error("ysb.js and ysb.py differ");
const back = toJson(js, names);
if (back.sounds.length !== 2 || back.sounds[0].name !== "REED" || back.sounds[1].pluck !== 90) throw new Error(`back: ${JSON.stringify(back.sounds)}`);
const c = back.clipper;
if (c.length !== 2 || c[0].name !== "GLASSY" || c[0].sound !== "GLASS PAD" || c[0].reverb !== "HALL" || c[0].changes[0].id !== 122 ||
    c[0].changes[0].name !== "M1" || c[1].slot !== 5 || c[1].reverb !== null) throw new Error(`back: ${JSON.stringify(c)}`);
const v1 = js.slice(0, SIZE1); v1[4] = 1;           // 0.10's bank (YARD's only) still reads
if (toJson(v1, names).sounds.length !== 2) throw new Error("a version 1 bank");
let threw = false;
try { fromJson({ sounds: [{ slot: 1, name: "X", tone: 200 }] }, names); } catch (e) { threw = true; }
if (!threw) throw new Error("tone 200 taken");
console.log("ysb.js: all checks passed (the same bytes as ysb.py, YARD's and Clipper's sounds, and back)");
