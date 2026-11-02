#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The editor protocol / external input fuzz (tests/editor_fuzz.c) under AddressSanitizer + UBSan.
#   sh tests/editor_fuzz.sh [ITER] [SEED]     (from the repo root, after ./build.sh)
set -e
cd "$(dirname "$0")/.."
OUT=build/host/edfz
mkdir -p "$OUT/gen"
# SMP_DATA read through a pointer: the user sample slots are addressed as offsets from it (one flash), which
# -fsanitize=bounds would take for an overflow of the array; AddressSanitizer still checks every byte
{ echo '#define SMP_DATA (smp_data_v)'; sed 's/^static const uint8_t SMP_DATA\[/static const uint8_t SMP_DATA_ARR[/' build/gen/felucca_samples.h;
  echo 'static const uint8_t *volatile smp_data_v = SMP_DATA_ARR;'; } > "$OUT/gen/felucca_samples.h"
${CC:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all \
    -I"$OUT/gen" -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/editor_fuzz" tests/editor_fuzz.c -lm
N=${1:-20000}
S=${2:-1}
"$OUT/editor_fuzz" 0 $((N * 10)) "$S"        # single frames, every command, lengths at the edges
"$OUT/editor_fuzz" 1 "$N" "$S"               # sequences: uploads, restores, presets, FM6, SYN kits, LIVE, UI, audio
"$OUT/editor_fuzz" 2 "$N" "$S"               # USB-MIDI and TRS bytes in, with the rest
"$OUT/editor_fuzz" 3 $((N / 20)) "$S"        # boots from a damaged flash (a child each)
"$OUT/editor_fuzz" 4 1 "$S"                  # every command over every engine / track / id / step
"$OUT/editor_fuzz" 5 "$N" "$S"               # MIDI in only, then no note may be left held
"$OUT/editor_fuzz" 6 $((N / 100)) "$S"       # backup / section / autosave round trips give the project back
# the reproducers of what it found (tests/editor_repro.c): each must run clean once fixed
${CC:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all \
    -I"$OUT/gen" -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/editor_repro" tests/editor_repro.c -lm
for r in 1 2 3; do "$OUT/editor_repro" $r; done
# (2.8.4) power-on after an install: the parts sound as their sounds, a working project Clipper left: default sounds
${CC:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all \
    -I"$OUT/gen" -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/boot_sound_test" tests/boot_sound_test.c -lm
"$OUT/boot_sound_test" > "$OUT/boot_sound.log" || { cat "$OUT/boot_sound.log"; exit 1; }
tail -n 1 "$OUT/boot_sound.log"
