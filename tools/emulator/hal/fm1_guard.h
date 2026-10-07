/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_guard.h: no MPU on the host */
#pragma once
#include <stdint.h>
#include "fm1_irq.h"

enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
static void fm1_guard_enable(uint32_t which) { (void)which; }
static void fm1_guard_lock_top(void) {}
static void fm1_guard_unlock_top(void) {}
