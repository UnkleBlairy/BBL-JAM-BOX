# Changelog

## 0.6 beta (2026-11-05): members' preview

SCRUB, a tape head for the sampler; a crossfader between two macros; CRUSH on the 808s; and a stack of fixes.

### New
- **SCRUB** (sampler, page 3, knob 2): move the knob and a tape head travels over your sample, so you can scratch and
  do hand tape stops. The whole knob covers the part set by START and LEN, however tiny. The note you last played sets
  the pitch along with your hand speed, so a low note and a high note give different textures. A key or trig always
  plays the sample as normal from its start; move SCRUB after that and the head travels from where the sound has got
  to until it meets the knob. It can be put on a macro, the crossfader, an LFO or a lock. 0 = off.
- **Crossfader**, in the style of the Octatrack: fade between two macros. Send MIDI CCs on the channels the FM-1
  already listens to: 102 = the A slot, 103 = the B slot (1-8 = M1-M8, 0 = empty), 104 = the fader. PERF shows it.
  Slots aren't saved, and saves keep your real values. Any controller that sends CCs can drive it.
- **CRUSH replaces AGE on the 808s**: one knob, sample rate and bits down together, Octatrack-style. A sound saved
  with AGE loads with that value as CRUSH, so it will sound different.
- **BBL Fader, a phone fader app (experimental, Windows)**: one double-click, scan the QR code and your phone is the
  crossfader, with no install. Your PC stays in the middle for now. See phone-fader/QUICK_START.txt. Mac and Linux
  users can use the script version in that folder.

### Fixes
- **Only MONO plays one note at a time now.** Turning PRESETS past MONO left the next machines stuck on it: FM SYN
  played one note of a chord, and the sampler could miss a key press. Saved sounds and last jams that caught this are
  put right on their own.
- **The ARP in the sequencer walks on through the steps** (it restarted on its first note every step), so a run of
  chord steps now climbs the arp. With the ARP on, MONO records chords too.
- **808 levels**: a hit no longer takes the level trim of the last hit, which made the kick louder on steps with a hat.
- **Leaving ACID**: tapping SEQ from ACID mode to KEY puts the track back on its steps. To leave an ACID line running
  while you play another track, pick that track first.

### Still not tested
- **Newer FM-1s** (the memory chip change in 0.5): I still haven't had a report from one of those units, so please
  tell me if yours saves.

## 0.5 beta (2026-11-04): members' preview

Bug fixes from your reports, saves on newer FM-1s, and a better Samples page.

### Fixes
- **Newer FM-1s can now save.** Some newer units have a different memory chip, and sounds, kits, the last jam and
  samples didn't work on them. They should now: this one hasn't been tested on a newer unit yet, so please tell me.
- **LFOs on VOL, PAN and the sends no longer add crunch or buzz**: those levels now glide.
- **A saved sound keeps its step note / chord**, so it sounds the same from the track key after loading. Sounds saved
  before this version start on middle C, as they did.
- **Clearing a track's steps needs OCT- pressed twice** within a moment ("OCT- AGAIN TO CLEAR"), so a pattern can't
  go by accident. The same goes for ACID and the FX lane.
- **ALGO back from the FX track** returns to the track you came from.
- **The Samples page no longer stops with "The FM-1 stopped answering"**: it now asks the FM-1 how much memory it has.

### Easier to use
- **The LFO and ENV DEST knobs skip anything that does nothing** on the track's machine.
- **Samples page**: a playhead on the waveform, and LOOP: the part plays round and round while START and END move it
  live, so you can find a loop by ear.
- **The Samples and install pages have the firmware's look.**

## 0.4 beta (2026-11-03): members' preview

A Metropolis-style ACID sequencer, length changes that land in time at once, and tracks that remember how you left them.

### ACID sequencer
- **ACID mode**: a stage sequencer in the style of the Intellijel Metropolis, on any track. In SEQ mode **tap
  OCT+** to flip the track between its steps and its ACID stages (tap again to go back; both are kept).
- **White keys = stages 1-16.** Press one to edit it. **STAGE** page (black key 8): PITCH (0-12 semitones from
  middle C, shown as the note), PULSE (1-8 repeats), GATE (MUTE, FIRST, EVERY, HOLD), OCT (-3..+3).
