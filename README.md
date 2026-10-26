# BBL JAM BOX

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

**Beta.** A live-jamming groovebox firmware for the M-VAVE FM-1, in the style of Digitone / Model:Cycles, by
Blair (blairysbeatlab). It is a **modified version of [Felucca](https://github.com/hugelton/Felucca)** by Leo Kuroshita
(@kurogedelic), Hügelton Instruments: most of Felucca has been taken out and the groovebox built on what was left.

Not made or supported by M-VAVE. Installing it is at your own risk.

- 6 tracks, a 16-step sequencer per track with parameter locks, chance and trig conditions
- **DIGITAL**: a 4-operator FM engine (SYN, BD, SD, HH, CYM, PERC)
- **ANALOG**: 808-style drums (KICK SNARE TOM RIM CLAP COWBELL HAT CYMBAL) with an AGE knob
- **SAMPLER**: your own sounds, uploaded from the browser (up to 32, about 11 s in all)
- Master DJ filter and comb, delay, a plate reverb, beat repeats, an arpeggiator, sound and kit saves

## Install

Read **[FLASHING_GUIDE.md](FLASHING_GUIDE.md)** first (also as [plain text](FLASHING_GUIDE.txt)): it explains how to go
back to the official V15 firmware. Then open the installer site in Chrome or Edge and follow it. No extra hardware is
needed; recovering from a failed install needs the [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Build

See [BUILDING.md](BUILDING.md) (Felucca's build: the JieLi toolchain and AC79 SDK). With them installed:

```
./build.sh                       # build/felucca.fwsc
./build.sh --release 0.1-beta    # build/felucca-0.1-beta.fwsc and build/release-0.1-beta/
python3 web/make_site.py build/felucca-0.1-beta.fwsc 0.1-beta OUT_DIR   # the installer site
```

The browser emulator: `sh web/emu/build_wasi.sh`, then serve `build/emu/`.

## Licence and credits

Free software: [GPL-3.0-only](LICENSE), like Felucca. Some parts keep their own licences: see
[LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md) and [LICENSES/](LICENSES/). The logo on the power-on screen
(`assets/logo/`) is Blair's and is not under the GPL.

Thanks to Leo Kuroshita for Felucca, and to everyone credited in [CREDITS.md](CREDITS.md).

## AI disclaimer

Like Felucca, this firmware is developed with the help of AI coding agents. The design, features, sound design and
direction are Blair's.
