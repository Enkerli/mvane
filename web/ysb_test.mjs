// SPDX-License-Identifier: GPL-3.0-only
// web/ysb.js against tools/ysb.py: the same bytes from the same sounds, and back (node web/ysb_test.mjs)
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fromJson, toJson, SIZE } from "./ysb.js";

const d = mkdtempSync(join(tmpdir(), "ysb-"));
const mine = { format: "mvane-yard-sounds", version: 1, sounds: [
  { slot: 3, name: "Glass Keys", mono: false, tone: 80, release: 70, glide: 30, attack: 2, decay: 50, res: 30, detune: 40, sub: 0, pluck: 90 },
  { name: "reed", mono: true, tone: 45, res: 10 }] };
const js = fromJson(mine);
if (js.length !== SIZE) throw new Error(`size ${js.length}`);
writeFileSync(join(d, "mine.json"), JSON.stringify(mine));
execFileSync("python3", [new URL("../tools/ysb.py", import.meta.url).pathname, join(d, "mine.json"), join(d, "py.ysb")]);
const py = new Uint8Array(readFileSync(join(d, "py.ysb")));
if (Buffer.compare(Buffer.from(js), Buffer.from(py))) throw new Error("ysb.js and ysb.py differ");
const back = toJson(js).sounds;
if (back.length !== 2 || back[0].slot !== 1 || back[0].name !== "REED" || !back[0].mono || back[1].pluck !== 90)
  throw new Error(`back: ${JSON.stringify(back)}`);
let threw = false;
try { fromJson({ sounds: [{ slot: 1, name: "X", tone: 200 }] }); } catch (e) { threw = true; }
if (!threw) throw new Error("tone 200 taken");
console.log("ysb.js: all checks passed (the same bytes as ysb.py, and back)");
