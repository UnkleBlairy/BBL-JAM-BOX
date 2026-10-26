// The sample uploader's transfer to the FM-1 over WebMIDI SysEx (firmware/src/smp_usb.c has the protocol): BEGIN, then
// each 4 KiB sector of the region image whose CRC on the device differs: WRITE in 512-byte blocks (data first, the
// table sector last), then END. A sector that fails is sent again from its start (3 tries). Unchanged sectors are
// skipped, so adding one sample to a full list is quick.
import { pack7, unpack7, Updater } from "./fm1ota.js";
import { crc32, SMP_TABLE } from "./smpimg.js";

const BEGIN = 0x60, SUM = 0x61, WRITE = 0x62, END = 0x63, SECTOR = 4096, BLOCK = 512;

function frame(cmd, body) {
  const m = [0x00, 0x59, cmd, body.length & 255, (body.length >> 8) & 255, (body.length >> 16) & 255, ...body];
  let s = 0;
  for (const b of body) s += b;
  m.push(~s & 255);
  return [0xF0, ...pack7(m), 0xF7];
}
const u32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];
const rd32 = (d, i) => (d[i] | d[i + 1] << 8 | d[i + 2] << 16 | d[i + 3] << 24) >>> 0;

// one request, its reply's body (null: no reply in ms, tries times)
async function ask(link, cmd, body, ms = 3000, tries = 3) {
  for (let t = 0; t < tries; t++) {
    link.drain();
    if (!link.send(frame(cmd, body))) throw new Error("The FM-1 was disconnected. Plug it back in and press Upload again: it carries on where it stopped.");
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const d = unpack7(p.slice(1, -1));
      if (d.length >= 7 && d[0] === 0 && d[1] === 0x59 && d[2] === cmd && d.length === d[3] + 7) return d.slice(6, 6 + d[3]);
    }
  }
  return null;
}

// img: the whole region (smpimg.js buildImage); say(text): progress. -> a message for the page
export async function uploadSamples(img, say) {
  if (!navigator.requestMIDIAccess) throw new Error("This browser has no Web MIDI: use Chrome or Edge.");
  let access;
  try { access = await navigator.requestMIDIAccess({ sysex: true }); }
  catch { throw new Error("The browser did not allow MIDI access. Allow it (the lock icon by the address) and try again."); }
  say("Looking for the FM-1…");
  const dev = await new Updater(access).find((id) => !/^ota-/i.test(id.model));
  if (!dev) throw new Error("FM-1 not found. Check the USB cable, and close other apps that use it (a DAW or another music app).");
  const link = dev.link;
  try {
    const b = await ask(link, BEGIN, []);
    if (!b) throw new Error("The FM-1 did not answer. Install the groovebox firmware first (the Install page), then try again.");
    if (b[0]) throw new Error("The FM-1 refused the upload (" + b[0] + ").");
    const n = Math.ceil(img.length / SECTOR), order = [];
    for (let s = SMP_TABLE / SECTOR; s < n; s++) order.push(s);
    order.push(0);                                    // the table last
    let done = 0, sent = 0;
    for (const s of order) {
      const off = s * SECTOR, sec = img.subarray(off, off + SECTOR);
      const r = await ask(link, SUM, u32(off));
      if (!r || r[0]) throw new Error("The FM-1 stopped answering. Press Upload again: it carries on where it stopped.");
      if (rd32(r, 5) !== crc32(sec)) {
        let ok = false;
        for (let t = 0; t < 3 && !ok; t++) {
          ok = true;
          for (let o = 0; o < SECTOR && ok; o += BLOCK) {
            const w = await ask(link, WRITE, [...u32(off + o), ...sec.subarray(o, o + BLOCK)], 4000, 1);
            ok = !!w && w[0] === 0;
          }
        }
        if (!ok) throw new Error("Writing the FM-1's memory failed. Press Upload again: it carries on where it stopped.");
        sent++;
      }
      done++;
      say("Uploading… " + Math.round(done / order.length * 100) + "%");
    }
    const e = await ask(link, END, []);
    if (!e) throw new Error("The FM-1 did not confirm. Press Upload again.");
    if (e[0]) throw new Error("The FM-1 says the samples don't check out. Press Upload again.");
    return (e[1] ? "Done: " + e[1] + " sample" + (e[1] === 1 ? "" : "s") + " on the FM-1" : "Done: the FM-1's sample memory is empty") +
      (sent ? "." : " (nothing had changed).");
  } finally {
    link.close();
  }
}
