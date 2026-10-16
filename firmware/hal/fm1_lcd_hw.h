/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LCD wiring, SPI1. The panel protocol is src/lcd.c.
 *
 * FM-1:       SPI1 group B (IOMAP_CON1 bit4 = 1): PC9 CLK, PC10 DO; PC7 CS, PC8 D/C;
 *             backlight PA2, active low.
 * SMK-37 Pro: SPI1 group A (IOMAP_CON1 bit4 = 0): PB6 CLK, PB7 DO; PC8 CS, PB5 D/C;
 *             no backlight control (always on). Group B (PC10) carries the 16 RGB pad
 *             LEDs (WS2812 class) instead: fm1_padled_send() switches SPI1 over for one
 *             frame while the LCD CS is high, as the stock firmware does (stock v016;
 *             AC79 SDK spi.h for the pin groups).
 *
 *   fm1_lcd_hw_init()          pins, backlight on, SPI1 master at BAUD 4
 *   fm1_lcd_baud(b)            SPI1 clock = lsb / (b + 1)
 *   fm1_lcd_send_cmd(c)        D/C low, CS low, one byte (then fm1_lcd_wait)
 *   fm1_lcd_send_data(p, n)    D/C high, CS low, DMA n bytes from RAM p
 *   fm1_lcd_wait()             SPI done (or timeout, counted), pending cleared
 *   fm1_lcd_deselect()         CS high
 * SMK-37 Pro only:
 *   fm1_padled_send(p, n)      LCD deselected and idle: SPI1 to PC10 at BAUD 8, DMA n bytes
 *                              (then fm1_lcd_wait, then fm1_padled_done)
 *   fm1_padled_done()          SPI1 back to the LCD pins at the LCD baud
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_LCD_PA_OUT (*(volatile uint32_t *)0x50000u)
#define FM1_LCD_PA_DIR (*(volatile uint32_t *)0x50008u)
#define FM1_LCD_PB_OUT (*(volatile uint32_t *)0x50040u)
#define FM1_LCD_PB_DIR (*(volatile uint32_t *)0x50048u)
#define FM1_LCD_PC_OUT (*(volatile uint32_t *)0x50080u)
#define FM1_LCD_PC_DIR (*(volatile uint32_t *)0x50088u)
#define FM1_LCD_IOMAP_CON1 (*(volatile uint32_t *)0x51020u)
#define FM1_LCD_SPI_CON (*(volatile uint32_t *)0x11D00u)
#define FM1_LCD_SPI_BAUD (*(volatile uint32_t *)0x11D04u)
#define FM1_LCD_SPI_BUF (*(volatile uint32_t *)0x11D08u)
#define FM1_LCD_SPI_ADR (*(volatile uint32_t *)0x11D0Cu)
#define FM1_LCD_SPI_CNT (*(volatile uint32_t *)0x11D10u)
#define FM1_LCD_GROUP_B 0x10u             /* IOMAP_CON1 bit4: SPI1 on PC8/PC9/PC10 */

#if BOARD_SMK37
#define FM1_LCD_CS_OUT FM1_LCD_PC_OUT
#define FM1_LCD_CS (1u << 8)              /* PC8 */
#define FM1_LCD_DC_OUT FM1_LCD_PB_OUT
#define FM1_LCD_DC (1u << 5)              /* PB5 */
#define FM1_LCD_CLK (1u << 6)             /* PB6 */
#define FM1_LCD_DO (1u << 7)              /* PB7 */
#define FM1_PADLED_DO (1u << 10)          /* PC10 */
#define FM1_PADLED_BAUD 8u                /* lsb / 9 = 6.67 MHz: one SPI byte per LED bit */
#else
#define FM1_LCD_CS_OUT FM1_LCD_PC_OUT
#define FM1_LCD_CS (1u << 7)              /* PC7 */
#define FM1_LCD_DC_OUT FM1_LCD_PC_OUT
#define FM1_LCD_DC (1u << 8)              /* PC8 */
#define FM1_LCD_CLK (1u << 9)             /* PC9 */
#define FM1_LCD_DO (1u << 10)             /* PC10 */
#define FM1_LCD_BL (1u << 2)              /* PA2, active low */
#endif

static uint32_t fm1_lcd_timeouts;
static uint8_t fm1_lcd_cur_baud = 4u;

FM1_INLINE void fm1_lcd_hw_init(void)
{
#if BOARD_SMK37
    FM1_LCD_IOMAP_CON1 &= ~FM1_LCD_GROUP_B;
    FM1_LCD_CS_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_DIR &= ~FM1_LCD_CS;
    FM1_LCD_PB_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PB_DIR &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PC_OUT &= ~FM1_PADLED_DO;     /* the LED line idles low (latch) */
    FM1_LCD_PC_DIR &= ~FM1_PADLED_DO;
#else
    FM1_LCD_PA_OUT &= ~FM1_LCD_BL;
    FM1_LCD_PA_DIR &= ~FM1_LCD_BL;
    FM1_LCD_IOMAP_CON1 |= FM1_LCD_GROUP_B;
    FM1_LCD_CS_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PC_DIR &= ~(FM1_LCD_CS | FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
#endif
    FM1_LCD_SPI_CON = 0x4021u;
    FM1_LCD_SPI_BAUD = 4u;
    fm1_lcd_cur_baud = 4u;
}

FM1_INLINE void fm1_lcd_baud(uint32_t b)
{
    FM1_LCD_SPI_BAUD = b;
    fm1_lcd_cur_baud = (uint8_t)b;
}

static void fm1_lcd_wait(void)
{
    uint32_t n;
    for (n = 0; n < 4000000u && !(FM1_LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == 4000000u)
        fm1_lcd_timeouts++;
    FM1_LCD_SPI_CON |= 0x4000u;
}

FM1_INLINE void fm1_lcd_deselect(void) { FM1_LCD_CS_OUT |= FM1_LCD_CS; }

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    FM1_LCD_DC_OUT &= ~FM1_LCD_DC;
    FM1_LCD_CS_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_BUF = c;
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    FM1_LCD_DC_OUT |= FM1_LCD_DC;
    FM1_LCD_CS_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    FM1_LCD_SPI_CNT = n;
}

#if BOARD_SMK37
FM1_INLINE void fm1_padled_send(const void *p, uint32_t n)
{
    FM1_LCD_CS_OUT |= FM1_LCD_CS;         /* the panel ignores the bus */
    FM1_LCD_SPI_BAUD = FM1_PADLED_BAUD;
    FM1_LCD_IOMAP_CON1 |= FM1_LCD_GROUP_B;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    FM1_LCD_SPI_CNT = n;
}

FM1_INLINE void fm1_padled_done(void)
{
    FM1_LCD_IOMAP_CON1 &= ~FM1_LCD_GROUP_B;
    FM1_LCD_SPI_BAUD = fm1_lcd_cur_baud;
}
#endif
