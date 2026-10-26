# Licensing checklist: BBL JAM BOX 0.1 beta

**This is not legal advice.** It's a practical checklist drawn from `LICENSING.md`, `CREDITS.md`, the
licence texts in `LICENSES/` and what is actually in this build (checked 2026-10-26). Items marked
**⚑ CHECK** are ones to confirm with a lawyer, M-VAVE or Leo Kuroshita.

Key: ☑ done · ☐ to do · ⚑ check with someone

---

## 1. GPL-3.0: make the source available

The firmware is GPL-3.0-only (`LICENSE`). Anyone you give the firmware to must be able to get the complete
source of **the exact version** they got, under the same licence.

- ☐ **Put the source on GitHub** at `github.com/UnkleBlairy/BBL-JAM-BOX` (public). The installer and home page
  already link there. It's a new repository (your fork of Felucca is already
  BBL-WaveCusher), pushed with this project's full history, so Leo's commits and credits come with it.
- ☐ **Tag the release** (`v0.1-beta`) so the source matches the `.fwsc` people install. Do this for every
  release.
- ☐ **Host the installer site from the same place** (GitHub Pages on the same repo) so the binary and its
  source sit side by side. GPL section 6(d) accepts "equivalent access to the Corresponding Source in the
  same way through the same place".
- ☑ The licence text travels with the firmware: `LICENSE`, `LICENSING.md` and `LICENSES/` are in the
  release folder (`build/release-0.1-beta/`) and on the site (`firmware/`).
- ☐ **Patreon:** you can charge for access, but you **can't stop anyone sharing** the firmware or source
  (GPL section 10). For a free tier this doesn't matter; for a paid tier later, it does.
- ⚑ **Build tools.** Building needs the JieLi toolchain and AC79 SDK (not in the repo; `BUILDING.md` says
  where to get them). That's normally fine (they're general tools / separately licensed), but confirm.

## 2. Credit Leo Kuroshita / Hügelton Instruments and mark it as a modified fork

GPL section 5(a) requires modified files to carry "prominent notices stating that you modified it, and
giving a relevant date".

- ☑ The home page says "A modified version of Felucca by Leo Kuroshita (@kurogedelic), Hügelton
  Instruments", and the guide ends with the same line.
- ☑ Leo's copyright lines are kept in the files he wrote.
- ☑ **Add a notice to the files we changed or wrote** (for example
  `Modified 2026 by Blair (blairysbeatlab): BBL JAM BOX`). These files have **no licence header at all**
  yet: `firmware/src/eng_sampler.c`, `smp_usb.c`, `ui_gb.c`, `icons_gb.h`, `icons_808.h`,
  `web/fm1smp.js`, `web/smpui.js`, `web/smpimg.js`, `tools/gen_icons_808.py`.
- ☑ **Update `README.md` and `LICENSING.md`** to say this is BBL JAM BOX, a fork of Felucca, with its
  own copyright line added beside Leo's. Both still describe Leo's Felucca 1.6.1 (and the README still has
  Leo's "Maintenance Notice").
- ☑ **Fixed: the FM-1's start-up screen said "Felucca / v0.1-beta / Hügelton Instruments"**
  (`firmware/src/ui_draw.c`, `SPLASH_LINES`). That presents our modified build as Leo's product. It now shows
  Blair's logo, the version and "BASED ON FELUCCA". (Same reason as the file notices above: a modified
  version should be clearly marked as modified, not passed off as the original.)
- ⚑ **The name "Felucca".** `LICENSING.md`: "'Felucca' and 'Hügelton Instruments' are names of Hügelton
  Instruments." Saying "based on Felucca" is fine; calling our build "Felucca" is not. ☑ It's now called
  BBL JAM BOX, the site's file is `bbl-jam-box-0.1-beta.fwsc`, and the credits match the build. (Inside the
  code, names like `felucca.c` and the build's own `felucca-X.Y.fwsc` stay: they're not shown to users.) Consider
  asking Leo if he's happy with how it's described; he's stepped back from the project but is reachable on
  GitHub.
- ☐ **The on-device credits screen is gone.** Leo's build showed authors, licences and source links under
  HOME held → ABOUT → CREDITS; this build has no MENU. GPL section 5(d) asks that interactive interfaces
  show "Appropriate Legal Notices". ⚑ Ask whether the website notice is enough or whether a small ABOUT
  screen should come back.

## 3. Third-party notices: what's really in this build

The installer page and `CREDITS.md` still list everything from Leo's full Felucca. This build has had most
of it removed. Credits should match what's shipped: list everything that's in, and preferably drop what
isn't (over-crediting is harmless legally but confusing).

**In this build, so keep the credit and licence text:**

