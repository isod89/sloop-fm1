<p align="center"><img src="assets/logo/sloop-logo.png" alt="SLOOP" width="420"></p>

<p align="center"><b>A live groovebox firmware for the M-VAVE FM-1 — for any style.</b><br>
Free and open source (GPL-3.0), based on <a href="https://github.com/hugelton/Felucca">Felucca</a> by Leo Kuroshita / Hügelton Instruments.</p>

<p align="center">
<a href="https://isod89.github.io/sloop-fm1/"><b>Install from the browser</b></a> ·
<a href="GUIDE.md">Complete guide</a> ·
<a href="SLOOP.md">Manual</a> ·
<a href="DEMARRAGE-RAPIDE-FR.md">Guide en français</a> ·
<a href="https://isod89.github.io/sloop-fm1/webapp/editor/">Web editor</a> ·
<a href="../../releases">Releases</a> ·
<a href="../../issues">Report a bug</a>
</p>

---

SLOOP turns the FM-1 into a four-track groovebox you play live: **three synths and a drum machine** with 16 sounds on the white keys, twelve synthesis engines, 153 sounds, 37 drum kits, your own samples, a song mode you play with your hands, USB audio, MIDI in on the jack and MIDI clock — and now **physical models** (guitars, sitar, bells, hand drums), a **noise** engine, **six-operator FM with DX7 patches**, **parameter locks**, **micro timing**, **fills**, a **quick chain** of sections and the **sequencer to MIDI out**. House, techno, hip-hop, trap, drum & bass, amapiano, synthwave, lo-fi, ambient, chiptune — it does not pick a style for you. No factory patterns, nothing to load: everything you hear, you play.

