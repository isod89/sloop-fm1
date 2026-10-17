#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# SLOOP in the browser: the firmware's sources against a simulated FM-1 (web/emu/sloop_web.c), compiled to
# WebAssembly with clang + wasm-ld (freestanding, tools/wasm_shim), plus the page.
#
# The design follows Felucca's web/emu/build.sh (GPL-3.0, Leo Kuroshita / Hügelton Instruments), itself
# after X0X's (charlesvestal/fm1-x0x, GPL-3.0). Emscripten works too: set EMCC=1.
#   web/emu/build.sh   ->  build/emu/{index.html, worklet.js, sloop.wasm, fonts/}
# CC=... to pick another clang (llvm-mingw, LLVM, emsdk's).
set -e
cd "$(dirname "$0")/../.."

GEN=build/gen
OUT=build/emu
mkdir -p "$GEN" "$OUT/fonts"

# (python3 first where it is real; on Windows the Store alias answers command -v but not --version)
for PY in "${PYTHON:-}" python3 python; do
    [ -n "$PY" ] || continue
    "$PY" --version >/dev/null 2>&1 && break
    PY=
done
[ -n "$PY" ] || { echo "emu: no python"; exit 1; }

# the generated headers: the tables, the CC0 samples, the drum kits, the FM6 patches and the boot logo
"$PY" tools/gen_tables.py "$GEN/felucca_tables.h"
"$PY" tools/gen_samples.py "$GEN/felucca_samples.h"
"$PY" tools/gen_drumkits.py "$GEN/felucca_drumkits.h"
"$PY" tools/gen_fm6_patches.py "$GEN/felucca_fm6.h"
"$PY" tools/gen_font.py "$GEN/felucca_font.h"
"$PY" tools/gen_icons.py "$GEN/felucca_icons.h"
"$PY" tools/gen_logo.py "$GEN/sloop_logo.h"

# (SLOOP keeps the version in ui.c; Felucca had it in felucca.c)
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/ui.c firmware/src/felucca.c 2>/dev/null | head -n 1)
[ -n "$VERSION" ] || VERSION=dev

EXPORTS="web_nor web_nor_size web_nor_erase web_flash_writes_count web_master web_boot web_sample_rate
web_now_ms web_buttons web_keys web_enc web_midi web_screen web_screen_draws web_lit_buttons web_lit_keys
web_dim_buttons web_dim_keys web_dim_level web_breath_buttons web_breath_keys web_mid_keys
web_out_l web_out_r web_render web_debug_buttons web_debug_notes
web_debug_obj_crc web_upload_buf web_upload_size web_obj_put web_order_put"

if [ "${EMCC:-0}" = 1 ]; then
    command -v emcc >/dev/null 2>&1 || { echo "emu: no emcc"; exit 1; }
    WEXP=$(for n in $EXPORTS; do printf -- "-Wl,--export=%s " "$n"; done)
    # emcc: the same freestanding entry as the clang path (STANDALONE_WASM gives _initialize)
    emcc -O2 -ffp-contract=off -fwrapv -fno-strict-aliasing -w ${EMU_CFLAGS:-} \
        "-DFELUCCA_VERSION=\"$VERSION\"" -I"$GEN" -Ifirmware/src \
        --no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=1048576 \
        $(for n in $EXPORTS; do printf -- "-sEXPORTED_FUNCTIONS=_%s " "$n"; done) \
        -o "$OUT/sloop.wasm" web/emu/sloop_web.c tools/wasm_shim/mem.c
else
    CC=${CC:-clang}
    command -v "$CC" >/dev/null 2>&1 || { echo "emu: no clang (set CC=, or EMCC=1 with Emscripten)"; exit 1; }
    WEXP=$(for n in $EXPORTS; do printf -- "-Wl,--export=%s " "$n"; done)
    # shellcheck disable=SC2086
    "$CC" --target=wasm32 -O2 -ffp-contract=off -fwrapv -fno-strict-aliasing -ffreestanding -nostdlib -w \
        ${EMU_CFLAGS:-} "-DFELUCCA_VERSION=\"$VERSION\"" \
        -Itools/wasm_shim -I"$GEN" -Ifirmware/src \
        web/emu/sloop_web.c tools/wasm_shim/mem.c \
        -Wl,--no-entry -Wl,--export-memory -Wl,--initial-memory=67108864 -Wl,-z,stack-size=1048576 \
        $WEXP -o "$OUT/sloop.wasm"
fi

sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/ (web)/g" web/emu/index.html > "$OUT/index.html"
cp web/emu/worklet.js "$OUT/"
if [ -d web/emu/fonts ]; then cp web/emu/fonts/* "$OUT/fonts/"; fi
echo "emu: $OUT/sloop.wasm ($(wc -c < "$OUT/sloop.wasm" | tr -d ' ') B), version $VERSION"
