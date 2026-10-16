#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the drum kit, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             cost / sample per preset and mix, relative to the idle + drums mix (tests/cpu_baseline.txt,
#                   +25 %; counted by the kernel or under callgrind, else timed at +35 %), ns printed;
#                   target: loop instructions of the render functions in build/felucca.dis
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (firmware/src/eng_fm6.c, fm6_core.c, fm6_bank.c): the 32
#                   algorithms' carriers, the operator envelopes ending the voice, retrigger, DC / clipping, the
#                   eight macros, the patch formats (packed, SysEx), the 6-voice cap, the patch bank on a
#                   simulated NOR; demos in build/fm6_demo/.
# After an intended change of the sound: GOLDEN_UPDATE=1 sh tests/run_tests.sh, review the diff
# of tests/golden.txt, commit it with the change. After an intended change of the cost (or a new
# compiler): BUDGET_UPDATE=1 (rewrites cpu_baseline.txt and target_budget.txt). VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

[ -f build/felucca.fwsc ] || { echo "run ./build.sh first"; exit 1; }
# the generated headers the FM6 engine needs (tools/build.py generate() makes them too; no Pillow needed)
mkdir -p build/gen
[ build/gen/felucca_tables.h -nt tools/gen_tables.py ] || python3 tools/gen_tables.py build/gen/felucca_tables.h
[ build/gen/felucca_fm6.h -nt tools/gen_fm6_patches.py ] || python3 tools/gen_fm6_patches.py build/gen/felucca_fm6.h

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/recovery_test" tests/recovery_test.c
run "application USB recovery and boot-loop guard" "$OUT/recovery_test"

$CC -o "$OUT/arranger_test" tests/arranger_test.c
run "song order, timing, repeats and missing scenes" "$OUT/arranger_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/song_audio_test" tests/song_audio_test.c -lm
run "song: four simultaneous tracks, scene transition and stop" "$OUT/song_audio_test" "$OUT/song-demo.wav"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/song_ui_test" tests/song_ui_test.c -lm
run "song screen: commands, load (OCT+ twice), display bounds" "$OUT/song_ui_test" "$OUT/song-screen.ppm"

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/studio_drums_test" tests/studio_drums_test.c -lm
run "drum lanes, kit audio, metronome, record arm, free take" "$OUT/studio_drums_test" "$OUT/drum-styles.wav"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/seq2_test" tests/seq2_test.c -lm
run "sequencer 2.0: no drift, ratchets, roll, erase / undo, ghost / hard, chords, mute / solo, nudge, locks" "$OUT/seq2_test"

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumkit_test" tests/drumkit_test.c -lm
run "synthesised drum kits: every kit x sound bounded, audible, finite, levels, cost" "$OUT/drumkit_test" "$OUT/drum-kits.wav" "$OUT/drum-kits.txt"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/userkit_test" tests/userkit_test.c -lm
run "user drum kits (KIT USR1..USR3): a user slot's sounds on the drum lanes" "$OUT/userkit_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/dsyn_test" tests/dsyn_test.c -lm
run "SYN1..SYN4 drum kits and the editor's DRUM SYNTH commands (v10)" "$OUT/dsyn_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/font_test" tests/font_test.c
run "the screen font at 1 bit a pixel: every glyph, the same pixels as 2.5" "$OUT/font_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/punch_test" tests/punch_test.c -lm
run "punch-in FX: 27 effects (11 on the black keys), bounded, dry after release, FX-held keys" "$OUT/punch_test" "$OUT/punch-fx.wav"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx251_test" tests/fx251_test.c -lm
run "after Felucca 1.4 / 1.5: INSERT, filter TYPE, ENV SYNC, LFO 2, drum mutes, MIDI LEARN" "$OUT/fx251_test"

