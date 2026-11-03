#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Host-only checks for scale lighting and the existing keyboard/UI. No SDK, Docker or FM-1 needed.
set -eu
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-python3}"
CC="${CC:-cc}"
OUT=build/scale-lights
mkdir -p "$OUT" build/gen

generate() {
    script="$1"
    header="$2"
    if [ ! -f "build/gen/$header" ] || [ "tools/$script" -nt "build/gen/$header" ]; then
        "$PYTHON" "tools/$script" "build/gen/$header"
    fi
}
generate gen_tables.py felucca_tables.h
generate gen_fm6_patches.py felucca_fm6.h
generate gen_font.py felucca_font.h
generate gen_icons.py felucca_icons.h
generate gen_samples.py felucca_samples.h
generate gen_drumkits.py felucca_drumkits.h
generate gen_logo.py sloop_logo.h

for test in led_pwm_test scale_lights_test scale_test ui_pages_test len_test; do
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal "tests/$test.c" -lm -o "$OUT/$test"
    if ! "$OUT/$test" "$OUT" > "$OUT/$test.log" 2>&1; then
        cat "$OUT/$test.log"
        exit 1
    fi
    tail -n 1 "$OUT/$test.log"
done
echo "SCALE_LIGHTS_HOST_SUITE_OK"
