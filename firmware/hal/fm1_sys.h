/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 system HAL: P33 access, watchdog, reset reason, reboot / UBOOT entry.
 * Call with interrupts off so nothing else touches P33.
 *
 *   fm1_reset_reason()   snapshot P3_RST_SRC / RST_SRC (call first thing)
 *   fm1_wdt_arm(t)       reset-mode watchdog, t = 0xA 1 s .. 0xF 32 s
 *   fm1_wdt_feed()       in the main loop
 *   fm1_reboot()         chip reset through P33 (P3_PR_PWR bit 4)
 *   fm1_enter_uboot()    "usb_update_mode" at 0x01C7FD80 + chip reset
 *   fm1_enter_update(p)  112-byte UPDATA_PARM record at 0x01C7FD88 + core reset
 *                        (the SPL then runs the staged update loader)
 *   fm1_core_reset()     PWR_CON core reset
 *   fm1_updata_parm_clear()  zero the record (CRC 0: ignored on a warm reset)
 *   fm1_mailbox_clear()  zero 0x01C7FD80..0x01C7FDFF (boot info, mailbox) at cold start
 *   fm1_mem_readable(a, n) / fm1_peek8(a)   debug reads of RAM / XIP only
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_P33_CON (*(volatile uint32_t *)0x13E08u)
#define FM1_P33_DAT (*(volatile uint32_t *)0x13E0Cu)
#define FM1_RST_SRC (*(volatile uint32_t *)0x100C0u)
#define FM1_BOOT_STATE ((volatile uint8_t *)0x01C7FD80u)
#define FM1_PWR_CON (*(volatile uint32_t *)0x10000u)
#define FM1_UPDATA_PARM ((volatile uint8_t *)0x01C7FD88u)
#define FM1_UPDATA_PARM_LEN 112u

static uint32_t fm1_p33_timeouts;

static uint8_t fm1__p33_xfer(uint8_t b)
{
    uint32_t n = 100000u;
    FM1_P33_DAT = b;
    FM1_P33_CON |= 1u << 4;
    (void)FM1_P33_CON;                           /* stands in for csync */
    while (FM1_P33_CON & (1u << 1))
        if (--n == 0) {
            fm1_p33_timeouts++;
            break;
        }
    return (uint8_t)FM1_P33_DAT;
}

static void fm1__p33_cs(uint32_t a)
{
    if (a & 0x8000u) {
        FM1_P33_CON |= (1u << 0) | (1u << 8);    /* R3 (RTC) domain */
    } else {
        FM1_P33_CON &= ~(1u << 8);
        FM1_P33_CON |= 1u << 0;
    }
}

static void fm1__p33_op(uint32_t op, uint32_t a, uint8_t d)   /* 0 wr, 1 or, 2 and, 3 xor */
{
    fm1__p33_cs(a);
    fm1__p33_xfer((uint8_t)((op << 5) | ((a >> 8) & 3u)));
    fm1__p33_xfer((uint8_t)a);
    fm1__p33_xfer(d);
    FM1_P33_CON &= ~((1u << 0) | (1u << 8));
}

static uint8_t fm1_p33_read(uint32_t a)
{
    uint8_t v;
    fm1__p33_cs(a);
    fm1__p33_xfer((uint8_t)(0x80u | ((a >> 8) & 3u)));
    fm1__p33_xfer((uint8_t)a);
    v = fm1__p33_xfer(0x5E);
    FM1_P33_CON &= ~((1u << 0) | (1u << 8));
    return v;
}
static void fm1_p33_write(uint32_t a, uint8_t d) { fm1__p33_op(0, a, d); }
static void fm1_p33_or(uint32_t a, uint8_t d) { fm1__p33_op(1, a, d); }
static void fm1_p33_and(uint32_t a, uint8_t d) { fm1__p33_op(2, a, d); }

/* ------------------------------------------------------- VDC14, SYSVDD --- */
/* SLOOP 2.7, from X0X 1.0.9 by Charles Vestal (GPL-3.0, fm1-x0x#21): VDC14 (P3_ANA_CON6 [2:0]) is 1.25 V + 50 mV a
 * level; SYSVDD (P3_ANA_CON9 [3:0], the cores and the SRAM) 0.93 V + 30 mV a level. The boot loader leaves 3 (1.40 V)
 * and 11 (1.26 V); M-VAVE's own firmware (V15) raises them to 7 (1.60 V) and 15 (1.38 V) as it starts, at the same
 * 360 MHz. At the boot loader's, with both cores running, some units read the flash and the SRAM wrong. Up only, a
 * level every 100 us, VDC14 first: SYSVDD is regulated from it (main.c fm1_cstart, before anything else) */
