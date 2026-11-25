// SPDX-License-Identifier: GPL-3.0-only
// web/ysb.js against tools/ysb.py: the same bytes from the same sounds (YARD's and Clipper's), and back
// (node web/ysb_test.mjs)
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fromJson, toJson, fx, SIZE, SIZE1, SIZE2 } from "./ysb.js";

const names = JSON.parse(readFileSync(new URL("./clp_names.json", import.meta.url)));
const d = mkdtempSync(join(tmpdir(), "ysb-"));
const mine = { format: "mvane-yard-sounds", version: 2, sounds: [
  { slot: 3, name: "Glass Keys", mono: false, tone: 80, release: 70, glide: 30, attack: 2, decay: 50, res: 30, detune: 40, sub: 0, pluck: 90,
    wave: 64, pw: 30, noise: 5, filter: 127, onset: 40, onset_level: 90, level: 70 },
  { name: "reed", mono: true, tone: 45, res: 10, favourite: true }],
  clipper: [{ name: "Glassy", sound: "GLASS PAD", reverb: "hall", favourite: true, changes: [{ name: "BRITE", value: 84 }, { id: 34, value: 6 }] },
            { slot: 5, name: "Organ+", sound: "SOUL ORGAN", changes: [] }],
  scales: [{ name: "Hijaz", pcs: 1459 }, { slot: 4, name: "", pcs: 2373 }] };
const js = fromJson(mine, names);
if (js.length !== SIZE) throw new Error(`size ${js.length}`);
writeFileSync(join(d, "mine.json"), JSON.stringify(mine));
execFileSync("python3", [new URL("../tools/ysb.py", import.meta.url).pathname, join(d, "mine.json"), join(d, "py.ysb")]);
const py = new Uint8Array(readFileSync(join(d, "py.ysb")));
if (Buffer.compare(Buffer.from(js), Buffer.from(py))) throw new Error("ysb.js and ysb.py differ");
const back = toJson(js, names);
if (back.sounds.length !== 2 || back.sounds[0].name !== "REED" || back.sounds[1].pluck !== 90 || back.sounds[1].filter !== 127 ||
    back.sounds[1].onset !== 40 || back.sounds[1].level !== 70 || back.sounds[1].onset_level !== 90 || back.sounds[0].onset !== 0 ||
    back.sounds[0].wave !== 0) throw new Error(`back: ${JSON.stringify(back.sounds)}`);
const c = back.clipper;
if (c.length !== 2 || c[0].name !== "GLASSY" || c[0].sound !== "GLASS PAD" || c[0].reverb !== "HALL" || c[0].changes[0].id !== 122 ||
    c[0].changes[0].name !== "M1" || c[1].slot !== 5 || c[1].reverb !== null) throw new Error(`back: ${JSON.stringify(c)}`);
if (back.scales.length !== 2 || back.scales[0].name !== "HIJAZ" || back.scales[0].pcs !== 1459 || back.scales[1].slot !== 4 ||
    back.scales[1].name !== "") throw new Error(`scales: ${JSON.stringify(back.scales)}`);
const v2 = js.slice(0, SIZE2); v2[4] = 2;           // 0.11's bank (no scales) still reads
if (toJson(v2, names).clipper.length !== 2 || toJson(v2, names).scales.length) throw new Error("a version 2 bank");
const v1 = js.slice(0, SIZE1); v1[4] = 1;           // 0.10's bank (YARD's only) still reads
if (toJson(v1, names).sounds.length !== 2) throw new Error("a version 1 bank");
let threw = false;
try { fromJson({ sounds: [{ slot: 1, name: "X", tone: 200 }] }, names); } catch (e) { threw = true; }
if (!threw) throw new Error("tone 200 taken");
// (0.17) favourites both ways, in both; the same FX on every Clipper sound (CHO replaced, REV and SPACE added), as ysb.py's
if (!back.sounds[0].favourite || back.sounds[1].favourite || !c[0].favourite || c[1].favourite) throw new Error("favourites");
{
  const r = fx(back.clipper, names, { CHO: 4, REV: 8, "#99": 12 }, { reverb: "plate" });
  if (r.done !== 2 || r.skipped.length) throw new Error(`fx: ${JSON.stringify(r)}`);
  const g = back.clipper[0].changes.map((x) => `${x.id}=${x.value}`).join(" ");
  if (g !== "122=84 34=4 36=8 99=12" || back.clipper[1].reverb !== "PLATE") throw new Error(`fx: ${g}`);
  writeFileSync(join(d, "fxin.json"), JSON.stringify(mine));
  execFileSync("python3", [new URL("../tools/ysb.py", import.meta.url).pathname, "fx", join(d, "fxin.json"), join(d, "fx.ysb"),
    "CHO=4", "REV=8", "#99=12", "--reverb", "plate"]);
  const mine2 = toJson(fromJson(JSON.parse(readFileSync(join(d, "fxin.json"))), names), names);
  fx(mine2.clipper, names, { CHO: 4, REV: 8, "#99": 12 }, { reverb: "plate" });
  if (Buffer.compare(Buffer.from(fromJson(mine2, names)), readFileSync(join(d, "fx.ysb")))) throw new Error("fx: ysb.js and ysb.py differ");
  const f = fx(back.clipper, names, { CHO: 1 }, { favourites: true });
  if (f.done !== 1 || back.clipper[1].changes.length !== 3) throw new Error("fx: favourites only");
}
console.log("ysb.js: all checks passed (the same bytes as ysb.py, YARD's and Clipper's sounds, scales, and back)");
