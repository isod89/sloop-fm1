# SLOOP editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c` (SLOOP is based on Felucca: the frames keep its "FL"
header). Commands 16-26 (user presets and live sync) form protocol v2; commands 27-30 (tracks) form
protocol v3; commands 31-32 (any track's parameters) form protocol v4; command 33 and the extra step,
`INFO` and `TRACK` bytes form protocol v5 (SLOOP 2.0); commands 34-36 (backup / restore) form protocol v6
(SLOOP 2.3); commands 37-40 (the steps' nudges and parameter locks) form protocol v7, commands 41-42 (the
steps' fill conditions) protocol v8 and commands 68-71 (the FM6 engine's patches) protocol v9 (all SLOOP 2.4);
command 43 (PERFORM: the live layers for a remote surface) forms protocol v11.

**v3 (four tracks):** the device has four tracks: 1..3 are synth parts, 4 is the drum track. One
of them is *selected* (the TRACKS page on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (SLOOP 2.0):** the drum track has 16 lanes (one sound per white key) with a level and a ratchet per
hit; `DRUM_STEP` reads and writes them. Synth steps carry a level and a ratchet per note. `INFO` ends with
the protocol version (5) and `TRACK` with the solo mask. Every addition is a byte appended at the end of
a reply or a request, so v1-v4 editors keep working (they see the drum lanes as GM notes, below).

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every request gets exactly one reply, with the same header and the same `<cmd>`. Requests
the device does not understand get no reply. Every data byte is 7 bit. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..3 (synth parts), 3 = the drum track |
| engine byte | 0..NENGINES−1; NENGINES = the drum track (it has no engine and no presets) |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1, and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v5) the protocol version (5); older firmware ends after the names / NTRK |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine with its defaults |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 64; firmware before SLOOP sent at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) lvl, hi, rat |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, lvl, hi, rat (v5)] | same as STEP_GET (after the write). Without the v5 bytes the step's levels and ratchets become 0 |
| 8 PRESET | engine, preset | engine, preset (applies the preset: sound, sends, arp; never the pattern, the mix or the key: `LEVEL PAN MUTE`, `LEN DIV SWG GATE`, `ROOT SCL QNT CHORD` stay) |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s |