#define FM1_P3_ANA_CON6 0x06u
#define FM1_P3_ANA_CON9 0x09u
static uint32_t fm1_vdc14(void) { return fm1_p33_read(FM1_P3_ANA_CON6) & 7u; }
static uint32_t fm1_sysvdd(void) { return fm1_p33_read(FM1_P3_ANA_CON9) & 15u; }
static void fm1_supply_raise(uint32_t reg, uint32_t mask, uint32_t level)
{
    uint32_t v = fm1_p33_read(reg) & mask;
    while (v < level && v < mask) {
        v++;
        fm1_p33_write(reg, (uint8_t)((fm1_p33_read(reg) & ~mask) | v));
        fm1_delay_us(100);
    }
}
static void fm1_supplies_stock(void)              /* M-VAVE's own: 1.60 V, then 1.38 V */
{
    fm1_supply_raise(FM1_P3_ANA_CON6, 7u, 7u);
    fm1_supply_raise(FM1_P3_ANA_CON9, 15u, 15u);
}

/* ------------------------------------------------------------ watchdog --- */
#define FM1_P3_WDT_CON 0x80u
#define FM1_P3_VLD_KEEP 0x17u
#define FM1_P3_RST_SRC 0x12u
#define FM1_P3_PR_PWR 0xA0u

static void fm1_wdt_arm(uint8_t t)
{
    fm1_p33_write(FM1_P3_WDT_CON, 0);
    fm1_p33_and(FM1_P3_VLD_KEEP, (uint8_t)~0x40u);           /* WDT_EXPT_EN = 0: plain reset */
    fm1_p33_write(FM1_P3_WDT_CON, (uint8_t)((t & 0x0Fu) | 0x40u));   /* timeout, reset mode, clear */
    fm1_p33_or(FM1_P3_WDT_CON, 0x10);                        /* enable */
}
static void fm1_wdt_feed(void) { fm1_p33_or(FM1_P3_WDT_CON, 0x40); }
static void fm1_wdt_stop(void) { fm1_p33_write(FM1_P3_WDT_CON, 0); }

/* ---------------------------------------------------- reset and reboot --- */
static struct {
    uint8_t p3_rst;      /* bit0 power-on, 1 VDDIO low, 2 WDT, 3 VCM, 4 long press, 5 1.2 V, 6 soft (P33) */
    uint32_t rst_src;    /* bit5 soft (PWR_CON) */
    uint8_t wdt_con;     /* as found at boot */
} fm1_boot;

static void fm1_reset_reason(void)
{
    fm1_boot.p3_rst = fm1_p33_read(FM1_P3_RST_SRC);
    fm1_boot.rst_src = FM1_RST_SRC;
    fm1_boot.wdt_con = fm1_p33_read(FM1_P3_WDT_CON);
}

/* SLOOP 2.7.1, after Felucca 1.6.1 (Leo Kuroshita, GPL-3.0, fm1_sys.h fm1_cpu1_halt): hold the second core with the
 * bus-invalid traps off. Holding core 1 cuts the access it has on the bus, and the chip flags that a few instructions
 * later as a bus-invalid access (DBG_MSG bits 4, 5, 16..21): with the traps on (fm1_guard.h FM1_GUARD_BUS) core 0 took
 * the exception. On a FM-1 whose core 1 once ran late (fm1_cpu1_wait: 8 ms, then one core), that was a FELUCCA CRASH
 * screen in mix_block (vec 1, DBG_MSG bit 21) and a reboot instead of a glitch. So: traps off, hold, let the flag
 * come, clear it, traps back as they were. Registers only: safe when core 1 never ran */
