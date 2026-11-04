# Building SLOOP

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `felucca.fwsc` | the installable package (app + loader) |

## Windows (WSL)

`INSTALL-SLOOP.bat` builds in a WSL distribution and opens the installer on
`http://localhost:8766/webapp/installer/`. It needs Python 3 with Pillow on Windows, a WSL
distribution with the JieLi toolchain, and the three SDK files (below) in `build/deps/ac79`.
Set `SLOOP_WSL_DISTRO` (default `Ubuntu`) and `SLOOP_TOOLCHAIN` (a Linux path, default
`/root/.jieli/toolchain`) if yours differ.

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.9-beta` makes a release build: the package identity becomes
`FM-1_909` and the version string `0.9-BETA`; the package is `build/felucca-0.9-beta.fwsc`.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_UAC` | 1 | USB audio input: the master output, 44.1 kHz stereo (after Felucca 1.0) |
| `FELUCCA_UART` | 1 | TRS MIDI IN (the 3.5 mm jack) |

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works and the
SAMPLE engine has only the generated drum kit.

## Tests

```
tests/run_tests.sh
```

Runs the host tests (flash storage, user presets, MIDI parser, update entry, update
loader, a DSP render, the 4-track mix, project formats, the SLICER, the regression suite,
the command-line installer) and, with Node.js, the web page tests. Run it after `./build.sh`
(it uses `build/` and needs `AC79_SDK` set as for the build).

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). The host CPU cost is relative
to the idle + drums mix, counted by the kernel on macOS and under callgrind on Linux when
valgrind is installed (exact, about 45 s more); without either it is timed, a rough check.
After an intended change of the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites
the hashes; `BUDGET_UPDATE=1` does the same for the cost files.

## Browser audio preview

The browser preview uses the firmware's DSP and sequencer with RAM-backed user samples.
It requires Python 3, a host C compiler, Node.js for tests, and Emscripten (`brew install emscripten` on macOS).
It does not need the JieLi toolchain, AC79 SDK, Docker, or an FM-1.

```sh
python3 tools/build_preview.py
tests/run_preview_tests.sh
```

The build produces `build/preview/sloop-preview.wasm`. The tests compare all twelve engines sample by sample
against the same adapter compiled for the host and verify sequencing, live mixer changes, stop, and user samples.
`web/make_site.py` includes the preview and caches it for the site app when the WASM build is present:

```sh
# Use a release package downloaded from isod89/sloop-fm1, or your own firmware build.
python3 web/make_site.py build/releases/sloop-2.6.fwsc 2.6 build/site
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/site
```

Open `http://localhost:8766/webapp/editor/?mock=1`, **Connect**, then **Play loop** to hear all four tracks.
**Show preview** opens the piano/pads, volume and editable sequence; **Hide preview** folds them while
playback continues. The piano keys and labeled computer keys audition the selected track.
The audio controls are available only in simulated mode. The physical-device editor continues to use Web MIDI.
The standalone downloaded HTML editor does not include the WASM asset.

## The browser emulator

`web/emu/` compiles the whole firmware — the panel, the screen, the sequencer, storage and the projects — to
WebAssembly and runs it against a simulated FM-1 in an AudioWorklet, so SLOOP can be played in a browser without
the instrument. (After Felucca's `web/emu/`, itself after X0X's, both GPL-3.0.) It needs the generated headers in
`build/gen` (the firmware build below makes them) and either clang + wasm-ld or Emscripten; no JieLi toolchain,
AC79 SDK, Docker or FM-1.

```sh
sh web/emu/build.sh                                   # build/emu/{sloop.wasm, worklet.js, index.html, fonts/}
node web/emu/emu_test.mjs build/emu/sloop.wasm        # boot, screen, LEDs, input, audio, flash, song files
python3 -m http.server 8766 --directory build/emu     # then open http://localhost:8766/
```

`EMCC=1 sh web/emu/build.sh` builds with Emscripten instead of clang; `CC=` picks another clang, and `PYTHON=`
another interpreter for the header generators. The page keeps the emulated flash's written sectors in the
browser's storage, so the working project, the sections A–D and the user presets survive a reload. **Load a song or
a backup** takes an editor `.sloopsong` or a `.json` backup and writes its objects the way the editor's `BK_PUT`
does. `web/emu/sloop_web.c` is the adapter: nothing of the firmware changes for it. What it leaves out: USB (MIDI,
audio, the editor, updates), TRS MIDI, the serial console, the second core, the battery and the U-BOOT paths.

## Install

On Windows, `INSTALL-SLOOP.bat` builds and opens the web installer (Chrome or Edge). The
`.fwsc` of each release is on the GitHub releases page.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/felucca.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/felucca.fwsc dev /tmp/felucca-site
cd /tmp/felucca-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
