# CLAUDE.md

## Project

A live-jamming groovebox firmware for the M-VAVE FM-1 in the style of
Digitone / Model:Cycles, started from Felucca (everything else of Felucca has
been taken out):

- One 4-op FM DIGITAL engine with machines: SYN, BD, SD, HH, CYM, PERC
- 6 tracks
- Minimal menus

Build flags: `FELUCCA_SIMPLE=1` and `FELUCCA_FM4=1`.

## Key files

- `firmware/src/ui_gb.c` — groovebox screen code
- `firmware/src/mfx.c` — master DJ filter and comb
- `firmware/src/ui.c` — macro system (PERF page), `pm_*` functions
- `firmware/src/eng_digital.c` — the FM engine (its presets and filter too)
- `firmware/src/engines.c` — the engine table and the tracks' power-on sounds
- `firmware/src/repeat.c` — PERF's beat repeats (1/8 1/16 1/32 REV)
- `firmware/src/gb_store.c` — sounds and kits; `persist.c` — device settings
- `firmware/src/ui_buttons.c` — page buttons (a release is a tap)

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
- SEQ button: from any page opens the SEQ page with step keys on (SEQ mode); on
  the SEQ page each tap goes SEQ -> PARAM -> KEY -> SEQ. The mode's name is at
  the bottom of the screen. SEQ's light is on in SEQ mode and breathes in PARAM.
- PARAM key mode (ui_input.c par_*, seq.c par_key): on a sound/FX page the 16
  white keys set one of the 4 knobs (key 1 = lowest, key 16 = highest) and play
  the step note. Hold SEQ + black key 1-4 picks the knob (underlined on screen).
  Black keys 1-6 pick the track. While recording, each key records its note with
  that value locked on the step. OCT+ held + key = a trigless lock (the value,
  no note). On pages that can't lock (SEQ, VOLUME, CLOCK) the keys just play.
  The MACH knob is left out (it needs its starting sound).
- Trigless locks: a step with locks but no note (core.h ST_LOCK). In SEQ mode
  hold OCT+ and press an empty step key to make one, then hold it and turn
  knobs. Its step key breathes; pressing it removes it on release (unless a
  knob is turned).
- A lock stays until the track's next trig (Elektron-style), not just one
  step: only a step that plays a note puts the earlier locks back. Trigless
  lock steps add theirs on top. Stopping puts everything back.
- OCT- clear (SEQ mode) wipes the track's locks and automation too.
- Parameter locks: with step keys on, hold step(s) on any sound page and turn
  a knob = lock. Locked dials show inverted (like macros). Hold step + OCT-
  clears that step's locks. Every knob locks: all sound pages incl. machine,
  every LFO setting, the OCT+-held second sets, and the master controls (DJ,
  COMB, delay TIME/FDBK/TONE, reverb SIZE/DAMP/TYPE: motion.c MOTION_G, ids
  P_COUNT+j). Not the CLOCK page. Removing a step removes its locks.
- COMB page with OCT+ held = DJ filter (so it can be locked; PERF's keys are the
  macros, no step keys there).
- Master FX must never click: DJ and COMB fade in/out over ~23 ms
  (MFX_CSLOPE), the comb loop soft-limits (mfx_soft), the DJ filter updates
  every 32 samples with smooth resonance. Test with a sine + 2nd-difference
  check (jump ~20 = clean) before changing mfx.c.
- ONE reverb only (streamlined): QUAD (fx.c rev_quad), a 90s rack plate
  (Dattorro tank at 11 kHz, stereo). ROOM/SPRING/HALL are not in this build.
  VERB page: REV, TIME, DAMP, GRIT. Clean by default; GRIT (G_RTYPE's id,
  squared so it only bites near the top) adds aliasing, sample-hold and down to
  8 bits. TIME 0..126 = RT60 0.3 s..~9 s (~3.5 s at 90); TIME 127 = FREEZE.
  QUAD maths is 64-bit and rounds toward 0 (qmul): no overflow crackle, and
  tails die to exactly 0 (rounding to the nearest left a faint tone circling
  for ever). FREEZE rounds to the nearest so its tail holds.
