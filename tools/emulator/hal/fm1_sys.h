/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_sys.h: resets restart the emulator, UBOOT / update stop it */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#include "emu_bridge.h"

#define FM1_UPDATA_PARM_LEN 112u
#define FM1_P3_WDT_CON 0x80u
#define FM1_P3_VLD_KEEP 0x17u
#define FM1_P3_RST_SRC 0x12u
#define FM1_P3_PR_PWR 0xA0u
static uint32_t fm1_p33_timeouts;

static uint8_t fm1_p33_read(uint32_t a) { (void)a; return 0; }
static void fm1_p33_write(uint32_t a, uint8_t d) { (void)a; (void)d; }
static void fm1_p33_or(uint32_t a, uint8_t d) { (void)a; (void)d; }
static void fm1_p33_and(uint32_t a, uint8_t d) { (void)a; (void)d; }
static void fm1_wdt_arm(uint8_t t) { (void)t; }
static void fm1_wdt_feed(void) {}
static void fm1_wdt_stop(void) {}

static struct {
    uint8_t p3_rst;      /* bit0 power-on, 1 VDDIO low, 2 WDT, 3 VCM, 4 long press, 5 1.2 V, 6 soft (P33) */
    uint32_t rst_src;
    uint8_t wdt_con;
} fm1_boot;

static void fm1_reset_reason(void) { fm1_boot.p3_rst = 1; fm1_boot.rst_src = 0; fm1_boot.wdt_con = 0; }
static void fm1_reboot(void) { emu_reboot(); }
static void fm1_enter_uboot(void) { emu_halt("UBOOT (not emulated)"); }
FM1_INLINE void fm1_core_reset(void) { emu_reboot(); }
FM1_INLINE void fm1_enter_update(const uint8_t *parm) { (void)parm; emu_halt("UPDATE (not emulated)"); }
FM1_INLINE void fm1_updata_parm_clear(void) {}
FM1_INLINE void fm1_mailbox_clear(void) {}
static int fm1_mem_readable(uint32_t a, uint32_t n) { (void)a; (void)n; return 0; }
FM1_INLINE uint8_t fm1_peek8(uint32_t a) { (void)a; return 0; }
