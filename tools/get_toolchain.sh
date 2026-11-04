#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Download JieLi's Linux toolchain (clang 4.0.1 for pi32v2) and link it as DEST/toolchain.
#   tools/get_toolchain.sh [DEST]      (default: ~/.jieli)
set -e
DEST="${1:-$HOME/.jieli}"
mkdir -p "$DEST"
curl -fL --retry 3 https://pkgman.jieliapp.com/s/linux-toolchain | tar -xJ -C "$DEST"
TC="$(ls -d "$DEST"/jieli-linux-toolchains-* | sort | tail -1)"
ln -sfn "$(basename "$TC")" "$DEST/toolchain"
echo "JIELI_TOOLCHAIN=$DEST/toolchain ($(basename "$TC"))"
