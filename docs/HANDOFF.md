# FM-1 groovebox: handoff for a second Claude

Blair is a complete beginner building custom firmware ("Felucca") for the M-VAVE FM-1: a live-jamming, pared-down Digitone / Model:Cycles style groovebox. Work happens in WSL2 Ubuntu in `~/Felucca`. Read `CLAUDE.md` in the repo first, including its "How we work" section. Blair wants small steps he can test on the unit, plain-English explanations, and commands in one paste-able block that starts with `cd ~/Felucca`.

## What is in this folder

| File | What it is |
|---|---|
| `samplelab.html` | Working browser prototype of the SAMPLER engine and the sample uploader app. This is the spec for Work package C. |
| `808lab.html` | Working browser prototype of the 8 analog-style drum machines with the AGE control. Spec for Work package B. |
| `symbols-mockup.html` | Four layout mockups. Blair picked layout B, "header symbol". Reference only. |

Open any of them in a browser to hear and see exactly what is wanted. The JavaScript in each file is the reference implementation: ranges, curves and defaults must match.

## Order of work

Do the packages in this order, one commit each (B and C can each be split into smaller commits). Blair tests every step on the unit, so do not start the next package until the previous one builds, the emulator (`web/emu`) shows it, and Blair has had a chance to flash it. Before coding anything, read the prototype, then tell Blair your plan and any CPU, RAM or flash concerns.

- **A** Header-symbol screen look and the pixel icons (no new sounds).
- **B** ANALOG engine: 8 drum machines with a single AGE knob.
- **C** SAMPLER engine and the sample uploader app.

## Rules for all three packages

- Do not change `G_COUNT` or the saved project format without telling Blair first.
- Every new parameter must be: a destination for LFO and ENV mod slots, macroable and recordable (`pm_record`, `pm_altered`), and p-lockable per step (the `motion_param` path). Enumerated params (MODE, SLOT, machine) accept mod and macros too, but the value is quantised to the nearest option.
- Keep the existing Inter Tight text for now. A pixel font swap is a possible later step.
- Update `ui_help.c` and `CLAUDE.md` for each new thing.
- Measure CPU with the heaviest realistic case and report headroom.

---

## Prompt A: header-symbol screen look

```
Read docs/808lab.html and docs/samplelab.html first (both have the target screen layout in their draw() functions). Then tell me your plan before coding.

1. Icons. If icons_gb.h is not in firmware/src yet, add it. The icon drawings are in the EX table (808lab.html) and the IC table (samplelab.html): small helper calls (H/V/L/R/F/C/poly) on a 16x16 grid. Generate C arrays (uint16_t rows, MSB = left) named ICON_TOM, ICON_RIM, ICON_CLAP, ICON_COWBELL, ICON_AGE, ICON_SAMPLE, ICON_ONCE, ICON_LOOP, ICON_PING and ICON_REV. Write a small script that renders them from the HTML so they can be regenerated.
2. Add gb_icon(x, y, scale, icon) to ui_gb.c: white pixels, respecting strip clipping like gb_oval, with the icon id in gb_view_t so change detection still works.
3. Dial pages use the "header symbol" layout: page icon at 2x (32x32) at (8,6); title starting at x=48; small page tag (such as 1/3) after the title; BPM top right; four columns (label, ring, value); bottom left page dots; bottom right the machine pill with the machine icon at 1x inside it, then the name. Icon per page: the machine icon on machine pages, filter type icon on FILTER, wave icon on LFO, ICON_DELAY, ICON_VERB, ICON_COMB, ICON_DJ_FILTER, ICON_MACRO on PERF, ICON_FADE / ICON_REPEAT / ICON_REVERSE on the PERF fade and repeat row labels where they fit.
4. When a drum triggers, the header icon flashes inverted (white block, icon in track colour) for about 90 ms.
5. Keep macro-inverse dials, tile animation and all other behaviour. Make sure the title never collides with the BPM.
6. Build, check every page in the emulator on all four track colours, show me screenshots, commit.
```

## Prompt B: ANALOG engine (808s)

```
Read docs/808lab.html. It is the spec.

Add a new engine "ANALOG" alongside the FM DIGITAL engine, chosen per track through the same machine selector. It has 8 machines: KICK, SNARE, TOM, RIM, CLAP, COWBELL, HAT, CYMBAL. The voice algorithms are vBD, vSD, vTM, vRM, vCP, vCB, vHH, vCY in the HTML. Port them to efficient C with no per-hit allocation: phase accumulators, biquad coefficients computed at trigger time, simple exponential envelopes. Match the ranges and curves exactly. Page 1 ranges are deliberately wider than a real 808. Defaults are in the MACH table.

Pages per machine: PAGE 1 (4 params), PAGE 2 (4 params, including DRIVE and LEVEL), and an AGE page that is a single knob.

AGE is ONE knob, 0 to 127. Use the HTML behaviour with DRIFT, HISS and GRIT fixed at weight 1.0 (the value 64 in the prototype). Put the three weights in one clearly named constants block so they are easy to change:
- DRIFT: per-hit pitch variance with a slow random walk, decay variance, level variance, late-only timing jitter up to 8 ms at full age;
- HISS: a noise burst per hit that runs through the voice's own decay, so it only sounds while the drum does (no constant hiss, no hum, no crackle);
- GRIT: per-track soft clip, bit reduction down to about 7 bits at full age, and a lowpass falling toward 3.6 kHz at full age.

Follow the rules in the README (mod, macros, p-locks, project format). Measure CPU with 4 tracks on the heaviest machines (HAT, CYMBAL, CLAP). If six oscillators per hat or cymbal hit are too heavy, reduce or share them and tell me the trade-off. Add it to the emulator, update help and CLAUDE.md, commit.
```

