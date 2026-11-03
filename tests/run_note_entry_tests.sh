#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Standalone NOTES-entry regressions. Needs Python/Pillow and a host C compiler;
# no device, target toolchain or other feature branch is required.
set -eu
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-python3}"
CC="${CC:-cc}"
OUT=build/note-entry
mkdir -p "$OUT"
"$PYTHON" - <<'PY'
import sys
sys.path.insert(0, 'tools')
from build import generate
generate()
PY
for test in note_entry_test note_entry_settings_test scale_test ui_pages_test len_test project_test; do
    "$CC" -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal "tests/$test.c" -lm -o "$OUT/$test"
    if ! "$OUT/$test" "$OUT" > "$OUT/$test.log" 2>&1; then
        cat "$OUT/$test.log"
        exit 1
    fi
    tail -n 1 "$OUT/$test.log"
done
echo "NOTE_ENTRY_HOST_SUITE_OK"
