// SPDX-License-Identifier: GPL-3.0-only
// web/zip.js: an archive unzip reads back whole (node web/zip_test.mjs)
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { zip, unzip } from "./zip.js";
const d = mkdtempSync(join(tmpdir(), "zip-")), bin = new Uint8Array(1000).map((_, i) => (i * 7) & 255);
writeFileSync(join(d, "a.zip"), zip([{ name: "one.txt", bytes: "hello\n" }, { name: "two.bin", bytes: bin }]));
const out = execFileSync("python3", ["-c", "import zipfile,sys;z=zipfile.ZipFile(sys.argv[1]);assert z.testzip() is None;" +
  "print(z.read('one.txt').decode(),end='');sys.stdout.flush();sys.stdout.buffer.write(z.read('two.bin'))", join(d, "a.zip")]);
if (out.slice(0, 6).toString() !== "hello\n" || Buffer.compare(out.slice(6), Buffer.from(bin))) throw new Error("not read back");
{                                                    // (0.17) unzip: ours back whole; a ZIP from Python's zipfile (stored) too
  const back = unzip(zip([{ name: "one.txt", bytes: "hello\n" }, { name: "two.bin", bytes: bin }]));
  if (back.length !== 2 || new TextDecoder().decode(back[0].bytes) !== "hello\n" || Buffer.compare(Buffer.from(back[1].bytes), Buffer.from(bin)))
    throw new Error("unzip: not ours back");
  execFileSync("python3", ["-c", "import zipfile,sys;z=zipfile.ZipFile(sys.argv[1],'w');z.writestr('yard-sounds.json','{}');z.close()", join(d, "p.zip")]);
  const p = unzip(new Uint8Array(readFileSync(join(d, "p.zip"))));
  if (p.length !== 1 || p[0].name !== "yard-sounds.json" || new TextDecoder().decode(p[0].bytes) !== "{}") throw new Error("unzip: Python's");
}
console.log("zip.js: all checks passed (stored files, read back whole by Python's zipfile; unzip reads them back)");