| Part | Licence | Status |
|---|---|---|
| Felucca itself (Leo Kuroshita, Hügelton Instruments) | GPL-3.0-only | ☑ `LICENSE` shipped |
| Felucca-Engines shared code: `firmware/src/dsp.c`, `engine.h`, `tools/gen_engine_tables.py` | MIT | ☑ `LICENSES/MIT-Felucca-Engines.txt` shipped |
| Second core, from X0X (charlesvestal), Melodee (keremimo) and Jangada (zednaked): `firmware/hal/fm1_cpu1.*`, `fx.c` | GPL-3.0 | ☑ credited in file headers and CREDITS |
| Inter Tight font (on-screen text) | SIL OFL 1.1 | ☑ `LICENSES/OFL-InterTight.txt` shipped |
| Fukiai icon font (Hügelton): the firmware's icons | MIT | ☑ `LICENSES/MIT-Fukiai.txt` shipped |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` inside every `.fwsc` | Apache-2.0 | ☑ `LICENSES/Apache-2.0.txt` shipped, credited on the installer |
| DotGothic16 font (browser emulator only, not in the release site) | SIL OFL 1.1 | ☑ in repo `LICENSES/` |

**Not in this build any more (credited only because the text came from Leo's Felucca):**

| Part | Licence | What to do |
|---|---|---|
| PHYS engine: DaisySP (MIT), Rings (MIT) | MIT | ☑ remove from installer credits and CREDITS |
| FM6 engine: msfa from Dexed | Apache-2.0 | ☑ remove from the installer credits ("FM6 engine: msfa...") |
| PHASE engine, DRUM voices, Hügelton Sample Pack | GPL / CC0 | ☑ remove from the installer credits |
| VSCO-2 / VCSL samples | CC0 1.0 | ☑ remove (the samples were taken out) |
| VOICE engine reference (klattsch) | MIT, credit only | ☑ remove |

- ⚑ Our own sound engines (the 808 port of `docs/808lab.html`, the sampler, the FM engine changes) are our
  work under GPL-3.0. Check that nothing in `docs/808lab.html` or `docs/samplelab.html` was copied from
  someone else's code or samples.

- ⚑ **Your logo** is in the source and the firmware but marked "all rights reserved, not under the GPL"
  (`LICENSING.md`). Mixing a non-GPL image into a GPL firmware is common for logos, but confirm it's fine.

## 4. CC0 samples
- ☑ No CC0 sample files are in this build (removed in commit `d070f74`). Samples users upload are their
  own responsibility: ☐ consider one line on the Samples page ("only upload sounds you have the right to use").

## 5. M-VAVE: branding, vendor files and V15

- ☑ **No M-VAVE files in the release.** `make_site.py` refuses any package without our own loader
  (`FELUCCA-LOADER-1`), and it accepted ours. The V15 file is **linked, not included**.
- ☑ Disclaimer on the home page, installer and guide: "Not made or supported by M-VAVE. M-VAVE and FM-1 are
  trademarks of their respective owners."
- ☑ **The name.** It's now **BBL JAM BOX**, your own name. "FM-1" is only used to say what it runs on
  ("for the M-VAVE FM-1"), which is the normal, safe way. Don't use M-VAVE's logo or product photos as if
  they were yours.
- ⚑ **V15 file licence.** M-VAVE's own repo (`github.com/m-vave/fm1_open`, `THIRD_PARTY.md`) says
  `FM-1_015.fwsc` is "distributed by M-VAVE under the Apache License, Version 2.0". We checked that file is
  byte-identical to the official V15 our installer expects (SHA-256 `db1642b2…edb8a`). Linking to it is
  clearly fine. Bundling it would also seem allowed by Apache-2.0, but we're **not** doing that (your rule),
  and `LICENSING.md` says Felucca never redistributes it.
- ⚑ **M-VAVE's "FM1 Open"** (their own GPLv3 firmware with an SDK linking exception) is a separate
  project. We use none of its code. If we ever borrow from it, its terms (and its binary SDK's separate
  terms) need reading first.

## 6. Before each release
- ☐ Source pushed and tagged on GitHub, same version as the `.fwsc`.
- ☐ Release folder / site contains `LICENSE`, `LICENSING.md`, `LICENSES/`.
- ☐ Credits on the installer match what's in the build.
- ☐ No vendor files (`make_site.py` checks this).
- ☐ Disclaimers present (beta, not M-VAVE, trademarks, no warranty).

---

### Things to ask someone (summary)
1. **Lawyer:** is the website notice enough, or does the device need an ABOUT/credits screen (GPL 5(d))?
2. **Leo Kuroshita:** is he happy with "based on Felucca" and how his work is credited?
3. **M-VAVE:** confirm the V15 recovery file's Apache-2.0 status if you ever want to bundle it.
