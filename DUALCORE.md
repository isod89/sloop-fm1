# SLOOP on both cores: the `dualcore` branch of sloop-lab

A thin fork of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) that makes the M-VAVE FM-1's second
processor core available to the audio render. Everything else is SLOOP, unchanged; the work is a short
series of patches meant to be re-applied on each SLOOP release, and offered upstream.

The second core's start-up sequence is [X0X](https://github.com/charlesvestal/fm1-x0x)'s (Charles Vestal),
after the findings of [Melodee](https://github.com/keremimo/melodee) (Kerem Kilic); X0X has run its drum
machines on core 1 since 1.0 (10/10/2026). This branch brings that HAL into SLOOP and, in the following
patches, splits SLOOP's `mix_block` into units that render on both cores with a bit-identical result.

## Status

**11/10/2026, 13:30: the 2.6-based build runs on an FM-1, and the SRAM below the RAM region is real.** First boot
of the `ram` branch's build (patches 1-5b of this branch plus the four reductions), straight over the official 2.6
from the local installer: ABOUT says 2 CORES; the console, 87 s after boot, idle, says `lowram 1` (every word of
the 29 KB below the RAM region took and gave back its pattern, and core 1 ran its self-test and its jobs with its
stacks in there), `cpu1 3`, `cpu1_trace 0xC1000002`, `cpu1_timeouts 0`, `cpu1_stack_free 2404` (of 2,816),
`stack_free 22956` (of 24,320), `sstack_free 7080` (of 7,936), `sp 0x01C79E38`, `audio_max_us 1273`, `cpu_pct 9`,
`flash 1`. So the main loop runs on the 24 KB main stack (456 B deep at the prompt, 1,364 B at most so far) and
the interrupts on the 8 KB supervisor stack (856 B at most so far): Felucca's guard comment can be settled. The
figures after heavy use (the editor, SPECTROGRAM, a project load, three PHYS SYMP parts) are still to come; they
decide the stacks' size.

**11/10/2026, afternoon: the split costs the RAM region nothing (patch 5).** The 2.6 merge had left 1,780 B of the
96 KiB RAM region (`.data` + `.bss`) free: SLOOP had already spilled its reverb lines (17.4 KB) and the 2.6 INSERT
state into it because the pool is full, and the series added 3.8 KB. An analysis of the ELF found 29.5 KB of SRAM below
the RAM region (0x01C00BD0..0x01C07F00) that Felucca's map leaves for a RAM-run debug build SLOOP does not have. Patch 5
makes it a section, `.lowram`, proved and zeroed at boot like `.pool`, and puts there the reverb lines, core 1's two
stacks and the render split's scratch (the second accumulator set, the drum buffers, the part buffers, the preset-fade
scratch): the RAM region drops from 96,524 to **73,756 B** (24.5 KB spare), nothing else changes, the audio is the same
by construction (placement only). The console says `lowram = 1` when every word of the region took and gave back a
pattern at boot. Patch 5b paints core 0's two stacks (24 KB and 8 KB, never measured before) and reports how deep they
go (`stack_free`, `sstack_free`): the figures from the device decide whether the pool may grow later. **Not yet run on
the FM-1**: the first boot of this build must show `lowram = 1`, 2 CORES, the reverb audible.

