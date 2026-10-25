# CLAUDE.md

## Project

A live-jamming groovebox firmware for the M-VAVE FM-1 in the style of
Digitone / Model:Cycles, started from Felucca (everything else of Felucca has
been taken out):

- One 4-op FM DIGITAL engine with machines: SYN, BD, SD, HH, CYM, PERC
- One ANALOG 808 engine (ported from docs/808lab.html) with machines: KICK
  SNARE TOM RIM CLAP COWBELL HAT CYMBAL
- One SAMPLER engine (docs/samplelab.html): plays uploaded sample chunks
- 6 tracks
- Minimal menus

Build flags: `FELUCCA_SIMPLE=1` and `FELUCCA_FM4=1`.

## Key files

- `firmware/src/ui_gb.c` — groovebox screen code
- `firmware/src/mfx.c` — master DJ filter and comb
- `firmware/src/ui.c` — macro system (PERF page), `pm_*` functions
- `firmware/src/eng_digital.c` — the FM engine (its presets and filter too)
- `firmware/src/eng_analog.c` — the 808 engine (AGE weights, level trims)
- `firmware/src/eng_sampler.c` — the sampler (sample memory layout at the top);
  `gb_store.c` smp_scan / smp_peaks read the table and the waveform
- `web/smpimg.js` — builds the sample memory image; `web/smpui.js` the uploader
  panel; `web/samples.html` + `web/fm1smp.js` the FM-1 Samples page and its USB
  transfer; `firmware/src/smp_usb.c` the FM-1's side
