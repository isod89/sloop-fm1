/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_timer.h: the 10 kHz tick is a host thread */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#include "fm1_irq.h"

FM1_INLINE void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)prio; emu_timer5_start(isr); }
FM1_INLINE void fm1_timer5_ack(void) {}