**11/10/2026: the series is on SLOOP 2.6.** Upstream's 2.6 (27 punch-in effects, INSERT on every track, LFO 2, ENV
SYNC, TEXTURE, MIDI channels per track, MIDI LEARN, the look-ahead limiter, Terminus Bold) was merged into `dualcore`
the day after it came out: four files in conflict (`fx.c`, `panel.c`, `ui_menu.c`, the UI test), and a new audit of
the render for state shared across parts, which found two things 2.6 added (the ENV SYNC steps, kept once for "the
part rendering now", and the preset-fade scratch in `mix_part`) and made them per track / per core. The settings
bit of 2ND CORE moved from 21 to 26 (2.6 took 21..25 for MIDI IN = CH n), and SYSTEM in the menu has four rows now
(2ND CORE, HARDWARE CALIBRATION, MIDI LEARN CLEAR, ABOUT). The 184 golden renders of 2.6 and the split test, with a
new scene for 2.6's features, give the same hashes under every mode of the host double, and no race under
ThreadSanitizer. The 2.6-based build has **not yet been run on the FM-1**: the figures below are the 2.5-based
build's.

**10/10/2026, evening: it runs on an FM-1.** The first device with this build (patches 1 to 4, commit `1d31ce3`)
came up with **2 CORES** on ABOUT: core 1 started at power-on, passed its self-test, and the render has been
split (the last synth part and the drums on core 1) from the first block. First measurement, the same evening,
on a pattern of the owner's (engines not noted): **CPU 31 % with 2ND CORE OFF, 23 % ON** (the INFO column of
GLO > SYSTEM: core 0's share of a half), the sound unchanged. The console figures (core 1's load, core 0's wait,
timeouts, core 1's stack) and a worst-case pattern are the next thing to collect (below, "What the device will tell").

| Pattern | 2ND CORE OFF | ON | core 0 freed |
|---|---|---|---|
| the owner's first pattern, 10/10/2026 (engines not noted) | 31 % | 23 % | 26 % of its load |

| Patch | What | State |
|---|---|---|
| 1 | `hal/fm1_cpu1.{h,S}` from X0X; `fm1_guard_pc_open`, `fm1_guard_top_locked`; the second core held before a reboot and before the loader (`fm1_sys.h`) | done, builds |
| 2 | `.c1_text` in the linker script and `tools/build.py`; the loop copied to RAM and the core started at boot (`main.c`, `cpu1.c`) with a self-test job; "2 CORES" / "1 CORE" on ABOUT; `cpu1`, `cpu1_trace` in the console; `FELUCCA_CPU1=0` builds the old way | done, builds; **runs on the FM-1** (first boot 10/10/2026: ABOUT says 2 CORES; the measurements are still to come) |
| 2b | **the host double of core 1** (`firmware/src/cpu1.h` API: `cpu1_run` / `cpu1_wait` / `cpu1_on`; `tests/cpu1_host.c`): `CORES=1` runs a job inline at the wait, `CORES=2` on a POSIX thread, `CHAOS=N` with random delays, `CPU1_TIMEOUT_AT=K` loses a job and holds the core; `tests/cpu1_test.c` in all modes and under ThreadSanitizer, from `tests/run_tests.sh` | done |
| 3 | **the render split** (`fx.c`): the three parts and the drums as render units (`mix_part`, `mix_drums`, `mix_units`), each adding into an accumulator set of its own (the dry mix and the three sends); core 1's share handed over through `cpu1_run` before this core renders its own, added after `cpu1_wait`; the timeout fallback (core 1 held, its units rendered here once; 8 ms in 2.7.1, 4 ms after it: the guards below). FM6's operator buses per part (`fm6_part_t`, in the engine arena, `fm6_core.c`, `eng_fm6.c`, `engines.c`). What stays on core 0 (`units_split`): two FORMANT parts together (one breath-noise xorshift), a part whose LFO wraps this block or whose PHYS model changed under a note (`rng()`, so the draws keep their order). `cpu1_units` (`cpu1.h`) forces a split: 0 none, a mask, -1 the firmware's choice | done: the 182 golden hashes unchanged under every mode of the host double and with no split (`tests/cpu1_modes.sh`); `tests/mix_split_test.c` (three FORMANT, FM6 or PHYS parts, S&H LFOs, mutes, the drums through the SLICER and the FILTER, the metronome) gives the same output for every split of the units, and no race under ThreadSanitizer; builds; **runs on the FM-1** (first boot 10/10/2026: ABOUT says 2 CORES; the measurements are still to come) |
| 4 | **the switch and the figures**: MENU > SYSTEM > **2ND CORE** ON / OFF (`ui_menu.c`; a bit of the settings word in `panel.c`, kept across restarts, old settings read as ON; live: the next block renders so, and the sound is the same either way); core 1's time in its job and core 0's time waiting for it, measured in `fx.c` with TIMER4 and smoothed per half as the CPU figure is (`cpu1_load_half`, called from the audio interrupt); on the console: `cpu1_busy_pct`, `cpu1_wait_pct`; ABOUT says `C1 OFF` when switched off. The INFO page keeps its one CPU column (core 0's load): it has no free column, so the figures live on the console | done, builds; **runs on the FM-1** (first boot 10/10/2026: ABOUT says 2 CORES; the measurements are still to come) |
| 2.6 | the series carried onto SLOOP 2.6 (11/10/2026: upstream's `main` merged into `dualcore`): `mix_part` wraps 2.6's preset fade with its scratch per core; the drum track's INSERT inside `mix_drums`; TEXTURE and FREEZE's feed after the two accumulator sets are added, the THROW glide before the hand-off; `units_split` follows LFO 2's tempo-synced increment and the values a fading part renders with; ENV SYNC's steps per track (`voice.c`); 2ND CORE in bit 26 of the settings; four rows in the SYSTEM menu; a 2.6 scene in `tests/mix_split_test.c` | done: builds (RAM 96,524 of 98,304 B), same hashes in every mode, no race; **not yet run on the FM-1** |
| 5 | **the SRAM below the RAM region**: `.lowram` in `app.ld` (RAMTEXT extended to 0x01C07F00, the boot status words of `crt0.S` just above it), proved and zeroed at boot (`main.c lowram_test`, console `lowram`), holding `rev_line`, `acc`, `drum_buf`, `part_buf`, `mix_part`'s `pnew` (`fx.c`, a section mark each) and core 1's stacks (`fm1_cpu1.S`'s `.bss.c1stack`, placed by name: X0X's file untouched); `tools/build.py` reports it | done: builds (RAM 73,756 of 98,304 B, `.lowram` 22,772 of 29,488 B, pool unchanged, image 576,564 B), `FELUCCA_CPU1=0` builds too; the host suite unchanged; **not yet run on the FM-1** |
| 5b | **the stacks measured**: core 0's main and supervisor stacks painted at boot below the pointers in use (`main.c stack_paint`), `stack_free` / `sstack_free` / `sp` on the console: how deep each goes, and which of them the main loop and the interrupts run on (Felucca's guard did not know) | done: builds; the figures come from the device |
| 6 | the 2.6 build on the device (the checklist below), then perf patterns (light, typical, worst: three PHYS SYMP / FM6 / GRAIN parts with 4-note chords, full drums, delay and reverb) and the measurements in this file, with 2ND CORE ON and OFF | next |
| 7 | the greedy per-block split by measured cost | |

Later, outside the dual-core series: reverb and delay settings that change per song section (after shifl's
song fragments), as a separate patch set.

## Testing on the Mac first

The render reaches core 1 only through `cpu1.h`'s three calls, and the host tests have their own
definitions of them (`tests/cpu1_host.c`), so the same code runs on the Mac in every way the device can
behave: `CORES=1` (inline, the job deferred to the wait), `CORES=2` (a real thread), `CORES=2 CHAOS=7` (a
slow, erratic thread), `CPU1_TIMEOUT_AT=K` (a lost job, never started: the fallback renders it once, then one
core), `CPU1_UNITS=mask` (a forced split: 0 none, 15 every unit that may go), and `CORES=2` under ThreadSanitizer.
`tests/cpu1_modes.sh` (run by `tests/run_tests.sh`) runs the golden renders and `tests/mix_split_test.c` under all of
them: a render that gives the same hashes in every mode is bit-exact and race-free as far as the host can tell (a
lost job's own block is the same too: nothing of it ran). After 2.7.1 `tests/mix_split_test.c` puts every engine,
the GM, user and SYN drum kits and a part's SLICER and FILTER on core 1, renders the engine switches' fades on core 0
while core 1 renders the other units, and fails if core 1 draws from `rng()` or renders a fading part; it also runs
under ASan + UBSan, and its ThreadSanitizer build keeps the firmware's own `memcpy` (this toolchain's TSan does not
check libc's);
`tests/cpu1_hold_test.c` holds a job in the middle and at its end (the device's late core: the fallback's block is
a glitch unless the job had finished); and `tests/cpu1_mailbox_test.c` runs the real hand-off (`hal/fm1_cpu1.h`,
`cpu1.c`'s state) on the host, `fm1_c1_main` on a thread as core 1 over a model of the chip's registers, misreads,
late and faulting jobs included. The device then answers only timing and what the model of the chip assumes.

What a block does now (`fx.c mix_block`): the sequencer's events and the DUCK on core 0; then the split for
this block (`units_split`); core 1's units handed over (`acc[1]`, its own `part_buf`); core 0's units into
`acc[0]` meanwhile; the wait (a timeout holds core 1 and renders its units here); `acc[0] += acc[1]`; then
the buses, DUST, PUNCH, the DJ filter and the master, serial as before. Nothing but the render runs while
core 1 works: the main loop is preempted by the audio interrupt, and TIMER5 only scans the panel and feeds
USB audio.

## What the device will tell

On a first boot of this build: ABOUT (hold HOME, SYSTEM, ABOUT) shows **2 CORES** when core 1 started and
passed its self-test, **1 CORE** otherwise; the serial console (MENU > AUDIO > USB SERIAL ON, restart, then any
serial terminal on the USB port, e.g. `screen /dev/tty.usbmodem* 115200`, and `status`) prints `lowram` (1: the SRAM
below the RAM region took and gave back a pattern at boot, word by word; 2: it did not, and this build must not be
used), `stack_free` and `sstack_free` (bytes of core 0's main and supervisor stacks never reached since boot: the
24 KB and 8 KB of `app.ld` were never measured; `sp` says which one the main loop runs on), `cpu1` (0 not started: not
built, 2ND CORE OFF, or barred (`cpu1_why` 5 or 6); 1 did not report in within 20 ms, 2 answered wrong or late, or held
since, 3 running), `cpu1_trace` (0xC1000001: reached
its entry; 0xC1000002: reached its loop; 0xC1DEADvv: it faulted on vector vv and parked), `cpu1_timeouts` (blocks
core 1 did not finish in 4 ms, read wrong or faulted in: after the first, it is held and SLOOP runs on one core),
`cpu1_why` (why it was held: 1 late, 2 its stack (the red zone, or its stack limit trapped), 3 a fault, 4 a wrong
read, or a self-test answered wrong; why it was not started: 5 barred after a hold or a crash while it ran, until power-off, 6 the boot before
failed), `cpu1_fault_pc` and `cpu1_fault_emu` (where it faulted, and its EMU_MSG then: bit 0 misaligned, 1 illegal,
3 its stack limit), `cpu1_bad` (jobs it refused: a wrong read), `cpu1_busflags_cut` and `cpu1_busflags`
(bus-invalid flags right after a hold, and after core 0's writes that followed it), `crash` (the last crash
record, with `dbg`: DBG_MSG, a bus or PC-limit flag),
`cpu1_units` (`cpu1 units MASK|auto` sets it live; `cpu1 late` makes the next job late on purpose, `cpu1 fault` makes
it go past core 1's stack, `fault yes` makes core 0 fault: device tests of the hold, the fault park and the crash
screen), `cpu1_busy_pct`
and `cpu1_wait_pct` (core 1's share of a half spent rendering, core 0's share spent waiting for it) and
`cpu1_stack_free` (bytes of core 1's 2,816 B stack the render never reached: it must stay above 256 on the
heaviest patterns; below that core 1 is held, its stack limit and the per-half check). With 2 CORES the last synth part and the drums render on core 1; the audio is bit for bit
SLOOP 2.6's. MENU > SYSTEM > 2ND CORE switches the split off and on while playing: `cpu_pct` (the console, or
INFO on the SYSTEM page) should drop with it on, by roughly the drums' and one part's share.

## The second core's guards (after SLOOP 2.7.1)

What a stalled, misreading or faulting core 1 does, and what the render does about it (each found by an audit of
2.7.1's dual-core code; `tests/cpu1_hold_test.c` stops a job in the middle on the host, as the device's hold does):

- **The mailbox** (`hal/fm1_cpu1.h`, after X0X 1.0.5+ / Felucca 1.6.1 and Jangada's measurements, fm1-x0x#10): core 1
  runs only the next job (`done + 1`, `done` kept in a register) and only when the check word core 0 wrote with it
  matches; anything else it counts (`mb.bad`) and core 0 holds it at the next wait. Each core's words on cache lines
  of their own. The timeout is 4 ms (as Jangada and Felucca), less than a half: one late half at most.
- **Its stack** (hardening: the deepest job today is 1,580 B of its 2,816, static, no recursion): it sat right
  above `.c1_text` and the flash driver with nothing checking it; now last in `.lowram`, over 1 KB left free and a
  256-byte red zone (`app.ld`; `tools/build.py` checks it), so an overflow never reaches code; core 1's own stack
  limit in its EMU bank traps within 256 bytes of the end (as Melodee, X0X, Felucca); core 0 checks the red zone and
  the stack's last 256 bytes after every half (`cpu1.c cpu1_guard_half`). Either holds core 1: one core.
- **A fault on core 1** (after Jangada, X0X and Felucca 1.6.1; `hal/fm1_irq.h fm1_fault_c` reads `cnum` first) no longer runs core 0's report (it stopped
  the audio for good and held itself before the reset): it leaves `cpu1_trace` 0xC1DEADvv, its EMU_MSG
  (`cpu1_fault_emu`: its stack limit reads as `cpu1_why` 2, not 3) and parks in RAM; core 0 holds it at once. Its EMU
  bank is set by it at start (divide-by-zero trap off, as core 0's).
- **Every reset holds core 1**: `fm1_cstart` first thing (a reset that does not reset it left it running its loop
  while `.bss` was cleared under it), UBOOT, the crash screen at once, the fault-in-a-fault reset. The crash screen
  feeds the 8 s watchdog through its 10 s, so it ends in `fm1_reboot`, not a watchdog reset.
- **The late path** (`fx.c mix_block`): core 0 renders again only the units core 1 did not finish (it marks each one
  done, `cpu1_units_done`): a finished unit rendered twice ran its SLICER step clock and its LFO 2 a block ahead until
  the next PLAY, and a job finished then held (a wrong read seen at the wait) now costs nothing at all. A
  part fading between sounds stays on core 0 (its render swaps `t->p` and back: held in between, it kept the old
  sound for good); the engines' caches are marked valid last (FM6 `fm6_eff`, PHYS SYMP's coefficients, GRAIN's zone
  index, a grain's voice after its window), a GRAIN reverse window is refilled when the position left it, the
  SLICER's record index is bounded where it is used, and DIST, the track FILTER and a voice's end (`env_tick`,
  LOFI's STEP amplitude) write their "done" flag last; a voice still active at stage 0, which only a hold can leave,
  is ended by `env_tick`. `tests/cpu1_hold_test.c` fails with the fade, FM6, GRAIN index, SYMP, SLICER and
  finished-unit fixes undone one at a time, with the two GRAIN grain fixes undone together (each alone stops the
  out-of-bounds read the 2.7.1 order allowed), and with a voice end's order and the stage-0 end undone together
  (either alone is enough: the test pins the pair).
  Accepted residuals, a few instructions wide: a sample's ADPCM decoder held between its predictor and its
  position, a MODAL / MEMB strike, and a unit held part way through, rendered again from where it was (its SLICER
  step clock or LFO 2 may end a block ahead until the next PLAY).
- **The traps across the late path** (not confirmed on a device): the bus-invalid traps stay off from the hold until
  core 0 has written core 1's memory again (`cpu1_late_end`), in case the flag 2.7 reported (DBG_MSG bit 21,
  `c0_wr_bus_inv`) came from that write. `cpu1 late` finds out: its job writes what core 0 writes again after the
  hold, and the console then shows `cpu1_busflags_cut` (sampled right after the hold) and `cpu1_busflags` (after core
  0's writes): bit 21 already in the first, the cut raised it; only in the second, core 0's writes did. Every hold
  records why before the traps come back (the late path, the per-half check, a failed self-test), so a flag that
  latches late traps with the crash screen's core 1 line right.
- **One core after a failure**: core 1 held for good, or a crash while it ran, in a job or between jobs (as X0X
  1.0.9: fm1-x0x#21's misreads come with both cores running, core 0's too): the next warm boots start one core until
  the FM-1 is switched off (`.noinit`, cleared at a power-on and by an update, whose core reset reads as a power-on);
  a boot that died in its first 30 s: the next boot only
  (as Felucca 1.6.1). The player sees "2ND CORE STOPPED" when it is held while playing, "1 CORE: AFTER A CRASH" on a
  boot that does not start it. X0X 1.0.9 and Felucca 1.6.1 go further and save 2ND CORE OFF in the settings.
- **Not done** (diagnostics, or by choice): a hang the watchdog resets does not bar core 1 (a stuck update loader ends
  that way too, and the boot guard covers a hang in a boot's first 30 s); core 1's ETM is not enabled, and the AXI
  invalid-access IDs are not recorded, so an AXI bus error raised on core 0 does not name its master.

## Installing a build of this branch on an FM-1

The package keeps SLOOP's development identity (FM-1_900); Felucca's update path does not refuse a package
with the same identity as the installed one, so it installs over the official 2.6 and the official 2.6
installs back over it. Your projects, presets, samples and settings are kept (2.6's own note: a project saved by
2.6 does not open in 2.5). If the device ran the 2.5-based build of this branch with 2ND CORE OFF, that bit now
reads as GLO > MIDI 2 > IN = CH 1: set IN back to NOTES once. From the browser:

```sh
python3 web/make_site.py build/felucca.fwsc dev ../sloop-site
cd ../sloop-site && python3 -m http.server 8000
# Chrome or Edge: http://localhost:8000/webapp/installer/  (Web MIDI needs localhost or https)
```

Or `python3 tools/fm1_install.py build/felucca.fwsc` (needs mido and python-rtmidi). Rescue as SLOOP's:
hold OCT- while switching on, then install again.

## Building

As SLOOP's `BUILDING.md`; on macOS run `sh build.sh` with Docker up. Memory of this build against 2.6
(upstream 2.6: RAM 92,696 B, image 573,052 B): RAM `.data+.bss` **73,756 B** of 98,304 (24,548 B spare: the reverb
lines, core 1's two stacks and the split's scratch moved to `.lowram`, 22,772 B of the 29,488 B below the RAM region,
`tools/build.py` prints both), pool unchanged (the FM6 buses fit in the engine arena), image 576,564 B (+3,512 B:
the split, the boot self-test and the stack figures), `.c1_text` 60 B. (Before patch 5 the RAM region was 96,524 B;
on 2.5, 88,472 B, image 567,296 B.) The analysis behind patch 5 and the next RAM options (a scratch union, the storage
buffer, the user banks read in place) is the owner's; its figures are in the commit messages.

## Licence and credits

GPL-3.0-only, as SLOOP, Felucca and X0X. `hal/fm1_cpu1.h` and `hal/fm1_cpu1.S` are Charles Vestal's
(X0X), brought in unchanged but for a notice, and modified after 2.7.1 with the mailbox of X0X 1.0.5+ / Felucca
1.6.1 (Leo Kuroshita), Jangada's layout and hardening (zednaked) and Melodee's guards (Kerem Kilic): the files say
which; the guard and system additions follow X0X's; the rest of the
branch is Copyright (C) 2026 Andre Rezemini. SLOOP is isod89's; Felucca is Leo Kuroshita's (Hügelton
Instruments). See `LICENSING.md`.