- **ACID** page (black key 9): LEN (1-16 stages), DIR (FWD, REV, PING, RAND, BRWN), SPD, GATE length.
- **Slides**: hold a stage key for its SLIDE pop-up: knob 1 = slide on/off, knob 2 = slide time. The note before
  glides into it, legato, 303-style.
- OCT- clears the track's ACID; hold a stage + OCT- resets that stage.

### Sequencer
- **Length and speed changes land in time at once**: add a page, change LEN or SPD while playing and the track jumps
  straight to where it would be if it had always been that length / speed, so tracks never drift apart and you never
  need to stop and start. Nothing restarts every few bars, so odd lengths (3, 5, 7 steps..) keep running against each
  other as polyrhythms.
- **New steps start on middle C** after a sound or kit load, a clear, SOUND RESET or CLEAR ALL, until you play a
  note on the keys (then new steps use your last note or chord, as before).
- **Kits keep each track's step note / chord**: set the kick two octaves down, save the kit, and it comes back that
  way. The key you load a sound or kit with no longer plays a note.

### Playing
- **Tracks remember their mode**: a track left in PARAM, LOCKS or ACID comes back to it when you pick it again.
  KEY and SEQ aren't remembered, so scrolling through tracks in SEQ mode stays in SEQ.
- **Each track keeps its own octave**: bass low on track 1, lead high on track 2, and swapping back and forth keeps
  both.
- **LOCKS mode has 8 keys** a track (white keys 1-8); keys 9-16 are free for later.

### Sound
- When too many notes play at once and a sound has to be cut for another, it now fades out smoothly instead of
  clicking (808 drums too).

## 0.3 beta (2026-11-01): members' preview

Two new synths, a much deeper sequencer, an FX track and a jam that survives switching off.

### New sounds
- **MONO**: a new mono analog-style synth (in the vein of a Minimoog). Two oscillators (WAVE picks the pair, MIX
  blends them, FINE and SEMI tune osc 2), a SUB oscillator (-1/-2/-3 octaves), **PW** that reshapes osc 1 (pulse
  width on square, wavefold on triangle, bit-crush on sine, **hard sync** on saw) and **SAT** drive before the
  filter. Every key press restarts the envelopes.
- **New MS-20-style filter** on MONO and FM SYN: high RES rings, squeals and whistles on its own. **TYPE** now
  blends smoothly from low-pass through band-pass to high-pass. CUT glides instead of stepping.
- **FM SYN rebuilt, Digitone-style.** A and B are the operators you hear (MIX between them), C and D modulate,
  each with its own **ATK / DEC / AMT** (DEC fully right = HOLD). 8 algorithms, **A:B** ratio pairs on one knob,
  C and D ratios shown as real ratios, **FB** and **DTUN**. Pages: OSC 1, MOD C, MOD D, TUNE.
  *FM SYN sounds saved in 0.2 load but sound different.*

### Sequencer
- **64 steps** on up to 4 pages (black keys 8-11 in SEQ mode). Tap a page key past the end to add a page, hold it
  to set the length, hold + OCT- to delete pages.
- **Live recording keeps your timing** (no forced quantize). Hold REC to turn quantize on for recording. Hold
  step(s) + SELECT to nudge them.
- **REC records every track you play**: finger drum on the track keys in SEQ mode.

### FX track
- **An FX track** (black key 7 in SEQ mode, or turn ALGO past 6): its own DELAY, VERB, three LFOs and its own
  sequencer lane to lock master FX per step. Tracks get a **SENDS** page.
