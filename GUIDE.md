<p align="center"><img src="assets/logo/sloop-logo.png" alt="SLOOP" width="420"></p>

# SLOOP 2.4.1 — The Complete Guide

Everything SLOOP does, every button, every combination, every page, in one place. This guide is written to be read from the top the first time, then used as a reference: the [cheat sheet](#26-cheat-sheet) at the end has every combination on one page.

SLOOP is a free, open-source (GPL-3.0) firmware for the M-VAVE FM-1, based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments. The short manual is [SLOOP.md](SLOOP.md); the project page is [README.md](README.md).

---

## How to read this guide

| Written | Means |
| --- | --- |
| **tap FX** | press FX and let go without touching anything else (shorter than about half a second) |
| **hold FX** | keep FX pressed |
| **FX + key 3** | hold FX, and while it is held press white key 3 |
| **FX + KNOB 2** | hold FX, and while it is held turn KNOB 2 |
| **SEQ + step + KNOB 4** | hold SEQ, hold a step key, turn KNOB 4 |
| **key 1 … key 16** | the 16 white keys, from the lowest (**F3** = key 1) to the highest (**G5** = key 16) |
| **GO** | a value that does something (LOAD, SAVE, NEW…): turn it one click, *AGAIN: …* shows, turn it one more click within about 1.5 s to do it |

The 16 white keys, numbered:

| Key | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Note | F3 | G3 | A3 | B3 | C4 | D4 | E4 | F4 | G4 | A4 | B4 | C5 | D5 | E5 | F5 | G5 |

The 11 black keys are F#3, G#3, A#3, C#4, D#4, F#4, G#4, A#4, C#5, D#5, F#5. The "first four black keys" are F#3, G#3, A#3 and C#4.

---

## Contents

1. [Install and update](#1-install-and-update)
2. [The panel](#2-the-panel)
3. [The big ideas: tracks, colours, tap and hold](#3-the-big-ideas-tracks-colours-tap-and-hold)
4. [Your first beat in 60 seconds](#4-your-first-beat-in-60-seconds)
5. [Every button, alone](#5-every-button-alone)
6. [Playing the keys](#6-playing-the-keys)
7. [The drum track](#7-the-drum-track)
8. [Recording](#8-recording)
9. [The layers, one by one](#9-the-layers-one-by-one)
10. [The step sequencer: steps, nudge, locks, fills](#10-the-step-sequencer-steps-nudge-locks-fills)
11. [Key, scales, chords and CHORD+](#11-key-scales-chords-and-chord)
12. [Mix: levels, mute, solo, fills, tap tempo](#12-mix-levels-mute-solo-fills-tap-tempo)
13. [Undo, clear, save, projects, autosave](#13-undo-clear-save-projects-autosave)
14. [Song mode: sections, chains, songs](#14-song-mode-sections-chains-songs)
15. [The screens](#15-the-screens)
16. [Every page, knob by knob](#16-every-page-knob-by-knob)
17. [The sounds and the ten engines](#17-the-sounds-and-the-ten-engines)
18. [Effects: master, punch-in, per track](#18-effects-master-punch-in-per-track)
19. [The visualiser](#19-the-visualiser)
20. [The HOME menu and the lights](#20-the-home-menu-and-the-lights)
21. [MIDI](#21-midi)
22. [USB audio](#22-usb-audio)
23. [Your own samples and drum kits](#23-your-own-samples-and-drum-kits)
24. [The web editor](#24-the-web-editor)
25. [Backup, rescue, going back, troubleshooting](#25-backup-rescue-going-back-troubleshooting)
26. [Cheat sheet](#26-cheat-sheet)

---

## 1. Install and update

**From the browser (recommended).** Open the [installer page](https://isod89.github.io/sloop-fm1/) in **Chrome or Edge**, connect the FM-1 by USB (a data cable, plugged straight in, no hub), press **INSTALL**, allow MIDI access, and wait for *Done*. The FM-1 restarts on the SLOOP logo.

**From the source folder (Windows).** Double-click **`INSTALL-SLOOP.bat`**: it builds the firmware and opens the installer at `http://localhost:8766/webapp/installer/`. Keep the black window open until the install is done. **`OPEN-EDITOR.bat`** opens the web editor.

**Updating** keeps your projects, presets, samples, kits and settings. The installer checks the package (SHA-256), SLOOP's update loader checks it again (CRC) before it starts the new firmware, and a damaged package is refused before anything is written.

**Before you go back to an older SLOOP** (2.3 or earlier), save a backup in the editor: 2.3 cannot read 2.4 projects (format FUN5).

---

## 2. The panel

| Control | Kind | Main job |
| --- | --- | --- |
| **PLAY** | button | start / stop all four tracks |
| **REC** | button | record (tap), clear the track (hold) |
| **SAVE** | button | song layer (hold), song screen / save pages (tap) |
| **HOME** | button | TRACKS screen, visualiser (tap), menu (hold), lock a layer |
| **ENV** | button | envelope pages |
| **LFO** | button | LFO pages |
| **FX** | button | punch-in layer (hold), effect pages (tap) |
| **SCL** | button | key / chord layer (hold), scale pages (tap) |
| **EDIT** | button | erase layer (hold), engine pages (tap) |
| **ARP** | button | note-repeat layer (hold), arpeggiator pages (tap) |
| **SEQ** | button | step layer (hold), sequencer pages (tap) |
| **GLO** | button | mix layer (hold), global pages (tap) |
| **OCT− / OCT+** | buttons | octave, ghost / hard hits, undo / redo, pages |
| **ALGORITHM** | knob | selects the track (1–4) |
| **PRESETS** | knob | the selected track's sound, or the drum kit |
| **SELECT** | knob | tempo, or the previous / next page |
| **KNOB 1–4** | knobs | what the four dials at the bottom of the screen show |
| **MASTER** | knob | output volume (headphones / line out) |
| **27 keys** | keys | 16 white (F3–G5) and 11 black |

---

## 3. The big ideas: tracks, colours, tap and hold

### Four tracks

| Track | Colour | Plays | Knob of the same colour |
| --- | --- | --- | --- |
| **1** | blue | a synth (3 synth tracks share 8 voices) | KNOB 1 |
| **2** | green | a synth | KNOB 2 |
| **3** | yellow | a synth | KNOB 3 |
| **4** | orange | the drum machine: 16 sounds, one per white key | KNOB 4 |

**ALGORITHM** picks the track you play, record and edit, on every screen. The screen and the whole editor take the colour of the selected track. **White** on the screen always means *what you are touching*; **red** always means *recording*.

### Tap and hold

Every function button has two lives:

- **Tap** it: its **pages** open (sound design, settings), as on any FM-1 firmware. Tap it again: the next page of that button. **SELECT** also walks the pages of that button, both ways. Each button remembers the last page you used.
- **Hold** it: a **layer**. While it is held, the 16 white keys and KNOB 1–4 do something else, and after 0.14 s the screen shows the 16 keys as 16 tiles (four rows of four: keys 1–4, 5–8, 9–12, 13–16) and the knobs as dials. Let go: back to playing.

The layers are **FX** (punch-in effects), **EDIT** (erase), **ARP** (note repeat), **SEQ** (steps), **SCL** (key and chords), **GLO** (mix) and **SAVE** (song). ENV and LFO have no layer: they only open pages.

**Landmarks:** while a layer is held (and always on the drum track) keys 1, 5, 9 and 13 glow dimly — the first key of each row of tiles — so you can find a tile without looking. Keys at full light are what is on.

**Lock a layer:** hold its button and **tap HOME**. The layer stays open with the button let go (*LOCK* on the screen, the button blinks): both hands free, one on the keys, one on the knobs. Any other button lets it go (HOME, the layer's own button, ENV…) and does only that; **PLAY, REC and OCT− / OCT+ keep working** inside a locked layer.

**Inside any layer:** PLAY still starts and stops, **SELECT** is always the tempo, and the ALGORITHM / PRESETS knobs wait (no jump when you let go).

---

## 4. Your first beat in 60 seconds

1. Turn **ALGORITHM** to track **4** (orange). The white keys play 16 drum sounds: **F3 kick**, G3 kick 2, A3 snare, B3 clap, **C4 hat**, D4 open hat… Turn **PRESETS** to pick a kit: try *808* or *BOOMBAP*.
2. Press **REC** (*rec ready*). **Play a beat freely**, at your own tempo — no click, no count-in. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard hits.
3. **Press REC on the "1" after your last bar.** The loop closes: its length sets the tempo, the hits snap to the grid, and it plays at once.
4. Press **REC** again while it plays: you record on top (overdub). Hold **ARP** and hold the hat key (C4): a 1/16 hat roll, recorded as ratchets.
5. Turn **ALGORITHM** to track **1** (blue, *808 BOOM*), press **REC**, play a bass line.
6. Hold **SCL** and press the key of your song (for example D). Select track 2, hold **SCL** and turn **KNOB 1** to *7TH*: every white key is now a chord of the key, one finger each.
7. Hold **FX** and hold a white key for a punch-in effect; still holding FX, turn **KNOB 2** for DUST, **KNOB 3** for DUCK.
8. A mistake? **EDIT + OCT−**: undo.

---

## 5. Every button, alone

| Control | Tap / turn | Hold |
| --- | --- | --- |
| **PLAY** | start / stop all tracks (in song mode: play the song). Works inside every layer | — |
| **REC** | playing: record now / stop recording · stopped: arm (*rec ready*) · free take: close the loop · count-in: cancel | ~2 s: **clear the selected track** (a ring fills; let go before it is full and nothing happens) |
| **SAVE** | on TRACKS or DRUMS: the **SONG** screen · on the SONG screen: save the song · elsewhere: the **SAVE pages** | the **song layer** (sections A–D, chain, song mode, SONG REC) |
| **HOME** | the **TRACKS** screen · on TRACKS: the **visualiser** (HOME again closes it) | 0.7 s: the **menu** (SCREEN, LIGHTS, AUDIO, SYSTEM); hold again to leave it |
| **ENV** | ENV pages (ENV, ENV DEST) | — |
| **LFO** | LFO pages (LFO, LFO DEST) | — |
| **FX** | FX pages (FX, FILTER, SLICER, DLY, REV/CHO) | punch-in layer |
| **SCL** | SCL pages (SCL, SCL 2) | key / chord layer |
| **EDIT** | EDIT pages (EDIT 1, EDIT 2, VOICE, VOICE 2) · on TRACKS with the drum track: the **DRUMS** screen · on DRUMS: grid ↔ kit · on the STEP page: clear the step | erase layer |
| **ARP** | ARP pages (ARP, ARP 2) | note-repeat layer |
| **SEQ** | SEQ pages (STEP, PATTERN, SONG) · on TRACKS with the drum track: the **DRUMS** screen · on DRUMS: grid ↔ kit | step layer |
| **GLO** | GLO pages (GLOBAL, MASTER, SYSTEM, DRUMS) | mix layer |
| **OCT− / OCT+** | synth track: octave down / up (−3 … +3) · both together: octave 0 | drum track: **ghost** (OCT−) / **hard** (OCT+) hits while held |
| **ALGORITHM** | select track 1–4, on every screen (inside a layer it waits; not during a free take, on the SONG screen or in the menu) | — |
| **PRESETS** | on TRACKS, HOME and the PRESETS page: the selected track's **sound** (all factory sounds by kind, then your user presets) · drum track: the **kit** · on the DRUMS screen: the kit | with **HOME** held: **by kind** — each click is the first sound of the next (or previous) kind, the kind flashes in the top bar, and that HOME press opens no menu and no visualiser |
| **SELECT** | on TRACKS, inside a layer and on the REC screen: the **tempo** · on a page: previous / next page of its button (stops at the ends) · DRUMS screen: grid ↔ kit · visualiser: the style · menu: the section | — |
| **KNOB 1–4** | the four dials at the bottom of the screen | — |
| **MASTER** | output volume (the visualiser ignores it) | — |

### Combinations at power-on and while stopped

| Combination | Does |
| --- | --- |
| **OCT−** held while switching on | **USB rescue** (*SLOOP USB RESCUE*): install again from the browser |
| **OCT− + OCT+** held while switching on | **hardware calibration**: teach each button and knob (also in the HOME menu → SYSTEM) |
| **OCT− + OCT+** held 5 s, stopped | **update mode** (UBOOT): a countdown shows from 2 s; let go to cancel. Your work is saved first |

---

## 6. Playing the keys

### Synth tracks (1–3)

- The keys play the selected track. The notes you play always go out on USB MIDI (the track's channel).
- **OCT− / OCT+**: octave −3 … +3; both together: back to 0. The OCT lights show when you are off 0.
- What the keys play depends on the track's **KEYS** setting and **CHORD** mode (SCL layer or SCL page):

| KEYS | CHORD | The keys play |
| --- | --- | --- |
| **OFF** | OFF | every key chromatic (the default) |
| **SNAP** | OFF | every key, rounded down to the scale |
| **WHITE** | OFF | the white keys walk the scale from C4 (C4 = the key's root); black keys are silent |
| any | **on** | the white keys play **chords** of the scale, one per key (C4 = the I chord); the black keys **change the chord** ([CHORD+](#chord-the-black-keys-change-the-chord)) |

- The **GM KIT** sound (SAMPLE engine) is the exception: the lowest key is a kick (C2), no scale.

### Drum track (4)

16 sounds, one per white key; a black key plays the sound of the white key on its left (two fingers on one sound, for fast rolls). Hold **OCT−** for ghost notes, **OCT+** for hard hits. See [The drum track](#7-the-drum-track).

### Velocity and levels

The FM-1's keys have no velocity: on the drum track every hit has one of four **levels** — **GHOST**, **SOFT**, **NORM** (as played), **HARD** — chosen with OCT− / OCT+ as you play, or later in the step sequencer. On synth tracks a step can carry an **accent** (STEP page, FLAG).

---

## 7. The drum track

### The 16 sounds

| Key | Sound | Key | Sound | Key | Sound | Key | Sound |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 · F3 | KICK | 5 · C4 | HAT | 9 · G4 | SNARE 2 | 13 · D5 | RIDE |
| 2 · G3 | KICK 2 | 6 · D4 | OPEN HAT | 10 · A4 | LOW TOM | 14 · E5 | SHAKER |
| 3 · A3 | SNARE | 7 · E4 | PEDAL HAT | 11 · B4 | HI TOM | 15 · F5 | CONGA |
| 4 · B3 | CLAP | 8 · F4 | RIM | 12 · C5 | CRASH | 16 · G5 | COWBELL |

- **Levels:** GHOST, SOFT, NORM, HARD (hold OCT− / OCT+ while you hit). Softer hits are darker as well as quieter.
- **Ratchets:** a hit can repeat x1–x4 inside its step (ARP rolls record them; SEQ + step + KNOB 3 sets them).
- **Choke:** the closed hat and the pedal hat cut the open hat.
- **6 drum voices**, separate from the synths' 8.

### Kits

**PRESETS** on the drum track (or KNOB 1 on the DRUMS kit page, or the editor) picks the kit:

| # | Kits |
| --- | --- |
| 1–5 | **ACOUSTIC** (sampled studio kit, CC0) and its treatments: DEEP, TIGHT, BRIGHT, DUST |
| 6–37 | 32 synthesised kits, 16 sounds each: 808, 909, 606, 80S, VINTAGE, TRAP, DRILL, BOOMBAP, LO-FI, PHONK, HOUSE, D.HOUSE, TECHNO, MINIMAL, ELECTRO, DISCO, GARAGE, JUNGLE, DUBSTEP, DEMBOW, AMAPIANO, AFRO, LATIN, TRIBAL, SYNTHWV, CHIP, ARCADE, GLITCH, INDUSTR, HYPER, AMBIENT, JAZZ |
| 38–41 | **USR1, USR2, USR3, USR4**: your own kit in one sample slot (about 7.4 s) |
| 42 | **USR3+4**: your own big kit over two slots (about 15 s) |

Every kit is level-matched. The kit is saved with projects and song sections. Changing the kit never touches the pattern. [Your own kits](#23-your-own-samples-and-drum-kits) work like any other: steps, levels, ratchets, fills, choke, MIDI.

### The DRUMS screen

Open it with **EDIT** or **SEQ** tapped on the TRACKS screen while the drum track is selected. It has two pages; **EDIT / SEQ tapped** or **SELECT** switch between them.

**Grid page** — the 16 sounds × 16 steps, levels as shades, ratchets as notches.

| Control | Does |
| --- | --- |
| KNOB 1 **sound** | picks the sound (lane) — you hear it |
| KNOB 2 **step** | moves to a step — you hear what it holds |
| KNOB 3 **hit** | right: a hit of that sound on that step (NORM) · left: clear it |
| KNOB 4 **level** | the hit's level up / down (GHOST, SOFT, NORM, HARD) |
| **white keys** | the 16 steps of the sound KNOB 1 picks: press to set the step (you hear it), again to clear it; the keys light that sound's steps |
| **first four black keys** | the page of steps: 1–16, 17–32, 33–48, 49–64 |
| **PRESETS** | the kit |
| **SAVE** | the SONG screen |
| **HOME** | back to TRACKS |

**Kit page** — 16 pads that flash on every hit; the keys play the pads.

| KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- |
| **kit** | **level** of the drum track | **reverb** send of the drums | **pan** of the drum track |

While the song plays in song mode, the grid's hit and level knobs wait (*STOP THE SONG FIRST*).

---

## 8. Recording

SLOOP records live and layers every pass on top of the last (overdub). Notes go to the nearest step **as you heard it**: the ~12 ms between a key and its sound are taken back, so what you play on the beat lands on the beat. Chords are kept on the synth tracks (up to 4 notes a step); held notes become ties.

### What REC does

| When | REC does | Then |
| --- | --- | --- |
| **Playing** | records the selected track **at once** | REC again stops recording; the loop plays on |
| **Stopped, project with notes** | arms (*rec ready*, the REC light blinks) | **your first note starts the loop and is step 1**; PLAY starts it too |
| **Stopped, empty project** | arms (*rec ready · play freely*) | a **free take**, or, with MODE *tempo*, as with notes |

REC again while armed disarms it (*REC OFF*). In **song mode**, REC waits until the song is stopped (*STOP THE SONG FIRST*), and arming REC switches back to loop mode.

While recording, the REC light is solid and the track shows a red *rec*. **Turn ALGORITHM** while recording and the take moves to the next track without stopping.

### The REC screen (armed, before the first note)

| Knob | Dial | Choices |
| --- | --- | --- |
| KNOB 1 | **mode** (empty project only) | **free**: a free take, the tempo follows you · **tempo**: record at the tempo set (SELECT) |
| KNOB 2 | **length** | the selected track's loop: **1, 2 or 4 bars** |
| KNOB 3 | **start** | **note**: your first note starts the loop · **count**: press **PLAY**, one bar of clicks (4, 3, 2, 1 on the screen), then recording starts; notes before only sound |

LENGTH and START apply when you record at a tempo (MODE *tempo*, or a project that already has notes); a free take makes its own length. MODE and START are settings of the FM-1 (they stay as you left them). During the count-in, **REC cancels it** and **PLAY** goes back to *rec ready*. SELECT sets the tempo on this screen.

### Free take: the loop follows you

1. On an empty project, **REC**, then play freely. The screen shows *free take*, the seconds, and the loop it would make right now (*2 bars · 92 bpm*).
2. **Press REC on the "1" after your last bar.** SLOOP picks 1, 2 or 4 bars at the tempo nearest the one set (within 3 % the set tempo is kept), writes your notes with their lengths and levels, and plays the loop at once. All four tracks take that length.
3. **PLAY** during a free take drops it. A take closes by itself after 24 s.

### Metronome and swing

- The **PLAY light flashes on every beat**: a silent metronome.
- An audible click: **GLO → GLOBAL → CLICK** = **OFF**, **REC** (only while a track records) or **ON** (whenever it plays). Its level follows GLO → DRUMS → LVL. The click is never recorded, and the count-in always clicks.
- **Swing** is MPC swing, 50 % (straight) to 75 %: **GLO → GLOBAL → SWING** for all tracks, **SEQ + KNOB 3** (or SEQ → PATTERN → SWG) per track; a track's swing adds to the global one. Triplet divisions are never swung.

### What gets recorded

- **Synth tracks:** the notes (and chords) with their lengths; ties for held notes; ARP rolls as ratchets; CHORD+ chords as they sound.
- **Drum track:** the hits with their levels; ARP rolls as **ratchets** (a 1/32 roll on a 1/16 track: x2 on each step).
- Never recorded: punch-in effects, the click, notes played in a layer that uses the keys for something else (FX, SEQ, GLO…).

---

## 9. The layers, one by one

All seven layers at a glance:

| Hold | Name | White keys | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | Tap |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **FX** | punch | a punch-in effect while the key is held | FILTER (master) | DUST | DUCK | TRK FILT (selected track) | FX pages |
| **EDIT** | erase | erase that sound / note from the pattern | SHIFT | LENGTH ×2 / ½ | TRANSPOSE | — | EDIT pages |
| **ARP** | roll | note repeat on the grid | RATE | — | — | — | ARP pages |
| **SEQ** | steps | steps 1–16 of the page | SOUND / NOTE | DIV | SWING | LENGTH | SEQ pages |
| **SCL** | key | the key of the song | CHORD | SCALE | KEYS | TRANSPOSE | SCL pages |
| **GLO** | mix | 1–4 mute · 5–8 solo · 9 fill · 10 fill bar · 16 tap tempo | level 1 | level 2 | level 3 | level 4 | GLO pages |
| **SAVE** | song | 1–4 play A–D · 5–8 save A–D · 13 loop / song · 14 SONG REC · 16 song screen | — | — | — | — | song screen / SAVE pages |

### FX — punch

| Control | Does |
| --- | --- |
| **FX + key 1–16** | the [punch-in effect](#punch-in-effects) of that key, on the whole mix, for as long as the key is held |
| **FX + KNOB 1** | master **FILTER**: left a low-pass, right a high-pass, centre off |
| **FX + KNOB 2** | master **DUST** (old sampler and vinyl) |
| **FX + KNOB 3** | master **DUCK** (sidechain pump from the kick) |
| **FX + KNOB 4** | **TRK FILT**: the same one-knob filter on the **selected track only** (any track, the drums too) |
| **FX + HOME** | lock the FX layer open (both hands free) |

Keys pressed while FX is held never play or record notes.

### EDIT — erase

| Control | Does |
| --- | --- |
| **EDIT + key**, playing | erases that sound (drums) or note (synths; with CHORD on, its chord) from **every step the playhead passes while you hold the key** — MPC style: hold the hat key for one bar and that bar's hats are gone. *ERASED* flashes |
| **EDIT + key**, stopped | erases that sound / note from the **whole pattern** at once |
| **EDIT + KNOB 1** | **SHIFT**: every step one later (right) / earlier (left) — turns the groove around |
| **EDIT + KNOB 2** | **LENGTH**: right ×2 (the pattern copied after itself, up to 64 steps), left ½ |
| **EDIT + KNOB 3** | **TRANSPOSE**: every note a semitone up / down (synth tracks) |
| **EDIT + OCT−** | **undo** |
| **EDIT + OCT+** | **redo** |

All the knob turns of one EDIT hold count as one change for undo.

### ARP — roll (note repeat)

| Control | Does |
| --- | --- |
| **ARP + key (held)** | the key repeats on the grid at the RATE, locked to the tempo and the swing; it ends with the key |
| **ARP + KNOB 1** | **RATE**: 1/8, 1/16, 1/32, 32T, 1/64 (also GLO → MASTER → ROLL) |
| **ARP + key + OCT− / OCT+** | on the drum track: ghost / hard rolls |

While recording, a roll is written as ratchets (a 1/32 roll on a 1/16 track: x2 on each step). Rolls ignore fills. (The **arpeggiator** is something else: tap ARP for its pages.)

### SEQ — steps

The 16 white keys are the 16 steps of the current page; the lit ones play.

| Control | Does |
| --- | --- |
| **SEQ + empty step** | sets it at once — drums: with the sound shown (KNOB 1, or the last pad you hit); synths: with the note or chord you played last |
| **SEQ + set step (tap)** | clears it (with its nudge, locks and fill condition) |
| **SEQ + several step keys held** | edit them together |
| **SEQ + first four black keys** or **SEQ + OCT− / OCT+** (no step held) | the page: steps 1–16, 17–32, 33–48, 49–64 (up to the track's length) |
| **SEQ + KNOB 1** (no step held) | the **sound** (drums, you hear it) / **note** to set |
| **SEQ + KNOB 2** | **DIV**: 1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1BAR, 2BAR |
| **SEQ + KNOB 3** | **SWING** of the track |
| **SEQ + KNOB 4** | **LENGTH**: 1–64 steps |
| **SEQ + step + KNOB 1** | drums: which **sound** of the step LEVEL and RATCHET edit (you hear it) · synths: the step's **note** (every note of a chord moves together) |
| **SEQ + step + KNOB 2** | **LEVEL**: ghost, soft, norm, hard |
| **SEQ + step + KNOB 3** | **RATCHET**: x1–x4 |
| **SEQ + step + KNOB 4** | **NUDGE**: −32 … +31 (in 1/64 of a step) |
| **SEQ + step + PRESETS** | a **parameter lock** on that step: the first click makes it one click away from the track's value, the next ones move it |
| **SEQ + step + ALGORITHM** | which parameter the lock is on |
| **SEQ + step + OCT+** | the step's **fill condition**: normal → FILL ONLY → NO FILL → normal |
| **SEQ + step + OCT−** | clears the step's nudge, locks and fill condition |

A held step that you edited with a knob is **kept** when you let go (only a plain tap clears). Details in [The step sequencer](#10-the-step-sequencer-steps-nudge-locks-fills).

### SCL — key and chords

| Control | Does |
| --- | --- |
| **SCL + any key** | sets the **key of the song**: the root of all three synth tracks (*KEY D*) |
| **SCL + KNOB 1** | **CHORD** of the selected synth track: OFF, TRIAD, 7TH, 9TH, SUS4, POWER |
| **SCL + KNOB 2** | **SCALE** of all three synth tracks: 16 scales |
| **SCL + KNOB 3** | **KEYS**: OFF, SNAP, WHITE |
| **SCL + KNOB 4** | **TRANSPOSE** the selected track, ±24 semitones |

Details in [Key, scales, chords and CHORD+](#11-key-scales-chords-and-chord).

### GLO — mix

| Control | Does |
| --- | --- |
| **GLO + key 1–4** | **mute** / unmute track 1–4 (fades out in a few ms; the pattern runs on in time) |
| **GLO + key 5–8** | **solo** track 1–4 (several solos add up) |
| **GLO + key 9 (held)** | **fill** for as long as the key is held |
| **GLO + key 10** | **fill bar**: the whole next bar plays as a fill (press again before the bar to cancel) |
| **GLO + key 16** | **tap tempo** (two taps or more) |
| **GLO + KNOB 1–4** | the **levels** of tracks 1–4 |
| **GLO + SELECT** | the tempo (also inside the visualiser) |

The tiles show what is heard; the *fill* and *bar* tiles light while they act.

### SAVE — song

| Control | Does |
| --- | --- |
| **SAVE + key 1–4** | play section **A–D**: playing, from the next bar (every track from its first step); stopped, it becomes the loop at once |
| **SAVE + key 1–4, several tapped while SAVE stays held** | a **quick chain** (*A B B C*…, up to 8, repeats allowed): let go and they play in turn, round and round |
| **SAVE + key 5–8** | **save the loop** into section A–D (over a used section: press the key again within 3 s) |
| **SAVE + key 13** | switch **loop** / **song** mode |
| **SAVE + key 14** | **SONG REC**: from the next bar, every section you play (and how long) is written into the song; again (or STOP) to end |
| **SAVE + key 16** | the **SONG** screen |

Details in [Song mode](#14-song-mode-sections-chains-songs).

---

## 10. The step sequencer: steps, nudge, locks, fills

Each track has up to **64 steps** with its own **length** (1–64) and **division** (1/32 to two bars). Tracks of different lengths loop on their own and stay in phase (polymeters). A synth step holds up to 4 notes (a chord) with a level and ratchet, a tie or a rest, an accent and a slide; a drum step holds any of the 16 sounds, each with its own level and ratchet.

There are three ways to enter steps:

1. **Live** — REC and play (see [Recording](#8-recording)).
2. **The SEQ layer** — hold SEQ, the white keys are steps (all tracks; see [SEQ — steps](#seq--steps)).
3. **The STEP page** (synth tracks) — tap **SEQ**: the step under the cursor is written from the keys, acid style:

| STEP page | Does |
| --- | --- |
| **keys** | the keys pressed together become the cursor's step (POLY: up to 4 notes; MONO, LEG and UNI: the last one); let go of all keys and the cursor moves on |
| KNOB 1 **STEP** | moves the cursor |
| KNOB 2 **NOTE** | transposes the step (an empty step gets your last note) |
| KNOB 3 **TIME** | **NOTE**, **TIE** (holds the previous note on) or **REST** |
| KNOB 4 **FLAG** | **–**, **ACC** (accent), **SLD** (slide to the next note), **A+S** (both) |
| **EDIT (tap)** | clears the cursor's step and moves on |

Not while recording or armed (the keys record live then). On the drum track, the STEP page shows *DRUM TRACK*: use the DRUMS grid or the SEQ layer.

The **PATTERN** page (tap SEQ again) holds the track's **LEN**, **DIV**, **SWG** (swing) and **GATE** (how long each note lasts).

### Nudge (micro timing)

**SEQ + step + KNOB 4** moves a step off the grid, from −32 to +31 in 1/64 of a step: minus = early (the step plays before its grid time, inside the previous step), plus = late. Its ratchets move with it; recording and swing stay on the grid. On the drum track the whole step moves, every sound in it. A nudged step shows a dot in the corner of its tile.

### Parameter locks

A **lock** gives one sound parameter another value **for that step only** (Elektron style).

1. Turn the parameter you want on its page (for example FX → DLY send, or LFO → RATE). That becomes the **lock parameter** (before you turn anything it is ENV DEST → FLT).
2. Hold **SEQ** and hold the step.
3. Turn **PRESETS**: the first click makes the lock, one click away from the track's value; the next ones move it. The title line shows it (*lock dst 14*; *lock flt --* = no lock yet).
4. **ALGORITHM** (step still held) steps through the other parameters, wrapping round.

At the next step without a lock on that parameter, it comes back to its base value (notes still ringing follow). A knob turned on the page while a lock is in force wins: that value becomes the new base. Several locks can sit on one step (one per parameter), **24 per track**. The sound parameters of the track can be locked (and the PATTERN page's GATE); the arp, key / scale / chord, STRUM / VLEAD, voice-mode, MUTE, LEN / DIV / SWG and the global pages cannot (*NOT LOCKABLE*; *NO LOCK LEFT* when the 24 are used). A locked step shows the same dot as a nudged one; **SEQ + step + OCT−** clears both. Undo (EDIT + OCT−) is for steps: it does not bring nudges and locks back.

### Fill conditions

**SEQ + step + OCT+** cycles the step's condition:

| Condition | Mark on the tile | Plays |
| --- | --- | --- |
| **normal** | — | always |
| **FILL ONLY** | small **F** (top left) | only during a fill |
| **NO FILL** | **×** (top left) | always, except during a fill |

Call a fill while you play: **GLO + key 9** (as long as you hold it) or **GLO + key 10** (the whole next bar). Build a beat whose rolls, crashes and pickup notes are FILL ONLY and whose main hat is NO FILL: one finger brings the fill in and out on the bar. A skipped step is silent whole (no note, no MIDI, no ratchet, no lock). On the drum track the condition is the step's, every sound in it. STOP ends any fill. The arp and the rolls ignore fills.

### Divisions

**DIV**: 1/4, 1/8, **1/16** (default), 1/32, 8T, 16T (triplets), **1/2**, **1BAR**, **2BAR**. A 64-step track at 2BAR lasts 128 bars: drones, chord changes, slow songs.

---

## 11. Key, scales, chords and CHORD+

### Key and scale

- **SCL + any key** sets the **key of the song** for the three synth tracks. (Also SCL page → ROOT, per track.)
- **SCALE** (SCL + KNOB 2 sets all three synth tracks; the SCL page sets the selected track alone): CHR (chromatic), MAJ, MIN, DOR (dorian), MIX (mixolydian), PEN (major pentatonic), MPEN (minor pentatonic), HARM (harmonic minor), PHRY (phrygian), LYD (lydian), LOC (locrian), MEL (melodic minor), BLUES, WHOLE (whole tone), DIMHW and DIMWH (diminished, half-whole and whole-half).
- **KEYS**: **OFF** (chromatic), **SNAP** (every key rounded down to the scale), **WHITE** (the white keys walk the scale from C4, the black keys are silent).
- **TRANSPOSE** (per track): ±24 semitones.

Changing a sound (PRESETS, a user preset) never changes the key, the chord mode, the pattern or the mix of its track.

### Chord mode: one finger, one chord

Hold **SCL** and turn **KNOB 1 CHORD** on the selected synth track:

| CHORD | Notes |
| --- | --- |
| **OFF** | single notes |
| **TRIAD** | 1-3-5 |
| **7TH** | 1-3-5-7 |
| **9TH** | 1-3-7-9 (the lo-fi / R&B voicing) |
| **SUS4** | 1-4-5 |
| **POWER** | 1-5-8 (root, fifth, octave) |

With a chord on, **the white keys walk the scale from C4**: C4 plays the chord of the key's I, D4 the II, E4 the III… and the keys below C4 carry on downwards. Every chord is built from the scale, so it always fits the song. With SCALE on CHR, the chords come from the minor scale. One finger plays the whole chord, and it is recorded as a chord.

### CHORD+: the black keys change the chord

With a chord mode on, **hold a black key while you play a white one** — or press it while the chord is held, and the chord changes under your finger:

| Black key | Changes the chord |
| --- | --- |
| **F#** | major ↔ minor |
| **G#** | adds the 7th |
| **A#** | sus4 |
| **C#** | adds the 9th |
| **D#** | an inversion |

Both octaves of black keys work, and you can hold several. The 7th and 9th come from the scale: in C major, G4 plays **G** (the V chord), **G#** makes it **G7**, **F# + G#** make it **Gm7**; on C4, F# + G# give C-E♭-G-B. What you play is recorded as it sounds.

### SCL 2: STRUM and VLEAD

Tap **SCL** twice (or SCL, then SELECT) for **SCL 2**:

| Knob | Does |
| --- | --- |
| KNOB 1 **TRN** | transpose the track, ±24 semitones |
| KNOB 2 **STRUM** | plays a chord's notes one after the other like a strummed guitar, 1–60 ms a note: right = low to high, left = high to low, centre = off. On the keys and on the chord steps the sequencer plays |
| KNOB 3 **VLEAD** | **ON**: voice leading — each chord is voiced nearest the last one, so a progression moves smoothly instead of jumping |

---

## 12. Mix: levels, mute, solo, fills, tap tempo

| Where | What |
| --- | --- |
| **GLO + KNOB 1–4** | levels of tracks 1–4 |
| **TRACKS screen, KNOB 2** | the selected track's level (on a muted track: unmutes it) |
| **GLO + keys 1–4 / 5–8** | mute / solo |
| **VOICE 2 page** | PAN and MUTE of a synth track |
| **TRACKS screen, KNOB 4** | the selected track's pan |
| **GLO → DRUMS** | the drum track's LVL and reverb, and its MIDI channel |
| **MASTER knob** | the output volume |

A muted track fades out in a few milliseconds and plays no new notes; its pattern runs on in time. Several solos add up.

**Tempo:** SELECT on TRACKS (or inside any layer), **GLO + key 16** tap tempo, or GLO → GLOBAL → BPM (40–240). With MIDI SYNC on, the tempo follows the master.

---

## 13. Undo, clear, save, projects, autosave

| Action | How |
| --- | --- |
| **Undo / redo** | **EDIT + OCT−** / **EDIT + OCT+**. One level: the last recording pass, erase, clear, step or pattern edit |
| **Clear the selected track** | **hold REC** ~2 s (after 0.7 s a ring fills; let go before it is full: nothing). Undo brings it back |
| **Clear the pattern** | SAVE → TOOLS → **CLRSQ** (GO) |
| **Reset the sound** | SAVE → TOOLS → **INIT** (GO): the engine's defaults and first preset |
| **New project** | SAVE → TOOLS → **NEW** (GO): the four tracks back to their power-on sounds, empty patterns, 90 BPM (stop the song first in song mode; undo brings back only the last track's pattern, so save a section first if in doubt) |
| **Save the loop as a section / project** | **SAVE + key 5–8** (A–D = project slots 1–4) |
| **Save / load a project** | SAVE → PROJECT: **SLOT** 1–4, **LOAD** (GO), **SAVE** (GO) |
| **Save a sound** | SAVE → USER: **SLOT** (1–32), **SAVE** (GO) |
| **Autosave** | automatic: when the transport is stopped and you have not touched anything for 2.5 s (at most every 20 s), the working project is kept in flash; at power-on SLOOP comes back exactly as you left it |

A **project** keeps the four tracks (sounds, patterns, kit, locks, nudges, fill conditions, the track filter) and the global settings of the song (tempo, swing, key…). **Settings of the FM-1** — SYNC, MIDI OUT, IN, the lights, the menu settings, the REC screen's MODE and START — are not part of a project: loading a project or NEW PROJECT never changes them.

### The SAVE pages (SAVE tapped, not on TRACKS)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **PRESETS** | the sound (factory by kind, then user presets) | the engine (next / previous) | **KIND**: the next / previous kind (basses, keys, organs, pads, leads, plucks, stabs, FX, your presets) | — |
| **USER** | **SLOT** 1–32 | **LOAD** (GO) | **ERASE** (GO) | **SAVE** (GO) |
| **PROJECT** | **SLOT** 1–4 | — | **LOAD** (GO) | **SAVE** (GO) |
| **TOOLS** | **CLRSQ** (GO) | **INIT** (GO) | — | **NEW** (GO) |

GO: one click shows *AGAIN: SAVE* (or LOAD…), a second click within about 1.5 s does it. Presets and projects are written to flash only while the song is stopped.

---

## 14. Song mode: sections, chains, songs

A song is up to **16 steps** of **4 sections, A–D**. Each section holds the four tracks (sounds, patterns, kit) and is one of the four project slots.

### Make it live, by playing

1. Make a loop (the verse). **SAVE + key 5**: *save A*. Change the loop (the chorus) and save it into **B** (key 6), a bridge into **C** (key 7), an ending into **D** (key 8). Saving over a used section asks for the key again within 3 s.
2. **Play the sections live: SAVE + key 1–4.** Playing: the section starts on the next bar, every track from its first step, always in time. Stopped: it becomes the loop at once.
3. **Quick chain:** keep SAVE held and tap several section keys — *A B B C*, any order, repeats allowed, up to 8 — then let go. The first plays on the next bar, then each next one after the previous has played its **pattern length** (the longest track's), round and round. The title line reads *chain A B B C*; the section playing is lit, the next one framed. A single section tap, STOP, or PLAY in song mode ends the chain.
4. **Record the song as you play it: SAVE + key 14.** From the next bar every section you play, and for how many bars, is written into the song. SAVE + key 14 again, or STOP, ends it (*SONG PARTS 5*). It is saved by itself once you stop. SONG REC records what a chain plays too.
5. **Play it back:** **SAVE + key 13** switches *loop* / *song*. In song mode **PLAY** plays the whole song and stops at the end (your loop is back afterwards).

### The SONG screen

Open it with **SAVE tapped on TRACKS** (or on DRUMS), or **SAVE + key 16**. It shows the chain and edits it by hand:

| Control | Does |
| --- | --- |
| KNOB 1 | the song step (cursor) |
| KNOB 2 | that step's section (A–D) |
| KNOB 3 | its length in bars (1–64) |
| KNOB 4 | the number of steps in the song (1–16) |
| **REC** | stores the current loop into the step's section (a used section: REC again within 3 s) |
| **SAVE** (tap) | saves the song |
| **OCT−** | loop / song mode |
| **OCT+** twice (within 3 s) | loads the step's section (unsaved work is replaced) |
| **PLAY** | play / stop |
| **SEQ** | the SEQ pages |
| **HOME** | back to TRACKS |

The song's settings can only be edited while it is stopped (*STOP FIRST*). An empty section cannot play (*EMPTY SECTION: REC*).

---

## 15. The screens

| Screen | How to get there | What it shows · what the knobs do |
| --- | --- | --- |
| **TRACKS** | tap **HOME** | the performance view: tempo, swing, transport, bar.beat; each track with its sound, steps, playhead, mute / solo / rec badges and level. Dials: KNOB 1 **swing** (global) · KNOB 2 **level** (selected track; on a muted track: unmutes it) · KNOB 3 **steps** (its length) · KNOB 4 **pan** |
| **Visualiser** | tap **HOME** on TRACKS | twelve full-screen visualisers; see [The visualiser](#19-the-visualiser) |
| **Layer** | hold a layer button | 16 tiles (the white keys) and the four dials, in the layer's colour |
| **DRUMS** (grid / kit) | tap **EDIT** or **SEQ** on TRACKS with the drum track | see [The DRUMS screen](#the-drums-screen) |
| **REC READY / FREE TAKE / COUNT-IN** | **REC** while stopped | the tracks, then **mode · length · start** on KNOB 1–3 (4-3-2-1 during a count-in); in a free take: the seconds and the loop it would make |
| **Hold ring** | hold **REC** | the ring of *clear track* filling |
| **SONG** | tap **SAVE** on TRACKS, or **SAVE + key 16** | the section chain; see [The SONG screen](#the-song-screen) |
| **Pages** | tap ENV, LFO, FX, SCL, EDIT, ARP, SEQ, GLO, SAVE | four colour-coded values with a graph (ENV, LFO, ARP, SCL, FX, SLICER…), or the four values in large type placed as the knobs are (1 2 / 3 4) |
| **Menu** | hold **HOME** | the settings of the FM-1 in four sections; see [The HOME menu](#20-the-home-menu-and-the-lights) |

The **title line** of every screen also carries short messages: *KEY D*, *ERASED*, *AGAIN: SAVE*, *NEXT: B*, *chain A B B C*, *lock flt 12*…

---

## 16. Every page, knob by knob

Tap a button to open its first page (or the last one you used); **tap it again** for its next page, or turn **SELECT** to go back and forth. On the drum track, pages that do not apply to it show *DRUM TRACK*.

| Button | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| **ENV** | **ENV** | ATK attack | DEC decay | SUS sustain | REL release |
| | **ENV DEST** | FLT: envelope → brightness (±) | PIT: envelope → pitch (±, the 808 punch) | SHP: envelope → the engine's shape (±) | — |
| **LFO** | **LFO** | RATE (Hz) | WAVE: SIN, TRI, SAW, SQR, S&H | PHS phase | FADE (fade-in time) |
| | **LFO DEST** | PIT: vibrato | FLT: filter wobble | SHP: shape | AMP: tremolo |
| **FX** | **FX** (sends) | DST drive | CHO chorus send | DLY delay send | REV reverb send |
| | **FILTER** | FILT: one-knob filter of the track — left LP, right HP, centre OFF | — | — | — |
| | **SLICER** | SLCR: OFF, GATE, STUT (stutter) | PAT: pattern 1–16 | RATE: 1/8, 1/16, 1/32, 8T, 16T, 32T | DEPTH |
| | **DLY** (global) | TIME: 1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/8D, 1/16D | FDBK feedback | COLR colour (tone of the repeats) | MIX |
| | **REV/CHO** (global) | SIZE reverb size | DAMP reverb damping | CRT chorus rate | CDP chorus depth |
| **SCL** | **SCL** | ROOT (the track's key) | SCL: the track's scale (SCL + KNOB 2 sets all three) | QNT keys: OFF, SNAP, WHITE | CHORD: OFF, TRIAD, 7TH, 9TH, SUS4, POWER |
| | **SCL 2** | TRN transpose ±24 | STRUM ±60 ms | VLEAD: OFF / ON | — |
| **EDIT** | **EDIT 1** | engine parameter 1 | 2 | 3 | 4 (see [the engines](#the-ten-engines)) |
| | **EDIT 2** | engine parameter 5 | 6 | 7 | 8 |
| | **VOICE** | VCE: POLY, MONO, LEG (legato), UNI (unison) | GLD glide | GLMOD: RATE / TIME | PRIO: LAST, LOW, HIGH (mono note priority) |
| | **VOICE 2** | ALLOC: ROT (rotate) / REUSE | DTUNE (unison detune) | PAN | MUTE |
| **ARP** | **ARP** | MODE: OFF, UP, DN, UPDN, RND, ORD | RATE: 1/4 … 16T | OCT: 1–4 octaves | GATE |
| | **ARP 2** | SWG swing | PROB probability of each note | HOLD: OFF / ON (latch) | ORD: NOTE (pitch order) / PLAY (the order you played) |
| **SEQ** | **STEP** | STEP (cursor) | NOTE | TIME: NOTE, TIE, REST | FLAG: –, ACC, SLD, A+S |
| | **PATTERN** | LEN 1–64 steps | DIV 1/4 … 2BAR | SWG track swing | GATE note length |
| | **SONG** | the SONG screen | | | |
| **GLO** | **GLOBAL** | BPM 40–240 | SWING (all tracks) | CLICK: OFF, REC, ON | TUNE ±50 cents |
| | **MASTER** | DUST | DUCK | FILT (DJ filter) | ROLL (note-repeat rate) |
| | **SYSTEM** | MIDI out: KEYS / SEQ | SYNC: INT, USB, TRS | IN: NOTES / CLOCK | CPU load (USB link state when no computer is connected) |
| | **DRUMS** | CH: the drum track's MIDI input channel (0 = off, default 10) | LVL drum level | REV drum reverb | — |
| **SAVE** | **PRESETS** | the sound | the engine | — | — |
| | **USER** | SLOT 1–32 | LOAD (GO) | ERASE (GO) | SAVE (GO) |
| | **PROJECT** | SLOT 1–4 | — | LOAD (GO) | SAVE (GO) |
| | **TOOLS** | CLRSQ: clear the pattern (GO) | INIT: reset the sound (GO) | — | NEW project (GO) |

Notes:

- **Global** pages (DLY, REV/CHO, GLOBAL, MASTER, SYSTEM, DRUMS) are the same whatever track is selected.
- **SYNC**, **MIDI** and **IN** are settings of the FM-1, kept when you load a project.
- On the **drum track**, the pages that work are SLICER, PATTERN (SEQ), the SONG screen and the global pages; its kit, level, reverb and pan are on the DRUMS kit page, and its filter is **FX + KNOB 4**.
- FM6 tracks ignore the ENV page and ENV DEST (each operator has its own envelope).
- On the screen, EDIT 1 and EDIT 2 carry the engine's own page names: ANALOG *OSC / FLT*, DIGITAL *OPS / MOD*, PHASE *PHS / LINE*, LOFI *CHIP / MOTN*, SAMPLE *SET / TONE*, VOICE *VOWL / TONE*, TRIO *OSC / TONE*, WHEEL *BARS / TONE*, GRAIN *GRAN / SPRY*, FM6 *OPS / PATCH*.
- Any sound parameter on these pages can be **locked** on a step (see [Parameter locks](#parameter-locks)), except the pattern, arp, key and voice-mode ones.

---

## 17. The sounds and the ten engines

### The sound bank

**76 factory sounds**, every one a full patch on one of the ten engines, all level-matched (the same LEVEL gives the same loudness). **PRESETS** browses them **by kind** — basses, keys, organs, pads, leads, plucks and bells, stabs, FX — with the kind shown next to the name; your **32 user presets** come after. **To move a kind at a time, hold HOME and turn PRESETS**: each click lands on the first sound of the next kind (basses → keys → organs …, then your presets, then round), and the kind flashes in the top bar. The PRESETS page has the same jump on KNOB 3, KIND. A new project starts at **90 BPM** with *808 BOOM* on track 1, *RHODES* on track 2, *LOFI FLUTE* on track 3 and the 808 kit on track 4.

| Kind | Sounds (engine) |
| --- | --- |
| **Bass** | 808 BOOM, 808 DIRTY, 808 SLIDE, SUB BASS, PLUGG BASS (they slide between held notes, two octaves under the keys) · REESE, WOBBLE, ACID 303, FUNK BASS (ANALOG) · FM BASS (DIGITAL) · CZ BASS (PHASE) · ROUND BASS (FM6) · FAT BASS (TRIO) · WOW BASS (VOICE) · GB BASS (LOFI) · UP BASS, DEEP BASS (SAMPLE) |
| **Keys** | RHODES, DX RHODES, WURLI, M1 PIANO, AFRO KEYS, CLAV (DIGITAL) · GRAND PNO, DUSTY PNO, LOFI KEYS (SAMPLE) · SOFT KEYS (PHASE) · TINE EP (FM6) |
| **Organ** | SOUL ORGAN, GOSPEL, JAZZ ORGAN, DIRTY B3, HOUSE ORGN (WHEEL) · DRAWBARS (FM6) |
| **Pad** | WARM PAD, DARK STR, ATMOS PAD (ANALOG) · SAW PAD (TRIO) · GLASS PAD (DIGITAL) · SOFT PAD (FM6) · CZ STRING (PHASE) · LOFI CLOUD, VIBE HAZE (GRAIN) · CHOIR AAH, SOUL OOH (VOICE) |
| **Lead** | SUPERSAW, G-FUNK LD (ANALOG) · SYNC LEAD, HOOVER (TRIO) · TALKBOX (VOICE) · GAME LEAD (LOFI) · LOFI FLUTE (SAMPLE) · FLUTE DUST (GRAIN) |
| **Pluck & bell** | TRAP PLUCK (ANALOG) · RESO PLUCK (PHASE) · PLUGG BELL, TRAP BELL, MUSIC BOX, KALIMBA, MARIMBA (DIGITAL) · VIBES (SAMPLE) · 8BIT ARP (LOFI) · GLASS BELL, WOOD BARS, NYLON PICK (FM6) |
| **Stab** | MIN STAB, MIN7 STAB, RAVE STAB, DUB CHORD (TRIO: one key plays the chord) · SYN BRASS (ANALOG) · CZ BRASS (PHASE) · BRASS SECT (FM6) · HORN STAB, STRING STB (SAMPLE) |
| **FX** | SCRATCH (scratch, backspin, rewind across the keys) · GM KIT (SAMPLE) |

To pick an engine directly: SAVE → PRESETS → **KNOB 2**. To start from the engine's defaults: SAVE → TOOLS → **INIT**.

### The ten engines

Each engine's eight parameters are on **EDIT 1** (KNOB 1–4) and **EDIT 2** (KNOB 1–4), tap EDIT once and twice; the screen shows them under the engine's own page names (for example DIGITAL: *OPS*, *MOD*). A dash is an unused knob.

| Engine | Sound | EDIT 1: KNOB 1 · 2 · 3 · 4 | EDIT 2: KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- | --- |
| **ANALOG** | virtual analogue: two oscillators, filter | WAVE (SAW, SQR, TRI, SIN, PWM) · DTN detune · MIX · NOIS noise | CUT cutoff · RES resonance · DRV drive · KTR key tracking |
| **DIGITAL** | 4-operator FM | ALG algorithm 1–8 · R2 · R3 · R4 (operator ratios) | IDX FM amount · MDEC modulator decay · FB feedback · — |
| **PHASE** | phase distortion (CZ style) | WAVE · WAVE2 (a second wave, or –) · DCW (the distortion amount) · ENV (its envelope) | DTN detune · LINE: MIX / RING · SUB sub-oscillator · — |
| **LOFI** | 8-bit chip | CHIP: 4BIT, 4B/2, 8BIT, 1BIT, STEP · WAVE: PLS, TRI, SAW, NOIS, WRAM · DUTY (pulse width / wave table) · CRSH crush (DCY on STEP) | SWP pitch sweep · VIB vibrato · ARP: OFF, OCT, MAJ, MIN (chip arpeggio) · TONE |
| **SAMPLE** | sampler | SET: PIANO, BASS, VIBES, HORNS, STRGS, FLUTE, SCRCH, PERC, **USR1–USR4** · TUNE ±24 · BITS (bit reduction) · LOOP: OFF / ON | CUT (tone) · — · DRV drive · — |
| **VOICE** | formant voice (choirs, talkbox) | VOWL vowel · VOWL2 second vowel · TALK (time from one to the other) · SHIFT formant shift ±12 | BUZZ · BRTH breath · Q · RAND |
| **TRIO** | three oscillators | WAVE (16 combinations) · INT2 · INT3 (intervals of oscillators 2 and 3, ±24) · DTN | MODE: LP, BP, HP, NOT (filter type) · CUT · RES · PW pulse width |
| **WHEEL** | tonewheel organ | REG (16 drawbar registrations: FLUTE, MELLO, HOLLW, SMOOT, 3BAR, BLUES, GOSPL, ROCK…) · SUB · BODY · TOP (drawbar groups ±8) | PERC: OFF, 2ND, 3RD, 2SOFT, 3SOFT, 2SLOW, 3SLOW · CLICK · DRV · ROTR: OFF, SLOW, FAST (rotary speaker) |
| **GRAIN** | granular | SRC (any sample set, USR1–USR4 too) · POS position · SIZE grain size · DENS density | PTCH ±24 · SPRD spread · RAND randomness · TONE |
| **FM6** | six-operator FM, DX7 patches | ALG: PAT (the patch's) or 1–32 · FB feedback · MLVL modulator level (brightness) · MRAT modulator ratio | MEG modulator envelopes slower / faster · VMOD velocity → brightness · DTUN carrier spread · PTCH: F1–F8 factory, B1–B27 your bank |

The **SAMPLE** sets are free recordings (CC0: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi), coloured like a record through an old sampler. **GRAND PNO** is a Steinway recorded note by note; long notes fade as on the real one.

### FM6 and DX7 patches

**FM6** is msfa, the synthesis core of Dexed, as Felucca 1.0 ported it: six operators with their own four-stage envelopes, keyboard scaling, velocity, ratio or fixed frequency and detune; 32 algorithms, feedback, LFO, pitch envelope; six voices per track. The eight EDIT values above are **macros on top of the patch**; the track's FLT moves MLVL (so LFO → FLT still brightens it), SHP the feedback, PIT the pitch.

Edit full patches in the web editor's **FM6** panel (Sound page, on an FM6 track): every operator, **Send to track**, **Store in bank** (B1–B27), **Import SysEx** (a DX7 voice or a 32-voice bank: thousands of DX7 and Dexed patches play on the FM-1) and **Export SysEx**. A project keeps the track's **PTCH**, not the patch itself: store an edited patch in the bank to keep it.

---

## 18. Effects: master, punch-in, per track

### Signal path

Each synth track: engine → **drive** (DST) → **SLICER** → **FILTER** (the track's own) → level (and DUCK) → pan and **sends** to the stereo chorus, the tempo delay and the reverb. The drum track has its own slicer, filter, level, pan and reverb send. Then the whole mix, with the effect returns: **DUST** → **punch-in effects** → **FILT** (the DJ filter) → **MASTER** volume → limiter.

### Master: DUST, DUCK, FILT, ROLL

On FX + KNOB 1–3, or GLO → MASTER:

| | Does |
| --- | --- |
| **DUST** 0–100 % | the mix through an old sampler and a record: drive into a soft clip, a lower sample rate (down to ~11 kHz), fewer bits (down to 8), a low-pass closing to ~3 kHz, and, while the transport plays, a little hiss and crackle (a stopped SLOOP is silent) |
| **DUCK** 0–100 % | every kick pumps the synth tracks down and back over an 1/8 note: the sidechain sound, in time at any tempo |
| **FILT** | a DJ filter: left of centre a low-pass closing, right a high-pass opening, centre OFF; it glides (no zipper noise) |
| **ROLL** | the note-repeat rate of ARP + key |

### Punch-in effects

**FX + white key**: the effect runs on the whole mix while the key is held and lets go cleanly when you release it. Loops and the gate are locked to the tempo and start on the grid.

| Key | Effect | Key | Effect |
| --- | --- | --- | --- |
| 1 · F3 | loop 1/4 | 9 · G4 | low-pass sweep |
| 2 · G3 | loop 1/8 | 10 · A4 | high-pass sweep |
| 3 · A3 | loop 1/16 | 11 · B4 | phone |
| 4 · B3 | loop 1/32 | 12 · C5 | bit crush |
| 5 · C4 | stutter (1/16 triplets) | 13 · D5 | alias (sample-rate drop) |
| 6 · D4 | reverse | 14 · E5 | gate 1/16 |
| 7 · E4 | tape stop | 15 · F5 | echo (dotted 1/8) |
| 8 · F4 | half speed | 16 · G5 | tape wobble |

### Per track

- **FILTER** (FX → FILTER, or **FX + KNOB 4** for the selected track): one knob, left a low-pass, right a high-pass, centre off. It stays when you change the sound, can be locked on a step, and is saved with the project.
- **Sends** (FX page): DST drive, CHO chorus, DLY delay, REV reverb. The delay and the reverb / chorus settings are global (DLY and REV/CHO pages). The delay's TIME includes dotted 1/8 and 1/16.
- **SLICER** (FX → SLICER): a tempo gate (GATE) or stutter (STUT), 16 patterns, 1/8 to 32T, with a depth.

---

## 19. The visualiser

On the TRACKS screen, **tap HOME**: the whole screen becomes a visualiser of what SLOOP plays.

| Control | Does |
| --- | --- |
| **SELECT** | the next / previous style (its name shows a second; the last one is kept with the settings) |
| **HOME**, or any page button | closes it |
| keys, PLAY, REC, layers | work as ever (a held layer shows its screen, then the visualiser comes back) |
| KNOB 1–4 | nothing (the tempo is GLO + SELECT meanwhile) |

| # | Style | Shows |
| --- | --- | --- |
| 1 | **OSCILLOSCOPE** | the mix's wave, standing still |
| 2 | **SPECTRUM** | 32 bands with falling caps |
| 3 | **SPECTROGRAM** | the spectrum scrolling down, black → blue → green → yellow → white |
| 4 | **LISSAJOUS** | the stereo image: the wider the cloud, the wider the sound |
| 5 | **VU METERS** | tracks 1–4 in their colours and the mix, with peak holds |
| 6 | **CIRCLE** | the wave round a ring that swells on every kick |
| 7 | **TAPE** | two reels turning with the song, the level, the bar and the tempo |
| 8 | **LCD** | the tempo in seven-segment digits, bar and beat, four track meters |
| 9 | **BOUNCE** | a ball per track, kicked up by its notes |
| 10 | **ORBIT** | four planets turning in 1, 2, 4 and 8 beats round a sun that pulses with the mix |
| 11 | **WIRES** | a string per track, set swinging by its notes |
| 12 | **SLOOP** | the logo alive: the sail's four bands are the tracks, the boat rocks on its wave |

The visualiser sees the mix **as if MASTER were all the way up**: with MASTER turned down, even to 0, it still moves at full size. It costs the sound nothing.

---

## 20. The HOME menu and the lights

**Hold HOME** (0.7 s) to open the menu; hold it again to close it. The menu is in four sections:

| Section | KNOB 1 | KNOB 2 | KNOB 3 |
| --- | --- | --- | --- |
| **SCREEN** | **COLOR**: the screen palette (GREEN, AMBER, CYAN, RED, MONO…) | **ZOOM**: ON shows the value you touch large and white | — |
| **LIGHTS** | **LIGHTS**: OFF, LOW, MID, HIGH | **KEYS**: OFF, C KEYS, WHITE KEYS, ALL KEYS | **NOTES**: OFF / ON |
| **AUDIO** | **SPEAKER LOWCUT**: ON cuts the lows the small speaker cannot play (~110 Hz) | **USB AUDIO**: MASTER / FULL | **USB SERIAL**: OFF / ON (developers; takes effect at the next start) |
| **SYSTEM** | **HARDWARE CALIBRATION** (the knob selects it, OCT+ opens it) | **ABOUT**, the version (the knob selects it, OCT+ opens it) | — |

| Control in the menu | Does |
| --- | --- |
| **SELECT** | the previous / next section |
| **KNOB 1, 2, 3** | set the section's rows directly (each row shows its knob's colour) |
| **PRESETS** | moves the cursor |
| **OCT+** | steps the cursor's setting round, or opens it (CALIBRATION, ABOUT) |
| **OCT−** | closes (from ABOUT: back to the section) |
| **hold HOME** | leaves the menu |

All of these are **settings of the FM-1**, kept when you load a project or start a new one.

### Lights

- **LIGHTS** — every button glows at that level, so its label can be read in the dark (on a black FM-1 the labels are unreadable unlit). What is on — the page, PLAY, REC, an octave — stays at full light and still blinks.
- **KEYS** — the Cs, every white key, or every key glow at the LIGHTS level too (KEYS turns LIGHTS on at LOW if it was off).
- **NOTES** — ON: the notes sounding on the selected synth track light their keys, played live or by the sequencer (each for at least a tenth of a second, so short sequencer notes show too). The drum track always lights its hits. Works on every page and in every layer.
- **Always:** PLAY flashes on every beat; REC blinks when armed and is solid while recording; the button of the page shown is lit; in a layer, keys 1, 5, 9, 13 glow dimly and what is on is fully lit; OCT− / OCT+ light when the octave is off 0; a locked layer's button blinks.

### Hardware calibration

If a button or knob does the wrong thing, HOME menu → SYSTEM → **HARDWARE CALIBRATION** (or hold **OCT− + OCT+** while switching on): press each button and turn each knob to the right as the screen asks. 30 s without input cancels it and keeps the old setup.

---

## 21. MIDI

### Where MIDI comes in

- **The MIDI IN jack** (3.5 mm TRS on the FM-1): a keyboard or pad controller with a MIDI output, through a TRS-to-DIN MIDI adapter. If nothing plays, try the other type of adapter (type A / type B).
- **USB**: a computer or phone (a DAW, a MIDI routing app) or a USB MIDI host box. A USB keyboard plugged straight into the FM-1 cannot work: both are USB devices, and a USB link needs a host.

Both work at once.

### Channels

| MIDI channel | Plays |
| --- | --- |
| **1, 2, 3** | synth tracks 1, 2, 3 |
| **10** | the drum track (the nearest of its 16 sounds; GLO → DRUMS → **CH** changes it, 0 = no drum input; the drums still go out on 10) |
| **4–16** | the **selected** track: set your keyboard to channel 4 and it follows ALGORITHM |

### MIDI settings (GLO → SYSTEM)

| Knob | Setting | Choices |
| --- | --- | --- |
| KNOB 1 | **MIDI** (out) | **KEYS**: only what you play on the keys goes out · **SEQ**: the sequencer, the arp and the rolls go out too |
| KNOB 2 | **SYNC** (clock in) | **INT**: SLOOP's own tempo · **USB** or **TRS**: follow that MIDI clock |
| KNOB 3 | **IN** | **NOTES**: notes and clock · **CLOCK**: only the clock and START / CONTINUE / STOP, every incoming note ignored |
| KNOB 4 | **CPU** | the processor load (or the USB link state while no computer is connected) |

All three are settings of the FM-1, kept when you load a project.

- **MIDI out:** each track on its channel (1–3 synths, 10 drums). With **SEQ**, set a DAW track to record the FM-1's MIDI and you get the pattern as notes, or let SLOOP drive another synth. Every note is ended, STOP ends whatever was still on, and notes that came in are never sent back (no MIDI loop).
- **Clock in:** START plays from the top, CONTINUE carries on, STOP stops; the tempo follows the master pulse by pulse (24 a beat), so SLOOP cannot drift. When the clock stops for half a second, PLAY on the FM-1 plays at its own tempo again. A MIDI START during the REC count-in starts and records at once.
- **Clock only (IN = CLOCK):** for a DAW or sequencer that sends notes to other gear on the same cable. A note-off still gets through, so nothing held when you switch is left hanging.
- **Bluetooth MIDI** is not supported: SLOOP never switches the radio on.

---

## 22. USB audio

On USB the FM-1 is also an audio input named **Felucca**: 44.1 kHz, 16-bit stereo, class compliant, no driver. In your DAW or Audacity choose that input and record: you get the master output, exactly what the headphones play (after DUST, DUCK and FILT; the click and count-in too if they are on). MIDI, the editor and the installer keep working on the same cable.

**HOME menu → AUDIO → USB AUDIO:**

- **MASTER** (default): the recording follows the MASTER knob, as the headphones do. Keep MASTER well up while you record.
- **FULL**: a fixed level, as with MASTER all the way up (kept from clipping by the limiter); MASTER then only sets the headphones. Best for a computer input with no level control of its own.

The first time, the computer sees the FM-1 as a slightly different device (MIDI + audio) and sets it up again. On macOS 13–15, keep **USB SERIAL** OFF (the default) or the audio input may not show.

---

## 23. Your own samples and drum kits

The FM-1 has **four sample slots**, **USR1–USR4**, of about **7.4 s** each (22 kHz mono). A slot holds either:

- **an instrument**, played by a synth track: engine **SAMPLE**, **SET = USR1 … USR4** (or the GRAIN engine's SRC), or
- **a drum kit**, played by the drum track: **KIT = USR1 … USR4**, or **USR3+4**, one big kit of about **15 s** over two slots.

A good setup: USR1 and USR2 as instruments, a big kit on USR3+4. You fill the slots from the web editor.

### An instrument (editor → Samples)

1. **Choose a slot.** Four tiles show what each slot holds and how full it is; the page warns when sending would replace something (or half of your USR3+4 kit).
2. **Make the sound**, one of two ways:
   - **From files (one note per file):** drop up to 16 WAV files (any rate, mono or stereo). Each plays at its own note, the keys between play the nearest one, pitched. The note is read from the file name (`KEYS_C4.wav`, C4 = 60) or set in the list; a keyboard picture shows which keys play which file; ▶ plays a file; the meter shows the time used.
   - **Chop a recording** (CHOP): open or drop a recording (WAV, MP3, AIFF…) and cut it into up to 16 chops, one per key.
3. **Send it and play it:** name it, **Send to USRn**, then **Play this slot on track 1, 2 or 3**: that track switches to SAMPLE with SET = USRn.

**CHOP** in detail:

| Control | Does |
| --- | --- |
| **TAP** (or the space bar) while it plays | a cut at that moment (*snap to the hit* puts it on the attack) |
| **Find hits** (with a sensitivity), **Grid**, **Equal parts** | cuts made for you |
| the box on a chop, **Keep all**, **Keep none**, **K** | keep or leave out chops (left-out chops go neither to the slot nor to the WAVs) |
| the length slider, or the handle at the bottom of the wave | shorten a chop; **Up to the next marker** undoes it |
| **Fit to slot** | shortens the longest chops just enough to fit |
| **Download WAVs** | the kept chops as WAV files |
| keys 1–9, 0 | listen to a chop |
| double-click / drag / ← → / Del | add / move / nudge / remove a marker |
| mouse wheel | zoom |

A recording of any length works: keep the chops you want, the slot takes about 7.4 s of them.

### A drum kit (editor → Drum kit)

1. **Your sounds.** Drop a WAV on any of the **16 pads** (KICK, KICK 2, SNARE, CLAP, HAT, OPEN HAT, PEDAL, RIM, SNARE 2, LOW TOM, HI TOM, CRASH, RIDE, SHAKER, CONGA, COWBELL), or **Choose files** to add several at once: they are sorted onto the pads by name (*kick*, *bd*, *snare*, *hh*, *open*, *crash*…; a second kick goes to KICK 2). Click a pad's wave to hear it. Each pad has **Pitch** (±12 semitones), **Gain** and **Length** (a cut ends with a short fade).
2. **Where it goes.** **USR3+4** (the big kit, about 15 s; the editor shares the sounds out over the two slots by itself), or a single slot **USR1–USR4** (about 7.4 s). Each choice shows what it holds now and what a send would replace. The meter shows the time used; too long? shorten the long sounds or press **Fit the slot**.
3. **Send it.** Name it, **Send**, then **Use … on the drum track** selects track 4 and sets its KIT (or turn PRESETS on the drum track: USR3+4 is the last kit).

**Save as ZIP** keeps the kit's 16 WAVs to share; **Open ZIP** (or dropping a .zip) loads one back.

On the FM-1 your kit works like any other: steps, levels, ratchets, fill conditions, the DRUMS grid, the keys, MIDI channel 10 (a note plays its lane's sound), the filter, the slicer, mute. The closed hat cuts the open one. A lane without a sound is silent, and so is an empty slot.

---

## 24. The web editor

Open it from the [installer page](https://isod89.github.io/sloop-fm1/), the [editor link](https://isod89.github.io/sloop-fm1/webapp/editor/), or **`OPEN-EDITOR.bat`**, in **Chrome or Edge** with the FM-1 on USB, and press **Connect** (allow MIDI). It follows the device live: turn a knob on the FM-1, the editor moves. It looks like the FM-1: the black screen, the four track colours, the FM-1's own pixel font. The language button switches English / Japanese.

**Layout:** the side bar has the pages in three groups — **Track** (Sound, Sequencer, Tracks), **Sounds** (Library, Samples, Drum kit), **Device** (Projects, Settings) — and the connection. On top: the page title, the messages, and on the Track pages the **four tracks** (number, sound, engine): click one to select it. With the side bar focused, the arrow keys move between pages.

**Values:** each value is a tile you **drag up or down like a knob** (Shift: fine), or a bar: click it, then the wheel or the arrow keys (Shift: steps of 10); **double-click** resets it.

| Page | What you do there |
| --- | --- |
| **Sound** | every parameter of the selected track, as on the device: a card per button (ENV, LFO, EDIT, VOICE, FX, SCL, ARP), its pages as rows of four coloured values. **Engine** and **Presets** pickers, **Init sound**, **Copy to library**, **Save to file / Load file** (a sound as a file). On an FM6 track, the **FM6** panel: the six operators, **Read from track**, **Send to track** (*Send while editing* to hear every change), **Store in bank** (B1–B27), **Import SysEx**, **Export SysEx**, **Export bank as SysEx** |
| **Sequencer** | the pattern settings (LEN, DIV, SWG, GATE) and the steps as the device's tiles; the **step list** takes typed note names (*C4 E4 G4*). On the drum track: a **grid** of the 16 sounds × the steps, with the **kit**: choose a **level** (GHOST, SOFT, NORM, HARD) and a **roll** (x1–x4), then click a cell — a hit; click again (same level and roll) — cleared; **Shift+click** — one level louder. Click a step for its **detail**: **Nudge**, **Parameter locks** (+ lock, pick the parameter, set or delete), **Fill** (normal, fill only, no fill). **Reload**, **Clear sequence** |
| **Tracks** | the four channel strips: level, pan, mute, select; SOLO and REC shown as on the device |
| **Library** | your sound library, kept in the browser: **Save current sound**, search by name or tag, filter by engine, sort, **Audition**, **To device slot**, **Rename**, **Tags**, **Duplicate**, **Delete**, **Export** / **Import** / **Export library**; copy factory presets in. Next to it the **32 user presets on the device**: **Load**, **Store current sound**, **To library**, **Erase**, **Export bank**. Drag sounds between the library and the slots |
| **Samples** | your instruments in USR1–USR4, in three steps (see [above](#an-instrument-editor--samples)), with CHOP |
| **Drum kit** | your own drum kits, in three steps (see [above](#a-drum-kit-editor--drum-kit)) |
| **Projects** | the four project slots (**Load**, **Save**) and **Backup** (see below) |
| **Settings** | GLOBAL (BPM, SWING, CLICK, TUNE), **MASTER** (DUST, DUCK, FILT, ROLL), DRUMS (CH, LVL, REV), and the system information (firmware, sync, slots) |

The editor writes presets, the FM6 bank and backups to flash only while the song is stopped, as the FM-1 does (it asks you to stop first). Try it without hardware: add `?mock=1` to the editor's address. The protocol is documented in [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md).

---

## 25. Backup, rescue, going back, troubleshooting

### Backup and restore

Editor → **Projects** → **Backup**:

- **Save a backup** writes everything on the FM-1 to one file (`SLOOP-backup-DATE.json`): the music you are working on, the projects 1–4 (the song sections A–D), the 32 user presets, the FM6 bank, the samples **USR1–USR4** (your drum kits with them) and the settings (colours, calibration, song order, lights, SYNC, MIDI OUT).
- **Restore from a file** puts it all back: what is on the FM-1 is replaced. Stop the song (PLAY) first. A damaged file is refused before anything is written, and each object is written as a save writes it (a cut-off restore never leaves half an object).

A 2.4 backup restores into 2.4 (or later) only. **Save a backup before every update and before going back.**

### Rescue and special modes

| Situation | Do |
| --- | --- |
| SLOOP does not start | hold **OCT−** alone while switching on (*SLOOP USB RESCUE*), then install again from the browser |
| An install stopped half-way | the FM-1 waits in update mode: press **INSTALL** again |
| Buttons or knobs do the wrong thing | hold **OCT− + OCT+** while switching on, or HOME menu → SYSTEM → HARDWARE CALIBRATION |
| A developer needs UBOOT | hold **OCT− + OCT+** for 5 s while stopped (*UPDATE MODE IN 3…*; let go to cancel) |

### Going back to the official firmware

On the installer page, open **Return to the official firmware (V15)**: save a backup with the editor first, download FM-1 V15 from m-vave.com, select its `FM-1.fwsc` (only that exact file is accepted) and install it. M-VAVE's own updater (M-UPGRADE) works too; close every other app that uses MIDI first. To come back, install SLOOP again and restore your backup.

### Troubleshooting

| Problem | Fix |
| --- | --- |
| The installer or editor does not find the FM-1 | Chrome or Edge, a data cable, no hub, allow MIDI; close every other app or tab that uses MIDI, then reload |
| The black keys are silent on a synth track | it plays chords or the scale: SCL → CHORD OFF and KEYS OFF (with a chord on, the black keys change the chord: CHORD+) |
| Recorded notes move | SLOOP quantises to the track's DIV: use 1/32, or nudge the step (SEQ + step + KNOB 4) |
| Notes fade out on a dense part | the processor is at its limit; SLOOP fades one voice at a time (never the bass or lead) rather than glitching. Fewer held notes or a lighter engine help |
| Nothing from the MIDI IN jack | try the other adapter type (A / B); check the channel (1–3, 10, 4–16) and that GLO → SYSTEM → IN is NOTES |
| The USB audio input does not show | replug the FM-1; on a Mac keep USB SERIAL OFF and restart the FM-1; in Audacity: Transport → Rescan Audio Devices |
| The USB recording is quiet | HOME menu → USB AUDIO = FULL, or MASTER up |
| An FM6 sound changed after loading a project | store the edited patch in the bank (editor → FM6 → Store in bank) and set PTCH to it |
| *STOP THE SONG FIRST* / *STOP FIRST* | that change (new project, song edit, flash write) waits until the transport is stopped |
| *EMPTY SECTION: REC* | the section you asked for has nothing in it: save a loop into it first (SAVE + key 5–8) |
| Something else | [open an issue](../../issues): what you did, what you expected, what happened, and the version (HOME menu → SYSTEM → ABOUT) |

---

## 26. Cheat sheet

### Buttons alone

| Do | Result |
| --- | --- |
| PLAY | start / stop |
| REC | record / arm / close a free take |
| hold REC ~2 s | clear the selected track |
| tap HOME | TRACKS · on TRACKS: visualiser |
| hold HOME | menu |
| tap SAVE | on TRACKS: SONG screen · else SAVE pages |
| tap ENV, LFO, FX, SCL, EDIT, ARP, SEQ, GLO | their pages (again: next page) |
| tap EDIT / SEQ on TRACKS, drum track | DRUMS screen (again: grid ↔ kit) |
| OCT− / OCT+ | octave (both: 0) · drums: ghost / hard while held |
| ALGORITHM | track |
| PRESETS | sound · drum kit |
| SELECT | tempo · pages · visualiser style · menu section |

### Layers

| Hold + | Keys | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | Other |
| --- | --- | --- | --- | --- | --- | --- |
| **FX** | punch-in effects 1–16 | master FILTER | DUST | DUCK | track FILTER | + HOME: lock |
| **EDIT** | erase sound / note | SHIFT | LENGTH ×2 / ½ | TRANSPOSE | — | OCT− undo · OCT+ redo |
| **ARP** | note repeat | RATE | — | — | — | OCT− / OCT+: ghost / hard (drums) |
| **SEQ** | steps · black keys 1–4: page | SOUND / NOTE | DIV | SWING | LENGTH | OCT− / OCT+: page |
| **SEQ + step** | more steps | drums: which sound · synths: NOTE | LEVEL | RATCHET | NUDGE | PRESETS lock · ALGORITHM lock parameter · OCT+ fill condition · OCT− clear nudge, locks, condition |
| **SCL** | key of the song | CHORD | SCALE | KEYS | TRANSPOSE | — |
| **GLO** | 1–4 mute · 5–8 solo · 9 fill · 10 fill bar · 16 tap tempo | level 1 | level 2 | level 3 | level 4 | SELECT: tempo |
| **SAVE** | 1–4 play A–D (several: chain) · 5–8 save A–D · 13 loop / song · 14 SONG REC · 16 song screen | — | — | — | — | — |
| **any layer + HOME** | lock it open | | | | | any other button unlocks |

### Chord mode (CHORD on)

| Hold | + white key |
| --- | --- |
| nothing | the scale's chord on that key (C4 = I) |
| F# | major ↔ minor |
| G# | + 7th |
| A# | sus4 |
| C# | + 9th |
| D# | inversion |

### Power-on and special

| Do | Result |
| --- | --- |
| OCT− held at power-on | USB rescue |
| OCT− + OCT+ held at power-on | hardware calibration |
| OCT− + OCT+ held 5 s, stopped | update mode (UBOOT) |
| EDIT + OCT− / OCT+ | undo / redo |
| GO values (LOAD, SAVE, ERASE, NEW, CLRSQ, INIT) | one click arms (*AGAIN: …*), a second within ~1.5 s acts |
| SAVE + key 5–8 on a used section | press again within 3 s |
| SONG screen: REC on a used section · OCT+ | press again within 3 s |

### Where is…?

| Looking for | It is |
| --- | --- |
| tempo | SELECT on TRACKS · GLO + key 16 (tap) · GLO → GLOBAL → BPM |
| swing | TRACKS KNOB 1 (global) · SEQ + KNOB 3 (track) · GLO → GLOBAL |
| metronome | GLO → GLOBAL → CLICK |
| a track's level / pan | TRACKS KNOB 2 / KNOB 4 · GLO + KNOB 1–4 |
| the drum kit | PRESETS on the drum track · DRUMS kit page KNOB 1 |
| the drum level / reverb | DRUMS kit page KNOB 2 / 3 · GLO → DRUMS |
| a track's filter | FX + KNOB 4 · FX → FILTER |
| delay time | FX → DLY → TIME |
| chords | SCL + KNOB 1 · SCL page |
| STRUM, VLEAD | SCL 2 |
| your projects | SAVE + keys 1–4 (play) / 5–8 (save) · SAVE → PROJECT |
| your sounds | SAVE → USER · PRESETS (after the factory sounds) |
| MIDI out / clock / clock only | GLO → SYSTEM: MIDI · SYNC · IN |
| lights, colours, USB audio level | hold HOME (menu) |
| the version | hold HOME → SYSTEM → ABOUT |

---

*SLOOP is based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments. Full credits and licences: [SLOOP.md](SLOOP.md#rescue-going-back-credits) and [LICENSING.md](LICENSING.md). M-VAVE and FM-1 are trademarks of their owners; SLOOP is not affiliated with them.*
