#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the FM-1 firmware and run the host tests on Windows, in a Linux container (Docker Desktop):
no WSL distribution needed. The tree is mounted at /work; the JieLi toolchain runs natively inside.

  py -3 tools/docker_build.py fw [build.py args...]   build/felucca.fwsc (tools/build.py)
  py -3 tools/docker_build.py test                    tests/run_tests.sh
  py -3 tools/docker_build.py deps                    fetch the toolchain and the 3 SDK files

Dependencies live in build/deps (not in git): the JieLi Linux toolchain (tools/get_toolchain.sh's
source) in build/deps/jieli, the AC79 SDK files tools/build.py checks (SDK_SHA256) in build/deps/ac79.
"""
import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / "build" / "deps"
IMAGE = "python:3.12-bookworm"            # Debian with gcc, curl, xz (the toolchain's libraries too)
TOOLCHAIN_URL = "https://pkgman.jieliapp.com/s/linux-toolchain"
SDK_URL = "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/AC79NN_SDK_V1.2.1_2023-12-13/cpu/wl82/tools/"


def docker(script, env=()):
    cmd = ["docker", "run", "--rm", "-v", f"{ROOT}:/work", "-w", "/work"]
    for e in env:
        cmd += ["-e", e]
    cmd += [IMAGE, "sh", "-c", script]
    return subprocess.run(cmd).returncode


def toolchain():
    found = sorted((DEPS / "jieli").glob("jieli-linux-toolchains-*"))
    return found[-1] if found else None


def deps():
    if not toolchain():
        (DEPS / "jieli").mkdir(parents=True, exist_ok=True)
        print("--> JieLi toolchain")
        if docker(f"curl -fsSL --retry 3 {TOOLCHAIN_URL} | tar -xJ -C /work/build/deps/jieli"):
            raise SystemExit("docker_build: toolchain download failed")
    sys.path.insert(0, str(ROOT / "tools"))
    from build import SDK_SHA256
    for rel, sha in SDK_SHA256.items():
        p = DEPS / "ac79" / "cpu" / "wl82" / "tools" / rel
        if p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == sha:
            continue
        print(f"--> SDK {rel}")
        p.parent.mkdir(parents=True, exist_ok=True)
        for _ in range(4):
            try:
                req = urllib.request.Request(SDK_URL + rel, headers={"User-Agent": "Mozilla/5.0"})
                data = urllib.request.urlopen(req, timeout=60).read()
                break
            except OSError:
                continue
        else:
            raise SystemExit(f"docker_build: could not download {rel}")
        if hashlib.sha256(data).hexdigest() != sha:
            raise SystemExit(f"docker_build: {rel} does not match AC79NN_SDK_V1.2.1")
        p.write_bytes(data)


def env():
    tc = toolchain().relative_to(ROOT).as_posix()
    return ["JIELI_DOCKER=0", f"JIELI_TOOLCHAIN=/work/{tc}", "AC79_SDK=/work/build/deps/ac79"]


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("fw", "test", "deps"):
        raise SystemExit(__doc__)
    deps()
    if sys.argv[1] == "deps":
        return 0
    pillow = "pip install -q --root-user-action=ignore Pillow 2>/dev/null; "
    if sys.argv[1] == "fw":
        args = " ".join(f"'{a}'" for a in sys.argv[2:])
        return docker(pillow + f"python3 tools/build.py {args}", env())
    return docker(pillow + "sh tests/run_tests.sh", env())


if __name__ == "__main__":
    sys.exit(main())
