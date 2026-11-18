/* Núcleo do criador do port (criar-port.html): tudo no navegador, nada sai
 * do computador. Lê o .cue, confere o .bin, e monta a ISO (ISO 9660) ou o
 * aplicativo (ZIP com APPS/YFM_PS2/ para o OPL) usando o próprio arquivo do
 * jogo como pedaço do Blob, sem copiar os 500 MB para a memória.
 * Também roda no Node (tools/ps2/creator/test_core.mjs). */
const YFM = (() => {
  const RAW = 2352;              /* setor do .bin (MODE2/2352) */
  const ISO_SECTOR = 2048;
  const GAME_ID = "SLUS_014.11"; /* Forbidden Memories USA */
  const EXPECTED_SIZE = 517872768;
  /* O ELF dentro da ISO; o OPL usa como ID: o mesmo do tools/ps2/iso/montar_iso,
   * para as capas da pasta ART (release/ART/RGPM_001.01_*.png) valerem. */
  const BOOT_NAME = "RGPM_001.01";
  const ISO_NAME = "RGPM_001.01.Yu-Gi-Oh FM PS2.iso";
  const APP_DIR = "YFM_PS2";
  const APP_TITLE = "Yu-Gi-Oh! Forbidden Memories PS2";

  const ascii = (s) => Uint8Array.from(s, (c) => c.charCodeAt(0) & 0xff);

  /* ---- .cue ---- */
  function parseCue(text) {
    const files = [];
    let current = null;
    for (const raw of text.split(/\r?\n/)) {
      const line = raw.trim();
      let m;
      if ((m = line.match(/^FILE\s+"([^"]+)"\s+(\S+)/i) || line.match(/^FILE\s+(\S+)\s+(\S+)/i))) {
        current = { name: m[1], type: m[2].toUpperCase(), tracks: [] };
        files.push(current);
      } else if ((m = line.match(/^TRACK\s+(\d+)\s+(\S+)/i)) && current) {
        current.tracks.push({ number: +m[1], mode: m[2].toUpperCase() });
      }
    }
    const tracks = files.reduce((n, f) => n + f.tracks.length, 0);
    const problems = [];
    if (!files.length) problems.push("O .cue não lista nenhum arquivo.");
    else if (files.length > 1 || tracks > 1)
      problems.push("O .cue tem mais de uma faixa. O Forbidden Memories (USA) tem uma faixa só: use o .cue/.bin de um único arquivo.");
    else if (files[0].tracks[0] && files[0].tracks[0].mode !== "MODE2/2352")
      problems.push(`A faixa é ${files[0].tracks[0].mode}; o esperado é MODE2/2352.`);
    return { files, binName: files[0] ? files[0].name : null, problems };
  }

  /* ---- .bin ---- */
  async function readBytes(blob, start, length) {
    return new Uint8Array(await blob.slice(start, start + length).arrayBuffer());
  }

  function indexOf(haystack, needle) {
    outer: for (let i = 0; i + needle.length <= haystack.length; i++) {
      for (let j = 0; j < needle.length; j++) if (haystack[i + j] !== needle[j]) continue outer;
      return i;
    }
    return -1;
  }

  /* O .bin é a imagem do Forbidden Memories USA? Erros impedem; avisos não. */
  async function checkBin(blob) {
    const errors = [], warnings = [];
    const size = blob.size;
    if (size % RAW !== 0) errors.push("O tamanho do .bin não é múltiplo de 2352: não parece uma imagem MODE2/2352.");
    const head = await readBytes(blob, 0, Math.min(size, 64 * RAW));
    const pvd = 16 * RAW + 24;
    const cd001 = head.length > pvd + 6 && head[pvd] === 1 && String.fromCharCode(...head.slice(pvd + 1, pvd + 6)) === "CD001";
    if (!cd001) errors.push("Não achei o sistema de arquivos do disco (CD001) no setor 16.");
    const found = indexOf(head, ascii(GAME_ID)) >= 0;
    if (!found) errors.push("Este disco não é o Yu-Gi-Oh! Forbidden Memories (USA, SLUS-01411).");
    if (!errors.length && size !== EXPECTED_SIZE)
      warnings.push(`O .bin tem ${size.toLocaleString("pt-BR")} bytes; o dump conhecido tem ${EXPECTED_SIZE.toLocaleString("pt-BR")}. Pode funcionar, mas confira o seu dump.`);
    return { ok: !errors.length, errors, warnings, size, id: found ? "SLUS-01411" : null };
  }

  /* ---- ELF embutido (gzip + base64) ---- */
  async function gunzipBase64(b64) {
    const bin = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
    const stream = new Blob([bin]).stream().pipeThrough(new DecompressionStream("gzip"));
    return new Uint8Array(await new Response(stream).arrayBuffer());
  }

  /* ---- CRC-32 (para o ZIP), em pedaços, com progresso ---- */
  const CRC_TABLE = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      t[n] = c >>> 0;
    }
    return t;
  })();
  function crcUpdate(crc, bytes) {
    let c = crc ^ 0xffffffff;
    for (let i = 0; i < bytes.length; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }
  async function crcBlob(blob, progress) {
    const CHUNK = 8 << 20;
    let crc = 0;
    for (let at = 0; at < blob.size; at += CHUNK) {
      crc = crcUpdate(crc, new Uint8Array(await blob.slice(at, at + CHUNK).arrayBuffer()));
      if (progress) progress(Math.min(1, (at + CHUNK) / blob.size));
    }
    return crc;
  }

  /* ---- escrita binária ---- */
  class Writer {
    constructor(size) { this.b = new Uint8Array(size); this.v = new DataView(this.b.buffer); }
    u8(at, x) { this.b[at] = x; }
    le16(at, x) { this.v.setUint16(at, x, true); }
    be16(at, x) { this.v.setUint16(at, x, false); }
    le32(at, x) { this.v.setUint32(at, x >>> 0, true); }
    be32(at, x) { this.v.setUint32(at, x >>> 0, false); }
    both16(at, x) { this.le16(at, x); this.be16(at + 2, x); }
    both32(at, x) { this.le32(at, x); this.be32(at + 4, x); }
    bytes(at, a) { this.b.set(a, at); }
    text(at, s, len, fill = 0x20) { for (let i = 0; i < len; i++) this.b[at + i] = i < s.length ? s.charCodeAt(i) : fill; }
  }

  /* ---- ISO 9660 ----
   * 0-15 área do sistema, 16 PVD, 17 terminador, 18/19 tabelas de caminhos,
   * 20 raiz, 21 SYSTEM.CNF, 22 YFM.SIZ (o tamanho do YFM.BIN em texto: o
   * cdrom0: do PS2 não responde fstat), a tradução PTBR.TXT (o
   * languages/pt-br.txt: na ISO só cabem nomes de 8 letras sem hífen), depois
   * o ELF e o YFM.BIN. */
  function isoDate7(d) {
    return [d.getUTCFullYear() - 1900, d.getUTCMonth() + 1, d.getUTCDate(), d.getUTCHours(), d.getUTCMinutes(), d.getUTCSeconds(), 0];
  }
  function isoDate17(d) {
    const p = (n, w) => String(n).padStart(w, "0");
    return `${p(d.getUTCFullYear(), 4)}${p(d.getUTCMonth() + 1, 2)}${p(d.getUTCDate(), 2)}${p(d.getUTCHours(), 2)}${p(d.getUTCMinutes(), 2)}${p(d.getUTCSeconds(), 2)}00`;
  }
  function dirRecord(name, lba, size, isDir, date) {
    const n = name.length, len = 33 + n + (n % 2 === 0 ? 1 : 0);
    const w = new Writer(len);
    w.u8(0, len);
    w.both32(2, lba);
    w.both32(10, size);
    w.bytes(18, date);
    w.u8(25, isDir ? 2 : 0);
    w.both16(28, 1);
    w.u8(32, n);
    w.bytes(33, name);
    return w.b;
  }

  function buildIso(elf, bin, ptbr = null, now = new Date()) {
    const cnf = ascii(`BOOT2 = cdrom0:\\${BOOT_NAME};1\r\nVER = 1.00\r\nVMODE = NTSC\r\n`);
    const date7 = isoDate7(now), date17 = isoDate17(now);
    const elfSectors = Math.ceil(elf.length / ISO_SECTOR);
    const binSectors = Math.ceil(bin.size / ISO_SECTOR);
    const siz = ascii(`${bin.size}\n`);
    const ptbrSectors = ptbr ? Math.ceil(ptbr.length / ISO_SECTOR) : 0;
    const LBA = { pvd: 16, term: 17, lpt: 18, mpt: 19, root: 20, cnf: 21, siz: 22, ptbr: 23, elf: 23 + ptbrSectors };
    LBA.bin = LBA.elf + elfSectors;
    const total = LBA.bin + binSectors;

    const head = new Writer(LBA.elf * ISO_SECTOR);
    /* Raiz: ".", "..", e os arquivos em ordem alfabética. */
    const files = [
      [`${BOOT_NAME};1`, LBA.elf, elf.length],
      ["SYSTEM.CNF;1", LBA.cnf, cnf.length],
      ["YFM.BIN;1", LBA.bin, bin.size],
      ["YFM.SIZ;1", LBA.siz, siz.length],
      ...(ptbr ? [["PTBR.TXT;1", LBA.ptbr, ptbr.length]] : []),
    ].sort((a, b) => (a[0] < b[0] ? -1 : 1));
    let at = LBA.root * ISO_SECTOR;
    for (const rec of [
      dirRecord([0], LBA.root, ISO_SECTOR, true, date7),
      dirRecord([1], LBA.root, ISO_SECTOR, true, date7),
      ...files.map(([name, lba, size]) => dirRecord(ascii(name), lba, size, false, date7)),
    ]) { head.bytes(at, rec); at += rec.length; }

    /* Tabelas de caminhos: só a raiz. */
    for (const [lba, big] of [[LBA.lpt, false], [LBA.mpt, true]]) {
      const o = lba * ISO_SECTOR;
      head.u8(o, 1);
      big ? head.be32(o + 2, LBA.root) : head.le32(o + 2, LBA.root);
      big ? head.be16(o + 6, 1) : head.le16(o + 6, 1);
    }

    /* PVD */
    const p = LBA.pvd * ISO_SECTOR;
    head.u8(p, 1); head.text(p + 1, "CD001", 5); head.u8(p + 6, 1);
    head.text(p + 8, "PLAYSTATION", 32);
    head.text(p + 40, "YFM_PS2", 32);
    head.both32(p + 80, total);
    head.both16(p + 120, 1); head.both16(p + 124, 1); head.both16(p + 128, ISO_SECTOR);
    head.both32(p + 132, 10);
    head.le32(p + 140, LBA.lpt); head.be32(p + 148, LBA.mpt);
    head.bytes(p + 156, dirRecord([0], LBA.root, ISO_SECTOR, true, date7));
    head.text(p + 190, "YFM_PS2", 128);
    head.text(p + 318, "RETROGAMES", 128);
    head.text(p + 446, "CRIAR-PORT.HTML", 128);
    head.text(p + 574, "YFM_PS2", 128);
    head.text(p + 702, "", 37 * 3);
    for (const [o, s] of [[813, date17], [830, date17], [847, "0000000000000000"], [864, "0000000000000000"]]) {
      head.text(p + o, s, 16); head.u8(p + o + 16, 0);
    }
    head.u8(p + 881, 1);
    /* Terminador */
    const t = LBA.term * ISO_SECTOR;
    head.u8(t, 255); head.text(t + 1, "CD001", 5); head.u8(t + 6, 1);
    /* SYSTEM.CNF */
    head.bytes(LBA.cnf * ISO_SECTOR, cnf);
    head.bytes(LBA.siz * ISO_SECTOR, siz);
    if (ptbr) head.bytes(LBA.ptbr * ISO_SECTOR, ptbr);

    const parts = [head.b, elf, new Uint8Array(elfSectors * ISO_SECTOR - elf.length), bin,
                   new Uint8Array(binSectors * ISO_SECTOR - bin.size)];
    return new Blob(parts, { type: "application/octet-stream" });
  }

  /* ---- ZIP (só armazenado), com as pastas ---- */
  function dosTime(d) {
    return {
      time: (d.getHours() << 11) | (d.getMinutes() << 5) | (d.getSeconds() >> 1),
      date: ((d.getFullYear() - 1980) << 9) | ((d.getMonth() + 1) << 5) | d.getDate(),
    };
  }
  /* entries: [{name, data: Uint8Array|Blob, crc}] (pastas terminam em "/") */
  function buildZip(entries, now = new Date()) {
    const { time, date } = dosTime(now);
    const parts = [], central = [];
    let offset = 0;
    for (const e of entries) {
      const name = ascii(e.name), size = e.data ? (e.data.size ?? e.data.length) : 0, dir = e.name.endsWith("/");
      const h = new Writer(30 + name.length);
      h.le32(0, 0x04034b50); h.le16(4, 20); h.le16(8, 0); h.le16(10, time); h.le16(12, date);
      h.le32(14, e.crc || 0); h.le32(18, size); h.le32(22, size); h.le16(26, name.length); h.bytes(30, name);
      parts.push(h.b);
      if (size) parts.push(e.data);
      const c = new Writer(46 + name.length);
      c.le32(0, 0x02014b50); c.le16(4, 20); c.le16(6, 20); c.le16(12, time); c.le16(14, date);
      c.le32(16, e.crc || 0); c.le32(20, size); c.le32(24, size); c.le16(28, name.length);
      c.le32(38, dir ? 0x10 : 0); c.le32(42, offset); c.bytes(46, name);
      central.push(c.b);
      offset += h.b.length + size;
    }
    const cdSize = central.reduce((n, b) => n + b.length, 0);
    const end = new Writer(22);
    end.le32(0, 0x06054b50); end.le16(8, entries.length); end.le16(10, entries.length);
    end.le32(12, cdSize); end.le32(16, offset);
    return new Blob([...parts, ...central, end.b], { type: "application/zip" });
  }

  const titleCfg = () => ascii(`title=${APP_TITLE}\nboot=YFM_PS2.ELF\n`);

  /* O aplicativo: APPS/YFM_PS2/{YFM_PS2.ELF, YFM.bin, title.cfg} e a
   * tradução em languages/pt-br.txt. */
  async function buildApp(elf, bin, progress, ptbr = null) {
    const cfg = titleCfg();
    const binCrc = await crcBlob(bin, progress);
    return buildZip([
      { name: "APPS/" },
      { name: `APPS/${APP_DIR}/` },
      { name: `APPS/${APP_DIR}/YFM_PS2.ELF`, data: elf, crc: crcUpdate(0, elf) },
      { name: `APPS/${APP_DIR}/YFM.bin`, data: bin, crc: binCrc },
      { name: `APPS/${APP_DIR}/title.cfg`, data: cfg, crc: crcUpdate(0, cfg) },
      ...(ptbr ? [{ name: `APPS/${APP_DIR}/languages/` },
                  { name: `APPS/${APP_DIR}/languages/pt-br.txt`, data: ptbr, crc: crcUpdate(0, ptbr) }] : []),
    ]);
  }

  /* Direto no pendrive (Chrome/Edge: File System Access). */
  async function writeAppTo(root, elf, bin, progress, ptbr = null) {
    const apps = await root.getDirectoryHandle("APPS", { create: true });
    const dir = await apps.getDirectoryHandle(APP_DIR, { create: true });
    const put = async (name, data) => {
      const w = await (await dir.getFileHandle(name, { create: true })).createWritable();
      await w.write(data);
      await w.close();
    };
    await put("YFM_PS2.ELF", elf);
    await put("title.cfg", titleCfg());
    if (ptbr) {
      const langs = await dir.getDirectoryHandle("languages", { create: true });
      const w = await (await langs.getFileHandle("pt-br.txt", { create: true })).createWritable();
      await w.write(ptbr);
      await w.close();
    }
    const w = await (await dir.getFileHandle("YFM.bin", { create: true })).createWritable();
    const CHUNK = 8 << 20;
    for (let at = 0; at < bin.size; at += CHUNK) {
      await w.write(await bin.slice(at, at + CHUNK).arrayBuffer());
      if (progress) progress(Math.min(1, (at + CHUNK) / bin.size));
    }
    await w.close();
  }

  return { RAW, GAME_ID, EXPECTED_SIZE, BOOT_NAME, ISO_NAME, APP_DIR, parseCue, checkBin, gunzipBase64, crcBlob, crcUpdate,
           buildIso, buildZip, buildApp, writeAppTo };
})();
if (typeof module !== "undefined") module.exports = YFM;