The local Studio build may append status `1` to a PROJECT reply when playback
prevents a load/save. No operation occurred; stop playback and try again.
An absent status byte retains the original reply format.
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11 SMP_BEGIN | slot 0..2 | slot, rc (0 ok). Erases the slot's header sector: the slot is empty from now on |
| 12 SMP_WRITE | slot, offset (3 × 7 bit, LSB first), pack7 data (≤ 256 bytes) | slot, offset, rc: 0 ok, 1 arguments, 2 erase, 3 write, 4 slot in use (send SMP_BEGIN first). Offset ≥ 512 and a multiple of 256; writes go in increasing order (a write at a 4 KiB boundary erases that sector) |
| 13 SMP_END | slot, pack7 header (480 bytes) | slot, rc: 0 ok, 1 size, 2 header, 3 data CRC, 4 flash, 5 zones |
| 14 SMP_ERASE | slot | slot, rc (erases the whole slot, ~1 s) |
| 15 SMP_INFO | — | slots, slot KiB, then per slot: zone count (0 = empty), name string, data KiB |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags) |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) | slot, rc (0 ok, 1 args, 2 flash, 3 stop the song first). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc (3: stop the song first). Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies it |
| 21 UP_ERASE | slot | slot, rc (3: stop the song first) |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute. The drum track's level is global `G_DRLVL` (GLO > DRUMS LEVEL); mute is the track's `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, lvl, hi, rat] (set) | track, index, n, note0..3, time, flags, vel, then (v5) lvl, hi, rat |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v5) | Request args | Reply args |
| --- | --- | --- |
| 33 DRUM_STEP | index (get), or index, on (3 bytes), lvl (5 bytes), rat (5 bytes) (set) | index, on (3 bytes), lvl (5 bytes), rat (5 bytes): the drum track's step, whichever track is selected |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 37 LOCK_GET | track | track, n, then n × (step, param, v14 value): the track's parameter locks (n ≤ 24, unsorted) |
| 38 LOCK_SET | track, step, param [, v14 value] (no value: delete the lock of that step and parameter) | track, step, param, rc, has (0/1), v14 (the lock's value after clamping; 0 when there is none). rc: 0 ok, 1 step ≥ NSTEP or param ≥ P_COUNT, 2 the parameter cannot be locked, 3 no free slot (24 a track) |
| 39 MICRO_GET | track | track, then NSTEP bytes: each step's nudge + 64 (so 32..95; 64 = on the grid) |
| 40 MICRO_SET | track, step, nudge + 64 | track, step, nudge + 64 (after clamping to −32..31) |

| cmd (v8) | Request args | Reply args |
| --- | --- | --- |
| 41 FILL_GET | track | track, then the track's 16 condition bytes as stored (2 bits per step, step i in byte i / 4 at bits 2 (i mod 4)..+1), **pack7** (19 bytes on the wire) |
| 42 FILL_SET | track, step, cond (0 normal, 1 fill only, 2 no fill; 3 = 0) | track, step, cond (as stored) |

| cmd (v9) | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128-byte packed patch |
| 69 FM6_PUT | target, index, the 128-byte packed patch | target, index, rc |
| 70 FM6_LIST | — | factory count, bank count, then per slot (factory first): used (0/1), name string ("" if empty) |
| 71 FM6_ERASE | bank index | index, rc |

| cmd (v11) | Request args | Reply args |
| --- | --- | --- |
| 43 PERFORM | op, op args (below); no op = 0 STATE | op, rc (0 ok, 1 arguments / unknown op, else per op), then the **live state** (below) |

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j).

**User sample slot** (80 KiB each: USR1..USR3 at flash 0xA0000, 0xB4000, 0xC8000 and, from SLOOP 2.4, USR4 at
0xE7000 — SMP_INFO says how many; SAMPLE / GRAIN sets USR1..USR4, the drum track's KIT USR1..USR4 and USR3+4; reference uploader
`tools/fm1_sample_upload.py`, slot builder `sampleio.user_slot`; the editor's port of it is
checked byte for byte by `web/test_web.mjs`): header at 0, ADPCM data at 512.

| Offset | Field |
| --- | --- |
| 0 | magic `"FSMP"` (u32 0x504D5346), u16 version 1, u8 zone count 1..16, u8 0 |
| 8 | name, 8 ASCII bytes (0-padded) |
| 16 | u32 data length (bytes), u32 CRC-32 (zlib) of the data, 8 bytes 0 |
| 32 | 16 zones × 28 bytes: u32 off (in the data), n (samples), loop start, loop end, rate (Hz / 44100 × 65536); i16 root × 16 (MIDI note), ADPCM predictor at the loop start; u8 step index at the loop start, lo note, hi note, looped (0/1) |

Data is IMA ADPCM, 4 bit, low nibble first, starting from predictor 0 and step index 0.
All little endian.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | 14 | SWING |
| | | | | 15 | FILT |

v5 formats: **SWING** 0..100 shown as the MPC swing, 50 % (straight) + value / 4 (so 75 % at 100);
**FILT** −64..63: 0 OFF, below 0 a low-pass closing (LP 1..100 %), above 0 a high-pass (HP 1..100 %).
**PCT** is the share of the range: value × 100 / max (rounded).

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie). Loading one applies the engine and
the parameters of the sound; the pattern stored with it is never loaded (changing a sound never changes
the sequence), and the mix, pattern and key parameters stay (as `PRESET`). The slots are numbered 0..31
(the device shows U01..U32).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or a frame that is too short. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash; the slot is still changed in RAM until power-off.
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE
(one detent arms, a second one within ~1.5 s acts, as PROJECT LOAD / SAVE). SAVE uses the automatic
name. SELECT and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (mismatch: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; SLOOP 2.0
added CHORD (id 49): P_COUNT 58, P_E0 50 (and G_COUNT 32: DUST, DUCK, FILT, ROLL, NEW at 27..31). An
editor takes them from `INFO`; older records load with the SLICER off and CHORD off. SLOOP 2.5 adds `G_DRDLY` (id 32,
GLO > DRUMS DLY, the drums' delay send): G_COUNT 33 (a 2.4 device says 32; the editor takes both). A project keeps it in
the byte after `sel` (format 5 unchanged; 0 in older projects).

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for (`SET` of G_ENGSEL, `PRESET`, `PROJECT` load, `UP_LOAD`).
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- The drum track: `DUMP` / `RELOAD` / `TRACK` give the engine byte NENGINES. Its `P_*` values exist
  (the pattern parameters `LEN DIV SWG GATE`, `PAN`, `MUTE` and `P_E0`, the kit, are used; the rest is
  ignored). `PRESET`, `SET` of `G_ENGSEL` and `UP_LOAD` do nothing there (`UP_LOAD` and `UP_STORE` answer
  rc 1). `DESC` of `P_E0` is the enum `KIT`; `P_E1..P_E7` describe engine 0. Its steps hold the 16 drum
  lanes (v5, `DRUM_STEP`); the v1-v4 step commands see them as GM notes (up to 4 per step).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its mix (`P_LEVEL`, `P_PAN`,
  `P_MUTE`), its pattern parameters (`LEN DIV SWG GATE`) and its key (`ROOT SCL QNT`, `CHORD`).
- Projects (`PROJECT`) save and load all four tracks and the selection (SLOOP 2.4: project format 5,
  "FUN5", format 4 + nudges and locks; formats 4 ("FUN4", SLOOP 2.0: the drum lanes, levels and ratchets),
  3, 2 and 1 from older firmware are converted when loaded, a format 1 project into track 1).
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine (the drum track: `P_E0` the kit, the others engine 0,
  as `DESC`). A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0. The drum track's level is still `G_DRLVL`
  (`SET` scope 1 or `TRACK_MIX`); its `P_LEVEL` is not used.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## v5: drum lanes, levels, ratchets (SLOOP 2.0)

- **Finding out:** `INFO` ends with 5. Older firmware ends after NTRK (or the engine names): use the
  v1-v4 commands only.
- **Drum lanes** (`firmware/src/drums.c` `LANE_NOTE`), one per white key from F3: 0 kick (36), 1 kick 2
  (35), 2 snare (38), 3 clap (39), 4 hat (42), 5 open hat (46), 6 pedal (44), 7 rim (37), 8 snare 2 (40),
  9 low tom (43), 10 hi tom (48), 11 crash (49), 12 ride (51), 13 shaker (70), 14 conga (63), 15 cowbell
  (56). A black key plays the lane of the white key left of it.
- **Levels** (2 bits): 0 NORM (as played), 1 GHOST, 2 SOFT, 3 HARD. **Ratchets** (2 bits): 0..3 = x1..x4
  hits in the step.
- **`DRUM_STEP`:** `on` is 16 bits (bit l = lane l), sent as 3 × 7 bits LSB first; `lvl` and `rat` are
  32 bits each (lane l in bits 2l..2l+1), sent as 5 × 7 bits LSB first. A set replaces the whole step;
  the lanes that are off read back with level and ratchet 0. An index ≥ NSTEP gets no reply. Not a push:
  `STEP_CHANGED` with the drum track selected means "re-read `DRUM_STEP` of that index".
- **The drum track through the old commands:** `STEP_GET` / `TRACK_STEP` give its first 4 lanes that are
  on as GM notes (lane order), time NOTE (REST if none), flag 1 (accent) if one of them is HARD, vel 100,
  and lvl / hi / rat 0. A `STEP_SET` / `TRACK_STEP` write puts each note on its nearest lane (GM 35..81,
  `LANE_OF_GM`), level HARD if the accent flag is set, else NORM, ratchet x1; time TIE / REST clears the
  step.
- **Synth steps:** `lvl` holds 2 bits per note (note k in bits 2k..2k+1, the same levels), `rat` 2 bits per
  note (x1..x4); both 8 bits, sent as 7-bit bytes with their top bits in `hi` (bit 0: lvl bit 7, bit 1:
  rat bit 7).
- **`TRACK`** ends with the solo mask (bit per track; GLO + key on the device). A soloed track plays,
  the others are faded out unless soloed too; mute and solo do not change `P_MUTE` of other tracks.
- **The kit** is the drum track's `P_E0`: `DESC` of `P_E0` with the drum track selected is the enum
  `KIT` (34 kits: ORIGINAL..DUST, the GM sample kit and its treatments, then the synthesised kits from
  808). `DESC` of `P_E1..P_E7` there still describes engine 0 (unused).

## v6: backup / restore (SLOOP 2.3)

`INFO` ends with 6. Objects: **0** the working project (a `project_t`, as the autosave), **1** the settings
(`persist_t`: colours, low cut, zoom, the panel calibration, the song order, the lights word: lights, SYNC,
the REC screen's mode and start, USB AUDIO and, since 2.4, MIDI OUT = SEQ (bit 14) and IN = CLOCK (bit 15)),
**2..5** the projects 1..4 (song sections A..D; length 0 = empty), **6..7** the user preset banks (`up_bank_t`,
16 records each; 0 = empty), **32..35** the user sample slots USR1..4 (35: SLOOP 2.4) (header + ADPCM data, as in flash; 0 =
empty). Numbers are 5 × 7 bit (u35, LSB first); data is pack7.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 BK_LIST | — | rc (0 ok, 4 no flash), count, then per object: id, length u35, CRC-32 u35 (zlib). Takes a snapshot of the working project and the settings for GET |
| 35 BK_GET | id, offset u35, count (2 × 7 bit, 1..256) | id, rc (0 ok, 1 arguments, 5 the snapshot is gone: LIST again), offset u35, count, pack7 data |
| 36 BK_PUT | op 0 begin: id 0..9 (8: v9, 9: v10), length u35, CRC-32 u35 · op 1 data: id, offset u35, pack7 (≤ 256 bytes, in order) · op 2 commit: id · op 3 abort: id | op, id, rc: 0 ok, 1 arguments, 2 not a valid object (CRC, magic, sizes, ranges), 3 stop the song first (projects and preset banks; since 2.5 the settings too when their song order differs), 4 flash, 5 no begin for this object (or more than 15 s ago) |

A restore stages one object in RAM (the project load buffer), checks it at the commit as a load checks it
(projects: magic, size and sum, older formats converted; banks: magic, record size, slot count; settings:
magic, palette, a permutation of the buttons and knobs, a valid song order) and writes it through the usual
A/B commit; the working project is loaded at once (the song must be stopped). Samples are restored with
`SMP_BEGIN` / `SMP_WRITE` / `SMP_END` (the header is the first 480 bytes of the object, the data from byte
512), an empty slot with `SMP_ERASE`. The editor's file is JSON: `{format: "sloop-backup", version: 1,
firmware, date, objects: [{id, len, crc, data (base64)}]}`; it is checked (lengths, CRCs) before anything is
written.

**The song order (SLOOP 2.5, the editor's Song page)** is bytes 48..83 of the settings object: `arr_config_t`
(`firmware/src/arranger.h`): count (1..16), loop (0/1), 2 reserved, then 16 × {section 0..3 = A..D, bars
1..64}; bytes 0..3 are the magic "PER3" (0x50455233, little-endian). The editor reads object 1 (BK_LIST, BK_GET),
changes those 36 bytes only and writes it back (BK_PUT). The commit answers rc 2 for an invalid order and,
since 2.5, rc 3 while the song plays if the order changed (as the SONG screen saves only when stopped).

## v7: step nudge and parameter locks (SLOOP 2.4)

`INFO` ends with 7. Each step of a track has a **nudge** (micro timing, `track_t.micro`): −32..31 in 1/64 of the
step's length, negative = early (the step fires inside the previous grid step), positive = late, 0 = on the grid.
On the drum track it moves the whole step (every lane). Sent as one 7-bit byte offset by 64 (−32 → 32, 0 → 64,
31 → 95). Each track also holds up to 24 **parameter locks** (`plock_t`: step, param, value): on that step the
track's `P_*` parameter takes the lock's value, and goes back to what it was at the next step that does not lock
it (Elektron style; notes still ringing follow, the engines read the parameters every block). A knob turned on the
device while a lock is in force wins: the value found is kept as the new base. Several locks may share a step
(different parameters); one lock per (step, parameter).

- **Lockable parameters** (firmware `seq.c p_lockable`): `P_LEVEL` .. `P_LD_AMP` (ids 0..16: level, the envelope,
  its destinations, the LFO and its destinations), `P_SGATE` (32), the sends `P_DIST` .. `P_REV` (33..36),
  `P_GLIDE` (38), `P_PAN` (39), `P_DETUNE` (44), the SLICER `P_SLCR` .. `P_SLDEPTH` (45..48) and the engine's
  `P_E0` .. `P_E7` (50..57). Not lockable: the pattern (`LEN DIV SWG`), the arp, the key (`ROOT SCL QNT TRN CHORD`),
  the voice mode and its options (`VCE GLMOD PRIO ALLOC`), `MUTE`. `LOCK_SET` on one answers rc 2 and writes nothing.
- **Values** are clamped to the parameter's range on that track (the engine parameters to the engine's; the drum
  track's `P_E0` to the kits), as `TRACK_PARAM`. A load clamps them again and frees locks on a parameter that is
  not lockable or a step past 63.
- **Pushes:** a nudge or a lock changed on the device (SEQ + a step held + KNOB 4 / PRESETS, OCT− to clear) is a
  change of that step: `STEP_CHANGED` (index) for the selected track; re-read `MICRO_GET` / `LOCK_GET` (two
  requests) with the step. The editor's own `LOCK_SET` / `MICRO_SET` push nothing. While a lock is in force the
  parameter's live value is what `GET` / `DUMP` return and `CHANGED` reports (coalesced, as any knob).
- **Projects** (`PROJECT`, the backup object 0 and 2..5) are format 5 ("FUN5", 3816 bytes): format 4 plus, per
  track, 64 nudge bytes, 24 × 4-byte locks and 16 bytes of fill conditions (v8, below). Format 4 projects (SLOOP
  2.0 .. 2.3) load with no nudge, no lock and no condition.
- A device that does not know these commands (v6 and older) sends no reply: use `INFO`'s version byte.
- **On the device:** SEQ held + a step key held: KNOB 1 sound / note, KNOB 2 LEVEL, KNOB 3 RATCHET, KNOB 4 the
  step's nudge; PRESETS a lock of the lock parameter (the sound parameter last touched on a page, ENV > FLT at
  power-on; ALGORITHM steps through the lockable ones), made at the track's value, then moved; the title shows it
  ("lock dst 14"); OCT− clears the held steps' nudges and locks. A tile with a nudge or a lock carries a dot in its corner.

## v8: fill conditions and quick chain (SLOOP 2.4)

`INFO` ends with 8. Each step of a track has a **fill condition** (`track_t.fill`, 2 bits per step, `core.h FC_*`):
0 **normal** (plays always), 1 **FILL ONLY** (plays only during a fill), 2 **NO FILL** (silent during a fill); 3 is read
as 0. A fill is on while the player holds GLO + white key 9 (*fill*), or for one whole bar after GLO + key 10 (*bar*:
armed, the next bar plays as a fill, then off; pressed again before the bar: cancelled). A step whose condition fails is
skipped whole: no note, no MIDI OUT, no ratchet, no parameter lock of its own (the bases come back, as at a step without
locks); it still ends the previous step's notes as a REST would, and the pattern position moves on. On the drum track the
condition is the whole step (every lane). The arp and the rolls are not conditions' business. STOP ends a held and an
armed fill.

- `FILL_GET` returns the 16 bytes as stored, pack7 (16 bytes do not fit 7-bit SysEx bytes: three groups of a top-bits
  byte and up to 7 bytes, 19 bytes); `FILL_SET` writes one step's condition and answers it as stored. Neither pushes.
- **Pushes:** a condition changed on the device (SEQ + a step held + OCT+ cycles normal → fill only → no fill; OCT− resets
  it with the nudge and locks) is a change of that step: `STEP_CHANGED` (index) for the selected track; re-read
  `FILL_GET` with `MICRO_GET` / `LOCK_GET`.
- **Projects:** the 16 bytes sit after the locks in each track of format 5 (3816 bytes in all; the format kept its
  magic, 2.4 being unreleased: a FUN5 image of a 2.4 development build without the field, 3752 bytes, reads as empty).
- **On the screen:** a FILL ONLY step's tile carries a small **F** in its top left corner, a NO FILL step's an **×**; the
  nudge / lock dot stays in the top right. The GLO layer's row 3 reads *fill* (lit while a fill plays) and *bar* (framed
  while armed, lit during its bar).
- **Quick chain** (no protocol: on the device only): SAVE held while playing, two or more section keys (1–4) tapped in any
  order, up to 8 (repeats allowed), then SAVE let go: the first section starts on the next bar as before, then each next
  one after the previous one has played its **pattern length** (the longest track: ceil(LEN × step / bar) bars, at least
  1, from the section's project in RAM), round and round until a single section tap, STOP or PLAY in song mode. The SAVE
  layer's sub line reads *chain A B B C*; the section playing is lit, the next one framed. SONG REC records what the chain
  plays as it records any section change. A device that does not know these commands (v7 and older) sends no reply:
  use `INFO`'s version byte.

## v9: FM6 patches (SLOOP 2.4)

`INFO` ends with 9; the engine list gains **FM6** (engine 9, after GRAIN; SLICE, when built, is 10). The commands keep
Felucca 1.0's numbers (68–71) so the two editors stay close; firmware `editor_fm6.c`.

- **The patch** is the DX7 single-voice layout of 155 bytes (six operators of 21 bytes, the sixth first, then the voice:
  pitch envelope, algorithm 0–31, feedback, oscillator key sync, the LFO, transpose, the 10-character name). On the wire it
  is the **128-byte packed record** of a DX7 32-voice bank (every byte already 7-bit: no pack7). The editor's `FM6` object
  (`unpack`, `pack`, `sanitize`) is the reference; `web/test_web.mjs` checks it against the firmware's factory patches
  (`build/gen/felucca_fm6.h`).
- **Targets:** **0** a synth track's own patch (index 0–2; the drum track has none), **1** a bank slot (0–26, in flash),
  **2** a factory patch (0–7, read only).
- **rc:** 0 ok, 1 bad arguments or index, 2 an empty bank slot (GET) or a flash error (PUT / ERASE), 3 stop the song first
  (bank writes only: a flash erase silences the audio for ~50 ms).
- A **PUT to a track** plays at once and is that track's patch until the track loads another (a project, a preset, a user
  preset, PTCH turned). A project keeps PTCH (P_E7: F1–F8 = 0–7, B1–B27 = 8–34), not the patch: to keep an edited patch,
  PUT it to a bank slot and set PTCH to it. User presets work the same way.
- **The bank:** 27 packed records, one flash object (A/B sectors at 0xE5000 / 0xE6000, as every SLOOP object); in the
  backup it is object **8** (27 × 128 bytes; 0 = empty), restored before the projects. A backup from 2.3 has no object 8:
  the bank is left as it is.
- **SysEx files** are the editor's business, not the device's: a single voice (`F0 43 0n 00 01 1B`, 155 bytes, checksum,
  `F7`: 163 bytes) or a bank (`F0 43 0n 09 20 00`, 32 × 128, checksum, `F7`: 4104 bytes). Import unpacks, sanitizes every
  value into its range and PUTs the record; export packs. A wrong checksum is read but reported.
- **Store cartridge** (editor 2.5, no new command): the ticked voices of a bank go to B1..Bn in one write. The editor
  reads object 8 (`BK_LIST`, `BK_GET`), replaces records 0..n-1 (sets their `used` bits), keeps the others, and writes the
  whole `fm6_bank_t` back with `BK_PUT` (begin / data / commit; rc 3 while the song plays). One sector erase instead of n.
  A bank it cannot read (another layout) is never overwritten. If asked, it then writes a user preset per voice (`UP_PUT`,
  one each): engine FM6, the voice's name, every value at its default and PTCH (P_E7) = 8 + the bank slot.
- No pushes: after a PTCH change on the device (a `CHANGED` of P_E7) the editor re-reads the track's patch.
- A device that does not know these commands (v8 and older) sends no reply: use `INFO`'s version byte (the editor hides
  its FM6 panel).

## v10: the SYN drum kits (SLOOP 2.5)

`INFO` ends with 10. The drum track's kits end with **SYN1..SYN4** (drums.c `DRUM_SYN`): synthesised kits of the user's,
kept in RAM (the drum voices play them from there) and in flash after the settings (`persist_t`, then `dsu_bank_t`:
one A/B object; firmware before 2.5 reads its `persist_t` and leaves the rest). A sound is the firmware's `dsnd_t`
(drum_synth.c), 22 bytes in this order: wave (0 OFF, 1 SINE, 2 TRI, 3 SQUARE, 4 FM, 5 BELL), src (noise 0 OFF, 1 WHITE,
2 METAL, 3 CYM, 4 CHIP; | 16 CLAP), pitch (MIDI note), fine (1/16 semitones), bend (semitones, 0..96), btime (DECAY_K
index), hold (2 ms units), decay (DECAY_K), tlev, t2 (the second partial's ratio x 32, 0 none), t2lev, click, nlev, nhold
(2 ms), ndec (DECAY_K), flt (mode 0..3 OFF LP BP HP | 4 the tone too | resonance 0..31 << 3), fcut (CUTOFF_HZ index),
fenv (signed: CUTOFF_HZ steps), hpf (CUTOFF_HZ index, 0 off), chip (MIDI note of the chip noise's clock / 8), drive,
level (128 + 4 x dB). DECAY_K: 5 ms .. 4 s over 0..127 (exponential), CUTOFF_HZ: 30 Hz .. 16 kHz. Every value received
is put into range before a voice sees it.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 DSYN_LIST | — | factory count (32), user count (4), stored (0: an edit not stored yet), the factory names, then per user kit: its name, the factory kit it started from |
| 73 DSYN_GET | which: a factory kit 0..31, or 64 + k (SYN k+1) | which, rc (1: no such kit), name, pack7 (crush, src, 16 x 22 bytes) |
| 74 DSYN_PUT | k 0..3, part, pack7 data | k, part, rc (0 ok, 1 arguments). part 0..15: a sound (22 bytes, in the synthesised lanes' order KICK SNARE CLAP HAT OPEN-HAT TOM-LO TOM-HI CRASH RIDE SHAKER CONGA RIM COWBELL CLAVE KICK-2 SNARE-2); 16: name (8 ASCII) + crush + src; 17: the whole kit from a factory kit (1 byte) |
| 75 DSYN_STORE | — | rc: 0 ok, 3 stop the song first, 4 flash |
| 76 DSYN_PLAY | k, lane 0..15, velocity 1..127 | k, lane, rc: the sound on the drum voices (at the next audio block) |

Backup object **9** is the four kits (`dsu_bank_t`, 1464 bytes: "DSU1", version 1, count 4, then per kit name 8,
crush, src, 2 reserved, 16 x 22). Its commit answers rc 3 while the song plays. A device before v10 answers the
begin of object 9 with rc 5: the editor skips it (as object 8 on a v8 device).

## v11: PERFORM (the live layers for a remote surface)

`firmware/src/perform.c`. A phone or tablet plays the layers the panel plays with a held function button:
each op runs the same code as the key (`ui_layers.c` `layer_key`, `seq.c` live sections), so the screen,
the LEDs and the messages follow as if the key was pressed. Every reply carries the live state, so a remote
follows the device by sending `PERFORM` 0 (STATE) about every 100-250 ms between its other requests.

| op | args | what it does |
| --- | --- | --- |
| 0 STATE | — | nothing: the state |
| 1 FX | fx 0..15, 127 off | punch-in effect held from the remote (FX + white key 1..16): LOOP 4, LOOP 8, LOOP 16, LOOP 32, STUTTER, REVERSE, STOP, HALF, LOW, HIGH, PHONE, CRUSH, ALIAS, GATE, ECHO, WOBBLE. Another fx replaces it (crossfaded). A panel key held under FX takes over; the remote neither replaces nor ends it |
| 2 SECTION | 0..3 | SAVE + key 1..4: stopped, the section is loaded as the loop; playing, it starts on the next bar (any chain stops); song playing / empty: nothing (the device says so) |
| 3 CHAIN | n 1..8, n × section | the quick chain (SAVE held + n taps): the first on the next bar, each for its pattern's bars, looped. rc 1 stopped, 2 an empty section, 3 the song plays |
| 4 MUTE | track 0..3, 0 off / 1 on / 2 toggle | `P_MUTE` of that track |
| 5 SOLO | track 0..3, 0 / 1 / 2 | its bit of the solo mask |
| 6 TRANSPORT | 0 toggle, 1 play, 2 stop | as PLAY (count-in, REC armed and free takes as the button). rc 1: song mode with an empty section |
| 7 FILL | 0 let go, 1 hold, 2 the next bar (again: off) | GLO + key 9 / 10 |
| 8 TAP | — | tap tempo (GLO + the last white key) |
| 9 SONGMODE | 0 loop, 1 song, 2 toggle | SAVE + key 13 |
| 10 SONGREC | — | SAVE + key 14: arm / stop SONG REC |
| 11 STORE | 0..3 | SAVE + key 5..8: the loop into the section; over a used one, send it twice within 3 s (the first arms) |

**Live state** (14 bytes, the chain, then 5 bytes; after op and rc):

| Byte | |
| --- | --- |
| flags | bit 0 playing, 1 song mode, 2 the song plays, 3 fill held, 4 fill next bar, 5 the FX is the remote's, 6 REC armed / count-in |
| bpm | v14 |
| beat | beats since PLAY or the section's first bar, mod 128 (bar = beat / 4) |
| fx | the punch-in effect asked for, 127 none |
| mute | bit per track |
| solo | bit per track |
| armed | bit per track recording |
| ready | bit per section A..D that holds a loop |
| section | the section playing (last jumped to, loaded or stored), 127 none |
| next | the section asked for the next bar, 127 none |
| songrec | 0 off, 1 armed, 2 recording |
| chain n, chain i | the quick chain: entries (0 none), the one playing |
| n × section | the chain |
| sel | the selected track (0..3) |
| 4 × bars | each section's length in bars as a chain plays it (its longest pattern), 0 = empty |

- **Holds end when the remote goes quiet.** A remote FX or FILL hold is let go 1.5 s after the last request of
  any kind: a dropped phone never leaves the mix looping. Keep sending (STATE, or PING) while a pad is held.
- The parameters of the master (`G_FILT`, `G_DUST`, `G_DUCK`, `G_ROLL`, `G_BPM`, swing) and the levels stay
  `SET` / `TRACK_MIX`: find their ids by `DESC` label.
- A device that does not know `PERFORM` (v10 and older) sends no reply: `INFO`'s version byte is 11.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next.
  The device holds only one incoming SysEx frame.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Felucca" (USB 1209:0001; SLOOP keeps the name so editors
  and installers find it). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since SLOOP 2.3 (after Felucca 1.0) the same USB device also has an audio input
  ("Felucca", 44.1 kHz stereo; bcdDevice 3.11): the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Safety.** Only `PROJECT` save, the sample-slot commands and `UP_PUT` / `UP_STORE` / `UP_ERASE` write flash, and only in
  Felucca's own storage; never the app or the update area. Since SLOOP 2.4 every one of them, and
  `BK_PUT` of a preset bank, answers rc 3 while the song plays (as the panel refuses to save then): a
  flash erase silences the audio for about 50 ms.
