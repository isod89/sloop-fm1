/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 Pro port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SMK-37 TRS MIDI OUT: UART1 TX on PC7, 31250 baud, no IRQ, one byte at a time (src/smk_midi_out.h polls it).
 * The stock firmware's setup (v016): IOMAP_CON3 bit 7 clear and [23:20] = 1 (UT1 TX onto an output channel), PC7
 * with pull-up and pull-down set, an output (low), input enabled (the pin takes the output channel), UART1 CON0
 * 0x3400 (pendings cleared) then enabled, transmit only. The clock and the divider as the FM-1's MIDI IN
 * (fm1_uart.h): PLL48M, 48 MHz / 31250 / 4 - 1 = 383. Sending: clear TPND (CON0 bit 13), write BUF; TPND (bit 15)
 * when the byte has gone (the SDK's putbyte). PC7 is free on the SMK (the FM-1's LCD chip select).
 *
 *   smk_uart1_tx_init()      before TIMER5 starts (PORTC, IOMAP RMW)
 *   smk_uart1_tx_put(b)      start a byte (only when the last one is done)
 *   smk_uart1_tx_done()      1: the last byte has gone */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define SMK_UT1_CON0   (*(volatile uint32_t *)0x12100u)
#define SMK_UT1_CON1   (*(volatile uint32_t *)0x12104u)
#define SMK_UT1_BAUD   (*(volatile uint32_t *)0x12108u)
#define SMK_UT1_BUF    (*(volatile uint8_t *)0x1210Cu)
#define SMK_UT_CLK_CON1 (*(volatile uint32_t *)0x10010u)   /* [11:10] UART clock: 1 = PLL48M */
#define SMK_UT_IOMAP3   (*(volatile uint32_t *)0x51028u)   /* UT1: b7 fixed IO, [23:20] TX output (stock: 1) */
#define SMK_UT_PC_OUT   (*(volatile uint32_t *)0x50080u)
#define SMK_UT_PC_DIR   (*(volatile uint32_t *)0x50088u)
#define SMK_UT_PC_DIE   (*(volatile uint32_t *)0x5008Cu)
#define SMK_UT_PC_PU    (*(volatile uint32_t *)0x50090u)
#define SMK_UT_PC_PD    (*(volatile uint32_t *)0x50094u)
#define SMK_UT_PIN      (1u << 7)                         /* PC7 */
#define SMK_UT_TPND     0x8000u
#define SMK_UT_CLR_TPND 0x2000u

FM1_INLINE void smk_uart1_tx_init(void)
{
    SMK_UT1_CON0 = 0x3400u;                            /* off, pendings cleared */
    SMK_UT1_CON1 = 0;
    SMK_UT_CLK_CON1 = (SMK_UT_CLK_CON1 & ~(3u << 10)) | (1u << 10);
    SMK_UT_IOMAP3 &= ~0x80u;
    SMK_UT_IOMAP3 = (SMK_UT_IOMAP3 & ~0xF00000u) | 0x100000u;
    SMK_UT_PC_PU |= SMK_UT_PIN;
    SMK_UT_PC_PD |= SMK_UT_PIN;
    SMK_UT_PC_OUT &= ~SMK_UT_PIN;
    SMK_UT_PC_DIR &= ~SMK_UT_PIN;
    SMK_UT_PC_DIE |= SMK_UT_PIN;
    SMK_UT1_BAUD = 48000000u / 31250u / 4u - 1u;       /* 383 */
    __asm__ volatile("csync" ::: "memory");
    SMK_UT1_CON0 |= 1u;                                /* UTEN; no RX, no IEs */
    __asm__ volatile("csync" ::: "memory");
}

FM1_INLINE uint32_t smk_uart1_tx_done(void) { return (SMK_UT1_CON0 & SMK_UT_TPND) != 0u; }

FM1_INLINE void smk_uart1_tx_put(uint32_t b)
{
    SMK_UT1_CON0 |= SMK_UT_CLR_TPND;
    SMK_UT1_BUF = (uint8_t)b;
    __asm__ volatile("csync" ::: "memory");
}
