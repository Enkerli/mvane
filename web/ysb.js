// SPDX-License-Identifier: GPL-3.0-only
// Your own sounds, YARD's and Clipper's: the FM-1's bank (yard.ysb, app.c yard_bank_t) to lists and back; the format
// of tools/ysb.py (version 2; version 1, 0.10's, had YARD's only). Clipper's names (web/clp_names.json, make
// clp-names) turn a change's label into its parameter and back.
export const MAGIC = 0x4259564d, CMAGIC = 0x4243564d, N = 32, CN = 32, OV = 14, HYBRID = 12;
export const SIZE1 = 8 + 32 * N, SIZE = SIZE1 + 8 + 64 * CN;
export const PARAMS = ["tone", "release", "glide", "attack", "decay", "res", "detune", "sub", "pluck", "wave", "pw", "noise", "filter"];
export const DEFAULT = { tone: 56, release: 60, glide: 30, attack: 24, decay: 64, res: 0, detune: 0, sub: 0, pluck: 0,
  wave: 0, pw: 0, noise: 0, filter: 0 };   // (0.11: the last four; 0 leaves a sound as 0.10 made it)
const MACROS = ["BRITE", "TIMBR", "MOTN", "SPACE"];

export function label(names, id, engine) {      // a parameter's label (an EDIT value: its engine's)
  if (id >= names.edit0) return (names.edit[String(engine)] || [])[id - names.edit0] || "";
  return names.labels[id] || "";
}
export const soundName = (names, e, p) => (names.sounds.find((s) => s.engine === e && s.preset === p) || {}).name || "?";
export function paramOf(names, text, engine) {  // a label (or #id) -> the parameter (HYBRID's macros: its M1..M4)
  const t = String(text).trim().toUpperCase();
  if (/^#?\d+$/.test(t)) return Number(t.replace("#", ""));
  if (MACROS.includes(t) && engine === HYBRID) return names.edit0 + 4 + MACROS.indexOf(t);
  const ids = [];
  for (let i = 0; i < names.params; i++) if (label(names, i, engine) === t) ids.push(i);
  if (ids.length !== 1) throw new Error(`${text}: ${ids.length ? "more than one parameter" : "no parameter"} by that name (use #id)`);
  return ids[0];
}

function slots(items, n, what) {
  const out = new Array(n).fill(null), later = [];
  for (const s of items || []) {
    if (s.slot === undefined || s.slot === null) { later.push(s); continue; }
    if (!Number.isInteger(s.slot) || s.slot < 1 || s.slot > n) throw new Error(`${s.name || "?"}: slot ${s.slot}, not 1..${n}`);
    if (out[s.slot - 1]) throw new Error(`${what}: slot ${s.slot} twice`);
    out[s.slot - 1] = s;
  }
  for (const s of later) {
    const k = out.indexOf(null);
    if (k < 0) throw new Error(`more than ${n} ${what}`);
    out[k] = s;
  }
  return out;
}
function putName(raw, o, s, fallback) {
  const name = String(s.name ?? fallback).toUpperCase();
  if (![...name].every((c) => c >= " " && c <= "~")) throw new Error(`${name}: the screen shows ASCII letters, digits and signs only`);
  for (let k = 0; k < Math.min(12, name.length); k++) raw[o + k] = name.charCodeAt(k);
  return name;
}
const getName = (raw, o) => { let s = ""; for (let k = 0; k < 12 && raw[o + k]; k++) s += raw[o + k] >= 32 && raw[o + k] <= 126 ? String.fromCharCode(raw[o + k]) : "?"; return s; };

export function toJson(raw, names) {            // yard.ysb's bytes -> {format, version, sounds, clipper}
  const out = { format: "mvane-yard-sounds", version: 2, sounds: [], clipper: [] };
  if (!raw || (raw.length !== SIZE1 && raw.length !== SIZE)) return out;
  const v = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  if (v.getUint32(0, true) !== MAGIC || (raw[4] !== 1 && raw[4] !== 2)) return out;
  for (let i = 0; i < N; i++) {
    const o = 8 + 32 * i;
    if (!(raw[o + 12] & 1)) continue;
    const s = { slot: i + 1, name: getName(raw, o), mono: !!(raw[o + 12] & 2) };
    PARAMS.forEach((k, j) => { s[k] = Math.min(127, raw[o + 13 + j]); });
    out.sounds.push(s);
  }
  if (raw.length !== SIZE || raw[4] !== 2 || v.getUint32(SIZE1, true) !== CMAGIC) return out;
  for (let i = 0; i < CN; i++) {
    const o = SIZE1 + 8 + 64 * i;
    if (!(raw[o + 12] & 1)) continue;
    const engine = raw[o + 13], preset = raw[o + 14], rtype = v.getInt8(o + 15), n = Math.min(OV, raw[o + 16]);
    const changes = [];
    for (let k = 0; k < n; k++) {
      const id = raw[o + 20 + 3 * k], value = v.getInt16(o + 21 + 3 * k, true);
      changes.push({ id, name: names ? label(names, id, engine) : "", value });
    }
    const r = names ? rtype - names.reverb.min : -1;
    out.clipper.push({ slot: i + 1, name: getName(raw, o), engine, preset, sound: names ? soundName(names, engine, preset) : "?",
      reverb: names && r >= 0 && r < names.reverb.names.length ? names.reverb.names[r] : null, changes });
  }
  return out;
}

export function fromJson(d, names) {            // the lists -> yard.ysb's bytes (version 2); throws on a bad sound
  if (d.format !== undefined && d.format !== "mvane-yard-sounds") throw new Error(`not mvane yard's sounds (${d.format})`);
  const raw = new Uint8Array(SIZE), v = new DataView(raw.buffer);
  v.setUint32(0, MAGIC, true);
  raw[4] = 2, raw[5] = N;
  slots(d.sounds, N, "YARD sounds").forEach((s, i) => {
    if (!s) return;
    const o = 8 + 32 * i, name = putName(raw, o, s, `USER ${i + 1}`);
    raw[o + 12] = 1 | (s.mono ? 2 : 0);
    PARAMS.forEach((k, j) => {
      const x = s[k] ?? DEFAULT[k];
      if (!Number.isInteger(x) || x < 0 || x > 127) throw new Error(`${name}: ${k} ${x}, not 0..127`);
      raw[o + 13 + j] = x;
    });
  });
  v.setUint32(SIZE1, CMAGIC, true);
  raw[SIZE1 + 4] = CN;
  slots(d.clipper, CN, "Clipper sounds").forEach((s, i) => {
    if (!s) return;
    if (!names) throw new Error("Clipper's names are needed (clp_names.json)");
    const o = SIZE1 + 8 + 64 * i, name = putName(raw, o, s, `CLIPPER ${i + 1}`);
    let engine = s.engine, preset = s.preset;
    if (engine === undefined || preset === undefined) {
      const m = names.sounds.find((x) => x.name === String(s.sound || "").toUpperCase());
      if (!m) throw new Error(`${name}: no Clipper sound named ${s.sound}`);
      engine = m.engine, preset = m.preset;
    } else if (!names.sounds.some((x) => x.engine === engine && x.preset === preset)) throw new Error(`${name}: no Clipper sound at ${engine}/${preset}`);
    let rtype = -1;
    if (s.reverb !== null && s.reverb !== undefined && s.reverb !== "") {
      const r = names.reverb.names.indexOf(String(s.reverb).toUpperCase());
      if (r < 0) throw new Error(`${name}: reverb ${s.reverb}, not one of ${names.reverb.names.join(", ")}`);
      rtype = names.reverb.min + r;
    }
    const ch = s.changes || [];
    if (ch.length > OV) throw new Error(`${name}: ${ch.length} changes, ${OV} at most`);
    raw[o + 12] = 1, raw[o + 13] = engine, raw[o + 14] = preset;
    v.setInt8(o + 15, rtype);
    raw[o + 16] = ch.length;
    ch.forEach((c, k) => {
      const id = c.id ?? paramOf(names, c.name, engine);
      if (!Number.isInteger(id) || id < 0 || id >= names.params) throw new Error(`${name}: parameter ${id}`);
      if (!Number.isInteger(c.value) || c.value < -32768 || c.value > 32767) throw new Error(`${name}: ${label(names, id, engine) || id}: value ${c.value}`);
      raw[o + 20 + 3 * k] = id;
      v.setInt16(o + 21 + 3 * k, c.value, true);
    });
  });
  return raw;
}
