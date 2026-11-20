/* Testa o núcleo do criador no Node com os arquivos de verdade:
 *   node tools/ps2/creator/test_core.cjs <cue> <bin> <elf> <pasta de saída>
 * Gera YFM_PS2.iso e YFM_PS2_app.zip na pasta. */
const fs = require("fs");
const path = require("path");
const { Readable } = require("stream");
const YFM = require("./core.js");

async function save(blob, file) {
  await new Promise((ok, fail) =>
    Readable.fromWeb(blob.stream()).pipe(fs.createWriteStream(file)).on("finish", ok).on("error", fail));
  console.log("gravado", file, blob.size);
}

(async () => {
  const [cuePath, binPath, elfPath, out] = process.argv.slice(2);
  const cue = YFM.parseCue(fs.readFileSync(cuePath, "latin1"));
  console.log("cue:", cue.binName, cue.problems);
  const bin = await fs.openAsBlob(binPath);
  const check = await YFM.checkBin(bin);
  console.log("bin:", check);
  if (!check.ok) process.exit(1);
  const elf = new Uint8Array(fs.readFileSync(elfPath));
  /* o ELF passa pelo mesmo gzip+base64 da página */
  const b64 = require("zlib").gzipSync(elf, { level: 9 }).toString("base64");
  const back = await YFM.gunzipBase64(b64);
  console.log("elf gzip+base64:", b64.length, "volta igual:", Buffer.compare(Buffer.from(back), Buffer.from(elf)) === 0);
  fs.mkdirSync(out, { recursive: true });
  const ptbrPath = path.join(__dirname, "..", "..", "..", "languages", "pt-br.txt");
  const ptbr = fs.existsSync(ptbrPath) ? new Uint8Array(fs.readFileSync(ptbrPath)) : null;
  console.log("tradução:", ptbr ? ptbr.length + " bytes" : "não achei");
  await save(YFM.buildIso(back, bin, ptbr), path.join(out, "YFM_PS2.iso"));
  let last = 0;
  const zip = await YFM.buildApp(back, bin, (p) => { if (p - last > 0.25) { last = p; console.log("crc", Math.round(p * 100) + "%"); } }, ptbr);
  await save(zip, path.join(out, "YFM_PS2_app.zip"));
})();
