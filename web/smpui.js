// The sample uploader's panel (the emulator page and the FM-1 samples page share it): pick a sound file, choose a
// part, Write it to the list, Upload the list. The page has the markup (ids sfile sfinfo swave sA sB spv srate snorm
// swr swmsg smbar smtxt slist sup sclr sumsg). opts:
//   initial         the chunks to start with (smpimg.js parseImage)
//   upload(img, say) -> Promise of a message: sends the region image (smpimg.js buildImage); say(text) for progress
//   changed(list)   the list changed (to keep it)
import { SMP_CAP, SMP_MAX, makeChunk, chunkFloat, usedBytes, buildImage } from "./smpimg.js";

export function mountUploader(opts) {
  const $ = (id) => document.getElementById(id);
  let src = null, list = (opts.initial || []).slice(), pac = null, pnode = null, busy = false;
  const msg = (id, t, cls) => { const e = $(id); e.textContent = t; e.className = "msg" + (cls ? " " + cls : ""); };
  const kb = (b) => (b / 1024).toFixed(b < 10240 ? 1 : 0) + " KB";
  const secs = () => [+$("sA").value / 1000 * (src ? src.dur : 0), +$("sB").value / 1000 * (src ? src.dur : 0)];
  const changed = () => { if (opts.changed) opts.changed(list); };
  function stopPlay() { if (pnode) { try { pnode.onended = null; pnode.stop(); } catch {} pnode = null; } $("spv").textContent = "Play"; }
  function play(samples, rate) {
    stopPlay();
    pac = pac || new AudioContext();
    const b = pac.createBuffer(1, samples.length, rate);
    b.copyToChannel(samples, 0);
    pnode = pac.createBufferSource();
    pnode.buffer = b;
    pnode.connect(pac.destination);
    pnode.onended = () => { pnode = null; $("spv").textContent = "Play"; };
    pnode.start();
  }
  function drawWave() {
    const c = $("swave"), g = c.getContext("2d"), W = c.width, H = c.height;
    g.fillStyle = "#141415"; g.fillRect(0, 0, W, H);
    if (!src) return;
    const [a, b] = secs(), xa = a / src.dur * W, xb = b / src.dur * W, n = src.mono.length;
    for (let x = 0; x < W; x++) {
      let m = 0;
      for (let i = Math.floor(x / W * n), e = Math.floor((x + 1) / W * n); i < e; i++) m = Math.max(m, Math.abs(src.mono[i]));
      g.fillStyle = x >= xa && x <= xb ? "#fff" : "#555";
      g.fillRect(x, H / 2 - m * H / 2, 1, Math.max(1, m * H));
    }
    g.fillStyle = "#5fe08a"; g.fillRect(xa, 0, 2, H); g.fillRect(xb - 2, 0, 2, H);
  }
  function render() {
    const used = usedBytes(list);
    $("smbar").style.width = Math.min(100, used / SMP_CAP * 100) + "%";
    msg("smtxt", kb(used) + " of " + kb(SMP_CAP) + " · " + list.length + " of " + SMP_MAX + " slots");
    const L = $("slist");
    L.textContent = "";
    list.forEach((c, i) => {
      const li = document.createElement("li");
      li.innerHTML = '<span class="n"></span><span></span><span class="sz"></span><button type="button">Play</button><button type="button">Remove</button>';
      li.children[0].textContent = "S" + String(i + 1).padStart(2, "0");
      li.children[1].textContent = c.name;
      li.children[2].textContent = (c.data.length / c.rate).toFixed(2) + " s · " + Math.round(c.rate / 1000) + " kHz · " + kb(c.data.length);
      li.children[3].addEventListener("click", () => play(chunkFloat(c), c.rate));
      li.children[4].addEventListener("click", () => { list.splice(i, 1); render(); changed(); msg("sumsg", "Not on the FM-1 until you upload.", ""); });
      L.appendChild(li);
    });
  }
  function sync(which) {
    if (+$("sA").value >= +$("sB").value) { if (which === "a") $("sB").value = Math.min(1000, +$("sA").value + 1); else $("sA").value = Math.max(0, +$("sB").value - 1); }
    const [a, b] = secs();
    if (src) msg("sfinfo", src.name + " · " + src.dur.toFixed(2) + " s · part " + a.toFixed(2) + "–" + b.toFixed(2) + " s (" + (b - a).toFixed(2) + " s)");
    drawWave();
  }
  $("sA").addEventListener("input", () => sync("a"));
  $("sB").addEventListener("input", () => sync("b"));
  $("sfile").addEventListener("change", async () => {
    const f = $("sfile").files[0];
    if (!f) return;
    stopPlay();
    msg("sfinfo", "Reading " + f.name + "…");
    try {
      const buf = await new OfflineAudioContext(1, 1, 44100).decodeAudioData(await f.arrayBuffer());
      const mono = new Float32Array(buf.length);
      for (let ch = 0; ch < buf.numberOfChannels; ch++) { const d = buf.getChannelData(ch); for (let i = 0; i < d.length; i++) mono[i] += d[i] / buf.numberOfChannels; }
      src = { name: f.name.replace(/\.[^.]*$/, ""), rate: buf.sampleRate, mono, dur: buf.duration };
      $("sA").value = 0; $("sB").value = 1000;
      $("spv").disabled = $("swr").disabled = false;
      sync("a");
    } catch { src = null; drawWave(); msg("sfinfo", "Could not read that file. Try a WAV, MP3 or OGG.", "bad"); }
  });
  $("spv").addEventListener("click", () => {
    if (pnode) { stopPlay(); return; }
    if (!src) return;
    const [a, b] = secs();
    play(src.mono.slice(Math.floor(a * src.rate), Math.floor(b * src.rate)), src.rate);
    $("spv").textContent = "Stop";
  });
  $("swr").addEventListener("click", () => {
    if (!src) return;
    const [a, b] = secs(), rate = +$("srate").value, free = SMP_CAP - usedBytes(list);
    if (list.length >= SMP_MAX) { msg("swmsg", "All " + SMP_MAX + " slots are full.", "bad"); return; }
    const c = makeChunk(src.mono, src.rate, a, b, rate, $("snorm").checked, (src.name.slice(0, 10) + (list.length + 1)).toUpperCase());
    if (!c) { msg("swmsg", "That part is too short.", "bad"); return; }
    if (c.data.length > free) { msg("swmsg", "Does not fit. " + kb(free) + " left: " + (free / rate).toFixed(2) + " s at this rate. Shorten it or pick a lower rate.", "bad"); return; }
    list.push(c);
    render();
    changed();
    msg("swmsg", "Written to S" + String(list.length).padStart(2, "0") + ": " + (c.data.length / rate).toFixed(2) + " s, " + kb(c.data.length) + ".", "ok");
    msg("sumsg", "Not on the FM-1 until you upload.", "");
  });
  $("sclr").addEventListener("click", () => { list = []; render(); changed(); msg("sumsg", "List cleared. Upload to empty the FM-1's memory.", ""); });
  $("sup").addEventListener("click", async () => {
    if (busy) return;
    busy = true;
    $("sup").disabled = $("sclr").disabled = true;
    try {
      msg("sumsg", await opts.upload(buildImage(list), (t) => msg("sumsg", t, "")), "ok");
    } catch (e) {
      msg("sumsg", String(e && e.message || e), "bad");
    }
    busy = false;
    $("sup").disabled = $("sclr").disabled = false;
  });
  render();
  drawWave();
}
