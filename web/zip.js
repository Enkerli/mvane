// SPDX-License-Identifier: GPL-3.0-only
// A ZIP archive of files kept as they are (stored, no compression): what "Download all" gives. zip([{name, bytes}]);
// unzip(bytes) reads one back (0.17: its stored files; a compressed one is left out) -> [{name, bytes}]
const T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();
export function crc32(b) { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = T[(c ^ b[i]) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }

export function zip(files, when = new Date()) {
  const enc = new TextEncoder(), parts = [], dir = [];
  const time = (when.getHours() << 11) | (when.getMinutes() << 5) | (when.getSeconds() >> 1);
  const date = ((when.getFullYear() - 1980) << 9) | ((when.getMonth() + 1) << 5) | when.getDate();
  let off = 0;
  for (const f of files) {
    const name = enc.encode(f.name), data = typeof f.bytes === "string" ? enc.encode(f.bytes) : f.bytes, crc = crc32(data);
    const h = new DataView(new ArrayBuffer(30));
    h.setUint32(0, 0x04034b50, true); h.setUint16(4, 20, true); h.setUint16(6, 0x0800, true);   // (UTF-8 names)
    h.setUint16(10, time, true); h.setUint16(12, date, true); h.setUint32(14, crc, true);
    h.setUint32(18, data.length, true); h.setUint32(22, data.length, true); h.setUint16(26, name.length, true);
    const c = new DataView(new ArrayBuffer(46));
    c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true); c.setUint16(8, 0x0800, true);
    c.setUint16(12, time, true); c.setUint16(14, date, true); c.setUint32(16, crc, true);
    c.setUint32(20, data.length, true); c.setUint32(24, data.length, true); c.setUint16(28, name.length, true);
    c.setUint32(42, off, true);
    parts.push(new Uint8Array(h.buffer), name, data);
    dir.push(new Uint8Array(c.buffer), name);
    off += 30 + name.length + data.length;
  }
  const size = dir.reduce((n, p) => n + p.length, 0), e = new DataView(new ArrayBuffer(22));
  e.setUint32(0, 0x06054b50, true); e.setUint16(8, files.length, true); e.setUint16(10, files.length, true);
  e.setUint32(12, size, true); e.setUint32(16, off, true);
  const all = [...parts, ...dir, new Uint8Array(e.buffer)], out = new Uint8Array(all.reduce((n, p) => n + p.length, 0));
  let o = 0;
  for (const p of all) out.set(p, o), o += p.length;
  return out;
}

export function unzip(raw) {
  const v = new DataView(raw.buffer, raw.byteOffset, raw.byteLength), dec = new TextDecoder(), out = [];
  let e = raw.length - 22;
  while (e >= 0 && v.getUint32(e, true) !== 0x06054b50) e--;   // the end of the central directory
  if (e < 0) throw new Error("not a ZIP file");
  let o = v.getUint32(e + 16, true);
  for (let k = v.getUint16(e + 10, true); k > 0; k--) {
    if (v.getUint32(o, true) !== 0x02014b50) throw new Error("a damaged ZIP file");
    const method = v.getUint16(o + 10, true), size = v.getUint32(o + 20, true), nl = v.getUint16(o + 28, true);
    const xl = v.getUint16(o + 30, true), cl = v.getUint16(o + 32, true), at = v.getUint32(o + 42, true);
    const name = dec.decode(raw.subarray(o + 46, o + 46 + nl));
    const data = at + 30 + v.getUint16(at + 26, true) + v.getUint16(at + 28, true);
    if (method === 0) out.push({ name, bytes: raw.subarray(data, data + size) });
    o += 46 + nl + xl + cl;
  }
  return out;
}
