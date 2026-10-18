/* SPDX-License-Identifier: GPL-3.0-only */
/* (tools/build_preview.py without Emscripten: clang --target=wasm32 + wasm-ld) the few libc calls the preview needs */
#pragma once
#include <stddef.h>
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
