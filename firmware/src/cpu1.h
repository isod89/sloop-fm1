/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* The second core as the render sees it: one job at a time.
 *   cpu1_run(fn, arg)  hands a job over: 1 = core 1 took it, 0 = run it yourself (no second core, or held)
 *   cpu1_wait()        0 = the job is done; -1 = core 1 did not finish in time: it is held from now on and
 *                      the caller runs the job itself, once
 *   cpu1_on()          1 while core 1 runs and answers
 * Between run and wait the caller must not touch what the job reads or writes. The firmware defines these
 * in cpu1.c over hal/fm1_cpu1.h; the host tests in tests/cpu1_host.c (inline, or a POSIX thread with
 * CORES=2, with random delays and a lost job on request), so the same render code is proven on the Mac. */
#pragma once
typedef void (*cpu1_job_t)(uint32_t);
static int cpu1_run(cpu1_job_t fn, uint32_t arg);
static int cpu1_wait(void);
static int cpu1_on(void);
static void cpu1_late_end(void);   /* (after 2.7.1) after cpu1_wait's -1 and the job run here: the hold's traps back on */
static volatile uint8_t cpu1_late_test;   /* (after 2.7.1, the console's "cpu1 late", FELUCCA_CDC builds) the next job
                                           * runs late: the hold of a busy core 1, tried on the device */
static volatile uint8_t cpu1_fault_test;  /* (after 2.7.1, the console's "cpu1 fault", FELUCCA_CDC builds) the next job
                                           * goes past core 1's stack: its stack limit and the fault park, tried */
/* the render units core 1 gets (fx.c units_split): -1 the firmware's share (the last part and the drums), else a
 * mask of them, bit k = synth part k, bit NPART = the drums; 0 = none (the menu's 2ND CORE OFF, the tests:
 * CPU1_UNITS=mask in the host double). Whatever the value, the output is the same, bit for bit */
static int32_t cpu1_units = -1;
/* (after 2.7.1) where a hold may stop core 1 in the middle of a job and leave a unit's state half written: the windows
 * the render must survive (fx.c mix_part's preset fade, voice.c's engine fade, the engines' caches, a unit done, a
 * voice's end). Nothing on the FM-1; the host double (tests/cpu1_host.c) stops the job there, as fm1_cpu1_wait's hold
 * does (tests/cpu1_hold_test.c) */
#ifndef CPU1_HOLD_POINT
#define CPU1_HOLD_POINT(site) ((void)0)
#endif
/* a 24 MHz tick counter for the load figures (fx.c cpu1_load_half): the device's TIMER4, readable by both
 * cores; the host double's is 0 (its figures mean nothing) */
#define CPU1_TICKS_PER_US 24u
static uint32_t cpu1_ticks(void);
