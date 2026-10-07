# SLOOP User Guide & Reference Manual

Welcome to the comprehensive, beginner-friendly guide for **SLOOP** on the M-VAVE FM-1 (based on upstream SLOOP 2.3).

SLOOP transforms the compact FM-1 into a self-contained, 4-track groovebox with 9 synth engines, a 16-pad drum machine, Elektron-style step sequencing, live performance effects, and a complete song arranger.

---

## Sixty Seconds to a Beat (Quick Start)

1. **Select the Drum Track:** Turn **`ALGORITHM`** (bottom right knob) to **Track 4** (orange, drums). The 16 white keys play 16 drum sounds: `F3` Kick, `G3` Kick 2, `A3` Snare, `B3` Clap, `C4` Closed Hat, `D4` Open Hat... Turn **`PRESETS`** (bottom left knob) to pick a kit (try *808* or *BOOMBAP*).
2. **Hit Record:** Press **`REC`** (*rec ready*). **Play a beat freely at your own tempo**—no click, no count-in. Hold **`OCT−`** while hitting for ghost notes, **`OCT+`** for hard accents.
3. **Close the Loop:** **Press `REC` on the "1" right after your last bar.** The loop immediately closes: SLOOP measures your phrase, calculates the tempo, snaps your hits to the grid, and loops playback at once!
4. **Overdub & Rolls:** Press **`REC`** again while it plays to overdub. Hold **`ARP`** and hold the hi-hat key (`C4`): an instant 1/16 hat roll is recorded as ratchets into your sequence.
5. **Add Bass & Chords:** Turn **`ALGORITHM`** to **Track 1** (blue, bass), press **`REC`**, and play a bassline. Hold **`SEL`** (the scale button) and press the root key of your song (e.g. `D`). Turn **`ALGORITHM`** to **Track 2** (green, keys), hold **`SEL`**, and turn **`KNOB 1`** to `7TH`: every single white key now plays a full, rich 7th chord in that key!
6. **Live Performance FX:** Hold **`FX`** and press any white key for momentary punch-in effects (stutters, tape stops, filter drops). While still holding `FX`, turn **`KNOB 2`** for vinyl `DUST` and **`KNOB 3`** for sidechain `DUCK`.
7. **Made a mistake?** Hold **`EDIT`** and press **`OCT−`** to immediately Undo!

---

## Choose Your Path

Whether you just received your FM-1, switched from another firmware, or want to master deep sound design, here is the fastest way to get what you need:

### By Who You Are

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. Just unboxed your FM-1 & installed SLOOP?                                │
│    • Safety net: Check USB Rescue Mode in Ch. 10 (Hold OCT− on power up)    │
│    • Play immediately: Follow "Sixty Seconds to a Beat" above               │
│    • Learn the physical hardware: Ch. 01 Hardware & Controls                │
│                                                                             │
│ 2. Coming from Felucca or baud girl firmware?                               │
│    • The big difference: Tap opens pages; Hold activates live Layers!       │
│    • Try the new drum engine: Ch. 04 Drums (16 pads on white keys)          │
│    • Master tape & sidechain bus: Ch. 02 Tracks & Mixer (DUST & DUCK)       │
│    • See what's new in SLOOP 2.0–2.3: Changelog                             │
│                                                                             │
│ 3. Already know SLOOP and want mastery?                                     │
│    • Advanced sequencing: Ch. 05 Step Sequencer (303 slides, parameter-locks)│
│    • Build complete tracks: Ch. 07 Song Mode & Arranger (Sections A–D)      │
│    • Deep synthesis: Ch. 03 Sound Design & 9 Engine Reference Guides       │
│    • Custom samples: Ch. 08 Presets & Projects (Web Editor CHOP slicer)     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### By What You Want to Do

