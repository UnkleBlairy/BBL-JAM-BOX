# Changelog

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
