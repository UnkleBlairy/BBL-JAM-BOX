// The sample memory image: what the sample uploader writes to the FM-1 (firmware/src/eng_sampler.c has the layout).
// Flash 0xA0000..0xDBFFF: a 4 KiB table, then the chunks' data as 8-bit mu-law (G.711), one byte a sample.
// Shared by the emulator page (web/emu/index.html) and the uploader. No DOM here.

export const SMP_BASE = 0xA0000, SMP_REGION = 0x3C000, SMP_TABLE = 0x1000;
export const SMP_CAP = SMP_REGION - SMP_TABLE;          // 241664 bytes = samples: ~11 s at 22 kHz
export const SMP_MAX = 32;                              // chunks S01..S32
export const SMP_RATES = [11025, 22050, 32000];
const MAGIC = 0x324D5346, NAME = 12, ENTRY = 24;

// 16-bit sample -> mu-law byte, and back (the firmware's SMP_ULAW)
export function ulawEnc(s) {
  let sign = 0;
  if (s < 0) { sign = 0x80; s = -s; }
  if (s > 32635) s = 32635;
  s += 0x84;
  let e = 7;
  while (e > 0 && !(s & (0x4000 >> (7 - e)))) e--;
  return ~(sign | (e << 4) | ((s >> (e + 3)) & 15)) & 0xFF;
}
export const ULAW_DEC = new Int16Array(256).map((_, b) => {
  const u = ~b & 0xFF, e = (u >> 4) & 7, m = u & 15, mag = (((m << 3) + 0x84) << e) - 0x84;
  return u & 0x80 ? -mag : mag;
});

const CRC = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
function crcAdd(c, b, a = 0, e = b.length) { for (let i = a; i < e; i++) c = CRC[(c ^ b[i]) & 255] ^ (c >>> 8); return c >>> 0; }
export const crc32 = (b) => (~crcAdd(0xFFFFFFFF, b)) >>> 0;

// A chunk from a decoded sound: mono samples (-1..1) at srcRate, seconds a..b, stored at rate (box filter, as the
// prototype), normalised to 0.97 peak if asked. -> {name, rate, data: Uint8Array of mu-law} or null (under 64 samples)
export function makeChunk(mono, srcRate, a, b, rate, normalise, name) {
  const s0 = Math.floor(a * srcRate), s1 = Math.floor(b * srcRate), ratio = srcRate / rate;
  const n = Math.floor((s1 - s0) / ratio);
  if (n < 64) return null;
  const f = new Float32Array(n);
  let pk = 0.0001;
  for (let j = 0; j < n; j++) {
    const i0 = s0 + Math.floor(j * ratio), i1 = Math.max(i0 + 1, s0 + Math.floor((j + 1) * ratio));
    let acc = 0;
    for (let i = i0; i < i1; i++) acc += mono[i] || 0;
    f[j] = acc / (i1 - i0);
    pk = Math.max(pk, Math.abs(f[j]));
  }
  const g = normalise ? 0.97 / pk : 1, data = new Uint8Array(n);
  for (let k = 0; k < n; k++) data[k] = ulawEnc(Math.round(Math.max(-1, Math.min(1, f[k] * g)) * 32767));
  return { name: name.slice(0, NAME), rate, data };
}

// a chunk's samples as -1..1 (to play it back)
export function chunkFloat(c) { const o = new Float32Array(c.data.length); for (let i = 0; i < o.length; i++) o[i] = ULAW_DEC[c.data[i]] / 32768; return o; }
export const usedBytes = (chunks) => chunks.reduce((a, c) => a + c.data.length, 0);

// the whole region (SMP_REGION bytes, from flash SMP_BASE): the table, then the data; the rest erased (0xFF)
export function buildImage(chunks) {
  if (chunks.length > SMP_MAX || usedBytes(chunks) > SMP_CAP) throw new Error("too much for the sample memory");
  const img = new Uint8Array(SMP_REGION).fill(0xFF), v = new DataView(img.buffer);
  img.fill(0, 0, SMP_TABLE);
  let off = 0;
  chunks.forEach((c, i) => {
    const e = 32 + i * ENTRY;
    for (let k = 0; k < NAME && k < c.name.length; k++) img[e + k] = c.name.charCodeAt(k) & 0x7F;
    v.setUint32(e + 12, off, true);
    v.setUint32(e + 16, c.data.length, true);
    v.setUint16(e + 20, c.rate, true);
    v.setUint16(e + 22, 0, true);
    img.set(c.data, SMP_TABLE + off);
    off += c.data.length;
  });
  v.setUint32(0, MAGIC, true);
  v.setUint16(4, 1, true);
  v.setUint16(6, chunks.length, true);
  v.setUint32(8, off, true);
  v.setUint32(12, crc32(img.subarray(SMP_TABLE, SMP_TABLE + off)), true);
  v.setUint32(28, (~crcAdd(crcAdd(0xFFFFFFFF, img, 0, 28), img, 32, 32 + chunks.length * ENTRY)) >>> 0, true);
  return img;
}

// the chunks in a region image (as the device checks it: a bad table or data = none)
export function parseImage(img) {
  if (!img || img.length < SMP_REGION) return [];
  const v = new DataView(img.buffer, img.byteOffset, img.byteLength), n = v.getUint16(6, true), dlen = v.getUint32(8, true);
  if (v.getUint32(0, true) !== MAGIC || v.getUint16(4, true) !== 1 || n > SMP_MAX || dlen > SMP_CAP) return [];
  if (((~crcAdd(crcAdd(0xFFFFFFFF, img, 0, 28), img, 32, 32 + n * ENTRY)) >>> 0) !== v.getUint32(28, true)) return [];
  if (crc32(img.subarray(SMP_TABLE, SMP_TABLE + dlen)) !== v.getUint32(12, true)) return [];
  const out = [];
  for (let i = 0; i < n; i++) {
    const e = 32 + i * ENTRY, off = v.getUint32(e + 12, true), len = v.getUint32(e + 16, true);
    let name = "";
    for (let k = 0; k < NAME && img[e + k]; k++) name += String.fromCharCode(img[e + k]);
    out.push({ name, rate: v.getUint16(e + 20, true), data: img.slice(SMP_TABLE + off, SMP_TABLE + off + len) });
  }
  return out;
}
