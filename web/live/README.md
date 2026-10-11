# SLOOP live: how it is built

SLOOP live is a static web page (an installable PWA) that plays SLOOP on the FM-1 over the editor's SysEx protocol. No
build step, no libraries, no server: ES modules served as they are. The user guide is [LIVE.md](../../LIVE.md); the
commands are in [EDITOR_PROTOCOL.md](../EDITOR_PROTOCOL.md) (v11 adds `PERFORM`).

## Run it

```
cd web/live && python3 -m http.server 8000
# http://localhost:8000/?mock=1      the demo device (no FM-1 needed)
# http://localhost:8000/             the FM-1 over USB (Chrome / Edge; Web MIDI needs localhost or https)
```

`web/make_site.py` copies this folder to `webapp/live/` of the GitHub Pages site, next to the installer and the editor.

| Query | |
| --- | --- |
| `?mock=1` | the demo device (`mock.js`), protocol v11 |
| `?mock=10` | the demo device as a firmware without `PERFORM` (the page falls back to Sound and Mix) |
| `?ws=ws://host:port` | a WebSocket relay of raw MIDI bytes instead of Web MIDI (for a computer or a Pi next to the FM-1) |

## The files

| File | What it does |
| --- | --- |
| `index.html` | the page and all of its CSS (the FM-1 at night: black, the four track colours, the Terminus pixel font at 16 / 32 px; backlit keys that glow when lit, recessed glass for the screens; light comes on at once and fades out, and `prefers-reduced-motion` turns the movement off) |
| `app.js` | the shell: first run, standby, plug / unplug, the top bar, the tabs, the tablet split, the help and setup sheets |
| `device.js` | `Device`: one connection and everything known about the FM-1 (INFO, descriptors, the live state, the tracks, the selected track's sound), as events. `AutoMidi`: keeps the MIDI access and connects when an FM-1 appears |
| `proto.js` | the frames (`F0 7D 46 4C cmd … F7`), `Link` (one request in flight, a queue with priorities and coalescing), the reply parsers, `fmtValue` |
| `transports.js` | `WebMidiTransport` (USB) and `WebSocketTransport` (a relay): `open(onData, onLost)`, `send(bytes)`, `close()` |
| `widgets.js` | the touch controls: `holdable`, `tapHold`, `fader`, `knob`, `xyPad`, `sheet`, `toast` |
| `parts.js` | what more than one tab shows: section tiles, track tiles, store / launch a section, the position in the section |
| `live.js` `arrange.js` `sound.js` `mix.js` | the four tabs; each returns `{el, paint, …}` |
| `help.js` `next.js` | the help sheet (how each tab is played) and the ideas for later, with the decision each needs |
| `mock.js` `mockdata.js` | the demo device; `mockdata.js` is generated from the firmware's own tables (below) |
| `sw.js` `manifest.webmanifest` `icon*` | offline cache and install; bump `CACHE` in `sw.js` when the files change |
| `test_live.mjs` | the Node tests (below) |

## How it talks to the FM-1

The FM-1 holds one incoming SysEx frame at a time, so `Link` sends one request and waits for its reply (matched by
command and, where it says, the first bytes) before the next. Pushes (`CHANGED`, `RELOAD`, `STEP_CHANGED`,
`TRACK_CHANGED`) can arrive at any time and go to `onPush`.

- **Connect**: `INFO` (the protocol version, the engines, `P_COUNT`, `P_E0`), `DESC` + `GET` of the globals the page
  uses (found by label: ids move between versions), `TRACK`, `WATCH 3` (pushes, including other tracks' mix), then the
  selected track's sound (`TRACK_DUMP`, `DESC` of the parameters shown, `NAMES`).
- **Follow**: `PERFORM 0` (STATE) every 120 ms. Its reply is the whole live state (transport, tempo, beat, FX, mutes,
  solos, sections ready / playing / next, SONG REC, the chain, the selected track, each section's bars). It also keeps
  `WATCH` alive.
- **Play**: a pad or a button sends its `PERFORM` op with `front: true`, ahead of any queued poll; its reply is the
  state, so the page repaints at once. Knobs and faders send `TRACK_PARAM` / `SET` / `TRACK_MIX` with a `key`: a newer
  value replaces a queued one, so a fast drag never builds a backlog.
- **Track parameters** are shown by id (`TP_IDS` in `device.js`) and checked against the label `DESC` gives for that id;
  a mismatch hides the control rather than send a value to the wrong parameter. The engine's eight (`P_E0..P_E7`) are
  read from `DESC` each time a sound loads, since some engines rename them by mode.
- **Safety**: the firmware lets go of a remote FX or FILL hold when no request arrives for 1.5 s. The page also lets go
  of its holds when it is hidden.

`PERFORM` runs each op through the same code the panel's layers run (`firmware/src/perform.c` calls `layer_key`, sets
`punch.req`, the live-section and chain variables), so the screen, the LEDs and the messages follow as if the key had
been pressed, and a key pressed on the FM-1 always takes over from the phone.

## Tests

| Command | What it checks |
| --- | --- |
| `node web/live/test_live.mjs` | framing, the link (one in flight, priorities, coalescing, reply matching), every parser, the value formats, and `Device` against the demo device: connect, sound, knobs, presets, user presets, track selection, unplug |
| `tests/perform_test.c` (in `tests/run_tests.sh`) | `PERFORM` on the real UI and sequencer (the `ui_pages_test` harness): every op, its reply, the watchdog, a panel key taking over |
| `tests/live_mockdata.c` (in `tests/run_tests.sh`) | `mockdata.js` matches the firmware's tables; after a change to engines, presets or parameters: `build/host/live_mockdata > web/live/mockdata.js` |

The page itself was checked in Chromium (Playwright) at phone sizes (390 × 844, 360 × 640, 844 × 390 landscape) and
tablet sizes (820 × 1180, 1180 × 820, 1024 × 600), against the demo device and a fake Web MIDI that plugs and unplugs it.

## Adding to it

- **A control on the Sound page**: add the parameter to `TP_IDS` and `TP_LABEL` in `device.js` (its id and the label
  `DESC` returns), then to a page in `sound.js`.
- **A new live action**: add an op to `PERFORM` (`perform.c`, `pf_handle`), document it in `EDITOR_PROTOCOL.md`, test it
  in `tests/perform_test.c`, add it to `PF` in `proto.js` and to the demo device in `mock.js`. The state reply only ever
  grows at its end, so an older page keeps reading a newer firmware.
- **A transport** (Bluetooth MIDI, a relay): an object with `open(onData, onLost)`, `send(bytes)` and `close()`.
- **Style**: every file starts with `// SPDX-License-Identifier: GPL-3.0-only`; no dependencies; plain lowercase labels
  as on the FM-1's screen; 44 px or more for anything a thumb needs.

## Ideas, and what each needs

`next.js` lists them (the help sheet shows them in the app): the song order on the phone (the order is not yet
readable over USB outside a backup), clearing a section and repeats in a chain (firmware ops), steps on the phone (the
protocol has it), mutate, recording knob moves as parameter locks, the SYN drum kits, scenes and morph, and no cable (a
relay for `WebSocketTransport`, or Bluetooth MIDI in the firmware).
