/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Charles Vestal: from X0X (github.com/charlesvestal/fm1-x0x, commit 00bff73),
 * after Melodee's dual-core audio by Kerem Kilic (github.com/keremimo/melodee). Brought into SLOOP
 * unchanged but for this notice (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab).
 * SLOOP 2.8.3: X0X's hardening brought in (its commits 20353c9, 33ed62b, ed35a45: the check word, a
 * wrong job refused, one core until power-off after it was given up, core 1's own stack guard), but
 * for fm1_cpu1_hold (SLOOP 2.7.1: fm1_sys.h's fm1_cpu1_halt, the bus-invalid traps off around the hold)
 * and the bar's section (.noinit.c1bar: placed after the other .noinit objects, app.ld, so none of
 * them moves). A FM-1 report (a backup over USB, SLOOP 2.8.1, 2 CORES): core 1 read its job's
 * function wrong (03380D04 for cpu1_units_job), jumped there and faulted; the crash screen then ran on
 * core 1, and fm1_reboot held core 1 first: it held itself, the red screen stayed until a power cycle. */
/* The AC79's second core (docs/plans/2026-10-04-dual-core-and-clock.md), with what Melodee's
 * dual-core audio (github.com/keremimo/melodee, GPL-3.0) found on the device.
 *
 * Start (the AC79 SDK's EnableOtherCpu): core 1's interrupt bank off (0x1EEF300..), its entry
 * address into the word at 0x01C7FFF8, bit 3 of 0x10008 held while it starts, C1_CON (0x1EEE004)
 * bit 3 set (enabled) and bit 1 cleared (out of hold). It boots through the chip's ROM, which the
 * PC limits refuse, so they are open until it has reported in from fm1_c1_main. Hold is bit 1
 * set, then bit 3 cleared.
 *
 * Work: one job at a time, a function and an argument; core 0 hands it over (mb.job) and waits
 * for it (mb.done), single writer each way, csync around each (the cores share the caches).
 *
 * Units that cannot run it (fm1-x0x#10, measured by Jangada, github.com/zednaked/jangada): on some
 * FM-1s core 1 reads words that are not in RAM (0x00200000 where core 0 reads 0), idle now and then,
 * often under load. So core 1 runs only the next job, and only when its check word (the function,
 * the argument and the job's number, written before the job) matches; the jobs it finished stay in a
 * register (Melodee's, keremimo/melodee#21: a stale function read as the next job fails the check).
 * Anything else it counts (mb.bad) and leaves, and core 0 holds it at the first one (fm1_cpu1_wait):
 * one core from then on. The mailbox keeps each core's writes on cache lines of their own
 * (Jangada's layout).
 *
 * Flash: the waiting loop is in RAM (.c1_text), so an idle core 1 never fetches from the flash;
 * it works only for the audio interrupt, which waits for it before it returns, and core 0 turns
 * the flash off only with interrupts off. So the flash is never off under it. */
#pragma once
#include <stdint.h>

#define FM1_C1_CON (*(volatile uint32_t *)0x1EEE004u)
#define FM1_C1_CLK (*(volatile uint32_t *)0x00010008u)
#define FM1_C1_T4 (*(volatile uint32_t *)0x10804u)       /* TIMER4, 24 MHz */
#define FM1_C1_SYNC() __asm__ volatile("csync" ::: "memory")

extern void fm1_c1_entry(void);
extern uint32_t _c1_ustack[], _c1_sstack_top[];
#define FM1_C1_STACK_WORDS (2816u / 4u)
#define FM1_C1_MARK 0x43314D4Bu                  /* "C1MK": how deep core 1's stack has gone */
typedef struct {
    volatile uint32_t job, arg;                 /* core 0: job + 1 hands fn(arg) over */
    void (*volatile fn)(uint32_t);
    volatile uint32_t check;                    /* FM1_C1_CHECK of fn, arg and job */
    uint32_t pad0[12];
    volatile uint32_t alive, done;              /* core 1: started; the last job it finished */
    volatile uint32_t bad, bad_job, bad_done, bad_fn;   /* jobs it would not run: how many, the last one */
    uint32_t pad1[10];
} fm1_c1_mb_t;
static fm1_c1_mb_t fm1_c1_mb __attribute__((aligned(64)));
volatile uint32_t fm1_c1_trace;                 /* breadcrumbs (fm1_cpu1.S, fm1_c1_main) */
static uint8_t fm1_c1_on;                       /* started and answering: core 0 may hand it work */
#define FM1_C1_CHECK(fn, arg, job) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (job) * 0x9E3779B1u)
static uint8_t fm1_c1_gave_up;                  /* 1 = it read a wrong job, 2 = it did not finish one, 3 = barred */
/* one core until power-off (after Melodee 1.0.2): set when core 1 is given up or core 0 crashes beside it;
 * .noinit survives a warm reset, power-off clears it. Not static: see fm1_crash (fm1_irq.h) */
#define FM1_C1_BAR_MAGIC 0x43314241u            /* "C1BA" */
struct { uint32_t magic, barred; } fm1_c1_bar __attribute__((section(".noinit.c1bar")));   /* (SLOOP: app.ld) */
static void fm1_cpu1_bar(void) { fm1_c1_bar.magic = FM1_C1_BAR_MAGIC; fm1_c1_bar.barred = 1; }

