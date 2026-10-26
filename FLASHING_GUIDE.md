# BBL JAM BOX: how to install it (and how to go back)

> ## ⚠️ BETA: flash at your own risk
> This is test firmware for the M-VAVE FM-1. It may have bugs. It is not made or supported by M-VAVE.
> Installing it is your choice and your risk: the authors accept no responsibility if your FM-1 is
> damaged or misbehaves. **Read this whole page before you start.**

You need about 15 minutes, a computer, and the USB cable that came with your FM-1 (or any USB **data**
cable: some cheap cables only charge).

---

## Before you start (do all of these)

1. **Use Chrome or Edge on a computer.** Other browsers (Safari, Firefox) and phones can't talk to the FM-1.
2. **Download the official V15 firmware and keep it.** This is your way back to normal. Get the file
   `FM-1_015.fwsc` from M-VAVE's own GitHub page:
   https://github.com/m-vave/fm1_open/blob/main/sdk/firmware/FM-1_015.fwsc
   (click the **Download raw file** button, top right of the file). Save it somewhere you'll find it,
   like your Downloads folder. Don't rename or change it: the installer checks it's the exact official file.
3. **Close every other music app** that might be using the FM-1: your DAW (Ableton, FL Studio, etc.),
   M-VAVE's own apps, and any other browser tabs with the installer or the Samples page open.
4. **Plug the FM-1 straight into the computer**, not through a USB hub. No extra hardware is needed.
5. **Make sure your laptop is charged or plugged in**, so it doesn't go to sleep halfway.

### What happens to your stuff
- **Your official V15 sounds:** we tested this on a real FM-1: a sound saved in V15 was still there after
  installing the beta, using it (saving sounds, uploading samples) and going back to V15. We can't promise
  that for every FM-1 or every kind of V15 data, so if something matters to you, back it up with
  M-VAVE's own tools first.
- **The beta's own saved sounds, kits and samples:** the beta **cannot make a backup**. When you go back to
  V15 they are lost (V15 can't use them anyway), and V15 clears the sample memory: after going back to the
  beta, press **Upload** on the Samples page to put your samples back (the page remembers your list).
- **Patterns are never saved** on the beta: it's made for jamming from scratch.

---

## Install the beta

1. Turn the FM-1 **on**, wait for it to start, and **stop it playing**.
2. Connect it to the computer by USB.
3. Open the beta's web page in Chrome or Edge and click **1. Install the firmware**.
4. Click **Install**. If the browser asks to let the page **control MIDI devices**, click **Allow**.
5. Watch the status line under the button. It goes:
   **Starting → Verifying → Switching to update mode → Writing → Restarting → Done**.
   It takes a few minutes.
6. **Do not unplug the USB cable or close the page while it's writing.**
7. When it says **Done**, the FM-1 restarts into the groovebox.

### If something goes wrong while installing

| You see | Do this |
|---|---|
| **FM-1 not found. Check the USB cable (a data cable), close other apps that use MIDI, and press Install again.** | Is the FM-1 switched **on**? Is another app or browser using it? Try another cable. Then press **Install** again. |
| The status never changes after you press **Install** | Nothing has been written. Reload the page (if asked "Leave site?", choose Leave), unplug the USB cable, **wait for the Windows "device removed" sound**, plug it back in, and press **Install** again. |
| **MIDI (SysEx) access was not allowed. ...** | Click the icon left of the web address, allow **MIDI devices**, and press **Install** again. |
| **Error: the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.** | **Leave the FM-1 switched on.** Unplug the USB cable, wait for the "device removed" sound, plug it back in, and press **Install** again. It carries on where it stopped. (We've seen this happen; this fixed it.) |
| **Stopped. Replug the USB cable and press Install again.** or **The USB connection was lost. Replug it and press Install again.** | Same as above: replug, then **Install** again. |
| **The FM-1 did not come back. Power-cycle it.** | Switch the FM-1 off and on again. |

**If an install fails and the FM-1 no longer starts at all:** recovering it needs extra hardware, the
**FM-1-transporter** (https://github.com/kurogedelic/FM-1-transporter). This is why you should never
unplug during **Writing**.

---

## Load your own samples (after installing)

1. On the beta's web page, click **2. Load samples**.
2. Pick a sound file (WAV, MP3, OGG or FLAC). Drag **Start** and **End** to choose the part you want,
   press **Play** to hear it, then press **Write** to add it to the list.
3. Add more if you like (up to 32, about 11 seconds in total at 22 kHz; the **Memory** bar shows how full it is).
4. Press **Upload to the FM-1**. The FM-1 shows **SAMPLES** and **LOADING** with a percentage, then **SAMPLES LOADED**.
5. On the FM-1, turn **PRESETS** to **SMPL**. The **SLOT** knob picks S01 to S32.

Each **Upload** replaces everything in the FM-1's sample memory with the page's list. The page keeps your
list in this browser, so next time you can add to it. If the USB is unplugged during an upload, just plug
it back in and press **Upload** again: it carries on where it stopped.

---

## Go back to official V15

1. Turn the FM-1 on, stop it playing, and connect it by USB.
2. Open the beta's web page, click **1. Install the firmware**, then click **Return to official V15**.
3. Under **Official FM-1.fwsc**, choose the `FM-1_015.fwsc` file you downloaded. It should say
   **Official V15 verified**. (If it says it can't validate it, the file isn't the exact official one:
   download it again from the link above.)
4. Click **Back up and return to official V15**.
5. A message says: **This beta can't make a backup. Going back to official V15 erases the beta's saved
   sounds, kits and samples (V15 can't use them). Continue?** Click **OK**.
6. Wait for **Done**, without unplugging. If you see **the update loader did not appear**, unplug, wait
   for the "device removed" sound, plug back in, and click **Back up and return to official V15** again.
7. The FM-1 restarts on V15 (it reports **FM-1_015**).

We've tested this round trip (V15 → beta → V15) on a real FM-1.

## Coming from Leo's Felucca (not the official firmware)?

We haven't tested this path ourselves. Felucca's own installer (https://hugelton.github.io/Felucca/) can
save a **complete backup** of your Felucca songs, sounds and samples to your computer: its
**Return to official V15** does this first. So the safe route is:

1. Use **Felucca's** installer to go **back to official V15** (it saves the backup file: keep it).
2. Install this beta as above.
3. To go back to Felucca later: install Felucca from its own installer, then restore your backup with
   Felucca's web editor (Felucca's installer says this is where backups are restored).

---

## Getting help

Report problems on the Patreon post or on GitHub (https://github.com/UnkleBlairy/BBL-JAM-BOX). Tell us what
the installer's status line said and what the FM-1's screen shows.

*BBL JAM BOX is a modified version of Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments.
Free software under GPL-3.0, with no warranty. M-VAVE and FM-1 are trademarks of their respective owners.*
