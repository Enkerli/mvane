// SPDX-License-Identifier: GPL-3.0-only
// The user's YARD sounds: the FM-1's bank (yard.ysb, app.c yard_bank_t) to a list and back; tools/ysb.py's format.
// The bank: "MVYB" (little-endian 0x4259564D), version 1, its size (32), two zeros; then 32 sounds of 32 bytes: a
// name (12, NUL-padded, printable ASCII), flags (1 used, 2 mono), the nine parameters 0..127, ten zeros.
export const MAGIC = 0x4259564d, VERSION = 1, N = 32, SIZE = 8 + 32 * 32;
export const PARAMS = ["tone", "release", "glide", "attack", "decay", "res", "detune", "sub", "pluck"];
export const DEFAULT = { tone: 56, release: 60, glide: 30, attack: 24, decay: 64, res: 0, detune: 0, sub: 0, pluck: 0 };

export function toJson(raw) {                   // yard.ysb's bytes -> {format, version, sounds: [{slot, name, mono, ...}]}
  const out = { format: "mvane-yard-sounds", version: VERSION, sounds: [] };
  if (!raw || raw.length !== SIZE) return out;
  const v = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  if (v.getUint32(0, true) !== MAGIC || raw[4] !== VERSION) return out;
  for (let i = 0; i < N; i++) {
    const o = 8 + 32 * i;
    if (!(raw[o + 12] & 1)) continue;
    let name = "";
    for (let k = 0; k < 12 && raw[o + k]; k++) name += raw[o + k] >= 32 && raw[o + k] <= 126 ? String.fromCharCode(raw[o + k]) : "?";
    const s = { slot: i + 1, name, mono: !!(raw[o + 12] & 2) };
    PARAMS.forEach((k, j) => { s[k] = Math.min(127, raw[o + 13 + j]); });
    out.sounds.push(s);
  }
  return out;
}

export function fromJson(d) {                   // the list -> yard.ysb's bytes; throws on a bad sound
  if (d.format !== undefined && d.format !== "mvane-yard-sounds") throw new Error(`not mvane yard's sounds (${d.format})`);
  const slots = new Array(N).fill(null), later = [];
  for (const s of d.sounds || []) {
    if (s.slot === undefined || s.slot === null) { later.push(s); continue; }
    if (!Number.isInteger(s.slot) || s.slot < 1 || s.slot > N) throw new Error(`${s.name || "?"}: slot ${s.slot}, not 1..${N}`);
    if (slots[s.slot - 1]) throw new Error(`slot ${s.slot} twice`);
    slots[s.slot - 1] = s;
  }
  for (const s of later) {
    const k = slots.indexOf(null);
    if (k < 0) throw new Error(`more than ${N} sounds`);
    slots[k] = s;
  }
  const raw = new Uint8Array(SIZE), v = new DataView(raw.buffer);
  v.setUint32(0, MAGIC, true);
  raw[4] = VERSION, raw[5] = N;
  slots.forEach((s, i) => {
    if (!s) return;
    const name = String(s.name ?? `USER ${i + 1}`).toUpperCase(), o = 8 + 32 * i;
    if (![...name].every((c) => c >= " " && c <= "~")) throw new Error(`${name}: the screen shows ASCII letters, digits and signs only`);
    for (let k = 0; k < Math.min(12, name.length); k++) raw[o + k] = name.charCodeAt(k);
    raw[o + 12] = 1 | (s.mono ? 2 : 0);
    PARAMS.forEach((k, j) => {
      const x = s[k] ?? DEFAULT[k];
      if (!Number.isInteger(x) || x < 0 || x > 127) throw new Error(`${name}: ${k} ${x}, not 0..127`);
      raw[o + 13 + j] = x;
    });
  });
  return raw;
}
