/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_irq.h: cli / sti are the emulator's CPU lock */
#pragma once
#include <stdint.h>
#include "emu_bridge.h"

enum { FM1_IRQ_EXCEPTION = 1, FM1_IRQ_ALNK0 = 11, FM1_IRQ_SPI1 = 16, FM1_IRQ_UART1 = 20,
       FM1_IRQ_SARADC = 24, FM1_IRQ_SPI2 = 37, FM1_IRQ_TIMER5 = 63, FM1_IRQ_SOFT0 = 120 };

static inline void fm1_irq_off(void) { emu_irq_off(); }
static inline void fm1_irq_on(void) { emu_irq_on(); }

#define FM1_CRASH_MAGIC 0x43525348u          /* "CRSH" */
typedef struct {
    uint32_t magic, count, vec, pc, rets, emu, dbg, sp, psr, icfg, uptime_ms;
    uint32_t etm[4];
    uint32_t early;
} fm1_crash_t;
fm1_crash_t fm1_crash;

static void fm1_fault(const fm1_crash_t *c);   /* application: report, then reset */

static void fm1_irq_init(void) {}
static void fm1_irq_attach(uint32_t n, void (*h)(void), uint32_t prio) { (void)n; (void)h; (void)prio; }
static void fm1_irq_mask(uint32_t n) { (void)n; }
static void fm1_irq_enable_all(void) { emu_irq_enable_all(); fm1_irq_on(); }
