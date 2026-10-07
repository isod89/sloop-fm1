/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_lcd_hw.h: an ST7789 that understands what lcd.c sends
 * (CASET 0x2A, RASET 0x2B, RAMWR 0x2C + pixel data, big-endian RGB565) into emu_lcd */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#include "emu_bridge.h"

static uint32_t fm1_lcd_timeouts;
static struct { uint32_t cmd, x0, x1, y0, y1, x, y, hi, half; } emu_st = {0, 0, 239, 0, 239};

FM1_INLINE void fm1_lcd_hw_init(void) {}
FM1_INLINE void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_wait(void) {}
FM1_INLINE void fm1_lcd_deselect(void) {}

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    emu_st.cmd = c;
    if (c == 0x2Cu) {
        emu_st.x = emu_st.x0;
        emu_st.y = emu_st.y0;
        emu_st.half = 0;
    }
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;
    if (emu_st.cmd == 0x2Au && n >= 4u) {
        emu_st.x0 = (uint32_t)b[0] << 8 | b[1];
        emu_st.x1 = (uint32_t)b[2] << 8 | b[3];
        return;
    }
    if (emu_st.cmd == 0x2Bu && n >= 4u) {
        emu_st.y0 = (uint32_t)b[0] << 8 | b[1];
        emu_st.y1 = (uint32_t)b[2] << 8 | b[3];
        return;
    }
    if (emu_st.cmd != 0x2Cu)
        return;
    for (i = 0; i < n; i++) {
        if (!emu_st.half) {
            emu_st.hi = b[i];
            emu_st.half = 1;
            continue;
        }
        emu_st.half = 0;
        if (emu_st.x < 240u && emu_st.y < 240u)
            emu_lcd[emu_st.y * 240u + emu_st.x] = (uint16_t)(emu_st.hi << 8 | b[i]);
        if (++emu_st.x > emu_st.x1) {
            emu_st.x = emu_st.x0;
            if (++emu_st.y > emu_st.y1)
                emu_st.y = emu_st.y0;
        }
    }
}
