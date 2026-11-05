# Releases

Who has what. `CHANGELOG.md` is the friendly version for users; this is the behind-the-scenes record.

## How versions work

- **The middle number means features:** 0.6, 0.7. Members get each one first.
- **The last number means fixes only:** 0.6.1 has 0.6's features plus bug fixes, and it's what the public gets.
- **Every fix goes in both:** on `main` for the next members' build, and on the public release's branch.
- **New features stay on `main`, which isn't pushed.** Public versions come from a branch per release
  (`release/0.6`), and only that branch goes to GitHub. Members get the source in their zip, as the GPL asks.
- **0.6.1 is also offered to members** when it goes public, so they aren't left with a bug the public has fixed.

## Who has what now

- **Members:** 0.6 beta
- **Public:** 0.2 beta (0.6.1 planned for 2026-11-07, after the video)

## The releases

| Version | Who | Date | Identity | Main features | Main fixes |
|---|---|---|---|---|---|
| 0.1 beta | Public | 2026-10-26 | FM-1_901 | First release: 6 tracks, FM, 808, sampler, 16 steps, locks, PERF, sounds and kits | |
| 0.2 beta | Public | 2026-10-30 | FM-1_902 | SEQ button gestures, LOCKS mode, touch modulation, all trig conditions, FILL, copy trig, new font | Held buttons misread, stuck messages |
| 0.3 beta | Members | 2026-11-01 | FM-1_903 | MONO, new filter, FM SYN rebuilt, 64 steps, unquantized recording, FX track, MIX pages, last jam | |
| 0.4 beta | Members | 2026-11-03 | FM-1_904 | ACID, length and speed changes land in time, tracks remember mode and octave, 8 LOCKS keys | Clicks when a voice is stolen |
| 0.5 beta | Members | 2026-11-04 | FM-1_905 | DEST knobs skip dead options, Samples page playhead and loop | Newer FM-1 saves, LFO buzz on VOL / PAN, saved step notes, OCT- twice to clear, Samples page "stopped answering" |
| 0.6 beta | Members | 2026-11-05 | FM-1_906 | SCRUB, crossfader, CRUSH, BBL Fader app | Stuck on MONO, sequencer arp, 808 levels, leaving ACID |
| 0.6.1 beta | Public, offered to members | Planned 2026-11-07 | FM-1_906 | Same as 0.6 | Whatever members report before then |

Public users jump from 0.2 to 0.6.1, so the public changelog has to cover 0.3 to 0.6 as well.

## Known bugs and gaps

| What | Versions | Status |
|---|---|---|
| ENV and FILTER pages do nothing on 808 tracks | All | Planned for the voice rework |
| Saves on newer FM-1s not confirmed on a newer unit | 0.5 on | Waiting for a report |
| Sample upload fix not confirmed by the member who reported it | 0.5 on | Waiting for a report |
| Last jam with the crossfader through a power cycle not checked on an FM-1 | 0.6 | To test |
| Felucca's MIDI clock drift and TRS MIDI timing fixes not brought in | All | After the public release |
| Second core crashes on Blair's FM-1 | All | Parked |
| Phone fader needs a Windows PC in the middle (script version for Mac and Linux) | 0.6 | Bluetooth to look at later |

## Toward version 1

Features for members first, in this order so far: slides, the ALL track, then the voice rework (it can change how saved
sounds play, so it goes last and on its own). Version 1 waits until those are in and public bug reports have settled.
