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
static uint32_t cpu1_timeouts;                           /* jobs core 1 did not finish in time (console) */

#if FELUCCA_CPU1
static volatile uint32_t cpu1_test_out;

/* cpu1.h's API over the HAL: the render hands its jobs through these */
static int cpu1_on(void) { return cpu1_state == CPU1_OK && fm1_c1_on; }
static uint32_t cpu1_ticks(void) { return fm1_ticks(); }
static int cpu1_run(cpu1_job_t fn, uint32_t arg) { return cpu1_state == CPU1_OK ? fm1_cpu1_run(fn, arg) : 0; }
static int cpu1_wait(void)
{
    if (fm1_cpu1_wait() == 0)
        return 0;
    cpu1_timeouts++;                                     /* fm1_cpu1_wait held the core: one core from here on */
    cpu1_state = CPU1_FAILED;
    return -1;
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
    fm1_guard_pc_open();
    if (fm1_cpu1_start() != 0) {
        fm1_guard_enable(FM1_GUARD_PC);
        cpu1_state = CPU1_NOSTART;
        return;
    }
    fm1_guard_enable(FM1_GUARD_PC);
    cpu1_test_out = 0;
    if (!fm1_cpu1_run(cpu1_selftest, 20261010u) || fm1_cpu1_wait() != 0 || cpu1_test_out != cpu1_lcg(20261010u)) {
        fm1_cpu1_hold();                                 /* answered, but wrong or late: one core from here on */
        cpu1_state = CPU1_FAILED;
        return;
    }
    cpu1_state = CPU1_OK;
}

static const char *cpu1_label(void)                      /* ABOUT: the hardware's state, and the menu's switch (short:
                                                          * it shares its row with the licence line) */
{
    return cpu1_state != CPU1_OK ? "1 CORE" : cpu1_units ? "2 CORES" : "C1 OFF";
}

/* bytes of core 1's user stack never written: fm1_cpu1_start filled it with a mark; the render on core 1
 * (fx.c mix_units: FM6, PHYS, GRAIN go deepest) eats it from the top. Console: cpu1_stack_free */
static uint32_t cpu1_stack_free(void)
{
    uint32_t i;
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
static uint32_t cpu1_ticks(void) { return 0; }
#endif
