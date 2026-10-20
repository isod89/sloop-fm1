/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* The second core of the AC7911: started at power-on (main.c), proved with a self-test job, reported on
 * ABOUT ("2 CORES" / "1 CORE") and in the console (cpu1, cpu1_trace, cpu1_timeouts, cpu1_stack_free). The
 * render hands it a share of its units each block (fx.c mix_units, through cpu1.h's cpu1_run / cpu1_wait).
 * The HAL is X0X's (hal/fm1_cpu1.h): one job at a time, core 0 hands it over and waits; the waiting loop
 * lives in RAM (.c1_text), so an idle core 1 never fetches from the flash and flash writes need no parking.
 * FELUCCA_CPU1=0 builds without any of it. */
enum { CPU1_OFF, CPU1_NOSTART, CPU1_FAILED, CPU1_OK };   /* not built / did not report in / self-test wrong / running */
static uint8_t cpu1_state = CPU1_OFF;
static uint8_t cpu1_off;                                 /* (panel.c: the menu's 2ND CORE, read from the settings) */
static uint32_t cpu1_timeouts;                           /* jobs core 1 did not finish in time (console) */
static uint8_t cpu1_why;                                 /* why it was held for good: 0 not, 1 a job late, 2 its stack
                                                          * too deep, 3 it faulted, 4 it read a wrong job (or answered the
                                                          * self-test wrong); why it was not
                                                          * started: 5 held or a crash beside it before this reset (cpu1_bar),
                                                          * 6 the boot before died in its first 30 s (console, crash screen) */
/* (after 2.7.1) one core until the FM-1 is switched off, after core 1 was held for good or a crash came while it ran
 * (main.c fm1_fault, as X0X 1.0.9: in a job or between jobs, fm1-x0x#21's misreads come with both cores running): kept
 * in .noinit across the crash screen's restart (fm1_reboot, a P33 soft reset), cleared by a power-on (main.c
 * fm1_cstart, as the boot guard) and by an update (the loader's core reset leaves P3_RST_SRC as the power-on left it,
 * 0x01: measured on an FM-1, so it reads as a power-on, for the boot guard too). X0X 1.0.9 and Felucca 1.6.1 go further and save 2ND CORE OFF in the settings; here the next power-on
 * tries two cores again. A hang the watchdog resets does not bar it (a stuck update loader ends that way too); the boot
 * guard covers a hang in a boot's first 30 s. Not static, as fm1_crash (fm1_irq.h): a file-local
 * global only ever given one constant is folded into a byte flag, and any garbage byte would keep core 1 off */
#define CPU1_BAR 0x43314241u                             /* "C1BA" */
uint32_t cpu1_bar __attribute__((section(".noinit")));

#if FELUCCA_CPU1
static volatile uint32_t cpu1_test_out;
static uint8_t cpu1_fault_why(void)                      /* (after 2.7.1) a fault core 1 parked on: 2 its stack limit (its
                                                          * EMU_MSG bit 3, fm1_c1_fault), else 3 */
{
    return (fm1_c1_fault_emu & (1u << 3)) ? 2u : 3u;
}
static uint8_t cpu1_held_why(void)                       /* a job core 1 did not deliver: a fault, a wrong read, or late */
{
    return (fm1_c1_trace & 0xFFFFFF00u) == FM1_C1_FAULT ? cpu1_fault_why() : fm1_c1_mb.bad ? 4u : 1u;
}

/* cpu1.h's API over the HAL: the render hands its jobs through these */
static int cpu1_on(void) { return cpu1_state == CPU1_OK && fm1_c1_on; }
static uint32_t cpu1_ticks(void) { return fm1_ticks(); }
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { return cpu1_state == CPU1_OK ? fm1_cpu1_run(fn, arg) : 0; }
static int cpu1_wait(void)
{
    if (fm1_cpu1_wait() == 0)
        return 0;
    cpu1_timeouts++;                                     /* fm1_cpu1_wait held the core: one core from here on */
    cpu1_why = cpu1_held_why();
    cpu1_state = CPU1_FAILED;
    cpu1_bar = CPU1_BAR;
    return -1;
}

static void cpu1_late_end(void) { fm1_cpu1_hold_end(); }

/* after each half (audio.c), core 1 idle: a fault it parked on outside a job, or a job that went within 256 bytes of
 * its stack's end (hal/fm1_cpu1.h fm1_cpu1_stack_ok: the red zone below the stack, app.ld), holds it for good, as a
 * late job does: one core from here on, before an overflow reaches the buffers below the stack */
static void cpu1_guard_half(void)
{
    uint8_t why;
    if (cpu1_state != CPU1_OK)
        return;
    if ((fm1_c1_trace & 0xFFFFFF00u) == FM1_C1_FAULT)
        why = cpu1_fault_why();
    else if (fm1_c1_mb.bad)                              /* a wrong job read while idle (fm1_cpu1.h) */
        why = 4u;
    else if (!fm1_cpu1_stack_ok())
        why = 2u;
    else
        return;
    fm1_cpu1_hold_begin();                               /* (after 2.7.1) recorded with the bus traps still off, as on the
                                                          * late path: a flag the hold raises traps with the crash screen's
                                                          * core 1 line right */
    cpu1_why = why;
    cpu1_state = CPU1_FAILED;
    cpu1_bar = CPU1_BAR;
    fm1_cpu1_hold_end();
}

static uint32_t cpu1_lcg(uint32_t x)                     /* 64 steps of an LCG: the self-test's work */
{
    uint32_t i;
    for (i = 0; i < 64u; i++)
        x = x * 1664525u + 1013904223u;
    return x;
}

static void cpu1_selftest(uint32_t arg)                  /* runs on core 1, from the flash, as the render will */
{
    cpu1_test_out = cpu1_lcg(arg);
}

/* at power-on, before the audio and the timers (X0X: started later, core 1 never reached its entry). The
 * reserved top of RAM is not locked yet (fm1_guard_lock_top comes after): its entry word can be written.
 * The PC limits are opened for the start (it boots through the chip's ROM) and armed again after. */
static void cpu1_boot(void)
{
    int late;
    cpu1_units = cpu1_off ? 0 : -1;                      /* (the settings were read before: persist_boot) */
    if (cpu1_off)                                        /* 2.7: 2ND CORE OFF (ON is the default): core 1 is never started,
                                                          * the boot is 2.6's; ON takes effect at the next power-on */
        return;
    if (cpu1_bar == CPU1_BAR || bootguard.failed) {      /* (after 2.7.1) held for good or a crash beside it before this
                                                          * warm reset (cpu1_bar: until power-off), or the boot before died in
                                                          * its first 30 s (bootguard, main.c: this boot only, as Felucca
                                                          * 1.6.1): one core; main.c says so once */
        cpu1_why = cpu1_bar == CPU1_BAR ? 5u : 6u;
        return;
    }
    fm1_guard_pc_open();
    if (fm1_cpu1_start() != 0) {
        fm1_guard_enable(FM1_GUARD_PC);
        cpu1_state = CPU1_NOSTART;
        return;
    }
    fm1_guard_enable(FM1_GUARD_PC);
    cpu1_test_out = 0;
    late = !fm1_cpu1_run(cpu1_selftest, 20261010u) || fm1_cpu1_wait() != 0;
    if (late || cpu1_test_out != cpu1_lcg(20261010u)) {
        fm1_cpu1_hold_begin();                           /* answered, but wrong or late: one core from here on */
        cpu1_state = CPU1_FAILED;
        cpu1_why = late ? cpu1_held_why() : 4u;          /* (after 2.7.1) in time but wrong: it read (or worked) wrong */
        cpu1_bar = CPU1_BAR;                             /* (after 2.7.1) and after a warm reset */
        fm1_cpu1_hold_end();                             /* (after 2.7.1) the traps back on after the record, as above */
        return;
    }
    cpu1_state = CPU1_OK;
}

static const char *cpu1_label(void)                      /* ABOUT: the hardware's state, and the menu's switch (short:
                                                          * it shares its row with the licence line) */
{
    return cpu1_state != CPU1_OK ? "1 CORE" : cpu1_units ? "2 CORES" : "C1 OFF";
}
/* (2.7) ABOUT: the two supplies as they are now (fm1_sys.h), "1.38V 1.60V": SYSVDD, VDC14 */
static void cpu1_supplies(char *b)
{
    uint32_t s = 930u + 30u * fm1_sysvdd(), v = 1250u + 50u * fm1_vdc14();
    b[0] = (char)('0' + s / 1000u), b[1] = '.', b[2] = (char)('0' + s / 100u % 10u), b[3] = (char)('0' + s / 10u % 10u);
    b[4] = 'V', b[5] = ' ';
    b[6] = (char)('0' + v / 1000u), b[7] = '.', b[8] = (char)('0' + v / 100u % 10u), b[9] = (char)('0' + v / 10u % 10u);
    b[10] = 'V', b[11] = 0;
}

/* bytes of core 1's user stack never written: fm1_cpu1_start filled it with a mark; the render on core 1
 * (fx.c mix_units: FM6, PHYS, GRAIN go deepest) eats it from the top. Console: cpu1_stack_free */
static uint32_t cpu1_stack_free(void)
{
    uint32_t i;
    if (cpu1_state == CPU1_OFF)                          /* never started: no figure (not 0, which reads as a full stack) */
        return FM1_C1_STACK_WORDS * 4u;
    for (i = 0; i < FM1_C1_STACK_WORDS && _c1_ustack[i] == FM1_C1_MARK; i++)
        ;
    return i * 4u;
}
#else
static void cpu1_boot(void) {}
static const char *cpu1_label(void) { return "1 CORE"; }
static int cpu1_on(void) { return 0; }
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { (void)fn; (void)arg; return 0; }
static int cpu1_wait(void) { return 0; }
static void cpu1_late_end(void) {}
static uint32_t cpu1_ticks(void) { return 0; }
#endif
