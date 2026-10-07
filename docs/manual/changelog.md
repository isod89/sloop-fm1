# SLOOP Changelog & Version History

---

## SLOOP 2.3

### Connectivity & Audio
* **TRS MIDI IN Jack Support:** The FM-1's 3.5 mm TRS MIDI input jack is fully functional:
  * Channels 1–3 play Synth Tracks 1, 2, and 3.
  * Channel 10 plays Track 4 (Drums).
  * Channels 4–16 play the active selected track.
  * Buffer reads by content to prevent hung notes.
* **MIDI Clock Input:** Under `GLO` $\rightarrow$ `SYSTEM` $\rightarrow$ `SYNC`, select **`USB`** or **`TRS`**. SLOOP follows external master tempo, START, CONTINUE, and STOP with 24 ppqn pulse sync (no clock drift).
* **USB Class-Compliant Stereo Audio Input:** Connect the FM-1 via USB to a PC, Mac, iOS, or Android device to record 44.1 kHz 16-bit stereo master audio directly into DAWs or recording apps without drivers.
  * Under `HOME` menu $\rightarrow$ `USB AUDIO`, select **`MASTER`** (follows the volume knob) or **`FULL`** (fixed full-scale output with hardware brickwall limiting).

### Workflow & Recording
* **Configurable REC Parameters:** On the armed `REC READY` screen:
  * `KNOB 1 (mode)`: Choose between **`free`** (free take where the loop length sets tempo) and **`tempo`** (record to fixed BPM).
  * `KNOB 2 (length)`: Choose 1, 2, or 4 bars.
  * `KNOB 3 (start)`: Choose **`note`** (first key press starts recording) or **`count`** (1-bar audible metronome count-in).
* **Night Mode Button & Key Illumination:**
  * Hold `HOME` for the system menu to configure `LIGHTS` (`OFF`, `LOW`, `MID`, `HIGH`) to illuminate all panel labels in dark environments.
  * `KEYS`: Glow for C keys or all white keys.
  * `NOTES`: Real-time note illumination across all pages and layers for sounding notes.

### Web Editor & Librarian
* **Full Machine Backup & Restore:** Save everything on the FM-1 into a single `.json` backup file (projects, songs, user presets, sample banks, and settings) and restore it in under two seconds.
* **Advanced Sample CHOP Tool:** Slice recordings of any length into up to 16 chops with automatic transient detection, grid slicing, length trimming, and one-click "Fit to slot" optimization.
* **Firmware Rollback:** Return to factory stock M-VAVE V15 firmware directly from the web installer.

### Stability & Performance
* Elimination of knob jitter and missed encoder clicks.
* Voice overload management: gentle per-voice shedding under heavy polyphony loads.
* Stricter checksum validation for presets and flash storage.

---

## SLOOP 2.2

* **Next-Generation Synthesized Drum Engine:** Re-architected synth drum models featuring tuned bodies, pitch drop curves, head resonance partials, attack clicks, filtered noise, and non-linear saturation drive.
* **37 Leveled Drum Kits:** Added specialized kits including `PHONK`, `AMAPIANO` (log drums), `GARAGE`, and `D.HOUSE`, plus studio acoustic CC0 sample kits.
* **68 Factory Presets Browsed by Kind:** Organized by musical category (`BASS`, `KEYS`, `ORGN`, `PAD`, `LEAD`, `PLCK`, `STAB`, `FX`) with level matching across all presets.
* **Acoustic Grand Piano Multisamples:** Clean Steinway-modeled sample sets for `GRAND PNO`, `DUSTY PNO`, and `LOFI KEYS`.
* **Layer Locking:** Hold any performance layer and tap `HOME` to lock the layer open for two-handed playing.
* **True Stereo Effects:** True stereo chorus and feedback delay network (FDN) reverb.
* **Automatic Flash Save Retry:** Enhanced data integrity and error recovery.

---

## SLOOP 2.1

* **Live Song Mode:** Hold `SAVE` to trigger sections A–D on the next bar, store active loops into sections, and record complete song arrangements live with automatic section tracking.
* **Hardware Visual Landmarks:** Dim 4-key guidance glow on keys 1, 5, 9, and 13 to guide hands across the 4x4 matrix without looking at the hardware screen.

---

## SLOOP 2.0

* **Dual-Action Function Buttons (Tap vs. Hold Layers):** Every button acts as a deep editing page on tap, and a live performance layer on hold.
* **16-Sound Drum Machine:** 16 distinct drum voices mapped across the 16 white keys with ghost (`OCT-`) and hard (`OCT+`) velocity hits and ratchets.
* **Performance Toolset:** Note repeat rolls (`ARP`), live erase (`EDIT`), step sequencer entry (`SEQ`), scale quantization and chords (`SEL`), mixer mute/solo (`GLO`).
* **Autosave & Undo:** Background flash autosave after idle periods, and 1-level undo/redo (`EDIT + OCT- / OCT+`).
* **Master DJ Effects:** Master `DUST` (vinyl texture), `DUCK` (kick sidechain), and `FILT` (bipolar DJ filter).
* **Web Editor Integration:** 16-lane drum grid editor, sample manager, and patch librarian over WebMIDI.