#define FM1__C1_CON (*(volatile uint32_t *)0x1EEE004u)
#define FM1__DBG_WR_EN (*(volatile uint32_t *)0x1EEE240u)
#define FM1__DBG_MSG_CLR (*(volatile uint32_t *)0x1EEE248u)
#define FM1__DBG_EN (*(volatile uint32_t *)0x1EEE340u)
#define FM1__DBG_BUS 0x003F0030u                 /* bus-invalid traps: AXI rd/wr (4, 5), C0/C1 fetch/rd/wr (16..21) */
FM1_INLINE void fm1_cpu1_halt(void)
{
    uint32_t locked = !(FM1__DBG_WR_EN & 1u), en, bus = FM1__DBG_BUS;
    volatile uint32_t n;
    __asm__("" : "+r"(bus));                     /* (as Felucca: ~bus not folded into a constant) */
    if (locked)
        FM1__DBG_WR_EN = 0xE7u;                  /* unlock (as fm1_guard.h) */
    en = FM1__DBG_EN;
    FM1__DBG_EN = en & ~bus;
    __asm__ volatile("csync" ::: "memory");
    FM1__C1_CON |= 0x2u;                         /* hold: bit 1 set, then bit 3 (enable) cleared */
    FM1__C1_CON &= ~0x8u;
    __asm__ volatile("csync" ::: "memory");
    for (n = 0; n < 256u; n++)                   /* the flag comes some instructions after the hold */
        ;
    FM1__DBG_MSG_CLR = bus;
    __asm__ volatile("csync" ::: "memory");
    FM1__DBG_EN = en;
    if (locked && (FM1__DBG_WR_EN & 1u))
        FM1__DBG_WR_EN = 0xE7u;                  /* lock again */
    __asm__ volatile("csync" ::: "memory");
}

static void fm1_reboot(void)
{
    __asm__ volatile("cli");
    fm1_cpu1_halt();                                   /* the second core held (2.7.1: no bus flag to trap on) */
    fm1_p33_or(FM1_P3_PR_PWR, 0x10);
    for (;;)
        ;
}

static void fm1_enter_uboot(void)
{
    static const char k[16] = "usb_update_mode";
    uint32_t i;
    __asm__ volatile("cli");
    /* CPU0 write limits (fm1_guard.h) may cover the mailbox: drop them all */
    if (!(*(volatile uint32_t *)0x1EEE240u & 1u))
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    *(volatile uint32_t *)0x1EEE348u = 0;
    for (i = 0; i < 16u; i++)
        FM1_BOOT_STATE[i] = (uint8_t)k[i];
    fm1_p33_or(FM1_P3_PR_PWR, 0x10);
    for (;;)
        ;
}

FM1_INLINE void fm1_core_reset(void)
{
    *(volatile uint32_t *)0x1EEE004u |= 0x2u;          /* the second core held first (fm1_cpu1.h): it */
    *(volatile uint32_t *)0x1EEE004u &= ~0x8u;         /* must not run old code while the loader writes */
    FM1_PWR_CON |= 0x10u;
    for (;;)
        ;
}

FM1_INLINE void fm1_enter_update(const uint8_t *parm)
{
    uint32_t i;
    __asm__ volatile("cli");
    if (!(*(volatile uint32_t *)0x1EEE240u & 1u))       /* drop CPU0 write limits, as fm1_enter_uboot */
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    *(volatile uint32_t *)0x1EEE348u = 0;
    fm1_cpu1_halt();                                    /* (2.7.1) held with the traps off, before the record */
    for (i = 0; i < FM1_UPDATA_PARM_LEN; i++)
        FM1_UPDATA_PARM[i] = parm[i];
    fm1_core_reset();
}

FM1_INLINE void fm1_updata_parm_clear(void)
{
    uint32_t i;
    for (i = 0; i < FM1_UPDATA_PARM_LEN; i++)
        FM1_UPDATA_PARM[i] = 0;
}

FM1_INLINE void fm1_mailbox_clear(void)   /* before fm1_guard_enable (it write-protects the top) */
{
    uint32_t *s;
    for (s = (uint32_t *)0x01C7FD80u; s < (uint32_t *)0x01C7FE00u; s++)
        *s = 0;
}

/* RAM and the XIP window only: SFR reads can have side effects */
static int fm1_mem_readable(uint32_t a, uint32_t n)
{
    return (a >= 0x01C00000u && a + n <= 0x01C80000u && a + n >= a) ||
           (a >= 0x02000000u && a + n <= 0x02100000u && a + n >= a);
}
FM1_INLINE uint8_t fm1_peek8(uint32_t a) { return *(const volatile uint8_t *)a; }
