#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile the real SLOOP DSP/clock for browser preview (requires emcc).

python3 tools/build_preview.py
Then web/make_site.py includes the generated audio assets in the local site.
No JieLi SDK, Docker, or connected FM-1 is required.
"""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'build/gen'
OUT = ROOT / 'build/preview'


def main():
    emcc = shutil.which('emcc')
    if not emcc and not (shutil.which('clang') and shutil.which('wasm-ld')):
        sys.exit('Install Emscripten (macOS: brew install emscripten), or clang and lld (wasm-ld).')
    GEN.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    for script, header in [('gen_tables.py', 'felucca_tables.h'), ('gen_samples.py', 'felucca_samples.h'),
                           ('gen_drumkits.py', 'felucca_drumkits.h'), ('gen_fm6_patches.py', 'felucca_fm6.h')]:
        subprocess.run([sys.executable, str(ROOT / 'tools' / script), str(GEN / header)], cwd=ROOT, check=True)
    exports = ['init', 'buffer', 'frames', 'rate', 'track', 'param', 'global', 'select', 'step', 'locks',
               'fm6', 'sample', 'kit', 'hit', 'note', 'play', 'position', 'panic', 'render']
    if not emcc:                                     # clang + wasm-ld, freestanding, with tools/wasm_shim
        shim = ROOT / 'tools' / 'wasm_shim'
        mem = OUT / 'mem.o'
        subprocess.run(['clang', '--target=wasm32', '-O2', '-fno-builtin', '-ffreestanding', '-c', str(shim / 'mem.c'),
                        '-o', str(mem)], cwd=ROOT, check=True)
        subprocess.run(['clang', '--target=wasm32', '-O2', '-fwrapv', '-fno-strict-aliasing', '-ffreestanding', '-nostdlib',
                        '-w', '-I' + str(shim), '-I' + str(GEN), '-I' + str(ROOT / 'firmware/src'),
                        str(ROOT / 'firmware/host/preview.c'), str(mem), '-Wl,--no-entry', '-Wl,--export-memory',
                        '-Wl,--initial-memory=16777216', '-Wl,-z,stack-size=131072']
                       + ['-Wl,--export=preview_' + n for n in exports] + ['-o', str(OUT / 'sloop-preview.wasm')],
                       cwd=ROOT, check=True)
        mem.unlink()
    else:
      subprocess.run(['emcc', str(ROOT / 'firmware/host/preview.c'), '-O2', '-fwrapv', '-fno-strict-aliasing',
                    '-Wno-unused-function', '-Wno-int-to-pointer-cast', '-I' + str(GEN), '-I' + str(ROOT / 'firmware/src'),
                    '--no-entry', '-sSTANDALONE_WASM=1', '-sFILESYSTEM=0', '-sINITIAL_MEMORY=16777216',
                    '-sSTACK_SIZE=131072', '-sEXPORTED_FUNCTIONS=' + str(['_preview_' + n for n in exports]),
                    '-o', str(OUT / 'sloop-preview.wasm')], cwd=ROOT, check=True)
    for name in ['preview.js', 'preview-worklet.js']:
        shutil.copy(ROOT / 'web' / name, OUT / name)
    print('Browser audio built:', OUT / 'sloop-preview.wasm')


if __name__ == '__main__':
    main()