- Mutes (seq.c keyboard_block): hold OCT+ and press black key 1-6 (the track
  keys, = track 1-6). Quick press latches the mute, holding mutes only while
  held. With OCT+ held those keys don't select the track.
- PERF page: black keys 1-6 are lit while that track is NOT muted.
- OCT+ held (any page): black keys 1-6 are lit while that track is NOT muted.
- Going to PERF turns the step keys off, so leaving PERF the keys are the keyboard.
  A held macro key is never a step key (knob turns go into the macro, not locks).
- A new step plays the last note or chord played on that track's keys (drums too);
  before any, the machine's own note.
- The DJ filter always starts open (centre) at power-on and on project load.
- Saving (gb_store.c): 16 sound slots per machine (6 machines = 96; more
  machines later). Hold SAVE = SAVE screen: white key = save the selected
  track's sound, black key (11) = save kit (6 sounds + macros + FADE + FX/COMB
  globals; not tempo/DJ). Tap SAVE = LOAD screen: key loads and the screen
  closes so the keys play it at once; SAVE or a page button closes it. Patterns
  are NEVER saved: it's for jamming from scratch.
- SAVE?/LOAD? screen is plain text only (no slot boxes): "SOUND - WHITE KEY",
  "KIT - BLACK KEY". After a save/load, "SOUND 3 / LOADED" flashes ~1 s. Key
  lights show which slots are used.
- No Felucca screens: no layers (holding a button = tapping it, holds are free
  for groovebox features), no MENU (HOME hold does nothing). The groovebox has
  its own PAGES table in params.c (16 pages).
- Saves while playing apply at once but write to flash at stop (a flash erase
  stops the audio). Old Felucca projects / user presets are gone in this build:
  their flash sectors hold the sounds and kits.
- 6 tracks (core.h NPART). Power-on sounds: BD SD CH OH PERC SYN (engines.c
  TRK_DEF). Track colours 1-6: orange, yellow, blue, purple, green, pink.
- HOME taps: VOLUME 1-4 -> VOLUME 5-6 -> CLOCK -> VOLUME 1-4 again (each knob = one
  track's level).
- Kits hold 6 sounds: 3 kits per storage object, 4 objects (the 4th in the old
  FM6 bank's flash pair). Kits saved by the 4-track build convert once at
  power-on; loading one leaves tracks 5-6 as they are.
- A PERF macro's global (BPM, DJ, COMB..) has track number PM_G (255), not
  NTRK, so adding tracks never turns a global into a track.
- The FM-1 starts fresh at power-on (no Felucca autosave / "RESTORED").
- The PRESETS knob does nothing for now (it browsed Felucca's sounds).
- LFO DEST skips DIST and CHO (their effects are gone; ids kept for old saves).
- LFO DEST reaches every knob that moves smoothly (mod.c MDX, appended after
  TIME): ATK DEC SUS REL, the ENV DEST and LFO DEST amounts (E.x, L.x), the
  delay (DTIME DFDBK DTONE), the reverb (RTIME RDAMP GRIT), RES, the filter
  envelope (F.x), the comb (C.MIX CTUNE CFDBK CDAMP) and DJ. Not the knobs that
  switch (TYPE WAVE DEST MACH SYNC TRIG). The delay, reverb, comb and DJ are
  shared: every track's LFO on one adds up (fx.c mod_g_begin). The reverb never
  wobbles into FREEZE. Names are 5 letters at most (that's what fits).
- Parameter ids (core.h P_*) never move: saved sounds map by count. New
  parameters go just before P_E0.
- Machines (engines.c MACHINES): each is an engine + that engine's machine
  number, its screen oval, drum or synth, its step-key note and starting
  preset. Adding an engine: a new eng_*.c (engine.h's engine_t), include it
  and add it to ENGINES in engines.c, then add its rows to MACHINES. The MACH
  knob can only switch between machines of one engine until a second engine
  exists (then it needs to switch engines too). 6 machines have save slots
  (gb_store.c GB_MACH); more need more sound storage objects.

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
