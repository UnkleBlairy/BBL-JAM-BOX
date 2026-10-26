# Changelog

## 0.1 beta (2026-10-26)

The first public beta of FM-1 Groovebox: a live-jamming groovebox for the M-VAVE FM-1, built from Leo Kuroshita's
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
