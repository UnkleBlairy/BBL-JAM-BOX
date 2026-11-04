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
- `firmware/src/eng_analog.c` — the 808 engine (CRUSH amounts, level trims)
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
- Sequencer: LEN 1..64, up to 4 pages of 16 (SEQ mode: black keys 8-11 pick the page
  shown, ui.bank / ui_input.c gbs_base; lit = shown, breathing = playing; with one page its key breathes while playing; the bottom says
  "SEQ 2/4"). For fast parts use more steps at x1, not x1/4. One SEQ page replaces Felucca's STEP /
  DETAIL / AUTOMATION (hidden, not deleted). Knobs: LEN, SPD (speed: x1/4 x1/2 x1
  x2 x4), CHNC (chance), TRIG (Elektron's list: - 1:2 2:2 1:3 .. 8:8 FILL !FILL PRE
  !PRE NEI !NEI 1ST !1ST; core.h GB_TC0, seq.c gb_tc_miss). All per track.
- FILL MODE: while playing, hold PLAY (250 ms) = fill on, a "FILL MODE / ENTER
  TRIGS" banner; a step entered then gets the FILL trig. A quick PLAY tap
  still stops (on release while playing). NEI = the track to the left.
- CHANCE: nothing held = the track's chance for every step; step held = that
  step's own chance (TRK = follow the track). A step can have chance AND trig.
- Step keys: empty step turns on at press; a lit step turns off on release,
  unless a knob was turned while holding it (hold-to-edit, like Elektron).
- Storage: track CHANCE lives in P_APROB; a step's TRIG lives in its acc
  byte as GB_TC0 + n (core.h gb_tc; patterns are never saved).
- SEQ button: a tap = KEY <-> SEQ mode, the page stays (from PARAM / LOCKS: SEQ).
  A double tap (350 ms) = open the SEQ page, the mode as it was.
  Hold SEQ 700 ms = PARAM / LOCKS picking (ui_input.c seq_pick), like the LFO
  picker: it stays after letting go (pages can change) until a knob is turned
  (PARAM for it), a white key (LOCKS), or SEQ is tapped (cancel, no toggle).
  Pop-up pages: WHITE KEY / = LOCKS, WIGGLE KNOB / = PARAM, TAP SEQ / = CANCEL. Hold SEQ + turn a knob = PARAM mode for that knob;
  hold SEQ + white key = LOCKS mode (ui_input.c seq_knob / seq_white). The
  mode's name is at the bottom of the screen. SEQ's light is on in SEQ mode and
  breathes in PARAM.
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
- Clearing needs OCT- pressed twice within 0.8 s (SEQ mode, ACID, the FX lane; on release; ui_input.c cla_poll, CLR2_MS). The first press
  says "OCT- AGAIN TO CLEAR". The track's clear wipes its locks and automation too.
- SEQ mode: hold a track key (black 1-6) + OCT- = that track's sound back to its machine's starting sound
  ("SOUND RESET"; steps, mix, SEQ and ARP settings stay; ui_input.c, apply_preset_to + machine_defaults).
- CLEAR ALL: hold OCT- 1 s alone (any mode) = "CLEAR ALL? / PRESS OCT+"; OCT+ = a fresh start: every track's
  steps, locks, LOCKS keys and the FX lane gone, the power-on sounds, mix and FX back (tempo, swing, clock, macros
  stay); let go = cancel (ui_input.c cla_*). No instant OCT- + OCT+ any more.
- Parameter locks: with step keys on, hold step(s) on any sound page and turn
  a knob = lock. Locked dials show inverted (like macros). Hold step + OCT-
  clears that step's locks. Every knob locks: all sound pages incl. machine,
  every LFO setting and the master controls (DJ,
  COMB, delay TIME/FDBK/TONE, reverb SIZE/DAMP/TYPE: motion.c MOTION_G, ids
  P_COUNT+j). Not the CLOCK page. Removing a step removes its locks.
