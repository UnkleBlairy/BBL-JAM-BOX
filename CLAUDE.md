# CLAUDE.md

## Project

Felucca firmware for the M-VAVE FM-1, being turned into a live-jamming
groovebox in the style of Digitone / Model:Cycles:

- One 4-op FM DIGITAL engine with machines: SYN, BD, SD, HH, CYM, PERC
- 4 tracks
- Minimal menus

Build flags: `FELUCCA_SIMPLE=1` and `FELUCCA_FM4=1`.

## Key files

- `firmware/src/ui_gb.c` — groovebox screen code
- `firmware/src/mfx.c` — master DJ filter and comb
- `firmware/src/ui.c` — macro system (PERF page), `pm_*` functions

## How we work

- Claude is the main developer. The user is a complete beginner: explain
  simply, say in plain words what changed, and give one clear next step.
- Small things (wording, colours, layout, positions): just do them, then tell
  the user when to look.
- Big or unclear requests (new pages, new sound engines, anything that could
  break the sound): give a 2–3 sentence plan first and wait for a yes.
- If a build fails, fix it yourself first. Only ask the user if you're stuck.
- After each change that works, commit with a short plain message
  (e.g. "fade label").
- Never use `git stash`, never delete the user's files, never rewrite history.
- "Undo that" means go back to the last commit.
- A separate Claude chat handles big design thinking, mockups, sound-design
  ideas and the colour picker web app. It built `ui_gb.c`, the PERF macro
  system, `mfx.c` and the browser emulator. If you need to know why something
  was built a certain way, ask the user to check with it.
- When the user makes a lasting design decision (colour, layout, knob
  behaviour, naming), add one short line under "Design decisions" below.

## Design decisions

- The let-go fade is called "FADE" (never "REL"): "FADE -", "FADE 1/16" ..
  "FADE BAR".
- PERF page has no FUNC/ARP knobs: macros always play PINGPONG, the arp is
  always UP-DOWN.
- PERF knobs: 1 FADE (the let-go fade, off..1 bar in 1/16s), 2 DJ filter,
  3 COMB MIX, 4 COMB TUNE. Screen bottom: FADE / FILT, COMB / TUNE.
- PERF: the last three black keys do nothing for now (were fade jumps);
  saved for a future use.

## Workflow (after every change)

1. Build firmware: `./build.sh`
2. Build the web emulator: `sh web/emu/build_wasi.sh`
3. Start the emulator server in the background (if it isn't already running):
   `cd build/emu && python3 -m http.server 8123`
4. Tell the user to refresh http://localhost:8123 — the browser emulator is
   the test. There is no automated test step.

## Flashing

1. `python3 web/make_site.py build/felucca.fwsc dev /tmp/felucca-site`
2. Serve `/tmp/felucca-site` on port 8000 (in the background):
   `cd /tmp/felucca-site && python3 -m http.server 8000`
3. Open http://localhost:8000/webapp/installer/
