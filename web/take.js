// SPDX-License-Identifier: GPL-3.0-only
// mvane yard's take (core/rec.h) <-> standard MIDI files, in the browser: the same as tools/mvt2mid.py and
// tools/yard.py. A take is 32-bit little-endian words after a 4-word head (magic "MVT1", version 1, count,
// duration ms); a word is dt (ms) | status | data1 | data2, most significant byte first; status 0 is a time word
// (a 24-bit gap: its top byte in dt, its low 16 bits in data1:data2).

export const MAGIC = 0x3154564d, MAX_WORDS = 30000, TPB = 500;   // 500 ticks a beat at 120 bpm: a tick is a ms

export function readTake(bytes) {               // -> { events: [[ms, st, d1, d2]], duration }
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (bytes.length < 16 || v.getUint32(0, true) !== MAGIC || v.getUint32(4, true) !== 1) throw new Error("not an mvane yard take");
  const count = v.getUint32(8, true), duration = v.getUint32(12, true);
  if (bytes.length < 16 + 4 * count) throw new Error("the take is cut short");
  const events = [];
  let t = 0;
  for (let i = 0; i < count; i++) {
    const w = v.getUint32(16 + 4 * i, true), st = (w >>> 16) & 0xff;
    if (st === 0) { t += ((w >>> 24) << 16) | (w & 0xffff); continue; }
    t += w >>> 24;
    events.push([t, st, (w >>> 8) & 0x7f, w & 0x7f]);
  }
  return { events, duration };
}

export function writeTake(events) {             // [[ms, st, d1, d2]] (in time order) -> take bytes
  const words = [];
  let last = 0;
  for (const [t, st, d1, d2] of events) {
    let gap = Math.max(0, t - last);
    last = t;
    while (gap >= 255) {
      const g = Math.min(gap, 0xffffff);
      words.push((((g >>> 16) << 24) | (g & 0xffff)) >>> 0);
      gap -= g;
    }
    words.push(((gap << 24) | (st << 16) | ((d1 & 0x7f) << 8) | (d2 & 0x7f)) >>> 0);
  }
  if (words.length > MAX_WORDS) throw new Error(`${words.length} events: the FM-1 keeps ${MAX_WORDS} at most`);
  const out = new Uint8Array(16 + 4 * words.length), v = new DataView(out.buffer);
  [MAGIC, 1, words.length, last].forEach((x, i) => v.setUint32(4 * i, x, true));
  words.forEach((w, i) => v.setUint32(16 + 4 * i, w, true));
  return out;
}

// MPE on 16 channels -> one channel, so a DAW makes one clip of it: notes on channel 1 (a note held twice sounds once),
// a member channel's pressure as poly aftertouch on its notes, its slide (CC74) and bend as channel 1's; the MPE
// set-up dropped (as tools/mvt2mid.py flatten)
export function flatten(events) {
  const held = Array.from({ length: 16 }, () => new Set()), count = new Map(), out = [];
  for (const [t, st, d1, d2] of events) {
    const kind = st & 0xf0, ch = st & 15;
    if (kind === 0x90 && d2) {
      held[ch].add(d1);
      count.set(d1, (count.get(d1) || 0) + 1);
      if (count.get(d1) === 1) out.push([t, 0x90, d1, d2]);
    } else if (kind === 0x80 || kind === 0x90) {
      if (held[ch].has(d1)) {
        held[ch].delete(d1);
        count.set(d1, count.get(d1) - 1);
        if (count.get(d1) === 0) out.push([t, 0x80, d1, 0]);
      }
    } else if (kind === 0xd0) {
      if (ch === 0) out.push([t, 0xd0, d1, 0]);
      else for (const n of [...held[ch]].sort((a, b) => a - b)) out.push([t, 0xa0, n, d1]);
    } else if (kind === 0xa0) out.push([t, 0xa0, d1, d2]);
    else if (kind === 0xb0) { if (![100, 101, 6, 38].includes(d1)) out.push([t, 0xb0, d1, d2]); }
    else if (kind === 0xe0) out.push([t, 0xe0, d1, d2]);
  }
  return out;
}