> **Status:** 2.6 (27 punch-in effects, a per-track INSERT effect, tempo-synced LFO and envelopes, MIDI LEARN, live drum mutes, a bolder screen; after 2.5's two new engines, drum synth and MIDI CCs). Still a beta: install at your own risk, and please [report](../../issues) what you find. Your projects, presets, samples and settings are kept when you update, and you can go back at any time (see [Going back](#going-back)).

## Contents

1. [What's new in 2.6, 2.5 and 2.4](#whats-new-in-26)
2. [Screenshots](#screenshots)
3. [Features](#features)
4. [Install](#install)
5. [Your first beat in 60 seconds](#your-first-beat-in-60-seconds)
6. [The controls](#the-controls)
7. [The menu: settings of the FM-1](#the-menu-settings-of-the-fm-1)
8. [MIDI and USB audio](#midi-and-usb-audio)
9. [The web editor](#the-web-editor)
10. [Compatibility](#compatibility)
11. [Troubleshooting](#troubleshooting)
12. [Specifications](#specifications)
13. [Documentation](#documentation)
14. [Building and tests](#building-and-tests)
15. [Contributing](#contributing)
16. [Credits and thanks](#credits-and-thanks)
17. [Licence](#licence)

---

## What's new in 2.6

- **27 punch-in effects:** eleven more on the black keys (hold FX): KILL LOW / MID / HIGH, REWIND, OCT UP, FLANGER, AUTO-PAN, FREEZE, LOOP 8T, MONO, FADE OUT.
- **From Felucca 1.4 / 1.5 (Leo Kuroshita):** an **INSERT** effect on every track (SOFT, HARD, FOLD, FUZZ, CRUSH, PHASR, FLANG, CHOR, with dry / wet), **LFO 2** (tempo SYNC, TRIG NOTE / FREE, POL BI / UNI), **ENV SYNC** (attack, decay and release as note values), the **ANALOG filter** as LP, BP or HP, a note's **LEN** in steps and each step's **velocity**, **MIDI LEARN** (GLO + F#3), and the latched-arp fix (#191).
- **TEXTURE:** a looping bed under the mix (vinyl, tape, rain, wind, radio, room tone or your own sample), breathing slowly with the tempo, without taking a track (GLO → TEXTURE).
- **Live drum mutes:** EDIT + a black key, then the sounds' keys.
- **Sustain pedal and pitch bend** from a MIDI keyboard (the bend's range: GLO → MIDI 2).
- **Each track on its own MIDI channel** (GLO → MIDI: CH1, CH2, CH3, OTHER), in and out, for an external sequencer (drums 10, bass 2, pad 11, lead 8…). Thanks to the viewer who asked!
- **Step entry from a MIDI keyboard:** on the STEP page, the notes your keyboard plays write the step, as the FM-1's keys do.
- **Program Change, 1/4D delay, REV / ECHO THROW** (FX + OCT− / OCT+), after Felucca 1.5.1.
- **Erase a saved loop:** SAVE → PROJECT → ERASE (KNOB 2) empties a project slot, song section A–D. Thanks to the viewer who asked!
- **The web editor offline:** *Download the editor* saves it as one file that runs in Chrome or Edge without internet.
- **MIDI IN on one channel** (GLO → MIDI 2 → IN = CH 1 … 16), for a sequencer driving other gear on the same cable.
- **A bolder screen:** Terminus Bold, all in capitals.
- **No more clicks** when loading a sound over held notes, or when a filter (track, drums, master FILT) starts to close.
- **A cleaner output:** the master limiter looks ahead, so loud kicks and basses no longer crackle with MASTER turned up (headphones especially).
- **Fixed in the web editor:** the same value set on a second track (DIV 1/8 on the drums, then on track 1) is sent again.
- **The installer finds an FM-1 running CLIPPER** (or another firmware naming its USB port its own way).

Details in [SLOOP.md](SLOOP.md#new-in-26). Projects, autosaves and backups from 2.5 load as they are; a project saved in 2.6 does not open in 2.5 (keep a backup before updating). Going back to 2.5 after changing GLO → MIDI or learning CCs: restore your SYN drum kits from a backup (2.5 does not read the 2.6 settings and starts them over).

## What's new in 2.5

- **PHYS: physical models.** An eleventh engine, from Felucca 1.0 (the models of DaisySP and Mutable Instruments Rings, by Emilie Gillet): plucked and struck strings, bars and bells, drum heads, and strings with sympathetic strings that ring along. Guitars (NYLON GTR, STEEL GTR, MUTED GTR), PLUCK BASS, KOTO, BANJO, SITAR and TANPURA, PHYS HARP, CHIMES, BIG BELL, VIBRA BAR, STEEL PAN, WOOD BLOCK, CELLO BOW, GLASS BOWL, TABLA, CONGA, TIMPANI and more: 26 sounds.
- **NOISE.** A twelfth engine, from Felucca 1.0: noise from analogue to digital, through a resonant filter. WIND, RAIN, OCEAN, VINYL, HISS, RISER, NZ SNARE, BITCRUSH, RADIO, NZ ARCADE, NZ METAL.
- **153 factory sounds** (76 in 2.4): 77 new, every one level-matched. Besides PHYS and NOISE: bass guitar, mono and saw basses, harp, steel drum, glockenspiel, celesta, xylophone, tubular bell, pan flute, harmonica, accordion, a 70s string machine (ENSEMBLE), an 80s poly synth, rock and chapel organs, granular clouds (STR CLOUD, SHIMMER, DRONE, PNO FREEZE, HORN CLOUD), chip sounds (NES BASS, CHIP CHORD, WAVE LEAD…), CZ lead / pad / reso, ROBOT and VOX PAD. See [the sound bank](SLOOP.md#the-sound-bank).
- **Two engines, no memory more.** GRAIN, FM6 and PHYS keep their big per-track state in one shared area (a track plays one engine at a time), so the new engines cost no memory at all; the pitch table now takes 768 bytes instead of 8 KB. Every sound of 2.4 renders bit for bit as before.
- **The web editor: piano roll, MIDI files, song.** On the **Sequencer** page: a **length** control (1–64 steps; the steps redraw at once), a **piano roll** to draw, delete and stretch notes and chords with the mouse, and **Export MIDI / Import MIDI** for the selected track (the drum track too, as GM drum notes), with a preview that says what moves or does not fit before anything is written. A new **Song** page edits the order of the sections A–D, as the FM-1's SONG screen does. Thanks to the viewer who suggested them!
- **USB audio at 48 kHz too: record into your phone.** The FM-1's USB audio input now also runs at 48 kHz when the phone, tablet or app asks for it (many only take 48 kHz), resampled in the FM-1 (after Felucca 1.1.5). Plug it into your phone and record into a sampler app. At 44.1 kHz nothing changed.
- **Drum synth: make your own drum kits.** A new page in the web editor changes every value of the synthesised drum sounds — the tone and its pitch drop, the click, the noise (white, the 808's metal, chip, clap), the filter, drive and level — and the FM-1 plays each change at once. Four kits of your own, **SYN1–SYN4**, after the others on the drum track's KIT, stored on the FM-1, in backups and as files.
- **DX7 cartridges in one go.** Drop a .syx file on the editor's FM6 panel, tick up to 27 of its voices (the first 27 are ticked) and **Store cartridge in bank** puts them in B1, B2… in one write, asking before it replaces anything. Choose to add them to the user presets too, and the FM-1's PRESETS and SELECT find them by name; or pick them with PTCH. Thanks to the viewer who asked!
- **A delay send for the drums.** GLO → DRUMS → **DLY** (next to REV) sends the drum track into the tempo delay: echoing hats, dub snares. CC 94 on the drum channel sets it too. Thanks to the viewer who asked!
- **MIDI CCs (after Felucca 1.1.5).** A controller's knobs now set the sound: CC 7 level, 10 pan, 74 the track's filter, 71 resonance, 73 / 75 / 72 attack / decay / release, 5 glide, 91 / 93 / 94 the reverb, chorus and delay sends, on the track the channel plays (as the notes). Until now SLOOP ignored every CC.
- **Swing reads 0 to 100.** 0 is straight, 100 the strongest (a step pair played 75 / 25, MPC's 75 %); it used to read 50–75 %. The swing itself and your projects did not change.
- **SEL, as printed on the button.** The key / scale button between FX and ENV is now called SEL everywhere (its pages SEL and SEL 2, the calibration, the guides); it was written SCL. Not to be mixed up with the SELECT knob.
- **The DRUMS page's level dial** goes from ghost on the left to hard on the right (its needle followed the wrong order; the levels were right).
- **Fixed** (2.4.1, folded into 2.5): imported DX7 patches with **AMS** above 0 (about one in four) played noise and static: they now sound as on a DX7. The **click** (and the REC count-in) is heard again with your own drum kits and with the drum track muted or another track soloed: it has its own sound now. In chord mode, a key on the **STEP** page writes the whole chord it plays, not only its root.

Projects, autosaves and backups from 2.4 load as they are. Going back to 2.4: a track playing PHYS or NOISE opens with another engine there, and 2.4 does not keep the SYN kits (a drum track on SYN1–4 plays USR3+4 there), so keep a backup.

## What's new in 2.4

| | |
| --- | --- |
| **FM6: six-operator FM, DX7 patches** | A tenth engine: the classic 6-op FM of Dexed (msfa), as Felucca 1.0 ported it — 32 algorithms, a full patch per track, 8 factory patches, a bank of 27 on the FM-1. The web editor edits every operator, **imports DX7 SysEx** (one voice or a 32-voice bank) and stores a whole cartridge in the bank at once. |
| **Parameter locks** | Hold a step: **PRESETS** gives one sound parameter another value for that step only, **ALGORITHM** picks which. 24 per track, several on one step. |
| **Micro timing** | Hold a step: **KNOB 4** nudges it, half a step early to half a step late, in 1/64. |
| **Fills** | Hold a step: **OCT+** makes it *fill only* or *no fill*. GLO + key **9** plays a fill while held, key **10** on the whole next bar. |
| **Quick chain** | Hold SAVE and tap A B B C…: let go and the sections play in turn, each for its pattern's length, looped. |
| **Clock only** | GLO → MIDI 2 → **IN** = **CLOCK**: follow a DAW's clock and ignore its notes. |
| **Sequencer to MIDI out** | GLO → MIDI 2 (or SYSTEM) → **OUT** = **SEQ**: the steps, the arp and the rolls go out on USB MIDI — drive another synth, or record the notes in a DAW. |
| **Your own drum kits** | A 4th sample slot (USR4). KIT USR1–USR4 or **USR3+4** (a big kit, about 15 s) on the drum track: 16 of your sounds, one per lane, built in the editor's new **Drum kit** page (drop WAVs on pads, sorted by name; pitch, gain, length; choose where; ZIP to share). |
| **New web editor** | Rebuilt in the FM-1's own look (black, the four track colours, its pixel font): each button's pages as rows of four coloured knobs, values you drag like knobs, the steps as the device's tiles. |
| **CHORD+** | In chord mode the black keys change the chord (major ↔ minor, 7th, sus4, 9th, inversion), even while it is held; STRUM strums the chords, VLEAD voices them smoothly. After HiChord and minichord. |
| **The visualiser** | On the TRACKS screen, tap HOME: 12 full-screen styles (oscilloscope, spectrum, spectrogram, Lissajous, VU meters, circle, tape, LCD, bounce, orbit, wires, the SLOOP logo), SELECT to change. |
| **A filter per track** | One knob, low-pass to the left, high-pass to the right, on each track and the drums: FX → FILTER, or FX held + KNOB 4. Lockable on a step. |
| **Bigger values** | Pages without a graph (EDIT, VOICE, GLOBAL…) show their four values in large type in the middle of the screen. |
| **SELECT turns the pages** | On a page (LFO, FX, EDIT…), SELECT goes to the next / previous page of its group. On HOME it is still the tempo. |
| **Drums with the keys** | On the drum grid page, the keys are the steps of the sound you pick: press to set, again to clear. Picking a sound or a step plays it. |
| **Longer steps, dotted delays** | DIV goes to **1/2**, **1 bar** and **2 bars**; the delay has **1/8 dotted** and **1/16 dotted**. |
| **Fixes** | A MIDI START during the REC count-in records at once; swing leaves triplets alone; turning DIV right after PLAY no longer skips a step; the SEQ layer's DIV reaches 1/2, 1 bar, 2 bars; the editor never writes flash while the song plays; *12 kHz* reads in full. |

Projects, autosaves and backups from 2.3 load as they are. The full list, and what came before: [SLOOP.md](SLOOP.md#new-in-24).

## What's new in 2.3

| | |
| --- | --- |
| **USB audio** | On USB the FM-1 is also an audio input (*Felucca*, 44.1 or 48 kHz stereo, no driver): record it in your DAW, in Audacity or in a sampler app on your phone over the same cable. Its level follows the MASTER knob, or stays at a fixed full level (HOME menu → **USB AUDIO**). From Felucca 1.0. |
| **MIDI keyboard on the jack** | The 3.5 mm TRS MIDI IN works: a keyboard or a pad controller through a TRS-to-DIN adapter. Channels 1–3 the synths, 10 the drums, 4–16 the selected track. No note left hanging. |
| **MIDI clock in** | GLO → SYSTEM → **SYNC** = USB or TRS: SLOOP follows a DAW or a drum machine — tempo, START, CONTINUE, STOP — pulse by pulse, with no drift. |
| **Record your way** | The REC screen has three dials: **mode** (*free*: the tempo follows your playing, or *tempo*: the tempo you set), **length** (1, 2 or 4 bars), **start** (your first note, or a one-bar **count-in** after PLAY). |
| **Lights for playing in the dark** | HOME menu: **LIGHTS** (every button glows, LOW / MID / HIGH), **KEYS** (the C keys or every white key), **NOTES** (the notes playing light their keys, by @renebohne). |
| **Backup and restore** | The web editor saves everything on the FM-1 in one file — the music in progress, projects, user presets, samples, settings — and puts it all back. |
| **CHOP, any length** | A recording longer than a sample slot (about 7 s) works: keep the chops you want, shorten them, or **Fit to slot**. |
| **Back to the official firmware** | From the installer page, with a backup first: select M-VAVE's FM-1 V15 file and install it. |
| **Steadier** | Knobs that answer every click; no dropped notes over USB MIDI; one voice fades on overload, never the bass or the lead; keys a millisecond faster; no stuck note after a VOICE change; stricter checks of what is read back from flash. |

The full list, and what came in 2.0, 2.1 and 2.2: [SLOOP.md](SLOOP.md#new-in-23). Release notes: [Releases](../../releases).

## Screenshots

<p align="center"><img src="assets/screens/sloop-2.6-screens.png" alt="SLOOP 2.6 screens on the FM-1" width="760"></p>

<p align="center"><sub>The FM-1's screen in 2.6, in its bold font: the tracks playing, the 27 punch-in effects (FREEZE held, REV THROW on), an INSERT on a track, LFO 2 synced to the tempo, a note's LEN on the STEP page, the TEXTURE bed, every track's MIDI channel (GLO → MIDI, MIDI 2) and MIDI LEARN.</sub></p>

<p align="center"><img src="assets/screens/editor-sound.png" alt="SLOOP web editor: the Sound page with INSERT, ENV SYNC and LFO 2" width="760"></p>

<p align="center"><sub>The web editor's Sound page in 2.6: the INSERT and its dry / wet, ENV SYNC, LFO 2 (SYNC, TRIG, POL).</sub></p>

<p align="center"><img src="assets/screens/editor-midi-texture.png" alt="SLOOP web editor: MIDI and TEXTURE settings" width="760"></p>

<p align="center"><sub>Settings: each track's MIDI channel, the other channels and the bend range; the TEXTURE bed.</sub></p>

<p align="center"><img src="assets/screens/sloop-screens.png" alt="SLOOP screens on the FM-1" width="760"></p>

<p align="center"><sub>And the rest, in 2.6's look: the drum grid and kit, a step held in the SEQ layer (its note, velocity, ratchet and a lock), the GLO mix layer, the SAVE layer's song sections and chain, PHYS, FM6, chords on the keys (SEL), the visualiser.</sub></p>

<p align="center"><img src="assets/screens/editor-drumsynth.png" alt="SLOOP web editor: the Drum synth page" width="760"></p>

<p align="center"><sub>The web editor's Drum synth page: every value of every drum sound, heard on the FM-1 at once.</sub></p>

<p align="center"><img src="assets/screens/editor-cartridge.png" alt="SLOOP web editor: a DX7 cartridge on the FM6 panel" width="760"></p>

<p align="center"><sub>A 32-voice DX7 cartridge dropped on the FM6 panel: 27 voices ticked, into the bank in one write, and as user presets if you choose.</sub></p>

<p align="center"><img src="assets/screens/editor-pianoroll.png" alt="SLOOP web editor: the piano roll" width="760"></p>

<p align="center"><sub>The piano roll: draw, delete and stretch notes and chords with the mouse; Export MIDI / Import MIDI next to it.</sub></p>


## Features

### Play it live: hold a button, touch a key

Every function button is a **layer**: hold it and the 16 white keys and the four knobs change job, and the screen shows how. Tap it and its pages open. Hold a layer button and tap HOME to **lock** it open, both hands free.

| Hold | The white keys | KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- |
| **FX** — punch | 27 punch-in effects on the whole mix. White keys: loops 1/4–1/32, stutter, reverse, tape stop, half speed, filter sweeps, phone, bit crush, alias, gate, echo, tape wobble. Black keys (2.6): kill low / mid / high, rewind, octave up, flanger, auto-pan, freeze, loop 1/8T, mono, fade out | FILTER · DUST · DUCK · the track's filter |
| **EDIT** — erase | erase a sound or a note as the loop plays (stopped: from the whole pattern); a black key: live drum mutes (2.6) | SHIFT · LENGTH ×2 / ½ · TRANSPOSE |
| **ARP** — roll | note repeat on the grid, recorded as ratchets | RATE (1/8 … 1/64) |
| **SEQ** — steps | the 16 steps of the page; a step held: a level, a ratchet, a nudge, parameter locks (PRESETS / ALGORITHM), a fill condition (OCT+) | SOUND / NOTE · DIV · SWING · LENGTH (a step held: SOUND / NOTE · LEVEL or, synths, VELOCITY · RATCHET · NUDGE) |
| **SEL** — key | the key of the song | CHORD · SCALE · KEYS · TRANSPOSE |
| **GLO** — mix | 1–4 mute, 5–8 solo, 9 fill (held), 10 fill on the next bar, 16 tap tempo; F#3: MIDI LEARN (2.6) | the levels of tracks 1–4 |
| **SAVE** — song | 1–4 play sections A–D (several tapped while held: a quick chain), 5–8 save the loop into them, 13 loop / song, 14 record the song, 16 the chain | — |

Keys 1, 5, 9 and 13 glow dimly while a layer is held: the first key of each row of the 4 × 4 grid on the screen. **EDIT + OCT− / OCT+** is undo / redo.

### Drums

- **16 sounds on the white keys**, kick to cowbell; a black key doubles the white key on its left (fast rolls with two fingers).
- **Ghost and hard hits:** hold OCT− / OCT+ while you play. Every hit keeps its level (GHOST, SOFT, NORM, HARD) and a **ratchet** (x1–x4).
- **37 kits**, all level-matched: a sampled acoustic kit in 5 treatments (CC0 studio recordings) and 32 synthesised kits — 808, 909, 606, 80s, vintage, trap, drill, boom bap, lo-fi, phonk, house, deep house, techno, minimal, electro, disco, UK garage, jungle, dubstep, reggaeton, amapiano, afrobeat, latin, tribal, synthwave, chiptune, arcade, glitch, industrial, hyperpop, ambient, jazz brushes. Each synthesised sound is built like on the classic machines; softer hits are darker as well as quieter. **Your own kits** too (2.4): KIT USR1–USR4 (or USR3+4, about 15 s) plays sample slots you fill in the editor's Drum kit page, a sound of yours per lane. And (2.5) **SYN1–SYN4**, synthesised kits you make in the editor's Drum synth page.
- **Grid and kit pages** on the FM-1 (EDIT or SEQ on the drum track): on the grid, the keys are the steps of the sound you pick (and you hear it); a 16-lane grid in the web editor.

### Synths and sounds

- **Twelve engines:** analog, 4-op FM, **6-op FM with DX7 patches (FM6, 2.4)**, phase distortion, three-oscillator, tonewheel organ, formant voice, granular, lo-fi chip, sampler, **physical models (PHYS, 2.5)**, **noise (NOISE, 2.5)**.
- **153 sounds, browsed by kind** — basses (sliding 808s, acid 303, reese, FM, bass guitar), guitars, sitar and koto (physical models), keys (Rhodes, a real Steinway grand, house and afro keys), organs and accordion, pads (string machine, granular clouds), leads (supersaw, talkbox, pan flute, harmonica), plucks and bells (harp, steel drum, glockenspiel, celesta, xylophone), stabs and dub chords — every one level-matched. **32 slots** for your own presets.
- Envelopes (with a pitch punch for 808s; 2.6: in note values with ENV SYNC), LFO (2.6: tempo-synced, free-running or unipolar), arpeggiator, glide and voice modes (POLY, MONO, LEGATO, UNISON), per-track drive, **INSERT effect** (2.6: soft / hard clip, wavefolder, fuzz, bit crush, phaser, flanger, chorus) and slicer, ANALOG's filter as LP, BP or HP (2.6), sends to a **stereo chorus**, a **tempo delay** (dotted 1/8 and 1/16 too) and a **stereo reverb**.
- **Key and chords (SEL):** the key of the song for all synths, 16 scales, one-key chords (triad, 7th, 9th, sus4, power), keys snapped to the scale or the scale on the white keys.

### Recording and the sequencer

- **Records as you play, no click needed:** while it plays, REC records at once and every pass is added on top (overdub). Notes land where you heard them: the ~12 ms of the keys are taken back.
- **The REC screen (2.3):** **mode** *free* — no tempo, no grid: play, press REC on the "1" after your last bar, and the loop's length sets the tempo — or *tempo* — record at the tempo you set; **length** 1, 2 or 4 bars; **start** on your first note, or after a one-bar **count-in**.
- **Hold REC ~2 s** to clear a track; **undo / redo** brings it back.
- **64 steps per track**, each track with its own length and division, 1/32 to two bars (polymeters stay in phase); chords up to 4 notes a step, ties, slide; **MPC swing**, 0 (straight) to 100; one sample-accurate clock for everything: no drift at any tempo.
- **Parameter locks, micro timing and fills (2.4):** hold a step to give it its own value of any sound parameter, to nudge it off the grid, or to make it play only (or never) during a fill.
- The PLAY light flashes on every beat (a visual metronome); an audible click is in GLO → GLOBAL → CLICK and is never recorded.

### Songs

Save up to four sections **A–D** (SAVE + keys 5–8), play them live on the next bar (SAVE + keys 1–4), and **record the song as you play it** (SAVE + key 14). Keep SAVE held and tap several sections for a **quick chain** (2.4). In song mode PLAY plays the whole chain. The SONG screen edits it by hand.

### Effects and master

- **27 punch-in effects** (FX + a key, white or black), locked to the tempo: DJ kills, rewind, reverb freeze and more.
- **Master:** **DUST** (an old sampler and a record: bits, rate, crackle), **DUCK** (the kick pumps the synths), **FILT** (a DJ filter: low-pass ← off → high-pass), and an output limiter.
- **TEXTURE (2.6):** a looping bed under the mix: vinyl, tape, rain, wind, radio, room tone or one of your samples, with level, tone and a slow breath over 1–16 bars.

### Your own samples

- **Four slots** (USR1–USR4) of about 7.4 s each, played by a synth track or as a drum kit (USR3+4: one kit of about 15 s); up to 16 WAV files per slot, each on its own key.
- **CHOP** in the web editor: open or drop a recording (WAV, MP3, AIFF…) of **any length**, tap along while it plays (each tap snaps to its hit), or find the hits, a tempo grid, equal parts; keep the chops you want, shorten them or **Fit to slot**; send them to a slot, one per key, or download them as WAV files.

### MIDI

- **USB MIDI** in and out, class compliant.
- **TRS MIDI IN** (the 3.5 mm jack, 2.3) for a keyboard or a pad controller.
- **MIDI clock in** (USB or TRS): tempo, START, CONTINUE, STOP.
- **MIDI CCs** (2.5): level, pan, filter, resonance, envelope, glide and the sends from a controller's knobs; **MIDI LEARN** (2.6): any knob of yours onto any parameter of a track.
- **MIDI out:** the keys always; the sequencer, the arp and the rolls too with GLO → MIDI 2 → OUT = SEQ (2.4).
- Details: [MIDI and USB audio](#midi-and-usb-audio).

### USB audio (2.3)

The FM-1 is also a **USB audio input**: record its master output on a computer, no driver, over the same cable as MIDI and the editor. Details: [USB audio](#usb-audio-record-the-fm-1-on-a-computer).

### Lights for playing in the dark (2.3)

Every button can glow so its label is readable on a black FM-1; the C keys or every white key can glow too; the notes playing can light their keys. Details: [The menu](#the-menu-settings-of-the-fm-1).

### Memory and safety

- **Autosave:** stop and leave it 2.5 s, your work is kept; at power-on SLOOP comes back exactly as you left it.
- **4 projects**, **32 user presets**, **27 FM6 patches**, **undo / redo**.
- **Full backup and restore** from the web editor (2.3).
- **Safe updates:** the installer checks the package (SHA-256) before writing it, the update loader checks it again (CRC) before starting it; an interrupted install finishes when you press Install again; **USB rescue** (OCT− at power-on).

## Install

### From the browser (recommended)

1. Open **[the SLOOP installer](https://isod89.github.io/sloop-fm1/)** in **Chrome or Edge** on a computer.
2. Connect the FM-1 by USB — a **data** cable, directly (no hub).
3. Press **INSTALL**, allow MIDI access, and wait for *Done*. The FM-1 restarts on the SLOOP logo.

Nothing to download or compile. Your projects, user presets, samples and settings are kept. After an install, **unplug and plug the FM-1 back in** once so the computer finds its USB audio input.

### Other ways

- **Python:** the `.fwsc` of a [release](../../releases) with `python tools/fm1_install.py sloop-2.4.fwsc` (needs `pip install mido python-rtmidi`).
- **Build it yourself:** see [Building and tests](#building-and-tests); on Windows, `INSTALL-SLOOP.bat` builds SLOOP and opens the installer locally.

### Going back

- **To an earlier SLOOP:** install the `.fwsc` of its [release](../../releases) (for example [2.3](../../releases/tag/v2.3)). Your samples, user presets and settings stay. **Projects saved by 2.4** (format FUN5: locks, nudges, fills) **do not open in 2.3 or older** — 2.3 sees them as empty, and its autosave can write over the music in progress; a 2.4 backup restores into 2.4 only. **Save a backup before you go back**, and restore it once you are on 2.4 again.
- **To the official firmware:** on the installer page, open **Return to the official firmware (V15)**. Save a backup with the editor first, download FM-1 V15 from m-vave.com and select its `FM-1.fwsc` — only that exact file is accepted. M-VAVE's updater, M-UPGRADE, works too. To come back, install SLOOP again and restore your backup.

### Rescue

- **The FM-1 no longer starts SLOOP:** hold **OCT−** alone while switching it on (*SLOOP USB RESCUE*), then install again.
- **An install was cut off:** the FM-1 stays in update mode; press INSTALL again and it finishes.
- If an FM-1 no longer starts at all, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

> Custom firmware is installed at your own risk. No warranty.

## Your first beat in 60 seconds

1. **ALGORITHM** to track **4** (orange, drums). The white keys are 16 drum sounds; **PRESETS** picks a kit (try *808* or *BOOMBAP*).
2. Press **REC** and play a beat freely, at your own tempo. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard ones.
3. **Press REC on the "1" after your last bar.** The loop closes, its length sets the tempo, the hits snap to the grid and it plays at once. (Prefer a set tempo, or a count-in? Turn KNOB 1 and KNOB 3 on the REC screen before you start.)
4. **REC** again while it plays: you record on top. Hold **ARP** and hold the hat key for a hat roll.
5. **ALGORITHM** to track **1**, **REC**, play a bass line. Hold **SEL** and press the key of your song; on track 2, hold SEL and turn **KNOB 1** to *7TH*: every white key now plays a chord.
6. Hold **FX** and press a key (white or black) for a punch-in effect; still holding FX, turn **KNOB 2** for DUST, **KNOB 3** for DUCK.
7. A mistake? Hold **EDIT** and press **OCT−**: undo.

## The controls

| Control | What it does |
| --- | --- |
| **MASTER** | volume (and the USB audio level, if USB AUDIO is on MASTER) |
| **SELECT** | on HOME and inside a layer: tempo · on a page: the previous / next page of its group · DRUMS: grid / kit |
| **ALGORITHM** | the selected track: 1 · 2 · 3 (synths) · 4 (drums) |
| **PRESETS** | the selected track's sound, or the drum kit |
| **KNOB 1–4** | what the four dials at the bottom of the screen show, each in its colour |
| **OCT− / OCT+** | octave (both: back to 0) · on the drum track, held: ghost / hard hits |
| **FX · SEL · ENV · LFO · EDIT · GLO** (top row) | tap: their pages · hold FX, SEL, EDIT, GLO: a layer. **SEL** is the second button of the top row, between FX and ENV (not the SELECT knob) |
| **HOME** | the TRACKS screen · on it: the full-screen visualiser (SELECT: 12 styles; HOME again closes it) · hold: the menu · tapped while a layer is held: lock it |
| **SAVE** | on TRACKS: the SONG screen · elsewhere: the SAVE pages · hold: the song layer |
| **ARP · SEQ** | tap: their pages · hold: note repeat · steps |
| **PLAY** | start / stop all four tracks; its light flashes on every beat |
| **REC** | playing: record now / stop · stopped: arm (the REC screen) · hold: clear the track |
| **EDIT + OCT− / OCT+** | undo / redo |

Colours: **blue** track 1 and KNOB 1, **green** 2, **yellow** 3, **orange** 4 (drums). White is what you touch; red is recording.

## The menu: settings of the FM-1

Hold **HOME**. The menu is in four sections, as the pages are: **SCREEN** (COLOR, ZOOM), **LIGHTS** (LIGHTS, KEYS, NOTES), **AUDIO** (LOWCUT, USB AUDIO, USB SERIAL), **SYSTEM** (HARDWARE CALIBRATION, ABOUT). **SELECT** goes from one section to the next, **KNOB 1, 2, 3** set the section's rows (each row shows its knob's colour), **PRESETS** moves the cursor, **OCT+** steps the cursor's setting round or opens it (CALIBRATION, ABOUT), **OCT−** closes. These are settings of the FM-1, not of a project: loading a project or NEW PROJECT does not change them, and the backup keeps them.

| Item | Choices | What it does |
| --- | --- | --- |
| **COLOR** (SCREEN) | 5 palettes | the screen's colours |
| **ZOOM** (SCREEN) | OFF / ON | a large readout of the value you turn |
| **LIGHTS** (LIGHTS) | OFF / LOW / MID / HIGH | every button glows at that level; what is active stays at full light |
| **KEYS** | OFF / C KEYS / WHITE KEYS / ALL KEYS | the C keys, every white key, or every key glow too |
| **NOTES** | OFF / ON | the notes playing on the selected synth track light their keys (sequencer notes too, at least a tenth of a second each), on every page and in every layer |
| **SPEAKER LOWCUT** (AUDIO) | OFF / ON | a low cut for the small built-in speaker |
| **USB AUDIO** | MASTER / FULL | the level of the USB audio input: follows the MASTER knob, or a fixed full level |
| **USB SERIAL** | OFF / ON | a serial console for developers; OFF (default) so macOS 13–15 show the USB audio input; at the next start |
| **HARDWARE CALIBRATION** (SYSTEM) | | the panel table, if a key or a knob answers wrongly |
| **ABOUT** | | the version (*SLOOP 2.4*) and its build date, the credits |

Two more settings of the FM-1 live elsewhere: **SYNC** (GLO → SYSTEM: INT, USB or TRS) and the REC screen's **mode** and **start**.

## MIDI and USB audio

### MIDI in

SLOOP takes MIDI from two places at once:

- **The MIDI IN jack** (3.5 mm TRS): a keyboard or a pad controller with a MIDI output, through a **TRS-to-DIN MIDI adapter**. If nothing plays, try the other adapter type (A / B).
- **USB**, from a computer or a phone (a DAW, a MIDI routing app) or a USB MIDI host box.

| MIDI channel | Plays |
| --- | --- |
| 1, 2, 3 | synth tracks 1, 2, 3 (2.6: GLO → MIDI → CH1, CH2, CH3 set any channel for each) |
| 10 | the drum track (the nearest of its 16 sounds; GLO → MIDI → CH4 changes the channel) |
| the others | the selected track: set your keyboard to channel 4 and it follows ALGORITHM (2.6: GLO → MIDI → OTHER = OFF ignores them) |

**Your own channels (2.6).** For an external sequencer with its own channels (drums 10, bass 2, pad 11, lead 8…): GLO → **MIDI** sets each track's channel (CH1, CH2, CH3, CH4 the drums: OFF or 1–16, in and out; two tracks never share one) and GLO → **MIDI 2** the rest: **IN**, **OTHER** (SEL: the channels no track has play the selected track; OFF: ignored), **OUT**, **BEND**. Notes, CCs and MIDI out follow. Settings of the FM-1, kept through power-off.

Your sequencer uses the other channels for other gear? GLO → MIDI 2 → **IN = CH 1 … CH 16** (2.6): SLOOP then answers that one channel only, on the selected track, and ignores the rest.

A USB keyboard plugged **straight into the FM-1** cannot work: both are USB devices, and a USB link needs a host (a computer, a phone, or a USB MIDI host box). Bluetooth MIDI is not supported: SLOOP, like Felucca, never switches the radio on.

### MIDI out

What you play on the keys always goes out on USB MIDI, each track on its channel (1–3 the synths, 10 the drums). GLO → MIDI 2 (or SYSTEM) → **OUT** = **SEQ** sends what the sequencer, the arp and the rolls play too: every note is ended, STOP ends whatever was still on, and notes that came in from a computer or the jack are never sent back. Record the FM-1's MIDI in a DAW and you get your pattern as notes.

### MIDI clock in

GLO → SYSTEM → **SYNC** = **USB** or **TRS** (INT: SLOOP's own tempo). START plays from the top, CONTINUE carries on, STOP stops; the tempo follows the master and the steps follow its 24 pulses a beat, so SLOOP never drifts. When the clock stops for half a second, PLAY on the FM-1 plays at its own tempo again.

**Clock only (2.4):** GLO → MIDI 2 → **IN** = **CLOCK**: SLOOP follows the clock and ignores incoming notes (and CCs), handy when a DAW or a sequencer also sends notes to other gear.

### MIDI CCs (2.5)

The knobs of a MIDI controller set the sound, on the track the channel plays (as the notes: 1–3 the synths, 10 the drums, 4–16 the selected track). A CC sets its parameter as a knob would, 0–127 over its range. The standard map of Felucca 1.1.5:

| CC | Sets |
| --- | --- |
| 5 | GLIDE |
| 7 | LEVEL (on the drum channel: GLO → DRUMS → LVL) |
| 10 | PAN |
| 71 | the engine's resonance (RES or Q: ANALOG, TRIO, VOICE, NOISE; the others ignore it) |
| 72 · 73 · 75 | release · attack · decay |
| 74 | the track's FILTER: 64 off, lower a low-pass, higher a high-pass (every engine, the drums too) |
| 91 · 93 · 94 | the reverb, chorus and delay sends (on the drum channel 91 and 94 are GLO → DRUMS → REV and DLY) |

Other CCs (mod wheel…) are ignored. From 2.6 the **sustain pedal** (CC 64) and **pitch bend** work too (the bend's range: GLO → MIDI 2 → BEND, 1–12 semitones).

**MIDI LEARN** (2.6, after Felucca 1.5): **GLO + F#3**, turn the knob of SLOOP you want, move a knob of your controller; GLO + F#3 again to finish. A learned CC sets that track's parameter on any channel, in place of the map above (up to 16; kept through power-off). HOME menu → SYSTEM → **MIDI LEARN CLEAR** forgets them. Details: [GUIDE.md](GUIDE.md#midi-learn-26).

### USB audio: record the FM-1 on a computer

On USB the FM-1 is also an **audio input named "Felucca"**: 44.1 or 48 kHz (2.5: phones and apps that only take 48 kHz), 16-bit stereo, class compliant — no driver on Windows, macOS or Linux. Choose it in your DAW or in Audacity and record: you get the master output, exactly what the headphones play (after DUST, DUCK and FILT). MIDI, the web editor and the installer keep working on the same cable.

- **The level:** HOME menu → **USB AUDIO**. **MASTER** (default): the recording follows the MASTER knob, as the headphones do — keep MASTER well up while you record. **FULL**: a fixed level, as with MASTER all the way up, kept from clipping by the limiter; MASTER then only sets the headphones (the right choice for an interface with no level control).
- **The first time** (and after an install), the computer sets the FM-1 up again as a MIDI + audio device: unplug and plug it back in if the input does not show. The MIDI port keeps its name.
- The audio input comes from Felucca 1.0.

## The web editor

Open it from the [installer page](https://isod89.github.io/sloop-fm1/) (or the [editor link](https://isod89.github.io/sloop-fm1/webapp/editor/)) in Chrome or Edge, with the FM-1 on USB, and press **Connect**. It follows the device live: turn a knob on the FM-1 and the editor moves.

**Offline (2.6).** *Download the editor* (under Connect, or on the installer page) saves the whole editor as one file, `sloop-editor-2.6.html`, also attached to each [release](https://github.com/isod89/sloop-fm1/releases). Double-click it: it opens in Chrome or Edge without internet (Chrome asks once to allow MIDI) and works as the online one. Its sound library (kept in the browser) is its own, apart from the online editor's: move sounds between them with Export / Import, or a backup.

- **Sound** — every parameter of the selected track, the engines and the presets; on an FM6 track the **FM6** panel: every operator, the bank, **Import / Export SysEx** (DX7 voices and banks).
- **Sequencer** — the steps, as tiles or as a **piano roll**, and the track's length (1–64 steps); on the drum track a grid of 16 sounds × the steps, with levels and ratchets, and the kit. Click a step for its nudge, parameter locks and fill condition. **Export MIDI / Import MIDI**: the selected track as a MIDI file, and back.
- **Tracks** — the four channel strips.
- **Song** — the order of the sections A–D (the SONG screen): which section, how many bars, how many times, and whether the song starts again at the end.
- **Library** — your user presets and preset files.
- **Samples** — the three user slots, files and **CHOP**.
- **Drum synth** (2.5) — your own synthesised kits SYN1–SYN4: every value of every drum sound, heard at once.
- **Projects** — the four projects, and **Backup**: *Save a backup* writes everything on the FM-1 to one file (with the FM6 bank); *Restore from a file* puts it all back (stop playback first).
- **Settings** — global, master (DUST, DUCK, FILT, ROLL), drums.

<p align="center"><img src="assets/screens/editor-backup.png" alt="SLOOP web editor: projects and backup" width="560"></p>

The protocol is documented in [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md).

## Compatibility

| | |
| --- | --- |
| Device | M-VAVE FM-1 (the official firmware can be put back at any time) |
| Installer and editor | **Chrome or Edge** on Windows, macOS or Linux (they use Web MIDI with SysEx) |
| Cable | a USB **data** cable, plugged directly (no hub) |
| USB audio | any computer that takes a class-compliant USB audio input (no driver) |
| MIDI IN jack | 3.5 mm TRS, through a TRS-to-DIN MIDI adapter (type A or B) |
| Not supported | Bluetooth MIDI; a USB keyboard plugged straight into the FM-1 |

## Troubleshooting

**The installer or the editor does not find the FM-1.** Use Chrome or Edge, a data cable, no hub, and allow MIDI access. Close every other app or tab that uses MIDI (a DAW, M-UPGRADE, another editor tab), then reload the page.

**An install stopped half-way.** The FM-1 waits in update mode: press INSTALL again. If SLOOP no longer starts, hold **OCT−** alone while switching on (*SLOOP USB RESCUE*) and install again.

**The black keys make no sound on a synth track.** That track plays chords or the scale on the white keys (in chord mode the black keys change the chord: CHORD+): hold **SEL** (between FX and ENV) and set **KNOB 1 CHORD** to OFF and **KNOB 3 KEYS** to OFF. The drum track always uses the black keys.

**Nothing plays from the MIDI IN jack.** Try the other adapter type (A / B); check the keyboard's channel (1–3 synths, 10 drums, the others the selected track; 2.6: GLO → MIDI sets them).

**The USB audio input does not show.** Unplug the FM-1 and plug it back in (after an install the computer must find it again). On a Mac (macOS 13–15), check that HOME menu → **USB SERIAL** is OFF (the default), then restart the FM-1. In Audacity: Transport → Rescan Audio Devices. On Windows: Sound settings → Recording → show disabled devices.

**The USB recording is too quiet, or follows the volume knob.** Set HOME menu → **USB AUDIO** to **FULL**, or turn MASTER up.

**Recorded notes move to the grid.** SLOOP quantises what you record to the steps of the track (its **DIV**: 1/4 … 1/32, triplets). For finer timing, set DIV to 1/32, or hold the step and nudge it (KNOB 4); for groove, use SWING.

**Notes fade out on a dense part.** The processor is at its limit: SLOOP fades one voice at a time (never the bass or the lead) rather than glitching. Fewer held notes or a lighter engine help.

**An FM6 sound changed after loading a project.** A project keeps the track's patch slot (PTCH), not the patch itself: store a patch you edited in the bank (editor → FM6 → Store in bank) and set PTCH to it.

**The lights or SYNC went back to OFF / INT.** You went back to an earlier SLOOP, which does not keep them; set them again in 2.4.

Something else? [Open an issue](../../issues): what you did, what you expected, what happened, and the version shown in HOME menu → ABOUT.

## Specifications

| | |
| --- | --- |
| Tracks | 3 synth parts (8 voices shared) + drums (16 sounds, 6 voices) |
| Sounds | 153 presets on 12 engines (browsed by kind, level-matched), 6-op FM with DX7 SysEx import and a 27-patch bank, 8 sampled sets (CC0), 4 slots for your own samples (USR1–USR4), 32 user presets |
| Drum kits | 37 (5 sampled, 32 synthesised, 16 sounds each), level-matched, plus your own: USR1–USR4 and USR3+4 (about 15 s) |
| Sequencer | 64 steps per track, own length and division each (1/32 to two bars); chords with a level and ratchet per note; drums with a level and ratchet per sound; ties, slide; per-step parameter locks (24 per track), micro timing (±½ step in 1/64) and fill conditions; swing 0 (straight) to 100 (MPC 75 %); one sample-accurate clock (no drift) |
| Recording | live, quantised as heard (latency-compensated), overdub; free take (the tempo follows you) or the tempo set; start on the first note or a one-bar count-in; 1, 2 or 4 bars |
| Performance | layers: punch-in FX, erase, note repeat, step entry, key / chords, mute / solo / fill / tap tempo, song sections and a quick chain of up to 8 |
| Effects | 27 punch-in effects; master DUST, DUCK, DJ filter, limiter; per track drive, slicer, sends to a stereo chorus, a tempo delay (dotted 1/8 and 1/16 too) and a stereo reverb |
| Memory | autosave, undo / redo, 4 projects, 32 user presets, 27 FM6 patches, song of 4 sections × 16 steps × 1–64 bars, full backup / restore (editor) |
| Audio | 44.1 kHz, fixed-point DSP; USB audio input (the master output, 16-bit stereo at 44.1 or 48 kHz, class compliant) |
| MIDI | USB class-compliant in / out (the keys, or the sequencer too); TRS MIDI IN (3.5 mm); MIDI clock in (USB or TRS), clock only if you like (IN = CLOCK) |
| Lights | button backlight (3 levels), C keys / white keys, played notes |
| Update | over USB from the browser (SHA-256 and CRC checked), USB rescue, return to the official V15 |

## Documentation

- [GUIDE.md](GUIDE.md) — **the complete guide**: every button, combination, layer, page, engine and editor page, with a one-page cheat sheet
- [SLOOP.md](SLOOP.md) — the manual (every page, layer, sound and kit) and the history of each version
- [DEMARRAGE-RAPIDE-FR.md](DEMARRAGE-RAPIDE-FR.md) — guide de démarrage en français
- [BUILDING.md](BUILDING.md) — building, build options and tests
- [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) — the editor's SysEx protocol
- [LICENSING.md](LICENSING.md) — the licences of the code and the assets

## Building and tests

See [BUILDING.md](BUILDING.md). In short: the JieLi toolchain and three files of the AC79 SDK, then `./build.sh` (Linux / macOS) or `INSTALL-SLOOP.bat` (Windows with WSL), which builds the firmware and serves the installer and the editor on `http://localhost:8766`.

`tests/run_tests.sh` runs the host test suite with no hardware: audio renders against golden hashes, CPU budgets, the sequencer's timing (no drift, swing, ratchets, rolls, the REC modes and the count-in, MIDI clock), the UI pages and layers, the knobs, flash storage, the update loader, MIDI and USB audio, and the web pages (editor, backup, CHOP, installer).

## Contributing

- **Bugs and ideas:** [open an issue](../../issues) — what you did, what you expected, what happened, and the version in HOME menu → ABOUT.
- **Pull requests** are welcome. Keep the style of the code around your change, add a host test when you can, and make sure `tests/run_tests.sh` passes. Contributions are credited in the release notes and the manual.
- By contributing you agree that your code is released under GPL-3.0, like the rest of SLOOP.

## Credits and thanks

- **[Felucca](https://github.com/hugelton/Felucca)** by **Leo Kuroshita** (@kurogedelic) / **Hügelton Instruments** — the engines, the sequencer, the editor, the installer, in 2.3 the USB audio input, the MIDI clock, the knob reading and many fixes, in 2.4 the FM6 engine and its DX7 patch editor (Felucca 1.0 / 1.0.1), and in 2.6 the INSERT effects, LFO 2 (SYNC / TRIG / POL), ENV SYNC, the ANALOG filter types, note LEN and step velocity, MIDI LEARN, the live drum mutes, the latched-arp fix (#191), the 1/4D delay and REV / ECHO THROW (Felucca 1.4 / 1.4.1 / 1.5 / 1.5.1). Thank you.
- **The Felucca community, who asked for those features first** (Felucca discussions and issues): **Wulgaren** (insert effects with dry / wet, #78; the arp report, #191), **etyrnal** (the lo-fi bit effect, #177; envelopes in note values, #175), **benjaminguerrer** (LFO sync, #154), **gospodindilly-ops** (the ANALOG filter types, #104), **Armstrong1980** (#178) and **oMauNaProducer** (#173) (note length, step velocity), **lostfictions** and **planist79** (MIDI LEARN, #170).
- **@renebohne** — the played-note key lights (pull request #11) and, in 2.6, MIDI Program Change (Felucca pull request #179).
- **ChanceTheMaker** and **keremimo** — the TRS MIDI input fix (Felucca Salt) and contributions to the MIDI clock.
- **Everyone who installed SLOOP, made music with it, commented, reported a bug or asked for a feature** — most of each update comes from your messages: parameter locks, DX7 patches, dotted delays and longer steps (2.4); in 2.6 a MIDI channel per track, sustain and pitch bend, step entry from a MIDI keyboard, TEXTURE, the fixed crackles, the web editor bug with the same value on two tracks, and the MIDI learn backup. You asked, tested and reported them.
- Samples: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi (all CC0). Font: Terminus and Terminus Bold (SIL OFL 1.1). Icons: Fukiai (MIT, Hügelton Instruments). PHASE engine after CrispyZebra (GPL); VOICE after klattsch (MIT); FM6: msfa from Dexed by Google Inc. and Pascal Gauthier (Apache-2.0).
- Interface ideas after teenage engineering's pocket operators and EP-133, Elektron's step entry and Akai's MPC (swing, note repeat, erase).

## Licence

Code: GPL-3.0-only (see [LICENSE](LICENSE), and [LICENSING.md](LICENSING.md) for the assets). No warranty. M-VAVE and FM-1 are trademarks of their owners; SLOOP is not affiliated with M-VAVE, teenage engineering, Elektron or Akai. Drum kit names describe styles, not products.