- No OCT+ second sets of knobs. OCT+ held + knob = a temporary change (ui_input.c
  mom_*): let go of OCT+ and it snaps back (no keep). On PERF too (DJ, COMB MIX, TUNE; not FADE).
- GLO pages: PERF, COMB, DJ (the DJ filter's own page, so it can be locked), AMP (AMP = the whole mix's level,
  G_AMP; PAN = the whole mix's balance, G_PAN, lockable and an FX LFO DEST "PAN"; lockable, LFO-able: ducking / pulsing; ~3 ms slew so a square never clicks, fx.c gain_ramp).
- LFO pages: LFO (RATE WAVE DEST AMT: matrix slot 1, SRC always LFO, ui_input.c msrc_fix), SYNC (SYNC TRIG POL
  FADE; PHASE on no page). ENV DEST page = DEST AMT (slot 2, SRC always ENV).
  ENV button: ENV -> VOL PAN (the track's LVL and PAN, lockable, LFO/ENV DESTs "VOL" (mod.c MDX) and "PAN") -> ENV DEST. The old fixed-depth LFO DEST /
  ENV DEST knobs are gone from the pages (a saved sound keeps its values).
- Holds are 500 ms (HINT_MS, FILL_MS); the mod picker needs 700 ms (TM_MS). A
  panel tap can last 250 ms. Timestamps stored as fm1_ms | 1 can be 1 ms ahead:
  compare them signed ((int32_t)(fm1_ms - t0)), or a fresh press reads as held
  for ever (that was the instant picker and the SEQ "lock-up" on the FM-1).
- Touch modulation (ui.c / ui_input.c tm_*): hold LFO 500 ms = pick mode (a
  quick tap still opens the LFO page). Pick mode stays after letting go: the
  banner "LFO: PICK A DEST" shows, any page can be opened. The first knob turned
  = the DEST, more turns of it = AMT on the big centred pop-up (left -, right +);
  it closes 1 s after the last turn (held or not), then an LFO tap opens the
  LFO page. Tap LFO again while picking = cancel, OCT- = DEST off. A knob
  turned before the 700 ms is an ordinary turn.
  While picking, LFO flashes with the beat and the DEST's page button is lit.
  While picking, the other sources' buttons (ENV, FILTER) only open their pages,
  and SELECT sets the current DEST's amount (the quick way back to it). The
  matrix's per-block values also apply at each note's start (voice.c), so
  knobs read only at note-on (a kick's RATIO) are modulated too.
  Knobs that step (TYPE WAVE PHASE FADE SYNC TRIG POL) modulate too (stepping);
  MACH and the SEQ / ARP / CLOCK knobs do nothing (no message). Stored in matrix slot 1 (P_M1SRC/DST/AMT).
  Same for hold ENV = "AMP ENV" (slot 2, SRC ENV) and hold the FILTER button
  (SCL) = "FILT ENV" (slot 3, SRC FENV: the filter envelope, mod.c MS_FENV, set
  by eng_digital.c / eng_sampler.c; 0 on an 808). One DEST per source.
- Reset a page: hold its page button (FX SCL ENV LFO EDIT GLO ARP, not SEQ) + OCT- = its knobs back to the
  start (the machine's starting sound on its pages, else defaults; not MACH). "OSC 1 RESET" (ui.c page_reset).
- Pop-ups (ui_gb.c gb_banner): a white box with the page's colour as ink, at the
  bottom over the mode/pill line and the track squares (hidden under it). Big
  chunky text only (AF_L, the title's size): pages of two lines (ui_gb.c
  gb_ban_fill), a long hint pages every 2 s (e.g. SEQ held: "WHITE KEY / =
  LOCKS" then "WIGGLE KNOB / = PARAM"); a message too long for one line splits
  at its middle space. The large font has ! : = > for them (gen_aa_font.py
  L_CHARS). Mode changes (SEQ KEY PARAM LOCKS) show no pop-up; messages ~1.2 s.
  None over the SAVE?/LOAD? screen (whose lines are all big too). The amount
  pop-up: "LFO > TUNE" big, the dial, the amount big under it. Every ui_message shows there. Held hints
  (ui_input.c hint_poll, 250 ms): SEQ "KNOB = PARAM  WHITE KEY = LOCKS" (its
  release is then no tap), OCT+ "TEMP CHANGE / TURN A KNOB TO TRY IT", then
  "RELEASE TO REVERT". The big centred pop-up (touch mod) is white too.
- Black background on every track (Blair kept it, 2026-11-04): the track's colour only as a strip behind the header
  (ui_gb.c).
- Layout: 5 px loop border, dials up (labels y 48, centre 94), bottom line at
  y 182 (key mode in the large font when it fits), squares at y 214.
- PARAM mode keeps its knob when you change pages; the bottom says "PARAM CUT".
- ARP button pulses with the beat while the selected track's ARP is on.
- FILL steps breathe (like trigless locks) instead of being lit.
- Copy / paste trig (ui_input.c clip_*): SEQ mode, hold a step + SAVE = "TRIG
  COPIED" (notes, CHANCE, TRIG, locks); hold an empty step = "PASTE TRIG / PRESS
  SAVE", SAVE pastes. Not saved.
- Master FX must never click: DJ and COMB fade in/out over ~23 ms
  (MFX_CSLOPE), the comb loop soft-limits (mfx_soft), the DJ filter updates
  every 32 samples with smooth resonance. Test with a sine + 2nd-difference
  check (jump ~20 = clean) before changing mfx.c.
- ONE reverb only (streamlined): QUAD (fx.c rev_quad), a 90s rack plate
  (Dattorro tank at 11 kHz, stereo). ROOM/SPRING/HALL are not in this build.
  VERB page (FX track): TIME, DAMP, GRIT. Clean by default; GRIT (G_RTYPE's id,
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
  before any, the machine's own note: middle C (60) for FM SYN, MONO, the 808s and the sampler; the FM drums keep
  their tuned notes (36 50 84 72 48: at 60 a kick would jump two octaves). A track clear, SOUND
  RESET and CLEAR ALL forget the keys' note (seq.c lastc), so new steps are back on that note until a key is played.
  A saved SOUND keeps the step note / chord (gb_store.c GB_SN_MARK, in bytes a sound doesn't use; sounds saved before: middle C).
  A KIT keeps each track's step note / chord (the last 32 bytes of its LOCKS-keys area, gb_store.c GB_KLK_LK, marked
  5A A5; kits saved before have none = middle C); loading it puts them back. The last jam keeps them too. The key that
  loads a sound / kit makes no note (seq.c kb_eat).
- Each track remembers PARAM, LOCKS or ACID mode (the modes whose black keys pick tracks; ui_input.c trk_mode_poll,
  trk_sg): picking the track again brings it back. KEY and SEQ are not remembered (scrolling tracks in SEQ stays in
  SEQ); a track left in KEY/SEQ picked from PARAM/LOCKS goes to SEQ; one not visited yet keeps the current mode.
  Each track keeps its own octave (OCT-/OCT+; trk_oct). Not saved.
- The DJ filter always starts open (centre) at power-on and on project load.
- Saving (gb_store.c): 16 sound slots per machine (6 machines = 96; more
  machines later). Hold SAVE = SAVE screen: white key = save the selected
  track's sound, black key (11) = save kit (6 sounds + macros + FADE + FX/COMB
  globals + the LOCKS keys, 264 bytes of them: ~30-40 keys, gb_store.c GB_KLK;
  not tempo/DJ). Tap SAVE = LOAD screen: key loads and the screen
  closes so the keys play it at once; SAVE or a page button closes it. Patterns
  are NEVER saved: it's for jamming from scratch.
- SAVE?/LOAD? screen is plain text only (no slot boxes): "SOUND - WHITE KEY",
  "KIT - BLACK KEY". After a save/load, "SOUND 3 / LOADED" flashes ~1 s. Key
  lights show which slots are used.
- No Felucca screens: no layers (holding a button = tapping it, holds are free
  for groovebox features), no MENU (HOME hold does nothing). The groovebox has
  its own PAGES table in params.c (+2 ACID pages, shown for a track playing ACID).
- Saves while playing apply at once but write to flash at stop (a flash erase
  stops the audio). Old Felucca projects / user presets are gone in this build:
  their flash sectors hold the sounds and kits.
- 6 tracks (core.h NPART). Power-on sounds: BD SD CH OH PERC SYN (engines.c
  TRK_DEF). Track colours 1-6: orange, yellow, blue, purple, green, pink.
- HOME taps: MIX 1-2 -> MIX 3-4 -> MIX 5-6 -> CLOCK -> MIX 1-2 again (each MIX page = two tracks'
  VOL and PAN; params.c MIX_PAN).
- PLAY twice (stop, then PLAY again within 400 ms) = ALL SOUND OFF: every voice cut, delay / reverb /
  comb emptied, stays stopped (seq.c PANIC_ALL).
- Kits hold 6 sounds: 3 kits per storage object, 4 objects (the 4th in the old
  FM6 bank's flash pair). Kits saved by the 4-track build convert once at
  power-on; loading one leaves tracks 5-6 as they are.
- A PERF macro's global (BPM, DJ, COMB..) has track number PM_G (255), not
  NTRK, so adding tracks never turns a global into a track.
- LAST JAM (gb_store.c jam_*): a second after everything goes quiet (stopped, no key,
  nothing sounding) the whole jam is written if it changed: machines, sounds, seq settings,
  all steps, locks, LOCKS keys, macros, FADE, FX lane, tempo, kit globals, selected track
  (not the DJ, it starts open; not the key mode). Power-on restores it ("LAST JAM"). Stop
  before switching off. Storage: OBJ_AUTOSAVE + OBJ_JAM2 (0xDA000, taken from the samples).
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
  selects it (screen grey, oval "FX"; black keys 1-6 go back to a track). LEN 1..64
  straight 1/16 steps (its SEQ page shows LEN only; page keys as a track's) that make no sound: tap a step = a trig, hold it + turn a
  delay/reverb/comb/DJ knob = a lock (other knobs: "FX: MASTER ONLY"). A trig's
  locks lay over those knobs until the lane's next trig (mod.c mg_ov; the knob
  keeps its own value, LFOs wobble on top). Hold step + OCT- clears its locks,
  OCT- clears the lane (LEN stays). No SPEED/CHANCE of its own.
- FX TRACK (fxl_sel; black key 7 in SEQ mode, or ALGO turned past 6, ALGO back = the track it came from; ui.c fxt_set): its own pages; no X
  screens: a button with no page on this side switches sides (GLO on a track = the FX track's PERF; OSC/ARP
  on the FX track = back to the track; ui.c fxt_set, side_page). SEQ tapped on PERF = back to the track's
  page with the steps on. GLO (PERF COMB DJ AMP) is the FX track's only. FX button = DELAY (TIME FDBK TONE) / VERB (TIME DAMP GRIT), only here;
  FILTER / ENV / LFO buttons (left to right) = its LFO 1 / 2 / 3 (RATE: synced note values, WAVE SIN TRI UP DOWN SQR S&H,
  0..1 from the knob's value, DEST, AMT; mod.c fxlfo_*, restart at PLAY). DESTs: AMP DJ the comb's 4, the
  delay's 3, the reverb's 3, D.SND R.SND (every track's sends: AMT left ducks them), D.OUT R.OUT (the
  echoes / the reverb's sound: the sidechain duck). Hold FILTER/ENV/LFO on the FX track = the picker for
  LFO 1/2/3. Macros can hold AMP and every FX LFO knob. Clearing an FX lane trig/lock puts the knobs back
  at once (ui.c fxl_refresh). Tracks' FX button = SENDS (DLY, REV); FX double-tapped (350 ms) on a track = the FX track's DELAY (ui_input.c page_tap), back on a track = SENDS again. Storage: the old unused
  globals (G_SLOT.. G_DRREV -> G_F1RATE.. G_AMP) + G_F1WAVE..G_F3WAVE; kits keep the LFOs (WAVEs in rsv),
  not AMP; the last jam keeps all. FX lane locks: AMP and the 3 AMTs too (MOTION_G).
- Next: the ALL track (ALGO past FX): relative moves on every track first (maybe snap later), with a way to
  leave tracks out.
- Parameter ids (core.h P_*) never move: saved sounds map by count. New
  parameters go just before P_E0.
- Machines (engines.c MACHINES): each is an engine + that engine's machine
  number, its screen oval, drum or synth, its step-key note and starting
  preset. Adding an engine: a new eng_*.c (engine.h's engine_t), include it
  and add it to ENGINES in engines.c, then add its rows to MACHINES. The
  PRESETS knob (ui.c machine_set) switches machines across engines.

- Screen header (ui_gb.c gb_header): the page's icon at 2x top left, then the
  title and its "n/N" tag, BPM top right at the title's size (AF_L). Icon: the machine's on sound/ENV/SEQ/ARP
  pages, the filter type on FILTER pages, the LFO wave on LFO pages, DELAY / VERB /
  COMB (DJ with OCT+) / MACRO (PERF); VOLUME and CLOCK have none. The title drops
  its tag, then shrinks, rather than touch the BPM. Bottom right: the machine pill
  with its icon. PERF's REV pill is the reverse icon.
- A drum hit flashes the header icon inverted (white block) for ~90 ms; only the
  top strip redraws for it (voice.c trk_hit_ms).

- LOCKS key mode (hold SEQ + white key; seq.c kit_*, ui_input.c kit_*): each white key is a
  playable trig-locked step. Unbaked keys play the sound as the knobs set it; hold a key + turn knobs = bake up to 8
  locks into that key only (heard at once; key + OCT- clears it). The knobs and dials always show the sound's own
  values (a held key shows its locks inverted); turning a knob edits the sound's own value. Recording: the step gets
  the key's locks. Black keys 1-6 pick the track; keys with locks are lit. Not saved (like macros).
  8 LOCKS keys a track (white keys 1-8, seq.c KIT_KEYS); keys 9-16 do nothing in LOCKS mode (free for later).
- PITCH (semitones, -24..+24; P_TRANS, so no new id and old saves load): FM SYN's TUNE page KNOB 2, the sampler's OSC 1
  KNOB 1 (replaced its old fine PITCH; E0 is ignored). Taken as each note starts (voice.c voice_pitch16), so it locks
  per step/key. Drums use TUNE. TRN no longer moves the keys (KB_TRN).
- Name: BBL JAM BOX (Blair = blairysbeatlab). "FM-1" only says what it runs on. Splash: Blair's logo
  (assets/logo/MEE.jpg, tools/gen_splash_logo.py), "BBL JAM BOX v0.1-beta", "BASED ON FELUCCA". The logo is
  not under the GPL. The site's package is bbl-jam-box-X.Y.fwsc.

- Live recording is unquantized: each note goes on the nearest step and keeps its NUDGE
  (-8..+7 sixteenths of a step, core.h step_nudge; notes on one step = a chord, the first
  note's timing). QUANTIZE per track (P_SQNT), OFF by default, is for RECORDING only (ON =
  new notes go in with no nudge); nudges always play (seq.c SEQ_QPLAY). Hold REC 500 ms =
  ON/OFF ("QUANTIZE OFF" pop-up). Hold step(s) + SELECT = nudge, the ruler pop-up (ui_gb.c
  gb_nudge). Off-grid steps don't breathe.
- Page keys (SEQ mode, black 8-11; ui_input.c gbs_page_*): tap = show it, or past LEN add it
  ("PAGE 3 ADDED", LEN = its end). Hold 500 ms = LEN snaps to that page's end (shorter keeps
  the later steps, just unplayed). Hold + OCT- = delete that page and the ones after (their
  steps and locks cleared; "PAGE 1 STAYS"). Turning LEN down never deletes.
- Screen font: Blair's own BLB Pixel 7 Chunky (assets/fonts/BLBPixel7-Chunky.ttf, SIL OFL 1.1),
  sizes from the Screen Lab app: S 12 px, M 15 px (+0.5 px tracking), L 28 px (gen_aa_font.py
  preset bbl-pixel). Not the Square Pixel-7 font (personal-use licence).

- ACID mode (Metropolis-style; seq.c acid_*, ui_input.c acid_*, ui.sgrid 4, song.grid 8): a track plays its steps
  OR its ACID stages (acid.play; both kept). In SEQ mode tap OCT+ = flip the track to ACID, tap again = back to its
  steps (also SEQ held / picker + OCT+; the picker pop-up has "OCT+ / = ACID"). SEQ and ACID mode follow the selected
  track (ui_input.c acid_follow): a SEQ tap from KEY gives ACID mode on an ACID track; the SEQ family shows STAGE/ACID
  in place of the SEQ page there. White keys = stages 1-16 (lit = plays a note, the playing one flips, the picked one
  breathes; stopped they sound their note), black 1-6 tracks, black 8 = STAGE page (PITCH 0..12 from middle C shown
  as the note, PULSE 1-8, GATE MUTE FIRST EVERY HOLD, OCT -3..+3), black 9 = ACID page (LEN 1-16, DIR FWD REV PING
  RAND BRWN, SPD = the track's, GATE % of a pulse). Hold a stage key 500 ms = its SLIDE pop-up ("SLIDE ON / TIME 40"):
  KNOB 1 SLIDE on/off, KNOB 2 its TIME (per stage). No skip/accent/scale/root (cut to keep it simple). One pulse = a
  1/16 at SPD, swung like the steps. SLIDE = the note before holds and glides in, legato (voice.c slide_glide 2,
  trk_slide). OCT- = the track's ACID cleared; stage held + OCT- = that stage reset; CLEAR ALL resets every ACID.
  Saved in the last jam, not in kits. Not lockable / LFO-able.
- No MASTER LENGTH reset any more (Blair, 0.4: it cut odd lengths / polyrhythms). seq.c mst_step counts straight
  1/16s since PLAY; a LEN / page / SPD change while playing jumps that track at once to where it would be had it played
  that way since PLAY (mst_follow; Elektron-style). The FX lane's step = the count mod its LEN. ACID goes its own way.
- Emulator page (web/emu/index.html): the firmware's look (black, BLB Pixel font, track colours, white boxes). Computer
  keys (Blair's map): 1-6 FX..GLO, Q-Y HOME..REC, 7 8 OCT- OCT+, white keys Z../ then U..], black keys S D F H J L ; '
  then 9 0 =. MIDI LEARN: press it, click any knob, move a control (bbl-emu-cc). One-file copy for Patreon:
  web/emu/bundle.py (no server needed).
- Screen mirror (test): firmware/src/mirror.c streams every lcd_fill / lcd_blit as RGB 3-3-2 run-length over USB
  SysEx (cmd 0x70, only while a page keeps asking; ~6-9 KB a full page; 04 = a whole picture, the page shows only those; behind by > 3 KB at a picture's end = the
  next draws left out, then one fresh full redraw, so the lag stays short); over the USB serial port (CDC, Web Serial; S start, M keep alive; Windows' MIDI
  only manages ~20 KB/s) or SysEx as the fallback; web/mirror.html (site: webapp/mirror/) draws
  it pixelated (the 8-bit look is wanted) and can play the FM-1's USB audio. H hides the controls (OBS).

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
- PRESETS knob = machine selector (switching engine fades over). ALGORITHM knob = track select (1-6).
- Sound save slots are per POOL (16 each; engines.c gb_pool): KICK (8BD FMBD), SNARE
  (8SD FMSD 8CLP), CYMBAL (8HH FMHH 8CYM FMCYM), PERC (8TOM 8RIM 8COW FMPRC), SYNTH, SAMPLE.
  The selected track's machine picks the pool (SAVE screen top right, "KICK 3 SAVED"); a
  sound keeps its machine, so loading switches the track's. The per-track saves moved into
  the pools once (gb_store.c gb_pool_move, magic GBS3).
- 808 pages: OSC 1 (4 knobs), OSC 2 (3 knobs + LEVEL), CRUSH (one knob). LEVEL
  lives in P_LN0, CRUSH in P_LN1 (old drum-lane ids, so saved, lockable,
  macroable, LFO-able). FM tracks skip the CRUSH page and hide LEVEL.
- CRUSH replaced AGE (Blair, Octatrack-style): one knob, sample-rate and bit reduction together, a held sample with no
  smoothing, so the aliasing stays. 0 = clean; top = ~40 x fewer samples, ~5 bits (CRUSH_RATE, CRUSH_BITS, top of
  eng_analog.c). Measured after AN_TRIM, so quiet machines crush alike. An old sound's AGE value loads as CRUSH.
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
- START / LEN = a window of LEN's size that START slides, the same size, until its end meets the sample's end: then
  START keeps going and the window shrinks (to 64 samples at the very end; eng_sampler.c smp_window). Each grain
  keeps the loop it started with (smp_v_t g[3]), so a resize fades in at the next grain. The LEN knob always shows the real
  size (ui_input.c smp_len_fit): START turned into the end lowers LEN with it, LEN can't wind up past the end, so a
  turn left always closes the braces in. Both live while a sound plays (knob, locks, LFOs,
  macros): the playhead keeps its place in the window (a scrub), each new grain takes the moved window so the grains'
  overlap fades it in (no clicks).
- STRCH and LOFI follow the knob live while a sound plays (knob, macros, LFOs
  sweep them): grains always run (Hann pairs half a grain apart; at 1.00x they
  add back to the sample exactly), each new grain starts STRCH further on.
- Sample memory: flash 0xA0000..0xD9FFF (232 KB; the last 8 KB went to the last jam),
  survives power-off and firmware updates. 8-bit mu-law (the user chose twice the time
  over 16 bits): ~10.6 s at 22 kHz in all. Table "FSM2" + CRCs; a bad table = no samples.
- Samples and install pages (web/samples.html, web/index_pkg.html): the firmware's look, as the emulator page; make_site.py inlines the font and logo (inline_look), so each stays one file. The install page's progress and status stay below its sections, as Install and Return to V15 share them.
- Uploader preview (web/smpui.js): Play shows a playhead on the waveform; Loop plays the part round and round, Start / End move it live
  (sliders 100000 steps, arrow keys for tiny moves). The page asks the FM-1 its memory size (fm1smp.js, BEGIN's cap).
- The uploader replaces the whole sample memory each Upload (up to 32 chunks).
  On the real FM-1: the installer site's Samples page (web/samples.html,
  webapp/samples/; web/fm1smp.js) over USB SysEx (firmware/src/smp_usb.c, the
  update's frames: BEGIN / SUM / WRITE / END). Only changed 4 KB sectors are
  sent, the table last; an Upload cut short resumes. The page keeps its list in
  the browser. The panel itself is web/smpui.js (emulator and page share it).
- Machine names and order (engines.c MACHINES, by type, 808 then FM): 8BD FMBD
  8SD FMSD 8TOM 8RIM 8CLP 8COW 8HH FMHH 8CYM FMCYM FMPRC FMSYN MONO SMPL.
- 808s start with CRUSH 0 (clean).
- FM SYN (eng_digital.c syn_render / SYN_D, Digitone-style): A and B are heard (MIX between them), C and D only
  modulate. Pages: OSC 1 = ALG (8: 1 C>A D>B, 2 C>A D>A, 3 C>A D>A+B, 4 C>A+B D>B, 5 C+D>A+B, 6 D>C>A, 7 D>C>A+B,
  8 D>C>A D>B), A:B (one knob through every pair of .5 1 2..8, A <= B, rising), C, D (ratios .25..16);
  MOD C = C.ATK C.DEC C.AMT MIX; MOD D = D.ATK D.DEC D.AMT FB (D on itself); TUNE = DTUN (A up / B down, ~25
  cents) PITCH. DEC at the top = HOLD. No velocity (the keys have none). Stored: E0..E6 + the old operator
  envelope ids P_FM3_* / P_FM4_* (TP labels C.ATK.. DTUN, LFO DESTs too). FM SYN sounds saved before load
  but sound different. The FM drum machines are unchanged (their legacy path). Filter: MONO's MS-20-like one
  (TYPE a blend in P_LN1, CUT glides in s[5]).
- MONO (eng_mono.c, ENGI_MONO 4, sound byte 4, SYNTH pool): a mono analog-style voice (Minimoog-ish, not a copy).
  Every press retriggers the amp AND filter envelopes from where they are (voice.c trk_vmode: always V_MONO). OSC 1: WAVE (pairs SW+SQ SW+TR SW+SN SQ+TR SQ+SN TR+SN, then SW+SW SQ+SQ TR+TR SN+SN), MIX (osc 1..2),
  FINE (osc 2 +-1 semitone, centre = in tune), SEMI (osc 2 0..+24). OSC 2: SUB (-1 -2 -3 octaves, a square),
  SUBVL, PW (osc 1 only, the most variety: SQR pulse width, TRI wavefold, SIN bits down to a square, SAW hard sync to a hidden
  oscillator 1x..8x: PW is its pitch, no extra knob), SAT (P_LN0, drive before the filter). One preset: a plain saw.
  MONO's filter (eng_digital.c mono_filter, MS-20-like; FM SYN has it too, params.c flt_blend): an SVF with a soft clip in the resonance loop, so
  high RES rings, squeals and self-oscillates. CUT (knob + LFOs) glides ~20 ms, coefficients every 8 samples
  (mono_cs); the filter env and key tracking don't glide. FILTER page TYPE on MONO = P_LN1, a smooth blend LP (0) .. BP (64)
  .. HP (127), lockable/LFO-able (params.c page_id_an). Filter env / ENV pages as usual. To become a swappable
  filter machine for every engine later.
- REC armed: a red "REC" pill left of the BPM; playing, it breathes twice a bar.
  It records EVERY track you play (seq.c rec_armed), so finger drumming on the
  black track keys in SEQ mode records each hit on its own track.
- The DEST knobs skip what does nothing on the track's machine (params.c mdst_live): knobs it hasn't, FM SYN's C.ATK..DTUN
  elsewhere, an FM drum's SUS REL, an 808's CUT RES F.* ATK DEC SUS REL. A saved sound holding one keeps it.
- LFO DEST order = the knobs as their pages show them, left to right (params.c
  MDST_ORDER), then PITCH AMP. PAN SHP DEPTH VEL TIME E8 are left out. VIB is
  called L.PIT.
- Bottom line: no page dots; key mode left, machine pill right (y 194). Nothing
  microscopic on screen. tools/emu_shot.c saves emulator screen pictures.
- Overlay on every page (ui_gb.c gb_overlay): six 16 px track squares at the bottom
  (track colour; white on a hit; muted = dark with an X; selected = thick edge)
  and the loop border: 3 px round the screen edge, clockwise from the top left,
  white as far as the selected track's loop has got. They redraw straight onto
  the panel (no full redraw).
- SAMPLER waveform: a held step shows its START/LEN locks. Turning START or LEN
  (or locking them) swaps the dials for a zoomed waveform (~1/6 of the chunk)
  round START or the end, the turned bracket bold, "START 20" under it.