$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/ui_pages_test" tests/ui_pages_test.c -lm
run "live UI: pages, layers (punch, steps, erase, roll, key, mix), holds, drums, REC, fuzz" "$OUT/ui_pages_test" "$OUT"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/pick_test" tests/pick_test.c -lm
run "SMK-37 Pro pad sound picker: pages by kind, pads load, < >, drum kits" "$OUT/pick_test" "$OUT"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/fbank_test" tests/fbank_test.c -lm
run "SMK-37 Pro FADER BANK screen: banks wrap, what each fader sets, closes by itself" "$OUT/fbank_test" "$OUT"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/k4led_test" tests/k4led_test.c -lm
run "SMK-37 Pro: KNOB BANK lit on a PRESETS detent; which modes are a grid" "$OUT/k4led_test" "$OUT"
$CC -O2 -w -Ifirmware/hal -o "$OUT/padgrid_test" tests/padgrid_test.c
run "SMK-37 Pro pads: steps and tiles in reading order, notes low at the bottom, DRUM KIT / STEPS" "$OUT/padgrid_test"
$CC -w -Ifirmware/src -o "$OUT/smk_vel_test" tests/smk_vel_test.c
run "SMK-37 menu KEY VELOCITY: each mode -> the note's velocity, the drum level, the settings bits" "$OUT/smk_vel_test"
# every key / pad velocity the app reads goes through KEY VELOCITY (smk_vel.h vel_apply): the keys and pads (seq.c key_vel),
# the outer keys (smk_extkey_take); console.c shows the raw ones
vel_applied() {
    [ "$(grep -c 'fm1_in.key_vel' firmware/src/seq.c)" = 1 ] && grep -q 'vel_apply(vel_mode, fm1_in.key_vel' firmware/src/seq.c &&
    [ "$(grep -c 'smk_extkey_take' firmware/src/seq.c)" = 1 ] && grep -q 'vel_apply(vel_mode, (uint32_t)e & 0xFFu)' firmware/src/seq.c &&
    ! grep -n -e 'fm1_in.key_vel' -e 'smk_extkey_take' firmware/src/*.c | grep -v -e '^firmware/src/seq.c' -e '^firmware/src/console.c' &&
    echo applied
}
run "SMK-37 menu KEY VELOCITY: applied wherever the app reads a key or pad velocity" vel_applied
$CC -o "$OUT/smk_wheel_test" tests/smk_wheel_test.c
run "SMK-37 wheels: BEND UP / DOWN, the calibration (full travel = full range, re-centre), back to 0 at rest" "$OUT/smk_wheel_test"
$CC -O2 -w -DSMK_MIX=1 -Ibuild/gen -Ifirmware/src -o "$OUT/smk_mix_test" tests/smk_mix_test.c -lm
run "SMK-37 LIMITER: SOFT leaves held chords alone at power-on, catches peaks, no clipping; CLASSIC as before" "$OUT/smk_mix_test"
$CC -o "$OUT/smk_knobs_test" tests/smk_knobs_test.c
run "SMK-37 KNOB LAYOUT: FM-1 / SMK, each knob one role, calibration per printed knob, labels" "$OUT/smk_knobs_test"
$CC -o "$OUT/smk_midi_out_test" tests/smk_midi_out_test.c
run "SMK-37 TRS MIDI OUT: the bytes, the ring, one byte at a time, OFF, no UART: no hang" "$OUT/smk_midi_out_test"
# every USB MIDI out packet goes to the TRS jack too (usb.c midi_out_event, before the USB check), and the TIMER5 tick
# drains it
trs_fed() {
    sed -n '/^static void midi_out_event/,/^}/p' firmware/src/usb.c | grep -q 'smo_put(pkt);' &&
    [ "$(grep -c 'midi_out_q\[mo_w % MQ\] =' firmware/src/usb.c)" = 1 ] &&
    sed -n '/^void fm1_timer5_irq/,/^}/p' firmware/src/main.c | grep -q 'smo_tick();' && echo fed
}
run "SMK-37 TRS MIDI OUT: fed from every USB MIDI out packet, drained by the TIMER5 tick" trs_fed
# the pads' DRUM KIT records into the drum track whatever track is armed; SEQ PLAY / REC work in the HOME menu
pads_rec_menu() {
    sed -n '/^static void pad_drum(/,/^}/p' firmware/src/seq.c | grep -q 'rec_hit(TDRUM, lane, lvl, 0);' &&
    sed -n '/^    if (ui.menu) {/,/^    }/p' firmware/src/ui_input.c | grep -q 'rec_toggle();' &&
    sed -n '/^    if (ui.menu) {/,/^    }/p' firmware/src/ui_input.c | grep -q 'transport_req = song.playing ? 2 : 1;' && echo checked
}
run "SMK-37 pads: DRUM KIT hits recorded into the drum track; PLAY / REC work in the HOME menu" pads_rec_menu
$CC -o "$OUT/smk_daw_test" tests/smk_daw_test.c
run "SMK-37 DAW mode: the DEFAULT map, types, MIDI bytes (CC, notes, bend, MMC), knobs, toggles, rate, flash object" "$OUT/smk_daw_test"
$CC -O2 -w -DSMK_MIX=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Itests -o "$OUT/daw_ui_test" tests/daw_ui_test.c -lm
run "SMK-37 DAW mode on the UI: < + >, knobs / faders / buttons / wheels, banks, PATCH, the editor, the menu" "$OUT/daw_ui_test" "$OUT"
# DAW mode, the audio context's part: the keys and pads go to seq.c daw_* (not the synth) while in DAW mode, the pads
# whatever the layer (smk_input.h SMK_EXT_DAW), a release where its press went; the faders, the wheels, the volume knob
# leave SLOOP alone; the store's own object (OBJ_DAW), saved with the settings
daw_routed() {
    sed -n '/^static void keyboard_block(void)/,/^}/p' firmware/src/seq.c | grep -q 'if (daw_isr) {' &&
    sed -n '/^static void ext_keys_block(void)/,/^}/p' firmware/src/seq.c | grep -q 'daw_pad(k - SMK_EXT_DAW, v);' &&
    sed -n '/^static void smk__padq(/,/^}/p' firmware/hal/smk_input.h | grep -q 'if (smk_pad_daw) {' &&
    sed -n '/^static void smk_faders(void)/,/^}/p' firmware/src/main.c | grep -q 'if (daw_on) {' &&
    sed -n '/^static void smk_wheels(void)/,/^}/p' firmware/src/main.c | grep -q 'daw_wheels(b, m);' &&
    grep -q 'if (smk_master_hold)' firmware/hal/smk_input.h &&
    sed -n '/^\(UI_SMALL \)*static void settings_save(void)/,/^}/p' firmware/src/project.c | grep -q 'daw_save();' && echo routed
}
run "SMK-37 DAW mode: keys, pads, faders, wheels and the volume knob routed; its settings kept" daw_routed
# the stock firmware's mapping in USRFLASH: its end marker erased before the first erase there, at boot over our data
$CC -DBOARD_SMK37=1 -Ifirmware/hal -o "$OUT/smk_stock_test" tests/smk_stock_test.c
run "SMK-37 the way back to M-Vave: the stock mapping's marker released before the first USRFLASH erase, once" "$OUT/smk_stock_test"
stock_hooked() {
    sed -n '/^static int fl_erase4k_quiet(/,/^}/p' firmware/src/felucca.c | grep -q 'st_stock_touch(off);' &&
    sed -n '/^\(UI_SMALL \)*static void persist_boot(void)/,/^}/p' firmware/src/project.c | grep -q 'st_stock_boot();' &&
    [ "$(cat firmware/src/*.c | grep -c 'fl_erase4k_ram\|fl_erase4k(')" = 1 ] && echo hooked
}
run "SMK-37 every app erase goes through the stock marker's hook; the boot check runs" stock_hooked
# no divide by 0 (the FM-1 runs with the div0 trap off, hal/fm1_irq.h: a real one would give a wrong value
# silently): the UI fuzz, the sequencer, the projects and a minute of random live use, with UBSan
UBSAN="${CC_UB:-cc} -O1 -w -fsanitize=integer-divide-by-zero -fno-sanitize-recover=integer-divide-by-zero -Ibuild/gen -Ifirmware/src -Ifirmware/hal"
$UBSAN -o "$OUT/ui_pages_ub" tests/ui_pages_test.c -lm && $UBSAN -o "$OUT/seq2_ub" tests/seq2_test.c -lm &&
    $UBSAN -o "$OUT/project_ub" tests/project_test.c -lm && $UBSAN -o "$OUT/soak_ub" tests/soak_test.c -lm || fail=1
mkdir -p "$OUT/ub"
run "no divide by zero (UBSan): UI fuzz, sequencer, projects, a minute of live use" \
    sh -c "'$OUT/ui_pages_ub' '$OUT/ub' >/dev/null && '$OUT/seq2_ub' >/dev/null && '$OUT/project_ub' >/dev/null && '$OUT/soak_ub' 1 >/dev/null && echo 'no divide by zero'"

# the second core's host double (tests/cpu1_host.c, firmware/src/cpu1.h): inline, a thread, a slow thread, a lost
# job; and the thread under ThreadSanitizer (the render's shared state shows up here once it uses core 1)
$CC -w -Ifirmware/src -o "$OUT/cpu1_test" tests/cpu1_test.c
run "second core, host double: inline, deferred to the wait (CORES=1)" env CORES=1 "$OUT/cpu1_test"
run "second core, host double: a thread (CORES=2)" env CORES=2 "$OUT/cpu1_test"
run "second core, host double: a thread with random delays (CORES=2 CHAOS=7)" env CORES=2 CHAOS=7 "$OUT/cpu1_test"
run "second core, host double: a lost job, the hold and the fallback (CPU1_TIMEOUT_AT=777)" env CORES=2 CPU1_TIMEOUT_AT=777 "$OUT/cpu1_test"
TSAN="${CC_UB:-cc} -O1 -g -w -fsanitize=thread -Ifirmware/src"
if $TSAN -o "$OUT/cpu1_tsan" tests/cpu1_test.c 2>/dev/null; then
    run "second core, host double under ThreadSanitizer (CORES=2 CHAOS=3)" env CORES=2 CHAOS=3 "$OUT/cpu1_tsan"
else
    echo "(cpu1 under TSan: this compiler has no ThreadSanitizer, skipped)"
fi

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/soak_test" tests/soak_test.c -lm
run "soak: ${SOAK_MIN:-10} minutes of random live use (bounded, no hanging voices, idle after stop)" "$OUT/soak_test" "${SOAK_MIN:-10}"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/stress_test" tests/stress_test.c -lm
run "stress (2.4): FONT_L = FONT_S at 2x, hard random use of every 2.4 addition, clean stop" "$OUT/stress_test" "${STRESS_FRAMES:-40000}"
# the same under AddressSanitizer + UBSan: any read or write out of bounds stops it (left shifts of negative values
# and the FM6 phase's wrap-around are left out: the DSP's two's complement idioms, as every compiler builds them)
ASAN="${CC_UB:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src -Ifirmware/hal"
if $ASAN -o "$OUT/stress_asan" tests/stress_test.c -lm 2>/dev/null; then
    run "stress under ASan + UBSan (no access out of bounds)" "$OUT/stress_asan" "${STRESS_ASAN_FRAMES:-15000}" 7
else
    echo "(stress under ASan: this compiler has no AddressSanitizer, skipped)"
fi

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions)" "$OUT/upreset_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"
$CC -Ifirmware/hal -o "$OUT/encoder_test" tests/encoder_test.c
run "knobs: one click = one step (slow, fast, pauses, bounce)" "$OUT/encoder_test"
HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets" "$OUT/uac_test"
$CC -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets" "$OUT/uac_test_nocdc"
$CC -DT_CDC=2 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_seroff" tests/uac_test.c -lm
run "USB audio input: descriptors (CDC built in, menu USB SERIAL OFF), ring and packets" "$OUT/uac_test_seroff"
run "USB SERIAL OFF: the descriptors of a build without CDC, byte for byte" \
    sh -c "[ \"\$(UAC_DUMP=1 '$OUT/uac_test_seroff' | tail -n 2)\" = \"\$(UAC_DUMP=1 '$OUT/uac_test_nocdc' | tail -n 2)\" ] && echo same"
$CC -DT_CDC=2 -DT_RATE=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_441" tests/uac_test.c -lm
run "USB audio input, menu USB RATE 44.1 (2.7): descriptors, ring and packets" "$OUT/uac_test_441"
$CC -DT_CDC=1 -DT_RATE=2 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_48" tests/uac_test.c -lm
run "USB audio input, menu USB RATE 48 (2.7): descriptors, ring and packets" "$OUT/uac_test_48"
$CC -w -DT_CDC=0 -DFELUCCA_UAC_48K=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_no48" tests/uac_test.c -lm
run "USB RATE 44.1: the descriptors of a build without 48 kHz (SLOOP 2.4), byte for byte" \
    sh -c "[ \"\$(UAC_DUMP=1 '$OUT/uac_test_441' | tail -n 2)\" = \"\$(UAC_DUMP=1 '$OUT/uac_test_no48' | tail -n 2)\" ] && echo same"
uac_in_app() { ${CC%% *} -E -Ibuild/gen -Ifirmware/hal -Ifirmware/src firmware/src/felucca.c 2>/dev/null | grep -q uac_service; }
run "USB audio input: built into the firmware (FELUCCA_UAC set before usb.c)" uac_in_app

$CC -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry" "$OUT/ota_test" build/felucca.fwsc

head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc

# SMK-37 Pro (BOARD_SMK37): the flash map, the OTA staging and the loader windows on SMK-layout packages
# (made here from the FM-1 binaries: only the layout and the protocol are under test)
$CC -DBOARD_SMK37=1 -Ifirmware/hal -o "$OUT/smk_flash_test" tests/smk_flash_test.c
run "SMK-37 Pro flash map: allow-lists never reach the head, BTIF/USRTRIM, USR, key_mac" "$OUT/smk_flash_test"
python3 tools/fm1pkg_make.py --board smk37 build/felucca.bin build/loader/ota.bin "$OUT/smk_new.fwsc" >/dev/null
python3 tools/fm1pkg_make.py --board smk37 "$OUT/old_app.bin" build/loader/ota.bin "$OUT/smk_old.fwsc" >/dev/null
run "SMK-37 Pro package layout (fm1pkg_make --board smk37): PRCT 0x9D000, stock region table"     python3 tests/smk_pkg_test.py "$OUT/smk_new.fwsc"
$CC -DBOARD_SMK37=1 -o "$OUT/ota_test_smk" tests/ota_test.c
run "M-UPGRADE entry, SMK-37 Pro build (staging 0xBB000, parm ota-SMK-37 Pro_016)" "$OUT/ota_test_smk" "$OUT/smk_new.fwsc"
for m in 1 2; do                         # the MODEL=elite / mke builds of ota.c
    $CC -DBOARD_SMK37=1 -DT_MODEL=$m -o "$OUT/ota_test_smk_m$m" tests/ota_test.c
    run "M-UPGRADE entry, SMK-37 model $m (1 Elite, 2 MKE-P37): identity and parm under its name" "$OUT/ota_test_smk_m$m" "$OUT/smk_new.fwsc"
done
for m in "elite:SMK-37 Elite_900" "mke:MKE-P37_900"; do
    f="build/sloop-smk37-${m%%:*}.fwsc"
    [ -f "$f" ] && run "SMK-37 ${m%%:*} build package: layout, identity ${m#*:}" python3 tests/smk_pkg_test.py "$f" "${m#*:}"
done
$CC -DBOARD_SMK37=1 -o "$OUT/ldr_test_smk" tests/ldr_test.c
run "update loader, SMK-37 Pro: app area 0x4000-0x9CFFF, protected ranges untouched"     "$OUT/ldr_test_smk" "$OUT/smk_old.fwsc" "$OUT/smk_new.fwsc"

$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
mkdir -p build/tracks_demo
run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
$CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
run "project formats (FUN4 / FUN3 / FUN2 / FUN1 -> FUN5), capture / apply, autosave" "$OUT/project_test"
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
mkdir -p build/slicer_demo
run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
mkdir -p build/fm6_demo
run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, the bank, demos" "$OUT/fm6_test" build/fm6_demo
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
# the CPU budget: counted by the kernel on macOS; elsewhere under callgrind when valgrind is there (exact, ~45 s;
# SKIP_CPU_VALGRIND=1 to time instead, which is only a rough check)
if [ -z "$SKIP_CPU_VALGRIND" ] && command -v valgrind >/dev/null 2>&1; then export CPU_VALGRIND=1; fi
run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
# SLICE (tests/slice_test.c) needs a FELUCCA_SLICE=1 build; the engine is not built by default

run "regression: target cost of the render loops" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt
# the render split over the two cores (fx.c mix_units): the golden renders and tests/mix_split_test.c (three
# FORMANT, FM6 or PHYS parts, S&H LFOs, mutes, the drums through the SLICER and the FILTER, the metronome: every
# split of the units against none) under every mode of the host double of core 1, and under ThreadSanitizer
run "the render split over the two cores: every mode of the second core's host double (tests/cpu1_modes.sh)" sh tests/cpu1_modes.sh

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py
run "the offline editor (one file, nothing next to it); the offline site (sw.js, manifest)" python3 tests/site_test.py

if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol, samples, packages, update protocol" node web/test_web.mjs
    rm -rf "$OUT/pwa-site" && python3 web/make_site.py build/felucca.fwsc 2.7 "$OUT/pwa-site" >/dev/null &&
        run "the site without internet in Chromium (skipped without Playwright)" node tests/pwa_test.mjs "$OUT/pwa-site"
else
    echo "== skip web tests (no node)"
fi

# the browser audio preview (web editor): the same engines in WASM as on the host, when it is built
# (python3 tools/build_preview.py: Emscripten, or clang + wasm-ld)
if [ -f build/preview/sloop-preview.wasm ]; then
    run "the editor's audio preview: WASM == host, all 12 engines" sh tests/run_preview_tests.sh
fi
[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
