#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the SLOOP FM-1 emulator for Windows (MSVC): firmware/src/felucca.c, unchanged, over the
shadow HAL in tools/emulator/hal, plus the Win32 panel (tools/emulator/emu_win32.c).

  py -3 tools/emulator/build_emu.py [--force] [--no-static-assert] [--run [emulator args...]]

The firmware's _Static_asserts stay on, as in the device build: a tree that would not compile for
the FM-1 does not compile here. --no-static-assert builds it anyway (to look around; what the
asserts guard, e.g. a project fitting one flash sector, is then broken).

Output: build/emu/sloop_emu.exe (flash image: build/emu/sloop_flash.bin). Rebuilds only when
firmware/src, tools/emulator or build/gen changed; regenerates build/gen headers when missing or
older than their generator.
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EMU = ROOT / "tools" / "emulator"
OUT = ROOT / "build" / "emu"
FWSRC = OUT / "fwsrc"
EXE = OUT / "sloop_emu.exe"
GEN = ROOT / "build" / "gen"
GENERATED = {"felucca_font.h": "gen_font.py", "felucca_icons.h": "gen_icons.py",
             "felucca_tables.h": "gen_tables.py", "felucca_samples.h": "gen_samples.py",
             "felucca_drumkits.h": "gen_drumkits.py", "sloop_logo.h": "gen_logo.py"}
VCVARS = [
    r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
]
DEFINES = ["FELUCCA_OTA=0", "FELUCCA_CDC=0"]   # no update / console paths (USB audio, MIDI jack: as built)


def generate():
    stale = [h for h, g in GENERATED.items()
             if not (GEN / h).exists() or (GEN / h).stat().st_mtime < (ROOT / "tools" / g).stat().st_mtime]
    if not stale:
        return
    print(f"--> generating build/gen ({', '.join(stale)})")
    sys.path.insert(0, str(ROOT / "tools"))
    os.chdir(ROOT)
    import build
    build.generate()


def write_if_changed(p, text):
    if not p.exists() or p.read_text(encoding="utf-8") != text:
        p.write_text(text, encoding="utf-8")


def copy_sources():
    """felucca.c, usb.c and midi_uart.c into build/emu/fwsrc: usb.c and midi_uart.c include their HAL
    ("../hal/fm1_usb.h", "../hal/fm1_uart.h") by a relative path, which would find the real registers,
    not the shadow; the copies include the shadow's. The felucca.c copy only makes its #includes of
    those two find the copies. Nothing else changes."""
    FWSRC.mkdir(parents=True, exist_ok=True)
    src = ROOT / "firmware" / "src"
    write_if_changed(FWSRC / "felucca.c", (src / "felucca.c").read_text(encoding="utf-8"))
    for name, hal in (("usb.c", "fm1_usb.h"), ("midi_uart.c", "fm1_uart.h")):
        text = (src / name).read_text(encoding="utf-8")
        old = f'#include "../hal/{hal}"'
        if old not in text:
            raise SystemExit(f"build_emu: {name} no longer includes ../hal/{hal}; update copy_sources()")
        line = '#line 1 "' + (src / name).as_posix() + '"\n'
        write_if_changed(FWSRC / name, line + text.replace(old, f'#include "{hal}"'))


def newest(*dirs):
    t = 0.0
    for d in dirs:
        for p in d.rglob("*"):
            if p.is_file() and p.suffix in (".c", ".h"):
                t = max(t, p.stat().st_mtime)
    return t


def compile_(lax):
    vcvars = next((p for p in VCVARS if os.path.exists(p)), None)
    if not vcvars:
        raise SystemExit("build_emu: MSVC not found (Visual Studio 2022 / Build Tools, C++ workload)")
    inc = [FWSRC, EMU / "hal", ROOT / "firmware" / "src", GEN]
    args = ["cl", "/nologo", "/O2", "/W3", "/std:c11", "/utf-8", "/MP", "/Zi",
            *[f'/I"{p}"' for p in inc], *[f"/D{d}" for d in DEFINES + (["EMU_NO_STATIC_ASSERT"] if lax else [])],
            f'/Fo"{OUT}\\\\"', f'/Fd"{OUT}\\\\"', f'/Fe"{EXE}"',
            f'"{EMU / "emu_firmware.c"}"', f'"{EMU / "emu_win32.c"}"',
            "/D_CRT_SECURE_NO_WARNINGS",
            "/link", "/DEBUG", "/INCREMENTAL:NO", "/SUBSYSTEM:WINDOWS", "winmm.lib", "user32.lib", "gdi32.lib"]
    cmd = f'call "{vcvars}" x64 >nul 2>nul && ' + " ".join(args)
    print("--> compiling build/emu/sloop_emu.exe (MSVC)")
    r = subprocess.run(cmd, shell=True, cwd=ROOT)
    if r.returncode:
        raise SystemExit("build_emu: compile failed (errors above). A failed static_assert fails the FM-1 "
                         "build too; --no-static-assert builds it anyway")


def main():
    argv = sys.argv[1:]
    run_args = argv[argv.index("--run") + 1:] if "--run" in argv else None
    own = argv[: argv.index("--run")] if run_args is not None else argv
    force, lax = "--force" in own, "--no-static-assert" in own
    OUT.mkdir(parents=True, exist_ok=True)
    stamp = OUT / "lax.flag"
    if lax != stamp.exists():                      # the other flavour was built last
        force = True
        stamp.touch() if lax else stamp.unlink()
    generate()
    copy_sources()
    src_t = newest(ROOT / "firmware" / "src", EMU, GEN)
    if force or not EXE.exists() or EXE.stat().st_mtime < src_t:
        compile_(lax)
    else:
        print("--> build/emu/sloop_emu.exe is up to date")
    if run_args is not None:
        subprocess.Popen([str(EXE), *run_args], cwd=OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
