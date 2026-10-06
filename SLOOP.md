<p align="center"><img src="assets/logo/sloop-logo.png" alt="SLOOP" width="480"></p>

# SLOOP 2.3

**A live groovebox firmware for the M-VAVE FM-1 — for any style.** Four tracks — three synths and a drum machine with 16 sounds on the white keys — nine synthesis engines, 68 sounds, 37 drum kits (808, 909, trap, phonk, house, techno, UK garage, jungle, amapiano, reggaeton, synthwave, chiptune, ambient, jazz…), your own samples, ghost notes and ratchets, note repeat, one-key chords, 16 punch-in effects, a vinyl / sidechain / DJ-filter master, and a teenage-engineering-style screen that always shows what your hands can do next. No factory patterns, nothing to load: everything you hear, you play.

SLOOP is free and open source (GPL-3.0), based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments.

> **Status:** 2.3, running on the FM-1. Still a beta: install at your own risk, and please report what you find (GitHub issues).

### New in 2.3

- **A MIDI keyboard on the MIDI IN jack.** The FM-1's 3.5 mm TRS MIDI input works: channels 1–3 play the synth tracks, 10 the drums, 4–16 the selected track. The input reads its buffer by content, so no note is left hanging (fix from Felucca [Salt], by ChanceTheMaker and keremimo). See [MIDI keyboards](#midi-keyboards).
- **MIDI clock in.** GLO → SYSTEM → **SYNC**: USB or TRS, a setting of the FM-1 that stays when you load a project. SLOOP follows the master's tempo, START, CONTINUE and STOP, pulse by pulse, so it never drifts (after Felucca 1.0, from contributions by ChanceTheMaker and keremimo). See [MIDI keyboards](#midi-keyboards).
- **USB audio: record the FM-1 on a computer.** On USB the FM-1 is also an audio input (*Felucca*, 44.1 kHz stereo, no driver): record its master output in your DAW or Audacity, MIDI and the editor still working on the same cable (after Felucca 1.0). HOME menu → **USB AUDIO**: the level follows the MASTER knob, or **FULL**, a fixed full level. See [USB audio](#usb-audio-record-on-a-computer).
- **Choose how REC records.** On the REC screen: KNOB 1 **mode** — *free* (the free take: the tempo follows you) or *tempo* (record at the tempo you set) — KNOB 2 the **length** (1, 2 or 4 bars), KNOB 3 the **start** — your first note, or a one-bar **count-in** after PLAY. See [Recording](#recording).
- **Lights for playing in the dark.** Hold HOME → **LIGHTS**: every button glows (LOW, MID, HIGH), so the labels are readable on a black FM-1; the active ones stay at full light. **KEYS**: the C keys, or every white key, glow too. **NOTES**: the notes playing light their keys, on every page and in every layer (by @renebohne). The glow is a short pulse on every scan, as in Felucca 1.0.1: no flicker, and the dim marks read as dim. See [Lights](#lights).
- **Backup and restore.** The editor saves everything on the FM-1 in one file — the music you are working on, the projects, the user presets, the samples, the settings — and puts it all back. See [The web editor](#the-web-editor).
- **CHOP: recordings of any length.** A recording longer than a slot (about 7 s) is no longer a dead end: tick the chops you keep, untick the rest, shorten any chop (its length slider, or drag the handle at the bottom of the wave), or press **Fit to slot** to shorten the longest ones just enough. Only the kept chops go to the slot or the WAVs, on consecutive keys. See [The web editor](#the-web-editor).
- **Back to the official firmware from the installer page**, a backup first: select M-VAVE's FM-1 V15 file and install it (as in Felucca 1.0).
- **Steadier.** The knobs answer every click: no more dead moments, double clicks or jumps. A note-off sent from a computer is never dropped any more when a lot of MIDI arrives at once (a hanging note), and a malformed MIDI message is ignored. When the processor is overloaded, SLOOP fades out one voice at a time, after two late halves in a row and never the bass or the lead, instead of cutting the oldest note. Keys play about a millisecond sooner. A key let go just after a change of VOICE (POLY, MONO…) no longer leaves its note stuck. The settings, projects and presets are checked more strictly when they are read back from flash. (All after Felucca 1.0.)
- The installer page shows the right numbers (68 sounds, 37 kits).

### New in 2.2

- **A new drum engine.** Every synthesised drum is now built like on the classic machines: a tuned body with a pitch drop and a hold, a second partial for the drum heads, a click for the attack, noise through a resonant filter, drive. Softer hits are darker as well as quieter. The 32 synthesised kits are rebuilt on it, each with 16 sounds of its own — new: **PHONK** (melodic cowbell), **AMAPIANO** (log drum), **GARAGE**, **D.HOUSE**. Kits 1–5 are now a sampled **ACOUSTIC** kit (CC0 studio recordings) and its treatments. Every kit is level-matched. See [Drum kits](#drum-kits).
- **68 sounds, browsed by kind.** PRESETS goes through basses, keys, organs, pads, leads, plucks and bells, stabs; the kind is shown next to the name. 14 new: 808 SLIDE, ACID 303, PLUGG BASS, SUPERSAW, M1 PIANO, AFRO KEYS, GRAND PNO, KALIMBA, PLUGG BELL, GLASS PAD, SAW PAD, RAVE STAB, DUB CHORD, HOUSE ORGN. Every factory sound is level-matched: the same LEVEL gives the same loudness. See [The sound bank](#the-sound-bank).
- **A real grand piano.** GRAND PNO is a Steinway recorded note by note (CC0); long notes fade as on the real one. DUSTY PNO and LOFI KEYS are the same piano through an old sampler.
- **Lock a layer.** Hold a layer button and tap HOME: the layer stays open with the button let go, both hands free (FX with one hand on the keys and the other on FILTER / DUST / DUCK). Any other button lets it go. See [The panel](#the-panel-tap-hold-layers).
- **Stereo effects.** The chorus is stereo, and the reverb is new: a feedback delay network, dense and wide, with no metallic ring.
- **More reliable.** Saves that fail are retried (*SAVE ERROR: RETRYING*) and everything is saved before an update; the end of a song gives your loop back; swing never plays a step twice; SONG REC counts bars right; a voice retriggered in UNISON, TRIO or PHASE no longer clicks; NEW PROJECT and saving a user preset wait until the song stops; the installer refuses a damaged package before writing it; the button lights no longer flicker.

### New in 2.1

- **Songs, live:** hold SAVE — keys 1–4 play sections A–D on the next bar, keys 5–8 save the loop into them, key 14 records the song as you play it (each section and its bars). See [Song mode](#song-mode).
- **Landmarks on the keys:** while a layer is held, and on the drum track, keys 1, 5, 9 and 13 glow dimly — the first key of each row of the 4 × 4 grid on the screen. What is on (an effect, a step, a sound) stays fully lit.

### New in 2.0

- **Hold a button, touch a key.** Every function button is a *layer*: hold it and the 16 white keys and the four knobs change job, the screen shows how. Tap it and its pages open as before.
- **16 drum sounds on the white keys**, black keys double them. **OCT− / OCT+ held** = ghost / hard hits. Hits keep their level and a **ratchet** (x1–x4) in the pattern.
- **Note repeat** (ARP + key), **erase as it plays** (EDIT + key), **steps under your fingers** (SEQ + key, Elektron style), **one-key chords in the song's key** (SCL), **mute / solo / tap tempo** (GLO).
- **Undo / redo** (EDIT + OCT− / OCT+), **hold REC to clear**, and an **autosave** that brings your beat back at power-on.
- **MPC swing** (50–75 %), a sample-accurate clock (no drift, any tempo), tighter glides for the 808s.
- **Master:** **DUST** (an old sampler and a record: bits, rate, crackle), **DUCK** (the kick pumps the synths), **FILT** (DJ filter: low-pass ← OFF → high-pass).
- **Web editor:** the drum track as a 16-lane grid with levels and ratchets, the kit, the master page.
- **Safer updates:** the installer checks the package's SHA-256 and only resumes SLOOP's own update loader; the loader checks the package CRC before it starts the new firmware, and refuses a flash chip it does not know.

---

## Contents

1. [Install](#install)
2. [Sixty seconds to a beat](#sixty-seconds-to-a-beat)
3. [The colour code](#the-colour-code)
4. [The panel: tap, hold, layers](#the-panel-tap-hold-layers)
5. [The drum track](#the-drum-track)
6. [Recording](#recording)
7. [Layers in detail](#layers-in-detail)
8. [Undo, clear, save, autosave](#undo-clear-save-autosave)
9. [Master: DUST, DUCK, FILT](#master-dust-duck-filt)
10. [Punch-in effects](#punch-in-effects)
11. [Screens](#screens)
12. [The sound bank](#the-sound-bank)
13. [Drum kits](#drum-kits)
14. [Your own samples](#your-own-samples-usr1usr3)
15. [Song mode](#song-mode)
16. [The web editor](#the-web-editor)
17. [Sound design pages](#sound-design-pages)
18. [MIDI keyboards](#midi-keyboards)
19. [USB audio: record on a computer](#usb-audio-record-on-a-computer)
20. [Lights](#lights)
21. [Specifications](#specifications)
22. [Rescue, going back, credits](#rescue-going-back-credits)

---

## Install

1. Double-click **`INSTALL-SLOOP.bat`** in the SLOOP folder. It builds the firmware and opens the installer at `http://localhost:8766/webapp/installer/`.
2. In **Chrome or Edge**, connect the FM-1 to the computer by USB (a data cable, directly — no hub).
3. Press **INSTALL**, allow MIDI access, and wait for *Done*. Keep the black window open until then.

The FM-1 restarts on the SLOOP logo. The editor is at `http://localhost:8766/webapp/editor/` (or **`OPEN-EDITOR.bat`**).

## Sixty seconds to a beat

1. **ALGORITHM** to track **4** (orange, drums). The white keys play 16 sounds: **F3 kick**, G3 kick 2, A3 snare, B3 clap, **C4 hat**, D4 open hat… **PRESETS** picks a kit: try *808* or *BOOMBAP*.
2. Press **REC**: *rec ready*. **Play a beat freely, at your own tempo** — no click, no count-in. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard ones.
3. **Press REC on the "1" after your last bar.** The loop closes: its length sets the tempo, the hits snap to the grid, the loop plays at once.
4. **REC** again while it plays: you record on top (overdub). Hold **ARP** and hold the hat key: a 1/16 hat roll, recorded as ratchets.
5. Turn **ALGORITHM** to track **1** (blue, *808 BOOM*), **REC**, play a bass line. Hold **SCL** and press the key of your song (e.g. D); on track 2 hold SCL and turn **KNOB 1** to *7TH*: every white key is now a chord of the key.
6. Hold **FX** and press a white key for a punch-in effect; still holding FX, turn **KNOB 2** for DUST, **KNOB 3** for DUCK.
7. Made a mistake? Hold **EDIT** and press **OCT−**: undo.

## The colour code

| Colour | Track | Knob |
| --- | --- | --- |
| **blue** | 1 · synth | KNOB 1 |
| **green** | 2 · synth | KNOB 2 |
| **yellow** | 3 · synth | KNOB 3 |
| **orange** | 4 · drums | KNOB 4 |

The four dials at the bottom of the screen show what KNOB 1–4 do now. White always means *what you are touching*. Red always means *recording*.

## The panel: tap, hold, layers

Every function button has two lives. **Tap** it (press and let go, touching nothing else): its pages open, as on any FM-1 firmware. **Hold** it: a **layer** — the 16 white keys and KNOB 1–4 change job while it is held, and after 0.14 s the screen shows the 16 keys as tiles and the knobs as dials. Let go: back to playing.

The tiles are four rows of four, keys 1–4, 5–8, 9–12, 13–16. To find them without looking at the screen, the first key of each row (1, 5, 9, 13) glows dimly while a layer is held, and on the drum track; the keys at full light are what is on.

**Lock a layer:** hold its button and tap **HOME** — the layer stays open when you let the button go, both hands free for the keys and the knobs (*LOCK* on the screen, the button blinks). Any other button lets it go (HOME, the layer's own button, ENV…) and does only that; PLAY, REC and OCT− / OCT+ keep working inside it.

| Hold | Keys | KNOB 1 · 2 · 3 · 4 | Tap |
| --- | --- | --- | --- |
| **FX** — *punch* | a punch-in effect while the key is held | FILTER · DUST · DUCK · — | FX pages |
| **EDIT** — *erase* | erase that sound / note from the pattern | SHIFT · LENGTH ×2 / ½ · TRANSPOSE · — | EDIT pages (drums: grid / kit) |
| **ARP** — *roll* | note repeat on the grid | RATE · — · — · — | ARP pages |
| **SEQ** — *steps* | steps 1–16 of the page | SOUND / NOTE · DIV · SWING · LENGTH | SEQ pages (drums: grid / kit) |
| **SCL** — *key* | the key of the song | CHORD · SCALE · KEYS · TRANSPOSE | SCL pages |
| **GLO** — *mix* | 1–4 mute · 5–8 solo · 16 tap tempo | level of tracks 1 · 2 · 3 · 4 | GLO pages |
| **SAVE** — *song* | 1–4 play section A–D (next bar) · 5–8 save the loop into A–D · 13 loop / song · 14 SONG REC · 16 the chain | — | TRACKS: the SONG screen · else the SAVE pages |

Other controls:

| Control | Action |
| --- | --- |
| **PLAY** | start / stop all four tracks (works inside any layer) |
| **REC** | playing: record now / stop · stopped: arm (the first note starts) · free take: close the loop |
| hold **REC** | clear the selected track (a ring fills: keep holding ~2 s; let go before and nothing happens) |
| **SAVE** | on TRACKS: the SONG screen · elsewhere: the SAVE pages |
| **EDIT + OCT− / OCT+** | undo / redo |
| **ALGORITHM** | select the track (on every page) |
| **PRESETS** | the selected track's sound, or the drum kit |
| **SELECT** | tempo (always, even inside a layer) |
| **OCT− / OCT+** | synth tracks: octave (both: back to 0) · drum track, held: ghost / hard hits |
| **HOME** | the TRACKS screen · hold: menu (colour, low cut, zoom, lights, keys, notes, USB audio, calibration, about) · tapped while a layer is held: lock it open |
| **ENV / LFO** | their pages |

## The drum track

Track 4 plays **16 sounds, one per white key** from the lowest F to the highest G; a black key plays the sound of the white key on its left (two fingers on one sound, for fast rolls).

| Key | Sound | Key | Sound | Key | Sound | Key | Sound |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F3 | kick | C4 | hat | G4 | snare 2 | D5 | ride |
| G3 | kick 2 | D4 | open hat | A4 | low tom | E5 | shaker |
| A3 | snare | E4 | pedal hat | B4 | hi tom | F5 | conga |
| B3 | clap | F4 | rim | C5 | crash | G5 | cowbell |

**Levels:** every hit has one of four levels — **GHOST**, **SOFT**, **NORM** (as played), **HARD**. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard hits; they are recorded so. **Ratchets:** a hit can repeat x1–x4 inside its step (ARP rolls record them; SEQ + a step + KNOB 3 sets them). The closed and pedal hats choke the open one.

## Recording

SLOOP records live and layers every pass on top of the last (overdub). Notes go to the nearest step **as you heard it**: the time between a key and its sound (~12 ms) is taken back, so what you play on the beat lands on the beat. Chords are kept on the synth tracks (up to 4 notes a step); held notes become ties.

| When | REC does | Then |
| --- | --- | --- |
| **Playing** | records the selected track **at once** | REC again stops recording, the loop plays on |
| **Stopped, project with notes** | arms (*rec ready*, the REC light blinks) | **your first note starts the loop and is step 1**; PLAY starts it too |
| **Stopped, empty project** | arms (*rec ready* · *play freely*) | a **free take** (see below), or MODE *tempo*: as with notes |

**The REC screen sets how it records** (armed, before the first note), with the knobs:

| Knob | Dial | Choices |
| --- | --- | --- |
| KNOB 1 | **mode** (empty project only) | **free**: a free take, the tempo follows you · **tempo**: record at the tempo set (SELECT) |
| KNOB 2 | **length** | the loop of the selected track: **1, 2 or 4 bars** |
| KNOB 3 | **start** | **note**: your first note starts the loop · **count**: press **PLAY**, one bar of clicks (4, 3, 2, 1 on the screen), then the loop starts recording; notes played before only sound |

MODE and START are settings of the FM-1: they stay as you left them. In a project with notes there is no MODE: it always records at the tempo set (a free take would change the tempo of what is already there). During the count-in, REC cancels it and PLAY goes back to *rec ready*.

**Free take — the loop follows you.** On an empty project there is no tempo yet, so you set it by playing:

1. REC, then play freely. The screen shows *free take*, the seconds, and the loop it would make right now (*2 bars · 92 bpm*).
2. **Press REC on the "1" after your last bar.** The time from your first note to that press is the loop: SLOOP picks 1, 2 or 4 bars at the tempo nearest the one set (within 3 % the set tempo is kept), writes your notes into it with their lengths and levels, and plays it at once. All four tracks take that length.
3. **PLAY** during a free take drops it. A take closes by itself after 24 s.

- While recording the REC light is solid and the track shows a red *rec*. Turn ALGORITHM and the take moves to the next track without stopping.
- The **PLAY light flashes on every beat**: a visual metronome. An audible click: GLO → GLOBAL → **CLICK** (`OFF`, `REC`, `ON`); it is never recorded.
- **Swing** is MPC swing: 50 % straight to 75 % (GLO → GLOBAL → SWING for all tracks, SEQ + KNOB 3 per track). The swing of a track adds to the global one.

## Layers in detail

### FX — punch

The 16 white keys are the [punch-in effects](#punch-in-effects); they run while the key is held. The knobs drive the [master](#master-dust-duck-filt): **KNOB 1 FILTER** (turn left: low-pass, right: high-pass, centre: off), **KNOB 2 DUST**, **KNOB 3 DUCK**. Keys pressed while FX is held never play or record notes.

### EDIT — erase

Hold EDIT and press a key: that sound (drums) or that note (synths; with CHORD on, the notes of its chord) leaves the selected track's pattern — **while playing**, from every step the playhead passes while you hold the key (MPC style: hold the hat key for one bar and the hats of that bar are gone); **stopped**, from the whole pattern at once. *ERASED* flashes. The knobs reshape the whole pattern:

- **KNOB 1 SHIFT** — every step one later / earlier (turns the groove around).
- **KNOB 2 LENGTH** — right: ×2 (the pattern copied after itself, up to 64 steps); left: ½.
- **KNOB 3 TRANSPOSE** — every note a semitone up / down (synth tracks).
- **OCT− undo · OCT+ redo** (the knob turns of one hold count as one change).

### ARP — roll (note repeat)

Hold ARP and hold a key: it repeats on the grid at the **RATE** of KNOB 1 — 1/8, 1/16, 1/32, 32T, 1/64 — locked to the tempo and the swing, so it always lands in time. On the drum track OCT− / OCT+ make it ghost / hard. While recording, a roll is written as ratchets (a 1/32 roll on a 1/16 track: x2 on each step). Rolls end with their key.

### SEQ — steps (step sequencer)

The 16 white keys are the 16 steps of the page; the lit ones play. The first four black keys (F#3, G#3, A#3, C#4) or **OCT− / OCT+** pick page 1–4 (steps 1–16, 17–32, 33–48, 49–64, up to the track's LENGTH).

- **An empty step:** press its key — it is set at once. Drums: with the sound shown (KNOB 1 picks it, or the last pad you hit); synths: with the note or chord you played last.
- **A set step:** press and let go — it is cleared. Hold it and turn a knob instead — it is edited, and kept: **KNOB 1** sound (drums) / note (synths), **KNOB 2 LEVEL** (ghost, soft, norm, hard), **KNOB 3 RATCHET** (x1–x4). Hold several step keys to edit them together.
- **No step held:** KNOB 1 the sound / note to set · KNOB 2 **DIV** (1/4 … 1/32, triplets) · KNOB 3 **SWING** of the track · KNOB 4 **LENGTH** (1–64 steps; each track loops on its own length, polymeters stay in phase).

### SCL — key and chords

- **Any key** sets the **key of the song**: the root of all three synth tracks (*KEY D*).
- **KNOB 1 CHORD** (selected synth track): OFF, TRIAD, 7TH, 9TH (1-3-7-9, the lo-fi / R&B voicing), SUS4, POWER. With a chord on, **the white keys walk the scale from C4** — C4 is the chord of the key's I, D4 the II, E4 the III… — and one finger plays the whole chord, recorded as a chord. With SCALE on CHR, the chords come from the minor scale.
- **KNOB 2 SCALE** for all synth tracks (16 scales: major, minor, dorian, mixolydian, pentatonics, harmonic, blues…).
- **KNOB 3 KEYS**: OFF (all keys chromatic), SNAP (every key rounded to the scale), WHITE (the white keys walk the scale, the black keys are silent).
- **KNOB 4 TRANSPOSE** the selected track, ±24 semitones.

Changing a sound (PRESETS, a user preset) never changes the key, the chord mode, the pattern or the mix of its track.

### GLO — mix

- White keys **1–4 mute** tracks 1–4 (a muted track fades out in a few ms and plays no new notes; its pattern runs on in time), keys **5–8 solo** them (several solos add up). The tiles show what is heard.
- The last white key (**G5**): **tap tempo** (two taps or more).
- **KNOB 1–4: the levels** of tracks 1–4.

## Undo, clear, save, autosave

- **Undo / redo:** hold EDIT, press OCT− / OCT+. One level: the last recording pass, erase, clear, step or pattern edit; redo takes it back again.
- **Clear a track:** hold REC. After 0.7 s the press is cancelled and a ring fills; keep holding ~1.3 s more and the selected track is cleared (*TRACK 2 CLEARED*). Let go before: nothing. Undo brings it back.
- **Save:** SAVE + keys 5–8 save the loop into section / project A–D (= SLOT 1–4); SAVE → PROJECT has SLOT, LOAD, SAVE too.
- **Autosave:** when the transport is stopped and you have not touched anything for 2.5 s (at most every 20 s), the working project is kept in flash; at power-on SLOOP comes back exactly as you left it.
- **New project:** SAVE → TOOLS → NEW (turn to GO): the four tracks back to their power-on sounds, empty patterns (undoable).

## Master: DUST, DUCK, FILT

On the whole mix, after the tracks' sends (FX + KNOB 1–3, or GLO → MASTER):

- **DUST** 0–100 %: the mix through an old sampler and a record — drive into a soft clip, a lower sample rate (down to ~11 kHz), fewer bits (down to 8), a low-pass closing to ~3 kHz, and while the transport plays a little hiss and crackle (a stopped SLOOP is silent).
- **DUCK** 0–100 %: every kick pumps the synth tracks down and back over an 1/8 note — the sidechain sound, in time at any tempo.
- **FILT**: a DJ filter. Left of centre a low-pass closing, right a high-pass opening, centre OFF. It glides (no zipper noise).
- **ROLL** (GLO → MASTER): the note-repeat rate of ARP + key.

## Punch-in effects

Hold **FX**, then hold a white key — the 16 white keys from the lowest F to the highest G. The effect runs on the whole mix while the key is held and lets go cleanly when you release it. Loops and the gate are locked to the tempo and start on the grid.

| Key | Effect | Key | Effect |
| --- | --- | --- | --- |
| 1 | loop 1/4 | 9 | low-pass sweep |
| 2 | loop 1/8 | 10 | high-pass sweep |
| 3 | loop 1/16 | 11 | phone |
| 4 | loop 1/32 | 12 | bit crush |
| 5 | stutter (1/16 triplets) | 13 | alias (sample-rate drop) |
| 6 | reverse | 14 | gate 1/16 |
| 7 | tape stop | 15 | echo (dotted 1/8) |
| 8 | half speed | 16 | tape wobble |

## Screens

- **TRACKS** (HOME) — the performance view: tempo, swing, transport, bar.beat; each track with its sound, its steps, the playhead, mute / solo / rec badges and its level. Dials: *swing · level · steps · pan* (KNOB 2 on a muted track unmutes it).
- **Layers** — while a layer button is held: 16 tiles (the white keys) and the knobs' dials, in the layer's colour.
- **DRUMS** (EDIT or SEQ tapped on TRACKS with the drum track) — **grid**: the 16 sounds × 16 steps, levels as shades, ratchets as notches; dials *sound · step · hit · level*. **kit**: 16 pads that flash on every hit; dials *kit · level · reverb · pan*. EDIT / SEQ tapped switches grid ↔ kit.
- **REC READY / FREE TAKE** — while REC is armed: the tracks, then **mode**, **length** and **start** on KNOB 1–3 (4-3-2-1 during a count-in); during a free take: the seconds and the loop it makes.
- **Holds** — the ring of REC (clear) while held.
- **SONG** — the section chain.
- **Sound pages** (ENV, LFO, FX, SCL, EDIT, ARP, SEQ, GLO, SAVE) — the full synth, colour-coded.

## The sound bank

68 starting points for any style: house and techno, hip-hop, trap and plugg, drum & bass, amapiano, synthwave, lo-fi, ambient, soul. Every one is a full patch on one of the nine engines: change it, save your own (32 user presets), or load your own samples. **PRESETS** browses them on a synth track **by kind** — basses, keys, organs, pads, leads, plucks and bells, stabs — the kind shown next to the name (the engine follows); your own presets come after. Every sound is level-matched: they all come out as loud at the same LEVEL. SLOOP starts (on a new project) at **90 BPM** with *808 BOOM* on track 1, *RHODES* on track 2, *LOFI FLUTE* on track 3 and the 808 kit on track 4.

| Kind | Sounds (engine) |
| --- | --- |
| **Bass** | 808 BOOM, 808 DIRTY, 808 SLIDE, SUB BASS, PLUGG BASS — they slide between held notes, two octaves under the keys · REESE, WOBBLE, ACID 303 (the resonant acid line, sliding where notes overlap), FUNK BASS (ANALOG) · FM BASS (DIGITAL) · CZ BASS (PHASE) · FAT BASS (TRIO) · WOW BASS (VOICE) · GB BASS (LOFI) · UP BASS, DEEP BASS (SAMPLE: a real upright) |
| **Keys** | RHODES, DX RHODES, WURLI, M1 PIANO (the house piano), AFRO KEYS (afro house, amapiano), CLAV (DIGITAL) · GRAND PNO (SAMPLE: a Steinway grand; long notes fade as on the real one), DUSTY PNO, LOFI KEYS (the same grand through an old sampler) · SOFT KEYS (PHASE) |
| **Organ** | SOUL ORGAN, GOSPEL, JAZZ ORGAN, DIRTY B3, HOUSE ORGN (the 90s house organ: bass lines and chords) (WHEEL) |
| **Pad** | WARM PAD, DARK STR, ATMOS PAD (ANALOG) · SAW PAD (TRIO) · GLASS PAD (DIGITAL) · CZ STRING (PHASE) · LOFI CLOUD, VIBE HAZE (GRAIN) · CHOIR AAH, SOUL OOH (VOICE) |
| **Lead** | SUPERSAW (eight detuned saws: trance, EDM), G-FUNK LD (ANALOG) · SYNC LEAD, HOOVER (TRIO) · TALKBOX (VOICE) · GAME LEAD (LOFI) · LOFI FLUTE (SAMPLE) · FLUTE DUST (GRAIN) |
| **Pluck & bell** | TRAP PLUCK (ANALOG) · RESO PLUCK (PHASE) · PLUGG BELL, TRAP BELL, MUSIC BOX, KALIMBA, MARIMBA (DIGITAL) · VIBES (SAMPLE) · 8BIT ARP (LOFI) |
| **Stab** | MIN STAB, MIN7 STAB, RAVE STAB, DUB CHORD (dub techno, into the delay) (TRIO: one key plays the chord) · SYN BRASS (ANALOG) · CZ BRASS (PHASE) · HORN STAB, STRING STB (SAMPLE) |
| **FX** | SCRATCH — scratch, backspin, rewind across the keys · GM KIT (SAMPLE) |

The sampled sounds (SAMPLE engine, **SET**: PIANO (a grand), BASS, VIBES, HORNS, STRGS, FLUTE, SCRCH, PERC) are free recordings (CC0: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi), retuned and coloured like a record through an old sampler.

## Drum kits

37 kits — **PRESETS** on the drum track, KNOB 1 on the kit page, or the editor. 1–5 are a sampled acoustic kit (CC0 recordings of a real snare, hi-hat, toms and cymbals) and its treatments; 6–37 are synthesised, so they cost almost no memory. Every synthesised kit has 16 sounds of its own, one per white key — KICK 2 and SNARE 2 are other sounds, not the same one retuned (the long 808 in TRAP, the log drum in AMAPIANO, the rumble in TECHNO). Each sound is built like on the classic machines: a tuned body with a pitch drop and a hold before it fades, a second partial for the drum heads, a click for the attack, noise through a resonant filter, drive. Softer hits are darker as well as quieter. The levels are measured: every kit is as loud as the others, each sound at its place in the mix.

| # | Kit | Style | # | Kit | Style |
| --- | --- | --- | --- | --- | --- |
| 1 | ACOUSTIC | studio | 20 | ELECTRO | electro |
| 2 | DEEP | soft | 21 | DISCO | disco |
| 3 | TIGHT | punchy | 22 | GARAGE | UK garage |
| 4 | BRIGHT | bright | 23 | JUNGLE | drum & bass |
| 5 | DUST | dusty | 24 | DUBSTEP | bass music |
| 6 | 808 | hip hop | 25 | DEMBOW | reggaeton |
| 7 | 909 | house | 26 | AMAPIANO | amapiano (log drum) |
| 8 | 606 | acid | 27 | AFRO | afrobeat |
| 9 | 80S | 80s pop | 28 | LATIN | latin |
| 10 | VINTAGE | rhythm box | 29 | TRIBAL | tribal |
| 11 | TRAP | trap | 30 | SYNTHWV | synthwave |
| 12 | DRILL | UK drill | 31 | CHIP | chiptune |
| 13 | BOOMBAP | hip hop | 32 | ARCADE | video game |
| 14 | LO-FI | lo-fi | 33 | GLITCH | glitch |
| 15 | PHONK | phonk (melodic cowbell) | 34 | INDUSTR | industrial |
| 16 | HOUSE | house | 35 | HYPER | hyperpop |
| 17 | D.HOUSE | deep house | 36 | AMBIENT | ambient |
| 18 | TECHNO | techno | 37 | JAZZ | jazz (brushes) |
| 19 | MINIMAL | minimal | | | |

The kit is saved with projects and song sections. MIDI notes in on the drum channel (10) play the nearest of the 16 sounds.

## Your own samples (USR1–USR3)

Three slots of about 7.4 s each hold your own sounds, played by a synth track: engine **SAMPLE**, **SET** = USR1 / USR2 / USR3. Load them from the web editor, tab **Samples**:

- **Files:** up to 16 WAV per slot (any rate, mono or stereo). The note each one plays at its own speed is in its name (`KEYS_C4.wav`, C4 = 60).
- **CHOP:** open or drop a recording (WAV, MP3, AIFF…) and cut it into up to 16 chops, one per key — live with **TAP** (or the space bar) while it plays (*snap to the hit* puts each tap on its attack), **Find hits**, **Grid** or **Equal parts**; then keep the chops you want (the box on each chop, **Keep all**, **Keep none**, or **K**), set a chop's length (the slider under the chops, or drag the handle at the bottom of the wave; **Up to the next marker** undoes it) or press **Fit to slot** when they are too long together — a recording of any length works, the slot takes about 7.4 s of chops — then **Send to USR1/2/3**, or **Download WAVs** (the kept chops only, each with its own number).

## Song mode

A song is up to 16 steps of 4 sections, **A–D** (each holds the four tracks: sounds, patterns, kit). Make it live, by playing:

1. Make a loop (the verse). Hold **SAVE** and press the **5th white key** (*save A*). Change the loop (the chorus) and save it into **B** with the 6th key, a bridge into **C**, an end into **D**. Saving over a used section asks for the key again within 3 s.
2. **Play the sections live:** hold SAVE and press white key **1–4**. Playing, the section starts on the next bar, every track from its first step, always in time; stopped, it becomes the loop at once.
3. **Record the song as you play it:** SAVE + key **14** (*rec*): from the next bar, every section you play and how many bars it plays are written into the song. Press it again, or STOP, to end: *SONG PARTS 5*. It is saved by itself once you stop.
4. **Play it back:** SAVE + key **13** switches *loop* / *song*; in song mode **PLAY** plays the whole song and stops at the end (your loop is back afterwards).

The **SONG screen** (SAVE tapped on TRACKS, or SAVE + key 16) shows the chain and edits it by hand: **KNOB 1** the step, **KNOB 2** its section, **KNOB 3** its bars, **KNOB 4** the number of steps; **REC** stores the loop into the step's section; **SAVE** (tap) saves the chain; **OCT−** loop / song; **OCT+ twice** loads a section. The four sections are the four project slots.

## The web editor

Open it from the installer page, or with **`OPEN-EDITOR.bat`** (`http://localhost:8766/webapp/editor/`), in Chrome or Edge with the FM-1 on USB, and press **Connect**. It follows the device live (turn a knob on the FM-1, the editor moves).

- **Sound** — every parameter of the selected track, the engines and presets, files.
- **Sequencer** — the pattern settings and the steps. On the **drum track**: a grid of the 16 sounds × the steps, with the **kit**. Choose a **level** (GHOST, SOFT, NORM, HARD) and a **roll** (x1–x4), then click: a hit; click it again (same level and roll): cleared; Shift+click: one level louder.
- **Tracks** — the four channel strips (level, pan, mute; SOLO and REC shown as on the device).
- **Library**, **Samples** (with CHOP), **Projects**, **Settings** (GLOBAL, **MASTER**: DUST, DUCK, FILT, ROLL; DRUMS).
- **Backup** (Projects tab): **Save a backup** writes everything on the FM-1 to one file (SLOOP-backup-DATE.json): the music you are working on, the projects 1–4 (the song sections A–D), the 32 user presets, the samples USR1–3 and the settings (colours, calibration, the song order, the lights, SYNC). **Restore from a file** puts it all back — what is on the FM-1 is replaced. A damaged file is refused before anything is written, every object is checked as a load checks it, and each one is written as a save writes it (a cut-off restore never leaves half an object). Stop the song (PLAY) before restoring.

The protocol is documented in [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) (v6).

## Sound design pages

The full Felucca engine is underneath: ten synthesis engines (analog, 4-op FM, phase distortion, lo-fi chip, sampler, formant voice, three-oscillator, tonewheel organ, granular, and DX7 — six-operator FM playing real DX7 voices from four banks of 32 loaded as `.syx` in the editor's Library tab), envelopes (with a pitch punch for the 808s), LFO, arpeggiator, scales and chords, glide and voice modes, per-track drive and slicer, chorus / delay / reverb sends (a stereo chorus, a tempo delay, a stereo reverb built as a feedback delay network: dense, no metallic ring), 32 user presets, 4 projects.

## MIDI keyboards

SLOOP takes MIDI from two places at once:

- **The MIDI IN jack** (3.5 mm TRS, on the FM-1): a keyboard or a pad controller with a MIDI output, through a TRS-to-DIN MIDI adapter. If nothing plays, try the other type of adapter (type A / type B).
- **USB**, from a computer or a phone (a DAW, a MIDI routing app) or a USB MIDI host box. A USB keyboard plugged straight into the FM-1 cannot work: both are USB devices, and a USB link needs a host.

| MIDI channel | Plays |
| --- | --- |
| 1, 2, 3 | synth tracks 1, 2, 3 |
| 10 | the drum track (the nearest of its 16 sounds; GLO → DRUMS → CH changes the channel) |
| 4–16 | the selected track: set your keyboard to channel 4 and it follows ALGORITHM |

**MIDI clock in:** GLO → SYSTEM → **SYNC** = **USB** or **TRS** (INT: SLOOP's own tempo). START plays from the top, CONTINUE carries on where it stopped, STOP stops; the tempo (BPM) follows the master, and the steps follow its 24 pulses a beat, so SLOOP cannot drift away from it. When the clock stops for half a second, PLAY on the FM-1 plays at its own tempo again. SYNC is a setting of the FM-1: it stays when you load a project.

Bluetooth MIDI is not supported: SLOOP, like Felucca, never switches the radio on.

## USB audio: record on a computer

On USB the FM-1 is also an audio input, named **Felucca**: 44.1 kHz, 16-bit stereo, class compliant, so no driver is needed. In your DAW or in Audacity, choose that input and record: you get the master output, exactly what the headphones play (after DUST, DUCK and FILT; the click and the count-in too, if they are on).

**Its level: HOME menu → USB AUDIO.**

- **MASTER** (default): the recording follows the MASTER knob, as the headphones do. Keep MASTER well up while you record.
- **FULL**: a fixed level, as with MASTER all the way up, kept from clipping by the output limiter, whatever the knob. MASTER then only sets the headphones: the right choice for an audio interface or a computer input with no level control of its own.

USB AUDIO is a setting of the FM-1: it stays as you left it. MIDI, the web editor and the installer keep working on the same cable while the computer records.

- The first time, the computer sees the FM-1 as a slightly different device (MIDI + audio) and sets it up again; the MIDI port keeps its name.
- The audio input comes from Felucca 1.0 (Leo Kuroshita): the same code, adapted to SLOOP.

## Lights

Hold **HOME** for the menu: **LIGHTS**, **KEYS** and **NOTES** are together there (with **USB AUDIO**, the level of the USB audio input: see [USB audio](#usb-audio-record-on-a-computer)). PRESETS moves, **KNOB 1** sets, OCT+ steps round, OCT− closes. They are saved with the settings of the FM-1, not with a project: loading a project or NEW PROJECT does not change them.

- **LIGHTS** — OFF, LOW, MID, HIGH: every button glows at that level, so its label can be read in the dark (on a black FM-1 the labels are unreadable unlit). What is on — the page, PLAY, REC, an octave — stays at full light and still blinks as before.
- **KEYS** — OFF, C KEYS, WHITE KEYS: the Cs, or every white key, glow at the LIGHTS level too (KEYS turns LIGHTS on at LOW if it was off). Played keys and the layer landmarks keep their own light.
- **NOTES** — ON: on a synth track, the notes sounding light their keys, played live or by the sequencer (the drum track always does). By @renebohne. It works on every page and in every layer: where the keys play or erase notes (EDIT, ARP, SAVE, SCL) the notes are lit — in SCL the scale and on the drum track in EDIT the sounds of the pattern then glow dimly underneath; where the keys are tiles (FX effects, SEQ steps, GLO mute / solo) the notes glow dimly and the tiles keep their full light.

The glow is a short pulse on every scan of the panel (about 900 times a second): no flicker. LOW, MID and HIGH are 0.5, 1 and 2 µs a scan; the landmarks (keys 1, 5, 9, 13 while a layer is held) and the notes under the tiles glow at 4 µs, a lit LED is about 95 µs.

## Specifications

| | |
| --- | --- |
| Tracks | 3 synth parts (8 voices shared) + drums (16 sounds, 6 voices) |
| Sounds | 68 presets on 9 engines (browsed by kind, level-matched), 8 sampled sets (CC0), 3 slots for your own samples |
| Sequencer | 64 steps per track, own length and division each; chords with a level and ratchet per note; drums with a level and ratchet per sound; ties, slide; MPC swing 50–75 %; one sample-accurate clock for steps, arp, rolls, slicer and song (no drift) |
| Performance | layers (hold a button: keys and knobs change job): punch-in FX, erase, note repeat, step entry, key / chords, mute / solo / tap tempo |
| Drum kits | 37 (5 sampled, 32 synthesised, 16 sounds each) |
| Effects | 16 punch-in effects; master DUST, DUCK, DJ filter; per track drive, slicer, sends to a stereo chorus, a tempo delay and a stereo reverb; master limiter |
| Recording | live, quantised as heard (latency-compensated), overdub; records at once while playing; free take sets loop length and tempo, or the tempo set, from the first note or a one-bar count-in |
| Memory | undo / redo, 4 projects, 32 user presets, autosave of the working project, song of 4 sections × 16 steps × 1–64 bars |
| Audio | 44.1 kHz, fixed-point DSP; USB audio input (the master output, 16-bit stereo, class compliant) |
| MIDI | USB class-compliant in / out; TRS MIDI IN (3.5 mm jack); channels 1–3 the synths, 10 the drums, 4–16 the selected track; MIDI clock in (USB or TRS) |
| Update | over USB from the browser (package SHA-256 and CRC checked) |

## Rescue, going back, credits

- **USB rescue:** hold **OCT−** alone while switching on (*SLOOP USB RESCUE*), then install again.
- **Interrupted install:** the FM-1 stays in update mode; press Install again and it finishes. A damaged package is refused, and the FM-1 keeps waiting for a good one.
- **Back to the official firmware:** on the installer page, open **Return to the official firmware (V15)**: save a backup with the editor first, download FM-1 V15 from m-vave.com, select its FM-1.fwsc (only that exact file is accepted) and install it. M-VAVE's own updater, M-UPGRADE, works too (close every other app that uses MIDI first). To come back to SLOOP, install it again and restore your backup.
- **Credits:** SLOOP is based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments — engines, sequencer, editor and installer. Played-note key lights: @renebohne (pull request #11). TRS MIDI input buffer fix: Felucca [Salt] by ChanceTheMaker, found by keremimo. Knob reading, MIDI input, overload shedding, LED glow, key debounce, MIDI clock, the USB audio input and the return to the official firmware after Felucca 1.0 / 1.0.1. Font: Terminus (SIL OFL 1.1). Samples: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi (all CC0). PHASE: CrispyZebra (GPL). VOICE after klattsch (MIT). Interface ideas after teenage engineering's pocket operators and EP-133, Elektron's step entry and Akai's MPC (swing, note repeat, erase) — SLOOP is not affiliated with any of them.
- **Licence:** GPL-3.0, no warranty. M-VAVE and FM-1 are trademarks of their owners; SLOOP is not affiliated with them. Drum kit names describe styles; they do not refer to any product.
