# Manual review notes

Open points found while writing [USER_MANUAL.md](USER_MANUAL.md). Nothing here was guessed into the manual.

## Discrepancies

| # | Where | Finding | Manual does |
| --- | --- | --- | --- |
| 1 | README.md, installer link | The browser installer and editor links point to `isod89.github.io/sloop-fm1`, while the repository is `julesdg6/sloop-fm1`. Which GitHub Pages site is current could not be verified offline. | Uses the README's URLs. Please confirm. |
| 2 | SLOOP.md "Install" | It describes only `INSTALL-SLOOP.bat` (a local build); README describes the hosted browser installer and `.fwsc` releases. | Documents the hosted installer and the CLI; links BUILDING.md for local builds. |
| 3 | DEMARRAGE-RAPIDE-FR.md | Title says "SLOOP 2.0", while the firmware (`firmware/src/ui.c`) says 2.2. | Uses 2.2. |
| 4 | SLOOP.md "Specifications" | Says MIDI channel 10 is the drum channel; the source (`params.c`, `seq.c`) makes it adjustable (DRUMS page, CH 0–16, default 10, 0 = off). | Documents the setting. |
| 5 | SLOOP.md / FR | REC-hold clear: "~2 s" (FR) versus "0.7 s then ~1.3 s" (SLOOP.md). The source uses 700 ms + `HOLD_CLEAR_MS` 1300 ms. | States both (about 2 s in total). |
| 6 | SLOOP.md, SYSTEM page | `MIDI`, `SYNC`, `ROUT` entries in `params.c` are placeholders (`--`). | Says no MIDI clock sync is documented in source. |
| 7 | `LICENSING.md` | Mentions `docs/panel.jpg`, which is not in the repository. | Not referenced; no panel photo is used. |
| 8 | Fresh-clone `docs/index.html` | The hosted site redirects to `webapp/installer/`; its `docs/firmware/sloop-2.2.fwsc` package is the one served. | Not used for instructions. |

## Unresolved questions

1. **Backup of projects/presets:** no documented way to copy projects, user presets or user samples off the FM-1. The manual says so rather than inventing one.
2. **Official firmware download and M-UPGRADE steps** are only referenced ("m-vave.com", "M-UPGRADE"); their exact procedure is not in the repository.
3. **Panel layout:** there is no photograph or diagram of the FM-1 panel in the repository, so controls are listed by name, not position or hardware label.
4. **TRS MIDI IN** exists as the `FELUCCA_UART` build option (untested, default off), so it is not presented as a feature.
5. **Counts** (68 presets, 37 kits) are taken from SLOOP.md/README and cross-checked only where the source exposes them (kit and sample-set definitions are generated at build time).
6. **MIDI out velocity/channels for synth keys:** the manual states that played keys are sent to MIDI out on the selected track's channel, per `seq.c`; hardware behaviour was not tested.
7. **Hold HOME → about** is used to read the version; this is from SLOOP.md and `ui_menu.c`.
