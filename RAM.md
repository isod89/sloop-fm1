# Less RAM for SLOOP: the `ram` branch of sloop-lab

On top of [`dualcore`](DUALCORE.md) (SLOOP 2.6 with the second core, whose patch 5 put the reverb lines, core 1's
stacks and the render split's scratch into the SRAM below the RAM region), this branch takes bytes out of the
96 KiB RAM region itself, one commit per idea, each with its measured delta, the 182 golden renders unchanged
and every mode of the host double the same. Offered to SLOOP as issues, not as a pull request (upstream merges
none). Nothing here changes a sound; two things change what a corner case does (below).

| Commit | What | RAM region after it | Image |
|---|---|---|---|
| (`dualcore`, patch 5 + 5b) | `.lowram`: the reverb lines, core 1's stacks, the split's scratch out of the RAM region; the stacks painted and measured | 73,756 B | 576,564 B |
| `fbafa34` | `midi_sel_on` (the track a MIDI note-on went to, per channel and note: 0..4) in nibbles, `midi_sel_get` / `midi_sel_set` (seq.c) | 72,732 B | +144 B |
| `d024b41` | `firmware/src/scratch.h`: one scratch for the main loop's short-lived buffers that are never alive together: the storage payload (`st_buf`), the visualiser's snapshot (`vis_l` / `vis_r`), the HOME oscilloscope's, the decoded OTA frame; the OTA staging (`hdr`, `b`, `mine`, `parm`) over the project buffer (`felucca.c OTA_STAGE_RAM`), as the FM6 bank and the editor's backups already are | 66,860 B | +496 B |
| `72300ea` | the user preset banks read where they are, in the flash's plain XIP window (`storage.c st_xip`, `upreset.c up_bank_xip[]`): no RAM mirror; a write stages the bank in the project buffer (`project.c up_stage`) and goes through `st_save`; the editor's backup reads the bank by its pointer | 60,716 B | -196 B |
| `904d386` | the DRAWBAR part state (`drw_part_t`: bar gains, percussion, rotary, each voice's wheel phases) in the engine arena, as GRAIN, FM6 and PHYS keep theirs (`engines.c eng_arena_t`) | **58,220 B** | -64 B |

From 96,524 B (2.6 + the split) to 58,220 B of 98,304: 40.1 KB spare. The pool (333,900 of 344,064 B) and the
`.lowram` region (22,772 of 29,488 B) do not move. `FELUCCA_CPU1=0` builds too (58,156 B).

## On the device

First boot 11/10/2026 (13:30), over the official 2.6: ABOUT 2 CORES; the console at idle: `lowram 1`,
`cpu1_timeouts 0`, `stack_free 22956`, `sstack_free 7080`, `sp 0x01C79E38` (the main loop on the main stack, the
interrupts on the supervisor stack), `flash 1`. The user preset, backup, DRAWBAR and update paths are still to be
exercised on it.

## What a user may notice

- **Without a working flash** (the JEDEC id did not match at boot, which no healthy FM-1 shows), a user preset can
  no longer be kept in RAM: SAVE > USER says NO FLASH instead of SAVED (RAM). With a flash, nothing changes: the
  banks are read through the XIP window, as the samples and the FM6 bank always were, and a save goes to the flash as
  before.
- **DRAWBAR after GRAIN, FM6 or PHYS on the same part**: the wheels' start phases spread again when DRAWBAR comes
  back (the arena was zeroed by the other engine); before, they ran on forever. After any other engine they stay as
  they were. No golden render covers that corner, so the suite cannot see it; the sound of a part that stays on
  DRAWBAR is the same, bit for bit.

## The rules the scratch relies on (`scratch.h`)

A member of `main_scratch` may only be used by the main loop, filled and consumed inside one call (or the caller's
next lines, as `dsu_boot` and `fm6_bank_scan` read the payload `st_current` leaves), never from the audio
interrupt or TIMER5's poll, and never across a return to the main loop. A flash access never happens inside a draw,
nor a draw inside a flash access. The OTA staging over the project buffer holds because an update session blocks the
main loop; the editor's backup, which also stages there, is abandoned by a session anyway. A new user of either
buffer must keep to this, or keep its own.

## Not done, on purpose

- `st_buf` itself (3,840 B): once the scratch exists, removing it saves 1.8 KB and touches every save path in
  `storage.c` (page-wise programming from up to three sources, the CRC over a clamped destination): not worth it yet.
- The user kit zones (`usr_zone`, 1,792 B) and the SYN drum kits (`dsu`, 1,464 B) stay in RAM: the audio interrupt
  reads the zones while the editor rewrites the slot (today the RAM copy is swapped under `irq_off` after a check),
  and the kits are the editor's working copy inside the settings object.
- The pool's big levers (the 59 KB canvas, the 1.49 s delay line, the four project slots in NOINIT) are SLOOP's
  product decisions and are only listed, with their numbers, in the owner's notes.

## Testing

As `dualcore`'s: `sh build.sh`, then `sh tests/run_tests.sh` (the stale target budget is upstream's). The host
builds keep what the device replaces: `UP_HOST` keeps the RAM preset banks for `tests/upreset_test.c`, the OTA
tests keep `ota.c`'s own staging (no `OTA_STAGE_RAM`), and `st_xip` only exists where `ST_XIP_PTR` is defined
(`felucca.c`). On the device, besides `dualcore`'s checklist (`lowram = 1`, 2 CORES, the stack figures): save,
load and erase a user preset (SAVE > USER), back up and restore a preset bank from the web editor, play a DRAWBAR
part, run an update.

## Licence and credits

GPL-3.0-only, as SLOOP. Copyright (C) 2026 Andre Rezemini, written with Claude (the commits say so); SLOOP is
isod89's, Felucca Leo Kuroshita's. See `LICENSING.md`.