* **Use Case A: Standalone Groovebox Production (Beats, Loops & Songs)**
  * You want to produce complete beats and songs with headphones on the go without diving into synth parameters.
  * **Core Mental Model:** 
    * The 4 tracks (1 Bass, 2 Keys, 3 Lead, 4 Drums) play together. 
    * **Sections A, B, C, D** are complete 4-track scenes (Verse, Chorus, Bridge, Outro).
  * **Beatmaker Tips & Traps to Avoid:**
    * *Trap 1: Don't lose your edits!* **Always store before you switch sections** (Hold `SAVE` + Key 5 `save A` before switching to B!).
    * *Trap 2: SEQ 3rd tap mystery:* Tapping `SEQ` 3 times opens the `SONG` arranger and lights up the `SAVE` button LED. That is normal—you haven't wiped anything!
    * *Accidental Erase?* If you hold `REC` too long and clear a track, press **`EDIT` + `OCT−`** immediately to Undo!
  * **Recommended Journey:**
    1. [Ch. 04 — Drums](04-drums.md): Master the 16 drum pads, ghost notes, and 37 kits.
    2. [Ch. 06 — Live Recording](06-live-recording.md): Free takes, overdubbing, and 16 punch-in effects.
    3. [Ch. 05 — Step Sequencer](05-step-sequencer.md): Elektron-style step programming and ratchets.
    4. [Ch. 07 — Song Mode](07-song-mode.md): Structuring sections A–D and recording a live arrangement.

