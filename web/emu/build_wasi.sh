#!/bin/sh
# The browser emulator without Emscripten: clang from wasi-sdk (downloaded once to ~/wasi-sdk), the same
# sources as web/emu/build.sh. Run after one ./build.sh.   web/emu/build_wasi.sh  ->  build/emu/
#   cd build/emu && python3 -m http.server 8123     then open  http://localhost:8123/
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "run ./build.sh once first"; exit 1; }
W="$HOME/wasi-sdk"
if [ ! -x "$W/bin/clang" ]; then
    echo "downloading wasi-sdk (once, 110 MB)..."
    curl -L -o /tmp/wasi-sdk.tgz https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux.tar.gz
    mkdir -p "$W" && tar xzf /tmp/wasi-sdk.tgz -C "$W" --strip-components=1
fi
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
mkdir -p build/emu/fonts
"$W/bin/clang" --sysroot="$W/share/wasi-sysroot" --target=wasm32-wasi -O2 -ffp-contract=off -std=gnu99 -w \
    -DFELUCCA_FM4=1 -DFELUCCA_SIMPLE=1 "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -Iweb/emu/shim \
    -mexec-model=reactor -Wl,--export-dynamic -Wl,--initial-memory=33554432 -Wl,-z,stack-size=1048576 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/ (preview)/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ready. Run:  cd build/emu && python3 -m http.server 8123   then open http://localhost:8123/"