## Prompt C: SAMPLER engine and sample uploader

```
Read docs/samplelab.html. Part 1 of the page is the uploader app, part 2 is the sample engine. It is the spec.

PART 1: the engine (firmware)
Add a new engine "SAMPLER" selectable per track like the other engines. It plays 16-bit mono sample chunks held in sample memory. It has everything else the other engines have (filter, envelope, LFO, effects, sends, macros, p-locks, mod slots). Only the oscillator pages are new.

Params, in the order the knobs are on the screen:
- PAGE 1: PITCH, STRETCH, START, LEN
- PAGE 2: MODE, SLOT, LOFI, LEVEL

Details (the HTML is the reference):
- PITCH: raw 0..127, 64 = original; semitones = (raw-64)*24/64, continuous, so plus or minus two octaves. It resamples like an old sampler: pitch changes length. Display as semitones with one decimal.
- STRETCH: raw 0..127, 64 = 1.00x; factor = 4^((raw-64)/64), so 0.25x to about 3.9x duration. Pitch-independent. A deliberately lo-fi grain method: about 40 ms grains, Hann window, 50% overlap, no smoothing. On the unit this needs to run in real time, so implement it as a streaming two-grain overlap-add rather than pre-rendering a buffer.
- START: raw 0..127 across the chunk. LEN: raw 0..127 as the fraction of the chunk from START, clipped at the chunk end, minimum 64 samples. LEN is a length, not an end point, so the window moves with START. The old design had an END param; it was replaced by LEN.
- MODE: 0 ONCE, 1 LOOP, 2 PING (ping-pong), 3 REV (once, reversed). Loops hold for the gate length (a setting) then release over a few ms. LOOP crossfades the end into the start over about 256 samples. PING plays forward then backward without repeating the end samples.
- SLOT: which chunk in sample memory (1..number of chunks, shown S01, S02...). By default track N uses slot N.
- LOFI: raw 0..127. The "Amiga" knob: bits = round(16 - 12*lofi/127); sample-and-hold of 10^(lofi/127) output samples; above about 15% use nearest-neighbour reads with no interpolation, below that use linear interpolation.
- LEVEL: output level.
- Monophonic per track: a new hit chokes the old one in about 4 ms.
- Screen: the "header symbol" layout (see Prompt A) with ICON_SAMPLE. Page 1/2 and 2/2. MODE shows its icon (ICON_ONCE, ICON_LOOP, ICON_PING, ICON_REV) instead of a number. Below the dials, draw a mini waveform of the selected chunk (peaks computed when a chunk is written) with the window from START to START+LEN bright and the rest dim, and a marker at each end of the window. If the slot is empty show NO SAMPLE.

PART 2: the uploader app (web)
- A web page (put it with the existing installer web app if that fits) where the user picks any audio file from their computer. It decodes it and shows the waveform with START and END sliders to choose a chunk, a play button to audition the chunk, a store-rate choice (11, 22 or 32 kHz), a Normalise option, and a Write button.
- Write converts the chunk to mono, box-filter resamples it to the chosen rate, optionally normalises it to 0.97 peak, converts to 16-bit and adds it to the "memory to upload" list. The user keeps adding chunks until the memory meter is full. A chunk that does not fit is refused with the number of seconds that would fit. Chunks can be previewed and removed. Names are the first 10 characters of the file name plus a number.
- Upload sends the list to the unit. Clear memory empties the list.
- Memory capacity in the prototype is a placeholder (256 KB to 4 MB). Find out the real capacity: where sample memory can live on the FM-1 (RAM, PSRAM or flash), how much is free, and whether samples must survive power-off. Tell me before choosing. If samples are held in RAM only, say so plainly and say what the user loses at power-off.
- The upload transport: find out how the web installer in web/ talks to the unit today (USB, serial, MIDI SysEx or WebUSB), and propose a protocol for the chunk transfer: a chunk table (id, 10-character name, sample rate, length in samples), data in checked blocks, a checksum, and the ability to resume or retry. Show me the proposal before implementing it.
- In the emulator (web/emu), pushing Upload should load the chunks straight into the emulated sample engine so I can test the whole flow in a browser.

Follow the rules in the README (mod destinations, macros, p-locks, project format). The project format must not store sample data; it stores which slot each track uses. Measure CPU for four sampler tracks at once, including the stretch grains, and tell me the headroom. Update help and CLAUDE.md, commit.
```

## Open questions to ask Blair

- The real sample memory size and whether samples must survive power-off.
- The final DRIFT, HISS and GRIT weights for AGE (the defaults of 1.0 are placeholders).
- Whether the loop gate length should be a knob instead of a global setting.
- A possible phase D: swapping the screen text to a pixel font (Silkscreen, which is open-licensed) generated with `tools/gen_aa_font.py`.