- `firmware/src/engines.c` — the engine table and the tracks' power-on sounds
- `firmware/src/repeat.c` — PERF's beat repeats (1/8 1/16 1/32 REV)
- `firmware/src/gb_store.c` — sounds and kits; `persist.c` — device settings
- `firmware/src/ui_buttons.c` — page buttons (a release is a tap)
- `firmware/src/icons_gb.h` — the screen icons (the user's Pixel Slots drawings,
  don't edit); `icons_808.h` is generated from `docs/808lab.html` by
  `tools/gen_icons_808.py`. Icon numbers IC_* and each machine's icon: engines.c
- `docs/808lab.html` — the browser prototype: the spec for the screen header and
  the ANALOG (808) machines

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
  its own PAGES table in params.c (17 pages).
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
- LFO DEST skips DIST and CHO (their effects are gone; ids kept for old saves).
- LFO DEST reaches every knob that moves smoothly (mod.c MDX, appended after
  TIME): ATK DEC SUS REL, the ENV DEST and LFO DEST amounts (E.x, L.x), the
  delay (DTIME DFDBK DTONE), the reverb (RTIME RDAMP GRIT), RES, the filter
  envelope (F.x), the comb (C.MIX CTUNE CFDBK CDAMP) and DJ. Not the knobs that
  switch (TYPE WAVE DEST MACH SYNC TRIG). The delay, reverb, comb and DJ are
  shared: every track's LFO on one adds up (fx.c mod_g_begin). The reverb never
  wobbles into FREEZE. Names are 5 letters at most (that's what fits).
- Drum tracks (BD SD HH CYM PERC) play one voice: a new hit takes over the
  sounding one (in place, no click) and any other fades (voice.c trk_note_on).
- Kick knobs: TUNE PNCH CLIK FM | RATIO FOLD CRSH. PNCH is the pitch drop's
  depth AND length (old PNCH + SWPT in one). FM is a second sine on the kick's
  sine at RATIO (.5 1 2 .. 16), not feedback. Kicks saved before this load with
  their knobs one place off.
- ARP page (ARP button; seq.c ARP_*): ARP on/off, MODE (UP DOWN UPDN CYCLE RAND
  PATRN), RATE (1/64 1/32 16T 1/16 8T 1/8 1/4), FADE (each note quieter).
  CYCLE = the order the keys were pressed; PATRN = a random order picked when a
  note is added, then repeated. Keys (KEY and PARAM mode) and the sequencer's
  notes both go through it: in the sequencer a step ratchets at RATE for the
  whole step (a chord step arps). Recording stores the key's note, not each
  arp note. Stored in the old arp ids (ARP = P_AHOLD, FADE = P_ASWING); no
  latch, swing or arp chance in this build.
- FX track = the FX lane (seq.c fxl, not a 7th track): in SEQ mode black key 7
  selects it (screen grey, oval "FX"; black keys 1-6 go back to a track). 16
  straight 1/16 steps that make no sound: tap a step = a trig, hold it + turn a
  delay/reverb/comb/DJ knob = a lock (other knobs: "FX: MASTER ONLY"). A trig's
  locks lay over those knobs until the lane's next trig (mod.c mg_ov; the knob
  keeps its own value, LFOs wobble on top). Hold step + OCT- clears its locks,
  OCT- clears the lane. Not saved. No LEN/SPEED/CHANCE of its own yet.
- Parameter ids (core.h P_*) never move: saved sounds map by count. New
  parameters go just before P_E0.
- Machines (engines.c MACHINES): each is an engine + that engine's machine
  number, its screen oval, drum or synth, its step-key note and starting
  preset. Adding an engine: a new eng_*.c (engine.h's engine_t), include it
  and add it to ENGINES in engines.c, then add its rows to MACHINES. The
  PRESETS knob (ui.c machine_set) switches machines across engines.

- Screen header (ui_gb.c gb_header): the page's icon at 2x top left, then the
  title and its "n/N" tag, BPM top right. Icon: the machine's on sound/ENV/SEQ/ARP
  pages, the filter type on FILTER pages, the LFO wave on LFO pages, DELAY / VERB /
  COMB (DJ with OCT+) / MACRO (PERF); VOLUME and CLOCK have none. The title drops
  its tag, then shrinks, rather than touch the BPM. Bottom right: the machine pill
  with its icon. PERF's REV pill is the reverse icon.
- A drum hit flashes the header icon inverted (white block) for ~90 ms; only the
  top strip redraws for it (voice.c trk_hit_ms).

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
- PRESETS knob = machine selector (switching engine fades over). ALGORITHM knob = the selected track's volume.
- Sound save slots are per TRACK (16 each), not per machine; a saved sound
  remembers its engine (gb_snd_t used: 1 = DIGITAL, 2 = ANALOG). Old machine
  slots became track slots.
- 808 pages: OSC 1 (4 knobs), OSC 2 (3 knobs + LEVEL), AGE (one knob). LEVEL
  lives in P_LN0, AGE in P_LN1 (old drum-lane ids, so saved, lockable,
  macroable, LFO-able). FM tracks skip the AGE page and hide LEVEL. 18 pages.
- AGE = DRIFT/HISS/GRIT, weights in one block at the top of eng_analog.c
  (all 64 until the user sends the final ones).
- 808s run their own envelopes and filters: the ENV and FILTER pages do
  nothing on an 808 track.
- 808 HAT/CLAP/CYMBAL/RIM/COW get a level trim (eng_analog.c AN_TRIM) so they
  sit with the FM drums (the prototype evened them with a compressor).
- The 808 machine pill is drawn filled (inverted) so you can tell it from FM.
- CLOCK page 4th dial = CPU % (audio load, read-only; 0 in the emulator).
- SAMPLER: one machine "SAMPLE" (PRESETS, after the 808s), one voice a track (a
  new hit takes over, the old sound fades in 4 ms; MACHINES drum = 2: one voice,
  full ENV). OSC 1: PITCH STRCH START LEN, OSC 2: MODE (icons ONCE LOOP PING
  REV) SLOT (S01..S32) LOFI LEVEL, page 3 = GATE (P_LN2: KEY = the note's
  length, 1/16..BAR = loops play that long; one-shots always play to the end).
  Track n starts on slot n. ENV and FILTER work (DIGITAL's filter).
- STRCH and LOFI follow the knob live while a sound plays (knob, macros, LFOs
  sweep them): grains always run (Hann pairs half a grain apart; at 1.00x they
  add back to the sample exactly), each new grain starts STRCH further on.
- Sample memory: flash 0xA0000..0xDBFFF (240 KB), survives power-off and
  firmware updates. 8-bit mu-law (the user chose twice the time over 16 bits):
  ~11 s at 22 kHz in all. Table "FSM2" + CRCs; a bad table = no samples.
- The uploader replaces the whole sample memory each Upload (up to 32 chunks).
  On the real FM-1: the installer site's Samples page (web/samples.html,
  webapp/samples/; web/fm1smp.js) over USB SysEx (firmware/src/smp_usb.c, the
  update's frames: BEGIN / SUM / WRITE / END). Only changed 4 KB sectors are
  sent, the table last; an Upload cut short resumes. The page keeps its list in
  the browser. The panel itself is web/smpui.js (emulator and page share it).
- Machine names and order (engines.c MACHINES, by type, 808 then FM): 8BD FMBD
  8SD FMSD 8TOM 8RIM 8CLP 8COW 8HH FMHH 8CYM FMCYM FMPRC FMSYN SMPL.
- 808s start with AGE 0 (clean).
- REC armed (the selected track): a red "REC" pill left of the BPM; playing, it
  breathes twice a bar. Recording follows the track: change tracks, keep
  recording (ui.c track_select).
- LFO DEST order = the knobs as their pages show them, left to right (params.c
  MDST_ORDER), then PITCH AMP. PAN SHP DEPTH VEL TIME E8 are left out. VIB is
  called L.PIT.
- Overlay on every page (ui_gb.c gb_overlay): six track squares at the bottom
  (track colour; white on a hit; muted = dark with an X; selected = thick edge)
  and the loop border: 3 px round the screen edge, clockwise from the top left,
  white as far as the selected track's loop has got. They redraw straight onto
  the panel (no full redraw).
- SAMPLER waveform: a held step shows its START/LEN locks. Turning START or LEN
  (or locking them) swaps the dials for a zoomed waveform (~1/6 of the chunk)
  round START or the end, the turned bracket bold, "START 20" under it.
