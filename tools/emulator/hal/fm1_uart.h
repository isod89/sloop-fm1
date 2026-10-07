/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_uart.h: the TRS MIDI IN jack with nothing plugged in. The
 * firmware reads its ring by content (midi_uart.c, UM_EMPTY), so a ring nobody writes stays idle.
 * midi_uart.c includes "../hal/fm1_uart.h"; build_emu.py compiles a copy that includes this one. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

FM1_INLINE void fm1_uart1_midi_init(volatile uint8_t *ring, uint32_t len) { (void)ring; (void)len; }
FM1_INLINE uint32_t fm1_uart1_rx_take(void) { return 0; }