void fm1_c1_main(void);
void __attribute__((section(".c1_text"), noreturn, used)) fm1_c1_main(void)
{
    /* its own guards, as Melodee sets them (its fm1_core1_main): a stack limit in core 1's EMU bank
     * (core 0's is at 0x1EEF0D0) over both its stacks less the lowest 256 bytes, and EMU_CON bit 2
     * and bits 16..20 off, bit 3 (the stack limit) on */
    uint32_t lo = (uint32_t)(uintptr_t)_c1_ustack + 256u, hi = (uint32_t)(uintptr_t)_c1_sstack_top - 1u;
    uint32_t done, last_bad = 0xFFFFFFFFu;
    *(volatile uint32_t *)0x1EEF2D8u = hi;
    *(volatile uint32_t *)0x1EEF2DCu = lo;
    *(volatile uint32_t *)0x1EEF2E0u = hi;
    *(volatile uint32_t *)0x1EEF2E4u = lo;
    *(volatile uint32_t *)0x1EEF2D0u = (*(volatile uint32_t *)0x1EEF2D0u & ~((1u << 2) | (0x1Fu << 16))) | (1u << 3);
    fm1_c1_trace = 0xC1000002;
    done = fm1_c1_mb.done;
    fm1_c1_mb.alive = 1;
    FM1_C1_SYNC();
    for (;;) {
        uint32_t j = fm1_c1_mb.job;
        FM1_C1_SYNC();
        if (j == done)
            continue;
        if (j == done + 1u) {
            void (*fn)(uint32_t) = fm1_c1_mb.fn;
            uint32_t arg = fm1_c1_mb.arg;
            if (fn && fm1_c1_mb.check == FM1_C1_CHECK(fn, arg, j)) {
                fn(arg);                        /* the work itself runs from the flash */
                done = j;
                FM1_C1_SYNC();
                fm1_c1_mb.done = j;
                FM1_C1_SYNC();
                continue;
            }
            fm1_c1_mb.bad_fn = (uint32_t)(uintptr_t)fn;
        }
        if (j != last_bad) {                    /* each wrong value once (it stays until core 0 holds it) */
            last_bad = j;
            fm1_c1_mb.bad_job = j;
            fm1_c1_mb.bad_done = done;
            FM1_C1_SYNC();
            fm1_c1_mb.bad++;
            FM1_C1_SYNC();
        }
    }
}

/* hold core 1 (SLOOP: fm1_sys.h's fm1_cpu1_halt, registers only; main.c fm1_cstart also holds it first thing at
 * boot, before the RAM it may still be reading is cleared, after a reset that did not hold it) */
static void fm1_cpu1_hold(void)
{
    fm1_cpu1_halt();                            /* (2.7.1, fm1_sys.h) the bus-invalid traps off around the hold */
    FM1_C1_SYNC();
    fm1_c1_on = 0;
}

/* 0 = core 1 runs and answered; else it is held again. The top of RAM must be writable
 * (fm1_guard_unlock_top) and the PC limits open (fm1_guard_pc_open): the caller's */
static int fm1_cpu1_start(void)
{
    uint32_t saved, t0, i;
    if (fm1_c1_on)
        return 0;
    fm1_cpu1_hold();
    if (fm1_c1_bar.magic == FM1_C1_BAR_MAGIC && fm1_c1_bar.barred == 1u) {
        fm1_c1_gave_up = 3;
        return -1;
    }
    for (i = 0; i < FM1_C1_STACK_WORDS; i++)
        _c1_ustack[i] = FM1_C1_MARK;
    fm1_c1_mb.alive = 0;
    fm1_c1_trace = 0;
    fm1_c1_mb.done = fm1_c1_mb.job;
    fm1_c1_mb.fn = 0;
    FM1_C1_SYNC();
    for (i = 0; i < 32u; i++)                   /* its interrupt bank: all off (core 0's is at 0x1EEF100) */
        *(volatile uint32_t *)(0x1EEF300u + 4u * i) = 0;
    *(volatile uint32_t *)0x01C7FFF8u = (uint32_t)(uintptr_t)&fm1_c1_entry;
    saved = FM1_C1_CLK;
    FM1_C1_CLK = saved | 0x8u;
    FM1_C1_SYNC();
    FM1_C1_CON |= 0x8u;
    FM1_C1_CON &= ~0x2u;
    t0 = FM1_C1_T4;
    while (!fm1_c1_mb.alive && FM1_C1_T4 - t0 < 24000u * 20u)   /* 20 ms */
        FM1_C1_SYNC();
    FM1_C1_CLK = saved;
    if (!fm1_c1_mb.alive) {
        fm1_cpu1_hold();
        return -1;
    }
    fm1_c1_on = 1;
    return 0;
}

/* hand core 1 a job: 1 = it took it (fm1_cpu1_wait before touching what it works on) */
static inline int fm1_cpu1_run(void (*fn)(uint32_t), uint32_t arg)
{
    uint32_t job;
    if (!fm1_c1_on)
        return 0;
    job = fm1_c1_mb.job + 1u;
    fm1_c1_mb.fn = fn;
    fm1_c1_mb.arg = arg;
    fm1_c1_mb.check = FM1_C1_CHECK(fn, arg, job);
    FM1_C1_SYNC();
    fm1_c1_mb.job = job;
    FM1_C1_SYNC();
    return 1;
}

/* 0 = done; -1 = it read a wrong job or did not finish in time: core 1 is held from now on (the
 * caller does the work) */
static inline int fm1_cpu1_wait(void)
{
    uint32_t t0 = FM1_C1_T4;
    for (;;) {
        FM1_C1_SYNC();
        if (fm1_c1_mb.bad) {                    /* a unit that cannot run it: one core */
            fm1_cpu1_hold();
            fm1_cpu1_bar();
            fm1_c1_gave_up = 1;
            return -1;
        }
        if (fm1_c1_mb.done == fm1_c1_mb.job)
            return 0;
        if (FM1_C1_T4 - t0 > 24000u * 8u) {     /* 8 ms: far past any stretch (or it faulted) */
            fm1_cpu1_hold();
            fm1_cpu1_bar();
            fm1_c1_gave_up = 2;
            return -1;
        }
    }
}
