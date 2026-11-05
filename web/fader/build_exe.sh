#!/bin/sh
# Builds "BBL Fader.exe" with the Windows Python, from WSL. Once: py.exe -3 -m pip install --user pyinstaller segno
# Out: build/fader/BBL Fader.exe
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WIN=$(wslpath -u "$(cmd.exe /c 'echo %TEMP%' 2>/dev/null | tr -d '\r')")/bbl-fader-build
rm -rf "$WIN"; mkdir -p "$WIN"
cp "$ROOT"/web/fader/bbl_fader.py "$ROOT"/web/fader/phone.html "$ROOT"/web/fader/BLBPixel7-Chunky.ttf "$WIN"/
cd "$WIN"
py.exe -3 -m PyInstaller --noconfirm --onefile --noconsole --name "BBL Fader" --add-data "phone.html;." \
    --add-data "BLBPixel7-Chunky.ttf;." --collect-data segno bbl_fader.py >build.log 2>&1 || { tail -20 build.log; exit 1; }
mkdir -p "$ROOT/build/fader"
cp "dist/BBL Fader.exe" "$ROOT/build/fader/"
ls -la "$ROOT/build/fader/BBL Fader.exe"
