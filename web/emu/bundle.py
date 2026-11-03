#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Blair (blairysbeatlab), BBL JAM BOX
"""The emulator as ONE .html file (opens from the computer, no web server): build/emu packed into it.
   EMU_VERSION=v0.4-beta sh web/emu/build_wasi.sh && python3 web/emu/bundle.py OUT.html
The page asks window.BBL_FILES for its files (index.html FILE()): here they are blob URLs made from the copies
inside the page (the wasm and the logo base64, the scripts as text; smpui.js's import of smpimg.js pointed at its
blob)."""
import base64, json, pathlib, sys

E = pathlib.Path(__file__).resolve().parents[2] / "build" / "emu"
out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "bbl-jam-box-emulator.html")
page = (E / "index.html").read_text()
b64 = lambda p: base64.b64encode((E / p).read_bytes()).decode()

font = 'url("fonts/BLBPixel7-Chunky.ttf")'
assert font in page
page = page.replace(font, 'url("data:font/ttf;base64,' + b64("fonts/BLBPixel7-Chunky.ttf") + '")')
logo = 'src="logo.jpg"'
assert logo in page
page = page.replace(logo, 'src="data:image/jpeg;base64,' + b64("logo.jpg") + '"')

js = lambda p: json.dumps((E / p).read_text())
loader = f"""<script>
(() => {{
  const bin = (s, type) => {{ const b = atob(s), u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i);
                              return URL.createObjectURL(new Blob([u], {{ type }})); }};
  const txt = (s) => URL.createObjectURL(new Blob([s], {{ type: "text/javascript" }}));
  const F = {{}};
  F["felucca.wasm"] = bin("{b64("felucca.wasm")}", "application/wasm");
  F["worklet.js"] = txt({js("worklet.js")});
  F["smpimg.js"] = txt({js("smpimg.js")});
  F["smpui.js"] = txt({js("smpui.js")}.split('"./smpimg.js"').join(JSON.stringify(F["smpimg.js"])));
  window.BBL_FILES = F;
}})();
</script>
"""
mod = '<script type="module">'
assert page.count(mod) == 1 and '"./smpimg.js"' in (E / "smpui.js").read_text()
page = page.replace(mod, loader + mod)
out.write_text(page)
print(f"{out}  {out.stat().st_size // 1024} KB")