- **Double-tap FX** on a track to jump to the master DELAY page.
- **PERF, COMB, DJ and AMP** (the whole mix's level and **PAN**) live on the FX track.

### Mixing and pages
- **MIX pages**: two tracks' VOL and PAN on each (HOME button).
- **VOL / PAN page** on the ENV button: modulate a track's level and pan with the LFO.
- **LFO page** tidied: RATE WAVE DEST AMT.
- **Reset a page**: hold its button + OCT-.
- **Reset a track's sound**: in SEQ mode hold its track key + OCT-.
- **CLEAR ALL**: hold OCT- for a second, then press OCT+ to confirm (a fresh start).
- **PLAY twice** (stop, then PLAY again quickly) = all sound off, for stuck notes.

### Saving
- **The last jam is kept**: switch off and on and everything is back (stop before switching off).
- Sounds save by type (KICK, SNARE, CYMBAL, PERC, SYNTH, SAMPLE: 16 each). Kits keep the LOCKS keys.

### Sampler
- **START and LEN** move live while a sound plays, with no clicks. LEN is the window's size: START slides it, and
  at the end of the sample START keeps going and the window shrinks. The LEN knob always shows the real size.

### Screen
- **Black background**, with the track's colour as a strip behind the page name.
- (Experimental) a **screen mirror** page for streaming the screen to a computer.

## 0.2 beta (2026-10-30)

A big update to how BBL JAM BOX plays: quicker gestures, touch modulation, Elektron-style trig conditions and a
new chunky pixel font.

### Playing
- **SEQ button:** tap = switch between KEY and SEQ mode (the page stays). Double tap = open the SEQ page.
  Hold SEQ = pick a key mode: turn a knob for **PARAM** mode, press a white key for **LOCKS** mode, or tap SEQ to
  cancel. It waits for you after you let go.
- **LOCKS key mode:** each white key is its own playable step. Hold a key and turn knobs to bake up to 8 locks into
  it. Play them live or record them.
- **PARAM mode** keeps its knob when you change pages.
- **ALGORITHM knob** now picks the track (1–6).
- **Temporary changes:** hold OCT+ and turn knobs. Let go and everything snaps back.
- **PITCH** in semitones on the FM synth and the sampler, lockable per step.

### Touch modulation
- **Hold LFO** (about ¾ of a second), then turn any knob: that knob becomes the LFO's destination. Keep turning to
  set the amount (left = minus, right = plus) on a big pop-up. It closes by itself a second after you stop.
- The same for the **amp envelope** (hold ENV) and the **filter envelope** (hold FILTER).
- Tap the same button again to cancel, OCT- removes the destination, SELECT goes straight back to the amount.
- Almost every knob can be modulated, even stepped ones (wave, type) and knobs read at the note's start (like the
  kick's RATIO).
- The LFO pages are tidier: RATE WAVE PHASE FADE on page one, SYNC TRIG POL on page two.

### Sequencer
- **All of Elektron's trig conditions:** 1:2 to 8:8, FILL, PRE, NEI, 1ST and their NOT versions.
- **FILL mode:** while playing, hold PLAY. Steps you enter then get the FILL condition, and they breathe so you can
  tell them apart.
- **Copy and paste a trig:** in SEQ mode hold a step and press SAVE to copy (notes, chance, condition, locks). Hold
  an empty step and press SAVE to paste.
- A **DJ page** so the DJ filter can be locked.
- The ARP button pulses with the beat while the arp is on.

### Screen
- **New font:** BLB Pixel 7 Chunky, Blair's own pixel font. Big and chunky everywhere, no tiny text.
- **Pop-ups** are white boxes at the bottom with big text. Long hints show in two halves.
- Bigger BPM, track squares and key mode; thicker loop border; no page dots.

### Fixes
- Holding buttons was sometimes read wrong on the FM-1 (a tap opened the mod picker, SEQ taps got lost). Fixed.
- Messages no longer stay stuck on the screen.

## 0.1 beta (2026-10-26)

The first public beta of BBL JAM BOX: a live-jamming groovebox for the M-VAVE FM-1, built from Leo Kuroshita's
Felucca 1.6.1. Most of Felucca was taken out and a new, simpler instrument built on what was left.

### The groovebox
- **6 tracks.** At power-on they're kick, snare, closed hat, open hat, percussion and synth. Each has its own colour.
- **Three sound engines.** Turn **PRESETS** to change a track's machine:
  - **FM (DIGITAL):** a 4-operator FM synth and FM drums (BD, SD, HH, CYM, PERC, SYN). The kick has a punch knob
    and an FM knob with a ratio.
  - **808 (ANALOG):** kick, snare, tom, rim, clap, cowbell, hat and cymbal, with an **AGE** knob for drift, hiss
    and grit. It starts clean.
  - **SAMPLER:** plays your own sounds (see Samples below), with pitch, stretch, start, length, play mode (once,
    loop, ping-pong, reverse), lo-fi and gate. Stretch and lo-fi can be swept live.
- **ALGORITHM knob** sets the selected track's volume. **HOME** steps through the volume pages and the clock page.
- Drum tracks play one hit at a time: a new hit takes over the old one smoothly.

### The sequencer
- **16 steps per track**, with length 1–16 and speed ×1/4 to ×4 per track.
- **Chance** for the whole track or for each step, and Elektron-style **trig conditions** (1/4 … 4/4).
- **Parameter locks** on every knob: hold a step and turn a knob. A lock lasts until the track's next note.
  **Trigless locks** (OCT+ and an empty step) change sounds without playing a note.
- **PARAM key mode:** the white keys set one knob's value and play the note, which is handy for recording
  melodies with locks.
- **Arpeggiator page:** up, down, up-down, cycle, random and pattern, at rates from 1/64 to 1/4, with fade. In the
  sequencer, a step ratchets.
- **FX lane** (black key 7 in SEQ mode): 16 silent steps that lock the delay, reverb, comb and DJ filter.
- **Mutes:** hold OCT+ and press a track key. A quick press latches, holding mutes only while you hold.
- **Recording** follows the track you select. A red **REC** pill shows when it's armed.

### Performance and effects
- **PERF page:** macros on the keys (always ping-pong), a **FADE** knob for the let-go fade, and beat repeats
  (1/8, 1/16, 1/32, reverse).
- **Master DJ filter** (low-pass to the left, high-pass to the right) and a **comb** filter, tuned so they never click.
- **Delay**, and **one reverb**: a 90s-style plate with a **GRIT** knob and **FREEZE** at the top of TIME.
- **LFOs** can reach almost every smooth knob, including envelopes, the delay, the reverb, the comb and the DJ filter.

### Saving
- **16 sound slots per track** (hold SAVE, then a white key) and **kits** (a black key): all 6 sounds plus macros
  and effects. Tap SAVE to load.
- **Patterns are never saved.** It's made for jamming from scratch.

### Screen
- A clean two-tone look with big text and four dials per page, and pixel icons for each page and machine.
- The header shows the page icon (it flashes on a drum hit), the title and the BPM.
- Six track squares along the bottom show hits and mutes, and a border round the screen edge shows how far
  through its loop the selected track is.
- Turning START or LEN on the sampler shows a zoomed waveform.
- **A new power-on screen** with Blair's logo, the version and "BASED ON FELUCCA".

### Samples
- **Samples page** on the installer site: pick a WAV, MP3, OGG or FLAC file, choose the part you want, and upload
  up to 32 sounds (about 11 seconds in total) over USB. The FM-1 keeps them when it's switched off.
- Only changed parts are sent, so adding one sound is quick. If the cable comes out, press Upload again and it
  carries on where it stopped.

### Installer
- A new **front page** with a clear BETA warning and two buttons: install the firmware, then load samples.
- **Return to official V15** now works from this beta. The beta can't make a backup, so the installer warns you
  first.
- The installer always fetches the newest firmware file (before, a browser could install an old cached copy).
- A **step-by-step flashing guide** (`FLASHING_GUIDE.md`, also as plain text).

### Removed from Felucca
Felucca's other engines (PHASE, PHYS, FM6, VOICE, SLICE and more), its built-in samples, menus, layers, projects,
song mode, the web editor and its backup, the insert effects, distortion, chorus and the other reverbs. The licence
files, README and credits now describe this fork.

### Tested on a real FM-1
- Installing from official V15, uploading a sample, and going back to V15 (a sound saved in V15 survived the
  whole round trip).

### Known limitations
- **No backup** of the beta's own sounds, kits and samples. Going back to V15 loses them, and V15 clears the
  sample memory: press Upload again after reinstalling.
- The first install from V15 can stop at "the update loader did not appear". Unplug, plug back in and press
  Install again: it carries on.
- The FX lane isn't saved, and has no length, speed or chance of its own yet.
- The 808's AGE knob uses placeholder amounts until the final ones are chosen.
- Going back to **Leo's Felucca** (rather than official V15) hasn't been tested.
- How much CPU the new engines use can only be read on the device (the CLOCK page's 4th dial). It hasn't been
  measured yet.
