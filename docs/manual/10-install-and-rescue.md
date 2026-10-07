# 10. Installation, Rescue & Technical Specifications

---

## 1. Installation Guide

Installing SLOOP on the M-VAVE FM-1 requires only a web browser (Chrome or Edge) with WebMIDI support:

1. Connect the FM-1 to your computer directly using a **USB-C data cable** (avoid passive unpowered USB hubs).
2. Start the local server or open the installer:
   * On Windows: Double-click **`INSTALL-SLOOP.bat`** in the repository folder.
   * On macOS / Linux: Run `tools/serve.py` and open `http://localhost:8766/webapp/installer/` in Chrome or Edge.
3. Click **`Connect`** and grant WebMIDI permission.
4. Click **`INSTALL`** and wait for the progress bar to show **Done** (keep the browser tab active during the transfer).
5. The FM-1 will automatically reboot and display the animated SLOOP logo.

---

## 2. Recovery & Rescue Procedures

### USB Rescue Mode
If an installation is interrupted, power is lost mid-flash, or the device fails to boot normally:
1. Turn off the FM-1.
2. **Hold down `OCT−` alone** and turn the power switch back on.
3. The display will show **`SLOOP USB RESCUE`**.
4. Connect via USB, open the web installer, and click **`INSTALL`** again. The firmware will re-flash cleanly.

### Interrupted Install Recovery
If the browser or cable disconnects while writing, the FM-1 stays in its bootloader update mode. Simply reconnect the cable, refresh the installer page, and click **Install** again. Damaged or partial firmware packages are automatically verified with SHA-256 and CRC checksums and will be rejected before flashing.

---

## 3. Returning to Official Stock Firmware

You can revert to the manufacturer's original stock firmware at any time:

1. Open the Web Editor and navigate to **Projects** $\rightarrow$ **Backup** to save a `.json` backup of your songs and patches.
2. In the web installer, open **`Return to the official firmware (V15)`**.
3. Download the official `FM-1.fwsc` (version 15) firmware package from the manufacturer's support page.
4. Select the file in the installer to flash the stock firmware.
5. *(You can also use M-VAVE's official M-UPGRADE utility if preferred).*
6. To return to SLOOP later, re-flash via the SLOOP installer and restore your `.json` backup file.

---

## 4. Hardware Specifications

| Component | Technical Details |
| :--- | :--- |
| **Platform** | M-VAVE FM-1 (ESP32-S3 microcontroller, PSRAM, Flash ROM) |
| **Display** | 240 × 240 color LCD with high-contrast custom layout engine |
| **Audio Quality** | 44.1 kHz, 16-bit stereo internal DSP processing |
| **USB Audio** | Class-compliant 44.1 kHz 16-bit stereo master output (no drivers required) |
| **Polyphony** | 8 shared synth voices across Tracks 1–3 + 6 voices for Track 4 Drums |
| **Synthesis** | 9 distinct sound engines (Virtual Analog, 4-Op FM, Phase Distortion, Lo-Fi Chip, Multisample, Formant Vocal, 3-Oscillator, Tonewheel Organ, Granular Cloud) |
| **Factory Sounds** | 68 level-matched presets categorized by musical kind + 32 user preset slots |
| **Drum Kits** | 37 level-matched kits (5 multisampled studio acoustic kits + 32 synthesized kits) |
| **Sequencer** | 4 tracks, up to 64 steps per track, independent lengths and divisions, per-step velocity levels (`GHOST`, `SOFT`, `NORM`, `HARD`), ratchets ($\times 1$ to $\times 4$), ties, slides |
| **Swing** | MPC swing algorithm (50% to 75%) with sample-accurate clock |
| **Performance** | 7 dedicated hold layers: Punch FX, Erase, Note Repeat, Step Sequencing, Key/Chords, Mix/Mute, Song Arranger |
| **Effects** | 16 momentary punch-in FX; master `DUST`, `DUCK`, and DJ filter; per-track overdrive, rhythmic slicer, stereo chorus, tempo delay, and FDN reverb |
| **Storage** | Automatic fail-safe background autosave, 4 full-machine project/section snapshots, 16-step song arranger chain |
| **MIDI** | USB MIDI (class-compliant) + 3.5 mm TRS MIDI IN (Channels 1–3 synths, Channel 10 drums, Channels 4–16 active track, MIDI clock in) |

---

## 5. Credits & License

* **Core Heritage:** SLOOP is built on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic) / Hügelton Instruments.
* **Played-Note Key Lights & UI Innovations:** Contributed by [@renebohne](https://github.com/renebohne).
* **TRS MIDI Buffer Architecture:** Inspired by Felucca [Salt] by ChanceTheMaker and keremimo.
* **Typography:** Terminus Font (SIL Open Font License 1.1).
* **Acoustic Samples:** Versilian Studios VSCO-2 Community Edition, VCSL, and Sonic Pi (all CC0 public domain).
* **License:** GNU General Public License v3.0 (GPL-3.0).
