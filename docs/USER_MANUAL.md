<p align="center"><img src="../assets/logo/sloop-logo.png" alt="SLOOP" width="420"></p>

# SLOOP 2.2 — User Manual

**SLOOP** is a free, open-source groovebox firmware for the **M-VAVE FM-1**. This manual is for musicians: it takes you from installation to your first beat, and it works as a reference afterwards.

> This manual describes **SLOOP 2.2**, the version string in the firmware source (`firmware/src/ui.c`). It was written from the firmware, the tests, the web editor and the existing documents in this repository. Where those disagree, the source was followed and the difference was logged in [MANUAL_REVIEW_NOTES.md](MANUAL_REVIEW_NOTES.md).

<p align="center"><img src="../assets/screens/screens.png" alt="SLOOP screens on the FM-1" width="760"></p>

## Contents

1. [What SLOOP is](#1-what-sloop-is)
2. [Important warnings](#2-important-warnings)
3. [System requirements](#3-system-requirements)
4. [Installing SLOOP](#4-installing-sloop)
5. [Recovering from an interrupted installation](#5-recovering-from-an-interrupted-installation)
6. [Returning to the official M-VAVE firmware](#6-returning-to-the-official-m-vave-firmware)
7. [The FM-1 controls and the colour system](#7-the-fm-1-controls-and-the-colour-system)
8. [Your first beat in five minutes](#8-your-first-beat-in-five-minutes)
9. [The four-track architecture](#9-the-four-track-architecture)
10. [Sounds: engines, presets, drum kits and samples](#10-sounds-engines-presets-drum-kits-and-samples)
11. [Recording and groove](#11-recording-and-groove)
12. [Step sequencing, note repeat, scales, chords, undo and erase](#12-step-sequencing-note-repeat-scales-chords-undo-and-erase)
13. [Effects: punch-in, DUST, DUCK, filter and sends](#13-effects-punch-in-dust-duck-filter-and-sends)
14. [Mixer, tempo, projects and saving](#14-mixer-tempo-projects-and-saving)
15. [Song mode](#15-song-mode)
16. [The web editor](#16-the-web-editor)
17. [MIDI and external operation](#17-midi-and-external-operation)
18. [Updating SLOOP safely](#18-updating-sloop-safely)
19. [Troubleshooting](#19-troubleshooting)
20. [Technical specifications](#20-technical-specifications)
21. [Glossary](#21-glossary)
22. [For contributors](#22-for-contributors)
23. [Credits, licence and trademarks](#23-credits-licence-and-trademarks)

---

## 1. What SLOOP is

SLOOP replaces the firmware of the FM-1 with a **four-track live groovebox**: three synthesiser tracks and one drum track. It has nine synthesis engines, 68 factory sounds, 37 drum kits, three slots for your own samples, 16 punch-in effects and a song mode you play with your hands. It does not ship factory patterns: everything you hear, you play.

**Supported hardware:** the **M-VAVE FM-1** only. The installer checks the model and refuses other devices. The firmware is built for the FM-1's chip and flash memory, and the update loader refuses a flash chip it does not know.

SLOOP is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments. It is **not affiliated with M-VAVE**.

## 2. Important warnings

> **Beta software.** The README and SLOOP.md describe SLOOP 2.2 as *still a beta*. Report problems through [GitHub issues](https://github.com/julesdg6/sloop-fm1/issues).

> **Custom firmware is installed at your own risk.** There is no warranty. If an FM-1 no longer starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

> **Never unplug the USB cable while the installer is writing.** If it stops, replug the cable and press Install again (see [section 5](#5-recovering-from-an-interrupted-installation)).

> **Back up first.** Installing SLOOP replaces the FM-1's official firmware. Download the official firmware from m-vave.com and keep it somewhere safe *before* you install, so that you can [go back](#6-returning-to-the-official-m-vave-firmware). Keep your own sample files on your computer; SLOOP's projects and presets live in the FM-1's flash.

> **Your SLOOP projects are stored on the device.** They are kept in the FM-1's flash. SLOOP saves everything before an update, but a manual backup route is not documented in the repository (see the review notes). Treat the working project as something you cannot copy off the device.

> **Do not use a hub or a charge-only cable.** Use a data cable plugged directly into the computer.

## 3. System requirements

| Item | Requirement |
| --- | --- |
| Device | M-VAVE FM-1 |
| Browser (for the installer and the web editor) | **Chrome or Edge**. The pages tell other browsers to open in Chrome or Edge, because they need Web MIDI. |
| USB cable | A **data** cable, connected directly to the computer (no hub) |
| MIDI | The FM-1 appears as a class-compliant **USB-MIDI** device. No driver is needed. The editor looks for a port named "Felucca". |
| Other software | Close other programs that use MIDI while installing or editing |
| Command-line installer (optional) | Python 3 with `pip install mido python-rtmidi` |
| Local web copy (optional) | Web MIDI needs a secure context, so a local copy must be opened from `localhost` |

## 4. Installing SLOOP

### 4.1 From the browser (recommended)

1. Open the [SLOOP installer](https://isod89.github.io/sloop-fm1/) in **Chrome or Edge**.
2. Connect the FM-1 to the computer by USB (data cable, no hub).
3. Press **INSTALL** and allow MIDI access when the browser asks.
4. Wait for **Done**. Do not unplug the cable before then.
5. The FM-1 restarts on the SLOOP logo.

Nothing has to be downloaded or compiled. The installer checks the package's SHA-256 and refuses a damaged one; the update loader checks the package CRC before it starts the new firmware.

### 4.2 From the command line

Download the `.fwsc` package from the [releases page](https://github.com/julesdg6/sloop-fm1/releases), then:

```
pip install mido python-rtmidi
python tools/fm1_install.py sloop-2.2.fwsc
python tools/fm1_install.py --info        # identity of the connected FM-1
```

The script asks for confirmation; options include `--port NAME`, `--yes` and `--force`. It only resumes an interrupted install if the FM-1 is waiting in **SLOOP's own** update loader.

### 4.3 Building the firmware yourself

Windows users can run `INSTALL-SLOOP.bat`, which builds in WSL and opens a local installer. Building needs a toolchain and SDK files, so it is a contributor task: see [BUILDING.md](../BUILDING.md).

## 5. Recovering from an interrupted installation

| Situation | What to do |
| --- | --- |
| The install stopped or the cable was pulled | The FM-1 stays in **update mode**. Replug it and press **Install** again; it finishes the write. |
| The package was damaged | The installer or loader refuses it and the FM-1 keeps waiting for a good one. Install again. |
| SLOOP does not start but the FM-1 powers on | **USB rescue:** hold **OCT−** *alone* while switching on. The screen shows *SLOOP USB RESCUE* and "OPEN THE INSTALLER". Connect USB and install again. If it says *UNKNOWN FLASH*, the flash chip was not recognised. |
| The FM-1 does not start at all | Use [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter). |

Holding OCT− and OCT+ together at power-on does not enter rescue (that combination keeps calibration).

## 6. Returning to the official M-VAVE firmware

Use M-VAVE's own updater (**M-UPGRADE**) with the official FM-1 firmware from m-vave.com. The installer's protocol is described in its source as M-UPGRADE compatible. If the FM-1 will not start, use the rescue steps above first.

## 7. The FM-1 controls and the colour system

The firmware names the controls as follows. No photograph of the panel is in the repository, so the table lists the controls by name rather than by position.

| Control | Type |
| --- | --- |
| **16 white keys** (plus black keys) | The keyboard. Keys are numbered 1–16 from the lowest F. |
| **KNOB 1 – KNOB 4** | Four knobs |
| **SELECT** | Tempo |
| **ALGORITHM** | Selects the track (1–4) |
| **PRESETS** | Sound of the selected track, or the drum kit |
| **PLAY**, **REC** | Transport |
| **OCT− / OCT+** | Octave (synths), ghost / hard hits (drums) |
| **HOME** | The TRACKS screen |
| **ENV, LFO** | Their sound-design pages |
| **FX, EDIT, ARP, SEQ, SCL, GLO, SAVE** | The seven **layer** buttons |

### How buttons are described in this manual

| Wording | Meaning |
| --- | --- |
| **Tap** FX | Press and release it without touching anything else. Its pages open. |
| **Hold** FX | Keep it pressed. It becomes a **layer**: the white keys and the knobs change job. |
| "Hold FX, then press key 4" | Keep FX down, press the 4th white key. |
| "Hold FX, then turn KNOB 2" | Keep FX down and turn the knob. |
| Turn KNOB 1 | Rotate only; no button. |
| Hold a layer button and tap HOME | **Lock** the layer open (see below). |

After 0.14 s of holding, the screen shows the 16 keys as four rows of four tiles and the knobs as dials. Keys 1, 5, 9 and 13 glow dimly as landmarks (the first key of each row) while a layer is held and on the drum track; lit keys at full brightness are what is **on**.

### Layer reference

| Hold | White keys | KNOB 1 · 2 · 3 · 4 | Tap |
| --- | --- | --- | --- |
| **FX** | A punch-in effect while held | FILTER · DUST · DUCK · — | FX pages |
| **EDIT** | Erase that sound or note | SHIFT · LENGTH ×2 / ½ · TRANSPOSE · — | EDIT pages (drums: grid / kit) |
| **ARP** | Note repeat | RATE · — · — · — | ARP pages |
| **SEQ** | Steps 1–16 of the page | SOUND/NOTE · DIV · SWING · LENGTH | SEQ pages (drums: grid / kit) |
| **SCL** | Key of the song | CHORD · SCALE · KEYS · TRANSPOSE | SCL pages |
| **GLO** | 1–4 mute · 5–8 solo · 16 tap tempo | Levels of tracks 1–4 | GLO pages |
| **SAVE** | 1–4 play sections A–D · 5–8 save into A–D · 13 loop/song · 14 SONG REC · 16 chain | — | On TRACKS: SONG screen; elsewhere: SAVE pages |

### Other controls

| Control | Action |
| --- | --- |
| **PLAY** | Start / stop all four tracks (works inside any layer) |
| **REC** | Playing: record now / stop. Stopped: arm. Free take: close the loop. |
| **Hold REC** | Clear the selected track (see [section 14](#clear-a-track)) |
| **EDIT + OCT− / OCT+** | Undo / redo |
| **SELECT** | Tempo, always, even inside a layer |
| **HOME** | TRACKS screen. **Hold** HOME: menu (colour, low cut, zoom, calibration, about). |

**Lock a layer:** hold a layer button, then tap **HOME**. The layer stays open after you let go (*LOCK* on screen, the button blinks), so both hands are free. Any other button releases it and does only that; PLAY, REC and OCT− / OCT+ keep working inside it.

### The colour system

| Colour | Track | Knob |
| --- | --- | --- |
| **Blue** | 1 · synth | KNOB 1 |
| **Green** | 2 · synth | KNOB 2 |
| **Yellow** | 3 · synth | KNOB 3 |
| **Orange** | 4 · drums | KNOB 4 |

White always means *what you are touching*. Red always means *recording*. The four dials at the bottom of the screen show what the knobs do now. The PLAY light flashes on every beat as a visual metronome.

<p align="center"><img src="../assets/screens/layer-punch-on.png" alt="The FX layer with punch-in effects" width="360"> <img src="../assets/screens/layer-steps.png" alt="The SEQ layer showing steps" width="360"></p>

## 8. Your first beat in five minutes

1. **Choose drums.** Turn **ALGORITHM** to track **4** (orange). The white keys now play 16 drum sounds: F3 kick, G3 kick 2, A3 snare, B3 clap, C4 hat. Turn **PRESETS** to pick a kit, for example *808* or *BOOMBAP*.
2. **Arm recording.** Tap **REC**. The screen says *rec ready · play freely*.
3. **Play a beat freely** at the tempo you like. There is no click and no count-in. Hold **OCT−** while hitting for ghost (quiet) notes, **OCT+** for hard hits.
4. **Close the loop.** Tap **REC** on the "1" after your last bar. The length of your take sets the loop and the tempo, the hits snap to the grid and the loop starts at once.
5. **Overdub.** Tap **REC** while it plays to record on top. Hold **ARP** and hold the hat key for a 1/16 roll; it is recorded as ratchets.
6. **Add a bass.** Turn **ALGORITHM** to track **1** (blue, *808 BOOM*), tap **REC** and play a bass line.
7. **Choose a key and chords.** Hold **SCL** and press the key of your song (e.g. D). On track 2, hold SCL and turn **KNOB 1** to *7TH*: every white key now plays a chord of the key.
8. **Add effects.** Hold **FX** and press a white key for a punch-in effect. Still holding FX, turn **KNOB 2** for DUST and **KNOB 3** for DUCK.
9. **Fix a mistake.** Hold **EDIT** and press **OCT−** to undo.

Leave it alone for a few seconds after stopping: the project is autosaved ([section 14](#autosave)).

## 9. The four-track architecture

| Track | Colour | Role | Default sound on a new project | MIDI channel |
| --- | --- | --- | --- | --- |
| 1 | Blue | Synth | 808 BOOM | 1 |
| 2 | Green | Synth | RHODES | 2 |
| 3 | Yellow | Synth | LOFI FLUTE | 3 |
| 4 | Orange | Drums | 808 kit | 10 (default; adjustable) |

A new project starts at **90 BPM**. Select a track with **ALGORITHM**. Each track has its own pattern length (1–64 steps) and division, so polymeters stay in phase. The three synth parts share 8 voices; the drum part has 16 sounds and 6 voices. All tracks share one sample-accurate clock.

Changing a sound (PRESETS or a user preset) never changes that track's key, chord mode, pattern or mix.

<p align="center"><img src="../assets/screens/live-tracks.png" alt="The TRACKS screen" width="360"> <img src="../assets/screens/live-grid.png" alt="The drum grid" width="360"></p>

## 10. Sounds: engines, presets, drum kits and samples

### Synthesis engines

Analog, 4-op FM (DIGITAL), phase distortion (PHASE), lo-fi chip (LOFI), sampler (SAMPLE), formant voice (VOICE), three-oscillator (TRIO), tonewheel organ (WHEEL) and granular (GRAIN). A preset picks its engine automatically. Underneath are envelopes, LFO, arpeggiator, glide, voice modes, per-track drive and slicer, and sends to effects. Open ENV, LFO and the FX / SCL / ARP / SEQ pages for the full parameter set.

### Factory sounds (68)

Press **PRESETS** on a synth track to browse **by kind**; the kind is shown next to the name. Your own presets (32 slots) come after the factory ones. All factory sounds are level-matched: the same LEVEL gives about the same loudness.

| Kind | Sounds (engine) |
| --- | --- |
| **Bass** | 808 BOOM, 808 DIRTY, 808 SLIDE, SUB BASS, PLUGG BASS (these slide between held notes and sit two octaves under the keys) · REESE, WOBBLE, ACID 303, FUNK BASS (ANALOG) · FM BASS (DIGITAL) · CZ BASS (PHASE) · FAT BASS (TRIO) · WOW BASS (VOICE) · GB BASS (LOFI) · UP BASS, DEEP BASS (SAMPLE) |
| **Keys** | RHODES, DX RHODES, WURLI, M1 PIANO, AFRO KEYS, CLAV (DIGITAL) · GRAND PNO, DUSTY PNO, LOFI KEYS (SAMPLE) · SOFT KEYS (PHASE) |
| **Organ** | SOUL ORGAN, GOSPEL, JAZZ ORGAN, DIRTY B3, HOUSE ORGN (WHEEL) |
| **Pad** | WARM PAD, DARK STR, ATMOS PAD (ANALOG) · SAW PAD (TRIO) · GLASS PAD (DIGITAL) · CZ STRING (PHASE) · LOFI CLOUD, VIBE HAZE (GRAIN) · CHOIR AAH, SOUL OOH (VOICE) |
| **Lead** | SUPERSAW, G-FUNK LD (ANALOG) · SYNC LEAD, HOOVER (TRIO) · TALKBOX (VOICE) · GAME LEAD (LOFI) · LOFI FLUTE (SAMPLE) · FLUTE DUST (GRAIN) |
| **Pluck & bell** | TRAP PLUCK (ANALOG) · RESO PLUCK (PHASE) · PLUGG BELL, TRAP BELL, MUSIC BOX, KALIMBA, MARIMBA (DIGITAL) · VIBES (SAMPLE) · 8BIT ARP (LOFI) |
| **Stab** | MIN STAB, MIN7 STAB, RAVE STAB, DUB CHORD (TRIO) · SYN BRASS (ANALOG) · CZ BRASS (PHASE) · HORN STAB, STRING STB (SAMPLE) |
| **FX** | SCRATCH · GM KIT (SAMPLE) |

The SAMPLE engine's sets (PIANO, BASS, VIBES, HORNS, STRGS, FLUTE, SCRCH, PERC) are CC0 recordings, retuned and coloured like an old sampler.

### The drum track

Track 4 has 16 sounds, one per white key from the lowest F to the highest G. A black key plays the sound of the white key on its left, so two fingers can share one sound for fast rolls.

| Key | Sound | MIDI note | Key | Sound | MIDI note |
| --- | --- | --- | --- | --- | --- |
| 1 · F3 | kick | 36 | 9 · G4 | snare 2 | 40 |
| 2 · G3 | kick 2 | 35 | 10 · A4 | low tom | 43 |
| 3 · A3 | snare | 38 | 11 · B4 | hi tom | 48 |
| 4 · B3 | clap | 39 | 12 · C5 | crash | 49 |
| 5 · C4 | hat | 42 | 13 · D5 | ride | 51 |
| 6 · D4 | open hat | 46 | 14 · E5 | shaker | 70 |
| 7 · E4 | pedal hat | 44 | 15 · F5 | conga | 63 |
| 8 · F4 | rim | 37 | 16 · G5 | cowbell | 56 |

Every hit has one of four **levels**: GHOST, SOFT, NORM (as played) and HARD. Hold **OCT−** while hitting for ghost notes, **OCT+** for hard hits. Hits can repeat ×1–×4 inside their step (a **ratchet**). The closed and pedal hats choke the open hat.

### Drum kits (37)

Choose a kit with **PRESETS** on the drum track, KNOB 1 on the kit page, or the web editor. Kits 1–5 are a sampled acoustic kit and its treatments; 6–37 are synthesised. Every kit is level-matched, and the kit is saved with projects and song sections.

| # | Kit | # | Kit | # | Kit |
| --- | --- | --- | --- | --- | --- |
| 1 | ACOUSTIC | 14 | LO-FI | 27 | AFRO |
| 2 | DEEP | 15 | PHONK | 28 | LATIN |
| 3 | TIGHT | 16 | HOUSE | 29 | TRIBAL |
| 4 | BRIGHT | 17 | D.HOUSE | 30 | SYNTHWV |
| 5 | DUST | 18 | TECHNO | 31 | CHIP |
| 6 | 808 | 19 | MINIMAL | 32 | ARCADE |
| 7 | 909 | 20 | ELECTRO | 33 | GLITCH |
| 8 | 606 | 21 | DISCO | 34 | INDUSTR |
| 9 | 80S | 22 | GARAGE | 35 | HYPER |
| 10 | VINTAGE | 23 | JUNGLE | 36 | AMBIENT |
| 11 | TRAP | 24 | DUBSTEP | 37 | JAZZ |
| 12 | DRILL | 25 | DEMBOW | | |
| 13 | BOOMBAP | 26 | AMAPIANO | | |

Kit names describe styles, not products.

### Your own samples (USR1–USR3)

Three slots of about 7.4 s each hold your own sounds. A synth track plays them with engine **SAMPLE**, **SET** = USR1 / USR2 / USR3. Load them from the web editor ([section 16](#16-the-web-editor)). Slots accept up to 16 WAV files each; the note a file plays at its own speed is in its name (`KEYS_C4.wav`, C4 = MIDI 60).

## 11. Recording and groove

SLOOP records live and layers every pass on the last one (overdub). Notes go to the nearest step **as you heard it**: the ~12 ms between a key and its sound is taken back, so playing on the beat lands on the beat. Chords are kept on the synth tracks (up to 4 notes a step); held notes become ties.

| When | Tap REC | What happens next |
| --- | --- | --- |
| **Playing** | Records the selected track at once | Tap REC again to stop recording; the loop plays on |
| **Stopped, project has notes** | Arms (*rec ready*, REC blinks) | Your first note starts the loop and is step 1; PLAY starts it too |
| **Stopped, empty project** | Arms (*rec ready · play freely*) | A **free take** (below) |

### Free take

1. Tap REC and play freely. The screen shows the seconds and the loop you would get (e.g. *2 bars · 92 bpm*).
2. Tap REC on the "1" after your last bar. SLOOP picks 1, 2 or 4 bars at the tempo nearest the one set (within 3 % the set tempo is kept), writes your notes in and plays at once. All four tracks take that length.
3. Tap PLAY during a free take to drop it. A take closes by itself after 24 s.

While recording the REC light is solid and the track shows a red *rec*. Turning ALGORITHM moves the take to another track without stopping.

### Click

GLO → GLOBAL → **CLICK**: `OFF`, `REC` or `ON`. The click is never recorded.

### Quantisation

Quantisation is automatic: notes land on the nearest step of the track's division (SEQ + KNOB 2 sets **DIV**, from 1/4 to 1/32 with triplets).

### Swing

SLOOP uses MPC-style swing from 50 % (straight) to 75 %. Set it for all tracks at GLO → GLOBAL → SWING, or per track with SEQ + KNOB 3; the track's swing adds to the global one.

### Ghost notes and ratchets

Hold OCT− / OCT+ on the drum track for ghost / hard hits. Ratchets (×1–×4) come from ARP rolls (recorded automatically) or from SEQ + a step + KNOB 3.

## 12. Step sequencing, note repeat, scales, chords, undo and erase

### SEQ — step entry

Hold **SEQ**. The 16 white keys are the 16 steps of the page; lit ones play. The first four black keys (F♯3, G♯3, A♯3, C♯4) or **OCT− / OCT+** choose page 1–4 (steps 1–16, 17–32, 33–48, 49–64, up to the track's LENGTH).

- **Empty step:** press its key to set it (drums: with the sound shown; synths: with the last note or chord you played).
- **Set step:** press and release to clear it. To edit it, hold it and turn a knob: KNOB 1 sound/note, KNOB 2 LEVEL (ghost, soft, norm, hard), KNOB 3 RATCHET (×1–×4). Hold several step keys to edit them together.
- **No step held:** KNOB 1 sound/note to set · KNOB 2 DIV · KNOB 3 SWING (track) · KNOB 4 LENGTH (1–64 steps).

### ARP — note repeat

Hold **ARP**, then hold a key. It repeats on the grid at the rate of **KNOB 1**: 1/8, 1/16, 1/32, 32T or 1/64, locked to tempo and swing. On the drum track OCT− / OCT+ make it ghost / hard. While recording, a roll is written as ratchets. A roll ends when you release the key.

### SCL — key, scale and chords

- **Press any key** while holding SCL to set the **key of the song** (the root of all three synth tracks).
- **KNOB 1 CHORD** (selected synth track): OFF, TRIAD, 7TH, 9TH, SUS4, POWER. With a chord on, the white keys walk the scale from C4 — C4 plays chord I, D4 chord II, and so on — and one finger plays the whole chord.
- **KNOB 2 SCALE** (all synth tracks): 16 scales including major, minor, dorian, mixolydian, pentatonics, harmonic and blues.
- **KNOB 3 KEYS:** OFF (chromatic), SNAP (rounded to the scale), WHITE (white keys walk the scale, black keys silent).
- **KNOB 4 TRANSPOSE** the selected track by ±24 semitones.

### EDIT — erase and reshape

Hold **EDIT** and press a key to remove that sound (drums) or note (synths) from the selected track. While **playing**, it is removed from every step the playhead passes while you hold the key; when **stopped**, from the whole pattern. *ERASED* flashes. The knobs reshape the pattern: KNOB 1 SHIFT, KNOB 2 LENGTH (right ×2, left ½), KNOB 3 TRANSPOSE (synths).

### Undo and redo

Hold **EDIT**, then press **OCT−** (undo) or **OCT+** (redo). One level: the last recording pass, erase, clear, step edit or pattern edit. Knob turns within one hold count as one change.

## 13. Effects: punch-in, DUST, DUCK, filter and sends

### Punch-in effects

Hold **FX**, then hold a white key. The effect runs on the whole mix while the key is down. Loops and the gate are locked to the tempo. Keys pressed while FX is held never play or record notes.

| Key | Effect | Key | Effect |
| --- | --- | --- | --- |
| 1 | Loop 1/4 | 9 | Low-pass sweep |
| 2 | Loop 1/8 | 10 | High-pass sweep |
| 3 | Loop 1/16 | 11 | Phone |
| 4 | Loop 1/32 | 12 | Bit crush |
| 5 | Stutter (1/16 triplets) | 13 | Alias (sample-rate drop) |
| 6 | Reverse | 14 | Gate 1/16 |
| 7 | Tape stop | 15 | Echo (dotted 1/8) |
| 8 | Half speed | 16 | Tape wobble |

### Master: DUST, DUCK, FILT

Set with FX + KNOB 1–3, or at GLO → MASTER.

| Control | Range | What it does |
| --- | --- | --- |
| **FILTER** (FX + KNOB 1) | Left: low-pass · centre: OFF · right: high-pass | A DJ filter that glides without zipper noise |
| **DUST** (FX + KNOB 2) | 0–100 % | Old sampler and record: drive, lower sample rate (down to ~11 kHz), fewer bits (down to 8), a closing low-pass (~3 kHz); hiss and crackle only while playing |
| **DUCK** (FX + KNOB 3) | 0–100 % | Every kick pumps the synths down and back over an 1/8 note (sidechain) |
| **ROLL** (GLO → MASTER) | — | Note-repeat rate of ARP + key |

### Per-track sends

Each track has drive, a slicer and sends to a **stereo chorus**, a **tempo delay** and a **stereo reverb**. Edit them on the track's sound pages or in the web editor.

## 14. Mixer, tempo, projects and saving

### Mixer: mute, solo, levels

Hold **GLO**: white keys **1–4** mute tracks 1–4 (a muted track fades out and plays no new notes, but its pattern keeps running in time); keys **5–8** solo them (several solos add up). KNOB 1–4 set the levels of tracks 1–4. On the TRACKS screen, KNOB 2 on a muted track unmutes it.

### Tempo

Use **SELECT** for tempo at any time. Alternatively hold GLO and press the last white key (**G5**) twice or more for **tap tempo**. A free take also sets the tempo from your playing.

### Clear a track

Hold **REC**. After 0.7 s the normal press is cancelled and a ring fills; keep holding about 1.3 s more (about 2 s in total) and the selected track is cleared (*TRACK 2 CLEARED*). Let go before the ring completes and nothing happens. Undo brings the track back.

### Projects and saving

- **SAVE → PROJECT** has SLOT (1–4), LOAD and SAVE.
- The four sections of song mode are the four project slots: hold SAVE and press keys 5–8 to save the loop into A–D (= slot 1–4).
- **New project:** SAVE → TOOLS → NEW (turn to GO). Tracks return to their power-on sounds with empty patterns (undoable). NEW PROJECT and saving a user preset wait until the song stops.
- A failed save is retried (*SAVE ERROR: RETRYING*), and everything is saved before an update.

### Autosave

When the transport is stopped and you have not touched the panel for 2.5 s (at most once every 20 s), the working project is written to flash. At power-on SLOOP returns to where you left it. A flash write stops the audio for about 50 ms, so it never happens while something plays.

### User presets

32 user preset slots hold sounds you design. Your presets appear after the factory ones in PRESETS.

## 15. Song mode

A song is up to 16 steps of four sections, **A–D**. Each section holds the four tracks: sounds, patterns and kit.

1. **Build sections.** Make a loop (the verse). Hold **SAVE** and press key **5** (*save A*). Change the loop and save it into **B** with key 6, into **C** with key 7 and **D** with key 8. Saving over a used section asks you to press the key again within 3 s.
2. **Play sections live.** Hold SAVE and press key **1–4**. While playing, the section starts on the next bar with every track from its first step. When stopped, it becomes the loop at once.
3. **Record the song.** Hold SAVE and press key **14**. From the next bar, every section you play and how many bars it plays are written into the song. Press it again, or stop playback, to end (*SONG PARTS n*). The song is saved by itself.
4. **Play it back.** Hold SAVE and press key **13** to switch *loop* / *song*. In song mode **PLAY** plays the whole song and stops at the end; your loop is back afterwards.

**SONG screen** (tap SAVE on TRACKS, or hold SAVE and press key 16): KNOB 1 step · KNOB 2 section · KNOB 3 bars · KNOB 4 number of steps. **REC** stores the loop into the step's section, **SAVE** (tap) saves the chain, **OCT−** switches loop / song, **OCT+** twice loads a section.

## 16. The web editor

Open the [web editor](https://isod89.github.io/sloop-fm1/webapp/editor/) in **Chrome or Edge** with the FM-1 on USB and press **Connect**. It follows the device live: turn a knob on the FM-1 and the editor moves.

<p align="center"><img src="../assets/screens/editor-drums.png" alt="SLOOP web editor: the drum track" width="760"></p>

| Tab | Use |
| --- | --- |
| **Sound** | Every parameter of the selected track, engines, presets, files |
| **Sequencer** | Pattern settings and steps. On the drum track: a 16-lane grid with the kit. Choose a level (GHOST, SOFT, NORM, HARD) and a roll (×1–×4), then click to place a hit; click again with the same level and roll to clear it; Shift+click raises it one level. |
| **Tracks** | Four channel strips: level, pan, mute; SOLO and REC shown as on the device |
| **Library** | Preset library |
| **Samples** | Upload samples and the CHOP tool |
| **Projects** | Project slots |
| **Settings** | GLOBAL, MASTER (DUST, DUCK, FILT, ROLL), DRUMS |

### Transferring samples

1. Open **Samples** and choose a slot (USR1, USR2 or USR3).
2. **Files:** add up to 16 WAV files per slot (any rate, mono or stereo). Name them with their root note, such as `KEYS_C4.wav`.
3. Send the slot to the FM-1. A slot holds about 7.4 s.
4. On a synth track set engine **SAMPLE** and **SET** to the slot.

### Slicing a sample (CHOP)

1. In **Samples**, open or drop a recording (WAV, MP3, AIFF…).
2. Cut it into up to 16 chops, one per key, using one of:
   - **TAP** (or the space bar) while it plays — *snap to the hit* puts each tap on its attack;
   - **Find hits**;
   - **Grid**;
   - **Equal parts**.
3. Press **Send to USR1/2/3**, or **Download WAVs**.

## 17. MIDI and external operation

SLOOP is a class-compliant USB-MIDI device. The device's MIDI port is named "Felucca".

| MIDI channel | Track | Notes |
| --- | --- | --- |
| 1 | Track 1 (synth) | Note on/off |
| 2 | Track 2 (synth) | Note on/off |
| 3 | Track 3 (synth) | Note on/off |
| 10 (default) | Track 4 (drums) | Note → nearest of the 16 sounds; see the drum table in section 10 |
| Any other | The currently selected track | Note on/off |

- **Drum channel:** the GM drum channel is set on the DRUMS settings page (**CH**, 0–16, default 10; 0 turns it off).
- **MIDI in:** only note on and note off are handled; velocity maps to the drum levels. Other messages are ignored.
- **MIDI out:** keys you play are also sent over USB-MIDI, on the channel of the selected track.
- **Not available:** the SYSTEM page entries MIDI, SYNC and ROUT are placeholders (`--`). No MIDI clock sync is implemented in the source. TRS MIDI IN exists only as an untested build option, off by default.

## 18. Updating SLOOP safely

1. Finish what you are doing and stop playback. SLOOP saves everything before an update.
2. Connect the FM-1 directly to the computer with a data cable.
3. Use the browser installer ([section 4.1](#41-from-the-browser-recommended)) or `tools/fm1_install.py` with the new `.fwsc`.
4. Do not unplug until *Done*. If it is interrupted, press Install again.
5. Check the version on the splash screen or in HOME (hold) → about.

## 19. Troubleshooting

| Symptom | Likely cause | Solution |
| --- | --- | --- |
| The installer says the browser cannot do it | The browser has no Web MIDI | Open the page in Chrome or Edge |
| *FM-1 not found* | Charge-only cable, a hub, or another app using MIDI | Use a data cable directly in the computer, close other MIDI programs, press Install again |
| Install stopped half-way | Cable pulled or computer slept | Replug and press Install again; the FM-1 stays in update mode |
| The installer rejects the package | Package damaged | Download it again |
| FM-1 shows *SLOOP USB RESCUE* | You held OCT− alone at power-on, or the app failed to start | Connect USB and run the installer again |
| *UNKNOWN FLASH* in rescue | The flash chip is not recognised | Do not install; ask in GitHub issues |
| FM-1 will not start at all | Failed write | Use FM-1-transporter |
| Editor cannot connect | Not Chrome/Edge, no MIDI permission, other app holds the port | Use Chrome/Edge, allow MIDI, close other MIDI apps |
| No sound from a track | Muted, soloed elsewhere or level at zero | Hold GLO and check keys 1–8 and the knobs |
| REC does nothing on stopped empty project | It armed instead | Play a note: that starts the free take |
| Keys play nothing while holding a layer | Layer keys do not play notes | Release the layer button (or unlock it) |
| Notes seem late or early | Different tempo or division | Check DIV (SEQ + KNOB 2); recording already compensates ~12 ms |
| Loop is not the length I wanted | Free take closed at the wrong "1" | Clear (hold REC) and record again; take closes by itself after 24 s |
| My loop disappeared | Track cleared | Hold EDIT, press OCT− to undo |
| Pumping on synths | DUCK is up | FX + KNOB 3 down |
| Crackle or lo-fi sound | DUST is up | FX + KNOB 2 down |
| Muffled or thin sound | The DJ filter is not at centre | FX + KNOB 1 to centre (OFF) |
| Sections do not start at once | They start on the next bar while playing | Wait for the bar, or stop first |
| *SAVE ERROR: RETRYING* | A flash write failed | Wait; SLOOP retries until it succeeds |
| No MIDI clock sync | Not implemented | Use the tap tempo or SELECT |
| Drum notes on an external keyboard play the wrong track | Channel mapping | Use channels 1–3 for synths and the drum channel (default 10) for drums |

## 20. Technical specifications

| | |
| --- | --- |
| Version | SLOOP 2.2 |
| Tracks | 3 synth parts (8 voices shared) + drums (16 sounds, 6 voices) |
| Sounds | 68 presets on 9 engines, 8 sampled sets (CC0), 3 slots for your samples |
| Sequencer | 64 steps per track, own length and division each; chords with level and ratchet per note; drums with level and ratchet per sound; ties, slide; MPC swing 50–75 %; one sample-accurate clock |
| Drum kits | 37 (5 sampled, 32 synthesised, 16 sounds each) |
| Effects | 16 punch-in; master DUST, DUCK, DJ filter; per-track drive, slicer, sends to stereo chorus, tempo delay, stereo reverb; master limiter |
| Recording | Live, quantised as heard (latency-compensated), overdub, free take |
| Memory | Undo / redo, 4 projects, 32 user presets, autosave, song of 4 sections × 16 steps × 1–64 bars |
| Audio | 44.1 kHz, fixed-point DSP |
| MIDI | USB class-compliant in / out; channels 1–3 synths, 10 drums (default) |
| Update | Over USB from the browser; package SHA-256 and CRC checked |
| Editor protocol | Version 5 |

## 21. Glossary

| Term | Meaning |
| --- | --- |
| **Layer** | What a function button becomes while held: the keys and knobs change job |
| **Tap / Hold** | Press and release / keep pressed |
| **Lock** | Hold a layer button and tap HOME to keep the layer open |
| **Free take** | A first recording on an empty project; its length sets loop and tempo |
| **Overdub** | Recording on top of what already plays |
| **Quantisation** | Snapping notes to the nearest step |
| **Swing** | Delaying every second step (MPC style, 50–75 %) |
| **Ghost / hard hit** | A quiet / loud drum hit (OCT− / OCT+) |
| **Ratchet** | A hit repeated ×1–×4 inside one step |
| **Note repeat (ARP)** | A held key retriggers on the grid |
| **Punch-in effect** | An effect that lasts only while its key is held |
| **DUST / DUCK / FILT** | Vinyl-sampler degradation / sidechain pump / DJ filter on the master |
| **Section (A–D)** | A saved set of four tracks used in song mode |
| **Engine** | A synthesis method (ANALOG, DIGITAL, PHASE…) |
| **USR1–USR3** | Slots for your own samples |
| **FWSC** | The installable SLOOP package (`.fwsc`) |
| **SysEx** | The messages the editor uses over USB-MIDI |

## 22. For contributors

- [BUILDING.md](../BUILDING.md) — toolchain, build, tests, installing your own build.
- [web/EDITOR_PROTOCOL.md](../web/EDITOR_PROTOCOL.md) — the editor's SysEx protocol.
- [LICENSING.md](../LICENSING.md) — licensing of code and assets.

Other documents: [README.md](../README.md), [SLOOP.md](../SLOOP.md), and the French quick-start [DEMARRAGE-RAPIDE-FR.md](../DEMARRAGE-RAPIDE-FR.md).

## 23. Credits, licence and trademarks

- SLOOP is a fork of **Felucca** by Leo Kuroshita (@kurogedelic), Hügelton Instruments: the engines, sequencer, editor and installer come from there.
- Font: Terminus (SIL OFL 1.1). Samples: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi (all CC0). PHASE engine after CrispyZebra (GPL); VOICE after klattsch (MIT). Icons: Fukiai.
- Interface ideas after teenage engineering's pocket operators and EP-133, Elektron's step entry and Akai's MPC (swing, note repeat, erase). SLOOP is not affiliated with any of them.
- **Licence:** code is GPL-3.0-only (see [LICENSE](../LICENSE) and [LICENSING.md](../LICENSING.md) for the assets). **No warranty.**
- M-VAVE and FM-1 are trademarks of their owners; SLOOP is not affiliated with M-VAVE. Drum kit names describe styles, not products.
