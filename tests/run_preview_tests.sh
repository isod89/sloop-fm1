#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run after python3 tools/build_preview.py; no FM-1 or JieLi SDK required.
set -eu
cd "$(dirname "$0")/.."
[ -f build/preview/sloop-preview.wasm ] || { echo 'Run python3 tools/build_preview.py first'; exit 1; }
mkdir -p build/host
# clang (as emcc, the WASM side): the GCC build of this adapter does not finish on Linux
CC="${CC:-$(command -v clang || echo cc)}"
"$CC" -O2 -fwrapv -fno-strict-aliasing -w -Ibuild/gen -Ifirmware/src tests/preview_native.c -o build/host/preview_native
node tests/preview_test.mjs