const vlq = (n) => { const o = [n & 0x7f]; while ((n >>>= 7)) o.unshift(0x80 | (n & 0x7f)); return o; };

export function writeMid({ events, duration }) { // a format-0 MIDI file, 120 bpm, 500 ticks a beat
  const trk = [0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20];
  let last = 0;
  for (const [t, st, d1, d2] of events) {
    trk.push(...vlq(t - last), st, d1, ...((st & 0xf0) === 0xc0 || (st & 0xf0) === 0xd0 ? [] : [d2]));
    last = t;
  }
  trk.push(...vlq(Math.max(0, duration - last)), 0xff, 0x2f, 0x00);
  const head = [0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, TPB >> 8, TPB & 0xff, 0x4d, 0x54, 0x72, 0x6b];
  const n = trk.length;
  return Uint8Array.from([...head, n >>> 24, (n >>> 16) & 0xff, (n >>> 8) & 0xff, n & 0xff, ...trk]);
}

export function readMid(bytes) {                // a standard MIDI file -> [[ms, st, d1, d2]], tracks merged, tempo honoured
  const s = String.fromCharCode(...bytes.slice(0, 4));
  if (s !== "MThd") throw new Error("not a MIDI file");
  const u32 = (o) => ((bytes[o] << 24) | (bytes[o + 1] << 16) | (bytes[o + 2] << 8) | bytes[o + 3]) >>> 0;
  const u16 = (o) => (bytes[o] << 8) | bytes[o + 1];
  const ntrk = u16(10), div = u16(12);
  if (div & 0x8000) throw new Error("SMPTE time is not supported");
  let pos = 8 + u32(4);
  const events = [], tempos = [[0, 500000]];
  for (let k = 0; k < ntrk && pos + 8 <= bytes.length; k++) {
    const n = u32(pos + 4), end = pos + 8 + n;
    let i = pos + 8, tick = 0, run = 0;
    const num = () => { let d = 0, x; do { x = bytes[i++]; d = (d << 7) | (x & 0x7f); } while (x & 0x80); return d; };
    while (i < end) {
      tick += num();
      let st = bytes[i];
      if (st < 0x80) st = run; else i++;
      if (st === 0xff) {
        const typ = bytes[i++], ln = num();
        if (typ === 0x51) tempos.push([tick, (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2]]);
        i += ln;
      } else if (st === 0xf0 || st === 0xf7) {
        i += num();
      } else {
        run = st;
        const kind = st & 0xf0;
        if (kind === 0xc0 || kind === 0xd0) events.push([tick, st, bytes[i++], 0]);
        else { events.push([tick, st, bytes[i], bytes[i + 1]]); i += 2; }
      }
    }
    pos = end;
  }
  tempos.sort((a, b) => a[0] - b[0]);
  const ms = (t) => {
    let out = 0, lastT = 0, us = 500000;
    for (const [tt, u] of tempos) { if (tt >= t) break; out += ((tt - lastT) * us) / div / 1000; lastT = tt; us = u; }
    return out + ((t - lastT) * us) / div / 1000;
  };
  return events.filter((e) => (e[1] & 0xf0) !== 0xc0).map(([t, ...r]) => [Math.round(ms(t)), ...r])
    .sort((a, b) => a[0] - b[0]);
}

export function notesOf(events, duration) {     // -> [{ on, off, note, vel, ch }] for a piano roll
  const open = new Map(), out = [];
  for (const [t, st, d1, d2] of events) {
    const kind = st & 0xf0, key = ((st & 15) << 7) | d1;
    if (kind === 0x90 && d2) open.set(key, { on: t, note: d1, vel: d2, ch: st & 15 });
    else if (kind === 0x80 || kind === 0x90) {
      const n = open.get(key);
      if (n) { n.off = t; out.push(n); open.delete(key); }
    }
  }
  for (const n of open.values()) { n.off = duration; out.push(n); }
  return out.sort((a, b) => a.on - b.on);
}
