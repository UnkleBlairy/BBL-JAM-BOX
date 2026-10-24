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
- 16-step sequencer only (LEN 1..16). One SEQ page replaces Felucca's STEP /
  DETAIL / AUTOMATION (hidden, not deleted). Knobs: LEN, SPEED (x1/4 x1/2 x1
  x2 x4), CHANCE, TRIG (- 1/4 2/4 3/4 4/4, Elektron-style). All per track.
- CHANCE: nothing held = the track's chance for every step; step held = that
  step's own chance (TRK = follow the track). A step can have chance AND trig.
- Step keys: empty step turns on at press; a lit step turns off on release,
  unless a knob was turned while holding it (hold-to-edit, like Elektron).
- Storage: track CHANCE lives in P_APROB (no ARP in this build); step i's TRIG
  lives in step[i+16]'s chance byte (core.h GB_TRIG) so the save format is
  unchanged.
- SEQ button: from any page opens the SEQ page with step keys on; on the SEQ
  page it toggles the step keys off/on.
- Parameter locks: with step keys on, hold step(s) on any sound page and turn
  a knob = lock. Locked dials show inverted (like macros). Hold step + OCT-
  clears that step's locks. Every sound-page knob locks (LFO AMT/DEST, filter
  TYPE too). Removing a step removes its locks.
- The DJ filter always starts open (centre) at power-on and on project load.
- Saving (gb_store.c): 16 sound slots per machine (6 machines = 96; more
  machines later). Hold SAVE = SAVE screen: white key = save the selected
  track's sound, black key (11) = save kit (4 sounds + macros + FADE + FX/COMB
  globals; not tempo/DJ). Tap SAVE = LOAD screen: key loads and the screen
  closes so the keys play it at once; SAVE or a page button closes it. Patterns
  are NEVER saved: it's for jamming from scratch.
- SAVE?/LOAD? screen is plain text only (no slot boxes): "SOUND - WHITE KEY",
  "KIT - BLACK KEY". After a save/load, "SOUND 3 / LOADED" flashes ~1 s. Key
  lights show which slots are used.
- Saves while playing apply at once but write to flash at stop (a flash erase
  stops the audio). Old Felucca projects / user presets are gone in this build:
  their flash sectors hold the sounds and kits.

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