* **Use Case B: Live Synth Jamming & Sound Design**
  * You want to explore the 9 engines, plug in a MIDI keyboard, craft custom patches, or record into a DAW.
  * **Recommended Journey:**
    1. [Ch. 09 — Settings, MIDI & USB Audio](09-settings-midi-usb.md): Connect TRS/USB MIDI and record 44.1 kHz USB audio.
    2. [Ch. 03 — Sound Design Guide](03-sound-design.md): Envelopes, LFO routings, per-track drive and slicer.
    3. [The 9 Synthesis Engines](03-sound-design.md#1-the-9-synthesis-engines): Deep dive into virtual analog, 4-op FM, phase distortion, or granular clouds.
    4. [Ch. 08 — Presets & Projects](08-presets-and-projects.md): Store patches in the 32 hardware user slots and manage backups.

* **Use Case C: Sampling & Crate-Digging (Custom Audio & Chops)**
  * You want to slice vinyl rips, drum breaks, or your own instruments across the 16 white keys.
  * **Core Mental Model:**
    * The FM-1 hardware has no analog audio input (the 3.5mm jack is MIDI IN). Audio is transferred via the Web Editor over USB into 3 user slots (`USR1`, `USR2`, `USR3`, ~7.4 s each).
    * Custom samples live on **Synth Tracks 1, 2, or 3** via the `SAMPLE` and `GRAIN` engines, not on Track 4.
  * **Recommended Journey:**
    1. [Ch. 08 — The Sampling & Crate-Digger Guide](08-presets-and-projects.md#4-the-sampling--crate-digger-guide-usr1usr3--chop): Slice recordings with `CHOP`, fit to slot, and flash over USB.
    2. [Ch. 03 — SAMPLE Engine Guide](engines/05-sample.md): Play chops polyphonically, dial in vintage 12-bit/8-bit grit, damping, and drive.
    3. [Ch. 03 — GRAIN Engine Guide](engines/09-grain.md): Transform sliced audio into lush granular soundscapes.
    4. [Ch. 06 — Live Recording](06-live-recording.md): Jam chops with vinyl `DUST`, sidechain `DUCK`, and punch-in tape drops.

---

## The Core Mental Model

Understanding these 4 core concepts makes navigating the FM-1 effortless:

### 1. The 4 Tracks & Color Code
SLOOP always runs 4 independent instruments simultaneously:

| Track | Type | Instrument | Fixed Color | Default Sound |
| :---: | :---: | :--- | :--- | :--- |
| **1** | Synth | Bass | **Blue** | *808 BOOM* |
| **2** | Synth | Keys / Chords / Pad | **Green** | *RHODES* |
| **3** | Synth | Lead / Melody | **Yellow** | *LOFI FLUTE* |
| **4** | Drums | 16-Sound Drum Machine | **Orange** | *808 Kit* |

* **Fixed Track Colors:** The 4 track identity colors (**Blue**, **Green**, **Yellow**, **Orange**) are permanently assigned to Tracks 1–4 across all tracks, waveforms, step sequencer lanes, and mixer tiles. They do **not** change when switching themes.
* Turn the **`ALGORITHM`** knob at any time to switch which track you are controlling.
* `KNOB 1 – 4` control the 4 parameters shown on screen (as top columns on edit pages, and rotary dials at the bottom on performance screens).

---

### 2. The Dual Life of Every Button: TAP vs. HOLD

Every function button on the FM-1 has two distinct modes:

* **TAP (quick press & release):** Opens visual menu pages on the screen for deep editing. Tapping the same button repeatedly cycles through related pages in that family (e.g. `1/2` $\rightarrow$ `2/2`).
* **HOLD (keep pressed down):** Activates a **Performance Layer**. The 16 white keys become direct function tiles and the knobs control live performance macros.
* **LOCK A LAYER:** Hold any layer button and tap **`HOME`**. The layer locks open (`LOCK` on screen), leaving both hands free for the keys and knobs! Tap any button to release.

---

### 3. The Knobs & Encoders

* **`MASTER` (Top Left):** Master volume.
* **`SELECT` (Top Right):** Global Tempo (BPM). Turn on any screen to adjust tempo without entering a menu.
* **`PRESETS` (Bottom Left):** Sound Browser. Turn to scroll synth presets (Tracks 1–3) or drum kits (Track 4).
* **`ALGORITHM` (Bottom Right):** Track Selector. Turn left/right at any time to switch active Track 1, 2, 3, or 4.
* **`KNOB 1 – 4`:** Parameter Knobs. Directly control the 4 parameter columns or dials shown on the active screen.

---

### 4. Step Sequencing vs. Song Arranging

* **Pattern (Micro / Meso):** Each of the 4 tracks has its own 1–64 step pattern with custom lengths and swing.
* **Sections (A, B, C, D):** SLOOP holds 4 full machine snapshots called Sections **A**, **B**, **C**, and **D** (e.g., Verse, Chorus, Bridge, Outro).
* **Song (Macro):** The Song Arranger chains sections A–D in any order for any number of bars.

---

## Master Hardware Reference Table

| Button | Mode | Screen / Layer | Scope | What it does & Knobs 1–4 |
| :--- | :--- | :--- | :--- | :--- |
| **`HOME`** | **Tap** | **TRACKS** | Main View | **4-Track Performance Screen.** Shows volume faders, mute/solo badges, and live patterns. <br>• K1: `SWING` • K2: `LEVEL` • K3: `STEPS` • K4: `PAN` |
| | **Hold** | **System Menu** | Settings | Hold for system menu: `LIGHTS`, `KEYS`, `NOTES`, `USB AUDIO`, `CALIBRATION`, `ABOUT`. |
| **`PLAY`** | **Tap** | Transport | Global | Start / stop playback. Flashes on each beat as a visual metronome. |
| **`REC`** | **Tap** | **REC READY / FREE TAKE** | Recording | **Live Recording.** <br>• *Stopped (empty):* Free take (tempo/length snap to your playing). <br>• *Stopped (with notes):* Arm mode (K1: Mode, K2: Length, K3: Start count). <br>• *Playing:* Immediate overdub recording on active track. |
| | **Hold** | Clear Track | Confirmation | Ring fills over ~2 sec; holding until full clears active track pattern. |
| **`OCT−` / `OCT+`**| **Tap** | Octave Shift | Performance | Shift active synth track $\pm 3$ octaves. Pressing both resets to 0. |
| | **Hold** | Drum Velocity | Drums | On Track 4 (Drums), hold `OCT−` for **Ghost** notes or `OCT+` for **Hard** hits. |
| | **Combo** | Undo / Redo | In EDIT layer | Inside `EDIT` layer: `OCT−` = **Undo**, `OCT+` = **Redo**. |
| **`ENV`** | **Tap** | **`ENV`** (1/2) | Track | **Amplitude ADSR.** K1: `ATK`, K2: `DEC`, K3: `SUS`, K4: `REL`. |
| | **Tap** | **`ENV DEST`** (2/2) | Track | **Envelope Modulation.** K1: `FLT` (Filter env), K2: `PIT` (Pitch punch), K3: `SHP` (Wave shape). |
| **`LFO`** | **Tap** | **`LFO`** (1/2) | Track | **LFO Generator.** K1: `RATE` (Hz/sync), K2: `WAVE`, K3: `PHASE`, K4: `FADE`. |
| | **Tap** | **`LFO DEST`** (2/2) | Track | **LFO Routing.** K1: `PIT` (Vibrato), K2: `FLT` (Wah), K3: `SHP` (PWM), K4: `AMP` (Tremolo). |
| **`FX`** | **Tap** | **`FX`** (1/4) | Track | **Track FX Sends.** K1: `DIST` (Drive), K2: `CHOR`, K3: `DLY`, K4: `REV`. |
| | **Tap** | **`SLICER`** (2/4) | Track | **Track Slicer.** K1: Mode (`OFF`, `GATE`, `STUT`), K2: Pattern 1–16, K3: Rate, K4: Depth. |
| | **Tap** | **`DLY`** (3/4) | Global | **Master Delay.** K1: `TIME`, K2: `FDBK`, K3: `COLR` (Filter), K4: `MIX`. |
| | **Tap** | **`REV/CHO`** (4/4) | Global | **Master Reverb & Chorus.** K1: `SIZE`, K2: `DAMP`, K3: Chorus Rate, K4: Chorus Depth. |
| | **Hold** | **`punch` Layer** | Live FX | White keys 1–16 trigger **16 punch-in effects** (repeat, drop, filter sweeps). <br>• K1: `FILTER` (DJ HPF/LPF) • K2: `DUST` (Vinyl) • K3: `DUCK` (Sidechain). |
| **`EDIT`** | **Tap** | **`EDIT 1`–`VOICE 2`** | Track (Synth) | **Deep Synth Design.** Engine parameters P_E0–P_E7, Mono/Poly, Glide, Detune, Pan. |
| | **Tap** | **`GRID` / `KIT`** | Track (Drums) | On Drum track: Toggles between 16x16 **Drum Grid** and 16-pad **Drum Kit**. |
| | **Hold** | **`erase` Layer** | Live Erase | Hold and tap keys to erase notes/drums live while playing or stopped. <br>• K1: `SHIFT` • K2: `LENGTH` ($\times 2$ or $\frac{1}{2}$) • K3: `TRANSPOSE`. |
| **`ARP`** | **Tap** | **`ARP`** (1/2) | Track | **Arpeggiator Setup.** K1: `MODE` (Up, Down, Random, Order), K2: `RATE`, K3: `OCT`, K4: `GATE`. |
| | **Tap** | **`ARP 2`** (2/2) | Track | **Arp Expression.** K1: `SWING`, K2: `PROB` (Probability), K3: `HOLD`, K4: `ORDER`. |
| | **Hold** | **`roll` Layer** | Note Repeat | Hold ARP + any key for instant note repeat (rolls) recorded as ratchets. <br>• K1: `RATE` (1/8, 1/16, 1/32, 32T, 1/64). |
| **`SEQ`** | **Tap** | **`STEP`** (1/3) | Track | **Numeric Step Editor.** K1: Step cursor (1–64), K2: Note, K3: Time (`NOTE`, `TIE`, `REST`), K4: Flags (`ACC`, `SLD`). |
| | **Tap** | **`PATTERN`** (2/3) | Track | **Pattern Settings.** K1: `LEN` (1–64), K2: `DIV` (Division), K3: `SWING` (50–75%), K4: `GATE`. |
| | **Tap** | **`SONG`** (3/3) | Global | **Song Chain Editor.** (See `SAVE` on Home). |
| | **Hold** | **`steps` Layer** | Step Sequence | White keys 1–16 = steps 1–16. Tap to add/delete steps. <br>• **Hold step key + turn knobs:** K1: Note/Sound, K2: Level, K3: Ratchet ($\times 1..\times 4$). <br>• Black keys 1–4 or `OCT−`/`OCT+`: Switch step pages (1–16, 17–32, 33–48, 49–64). |
| **`SEL`** *(Scale)* | **Tap** | **`SCL`** (1/2) | Track | **Scale & Chords.** K1: `ROOT` (C..B), K2: `SCALE`, K3: `QUANT`, K4: `CHORD` (5th, 7th, etc.). |
| | **Tap** | **`SCL 2`** (2/2) | Track | K1: `TRANS` (Global transpose). |
| | **Hold** | **`key` Layer** | Quick Key Set | Tap any white key to instantly set the root key of the whole song! <br>• K1: `CHORD`, K2: `SCALE`, K3: `KEYS`, K4: `TRANSPOSE`. |
| **`GLO`** | **Tap** | **`GLOBAL`** (1/4) | Global | **Tempo & Sync.** K1: `BPM`, K2: `SWING`, K3: `CLICK` (Metronome), K4: `TUNE`. |
| | **Tap** | **`MASTER`** (2/4) | Global | **Master Bus FX.** K1: `DUST`, K2: `DUCK`, K3: `FILT`, K4: `ROLL`. |
| | **Tap** | **`SYSTEM`** (3/4) | Global | **Hardware Setup.** K1: `USB`, K2: `SYNC` (`INT`, `USB`, `TRS`), K3: `ROUT`, K4: `CPU`. |
| | **Tap** | **`DRUMS`** (4/4) | Global | **GM Drums.** K1: MIDI Channel, K2: Level, K3: Reverb send. |
| | **Hold** | **`mix` Layer** | Mixer / Mute | • Keys 1–4: Mute Tracks 1–4 <br>• Keys 5–8: Solo Tracks 1–4 <br>• Key 16: Tap Tempo <br>• Knobs 1–4: Volume faders for Tracks 1, 2, 3, 4. |
| **`SAVE`** | **Tap (on Home)** | **`SONG`** | Song Mode | **Arranger Screen.** Set song sequence of sections A–D. <br>• K1: Step (1–16) • K2: Section (A–D) • K3: Bars • K4: Total steps <br>• Tap `SAVE` again to save chain. |
| | **Tap (elsewhere)**| **`PRESETS`** (1/4) | Storage | **Preset Category Browser.** K1: Category step, K2: Engine jump. |
| | **Tap** | **`USER`** (2/4) | Storage | **32 User Patch Slots.** K1: Slot (1–32), K2: Load, K3: Erase, K4: Save. |
| | **Tap** | **`PROJECT`** (3/4) | Storage | **Project Slots 1–4 (Sections A–D).** K1: Slot, K3: Load, K4: Save. |
| | **Tap** | **`TOOLS`** (4/4) | Storage | **Utilities.** K1: Clear pattern, K2: Init sound, K4: **NEW PROJECT**. |
| | **Hold** | **`song` Layer** | Live Arranger | • Keys 1–4: Launch section A–D on next bar <br>• Keys 5–8: Save loop into section A–D <br>• Key 13: Loop mode $\leftrightarrow$ Song mode <br>• Key 14: Song REC (record live arrangement) <br>• Key 16: Open Song editor screen. |

---

## Detailed Topic Guides

Dive deep into specific areas of SLOOP with dedicated guides:

1. [**Hardware & Controls Guide**](01-hardware-and-buttons.md) — Knobs, buttons, locking layers, and display.
2. [**The 4 Tracks & Mixer**](02-tracks-and-mixer.md) — Balancing parts, mutes, solos, and the master bus.
3. [**Sound Design & Synth Engines**](03-sound-design.md) — The 9 synthesis engines, envelopes, LFOs, and effects.
4. [**Drum Machine & Kits**](04-drums.md) — The 16 drum sounds, 37 drum kits, grid editing, and ratchets.
5. [**Step Sequencer Guide**](05-step-sequencer.md) — Programming steps, ties, slides, and parameter locks.
6. [**Live Recording & Performance**](06-live-recording.md) — Free takes, overdubbing, 16 punch FX, and rolls.
7. [**Song Mode & Arranger**](07-song-mode.md) — Building sections A–D, recording songs live, and chaining.
8. [**Presets, Projects & Web Editor**](08-presets-and-projects.md) — 68 factory sounds, user patches, backups, and USB librarian.
9. [**Settings, MIDI & USB Audio**](09-settings-midi-usb.md) — Hardware options, clock sync, and DAW recording.
10. [**Installation, Rescue & Specifications**](10-install-and-rescue.md) — Setup, recovery modes, specs, and license.

* [**Changelog & Version History**](changelog.md) — Release notes and changelog from SLOOP 2.0 through 2.3.
