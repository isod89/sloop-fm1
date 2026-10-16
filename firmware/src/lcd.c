/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ST7789-class 240x240 panel on SPI1: PC9 CLK, PC10 DO, PC7 CS, PC8 D/C,
 * backlight PA2 active low. Polled DMA transfers; every source buffer must be
 * in RAM. A pixel transfer is left running (lcd_busy): the next LCD access, or
 * a write to its source buffer (lcd_sync), waits for it, so the main loop works
 * while the last strip of a frame goes out. */
/* pins and SPI1: hal/fm1_lcd_hw.h */
#ifndef LCD_BAUD
#define LCD_BAUD 4u                /* lsb/(BAUD+1): 4 = 12 MHz */
#endif

static uint8_t lcd_small[64];
static uint8_t lcd_busy;           /* a lcd_data DMA may still run; CS is low */

static void lcd_spin(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++)
        ;
}

static void lcd_sync(void)          /* finish the running transfer (before touching its buffer) */
{
    if (!lcd_busy)
        return;
    fm1_lcd_wait();
    fm1_lcd_deselect();
    lcd_busy = 0;
}

static void lcd_cmd(uint8_t c)
{
    lcd_sync();
    fm1_lcd_send_cmd(c);
    fm1_lcd_wait();
    fm1_lcd_deselect();
}

static void lcd_data(const void *p, uint32_t n)
{
    if (!n)
        return;                    /* SPI_CNT = 0 never completes */
    lcd_sync();
    fm1_lcd_send_data(p, n);
    lcd_busy = 1;                  /* completed by lcd_sync */
}

static void lcd_window(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    lcd_sync();
    lcd_small[0] = (uint8_t)(x0 >> 8);
    lcd_small[1] = (uint8_t)x0;
    lcd_small[2] = (uint8_t)(x1 >> 8);
    lcd_small[3] = (uint8_t)x1;
    lcd_cmd(0x2A);
    lcd_data(lcd_small, 4);
    lcd_small[4] = (uint8_t)(y0 >> 8);       /* own bytes: the x transfer may still read [0..3] */
    lcd_small[5] = (uint8_t)y0;
    lcd_small[6] = (uint8_t)(y1 >> 8);
    lcd_small[7] = (uint8_t)y1;
    lcd_cmd(0x2B);
    lcd_data(lcd_small + 4, 4);
    lcd_cmd(0x2C);
}

static uint16_t lcd_fillbuf[240];

static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, n, k, sw;
    sw = (uint16_t)((c >> 8) | (c << 8));
    if (!w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    lcd_sync();
    k = 240u / w * w;                       /* whole rows per transfer (narrow fills: one DMA) */
    for (i = 0; i < k; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    for (n = w * h; n; n -= k) {
        if (k > n)
            k = n;
        lcd_data(lcd_fillbuf, k * 2u);
    }
}

static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (!w || !h)
        return;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    lcd_data(px, w * h * 2u);
}

#if BOARD_SMK37
/* SMK-37 Pro ST7789V 240x240: the stock v016 table with a SWRESET in
 * front (stock relies on the power-on reset) and NORON instead of its DISPOFF + 50 ms;
 * lcd_init sends DISPON after the clear. Same format as below. */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,
    0xB7, 1, 0x35,
    0xBB, 1, 0x32,
    0xC2, 1, 0x01,
    0xC3, 1, 0x15,
    0xC4, 1, 0x20,
    0xC6, 1, 0x0F,
    0xD0, 2, 0xA4, 0xA1,
    0xE0, 14, 0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34,
    0xE1, 14, 0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34,
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};
#else
/* ST7789V init: reset, then 16-bit colour, scan direction and IPS inversion;
 * voltage and gamma stay at the defaults. Format: cmd, n, n data bytes;
 * cmd 0x00 = wait (data byte: ~ms). */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};
#endif

static void lcd_init(void)
{
    uint32_t r, x;
    fm1_lcd_hw_init();
    lcd_spin(2000000u);
    for (r = 0; r < sizeof LCD_SEQ; r += 2u + LCD_SEQ[r + 1u]) {
        if (LCD_SEQ[r] == 0x00u) {                       /* pseudo command: wait */
            lcd_spin(LCD_SEQ[r + 2u] * 25000u);          /* ~1 ms per unit (lcd_spin(3000000) ~ 120 ms) */
            continue;
        }
        lcd_cmd(LCD_SEQ[r]);
        for (x = 0; x < LCD_SEQ[r + 1u]; x++)
            lcd_small[x] = LCD_SEQ[r + 2u + x];
        if (LCD_SEQ[r + 1u])
            lcd_data(lcd_small, LCD_SEQ[r + 1u]);
    }
    fm1_lcd_baud(LCD_BAUD);
    lcd_fill(0, 0, 240, 240, 0);
    lcd_cmd(0x29);
}

#if BOARD_SMK37
/* The 16 RGB pads (SMK-37 Pro): WS2812-class LEDs on SPI1 group B (PC10), one SPI byte per
 * LED bit (0xF0 = 1, 0xC0 = 0, MSB first, GRB), 250 zero bytes of reset first (~300 us; the
 * stock firmware's 36 bytes = 43 us are too short for newer WS2812 parts: a glitch on PC10 at the
 * pin switch then counts as a first bit and shifts the chain by one, so a lit pad turns its chain
 * neighbour green). The frame goes out between LCD transfers (LCD CS high), blocking for ~0.8 ms;
 * padled_flush() only sends when a colour changed. */
#define PADLED_N 16u
#define PADLED_LATCH 250u
static uint8_t padled_buf[PADLED_LATCH + PADLED_N * 24u];
static uint32_t padled_rgb[PADLED_N];
static uint8_t padled_dirty = 1;
static uint32_t padled_hold_until;              /* (console padtest: the UI leaves the pads alone until then) */
static uint32_t bled_hold_until;                /* (console bled: one button LED alone until then) */
static uint8_t bled_id;
/* hardware pad (0..7 top row, 8..15 bottom row, left to right; smk_input.h SMK_PAD_OF_WHITE) -> position in
 * the chain (stock table) */
static const uint8_t PADLED_SLOT[PADLED_N] = {0, 15, 14, 13, 12, 11, 10, 9, 1, 2, 3, 4, 5, 6, 7, 8};

static void padled_set(uint32_t pad, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t c = (uint32_t)g << 16 | (uint32_t)r << 8 | b;
    if (pad < PADLED_N && padled_rgb[pad] != c) {
        padled_rgb[pad] = c;
        padled_dirty = 1;
    }
}

static void padled_flush(void)
{
    uint32_t p, i;
    if (!padled_dirty)
        return;
    padled_dirty = 0;
    for (p = 0; p < PADLED_N; p++) {
        uint8_t *o = padled_buf + PADLED_LATCH + PADLED_SLOT[p] * 24u;
        for (i = 0; i < 24u; i++)
            o[i] = (padled_rgb[p] >> (23u - i)) & 1u ? 0xF0u : 0xC0u;
    }
    lcd_sync();
    fm1_padled_send(padled_buf, sizeof padled_buf);
    fm1_lcd_wait();
    fm1_padled_done();
}
#endif
