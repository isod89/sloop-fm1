/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_time.h: TIMER4 from the host performance counter */
#pragma once
#include <stdint.h>
#include "emu_bridge.h"

#define FM1_TICKS_PER_US 24u

static inline void fm1_time_init(void) {}
static inline uint32_t fm1_ticks(void) { return emu_ticks24(); }
static inline uint32_t fm1_micros(void) { return emu_ticks24() / FM1_TICKS_PER_US; }
static inline void fm1_delay_us(uint32_t us)
{
    uint32_t start = emu_ticks24(), span = us * FM1_TICKS_PER_US;
    while ((uint32_t)(emu_ticks24() - start) < span)
        ;
}
static inline void fm1_delay_ms(uint32_t ms) { emu_sleep_ms(ms); }
