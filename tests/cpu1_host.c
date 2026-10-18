/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* The second core on the host: firmware/src/cpu1.h's API for the tests, so the render that hands jobs to
 * core 1 on the FM-1 runs on the Mac in three ways:
 *   CORES=1 (default)   the job runs on this thread, but only at cpu1_wait: what the caller does between
 *                       run and wait must not depend on the job, as on the device (one thread: deterministic)
 *   CORES=2             the job runs on a POSIX thread, started at the first cpu1_run; cpu1_wait blocks for
 *                       it. Built with -fsanitize=thread, every shared access not in the plan's list shows up
 *   CHAOS=N             with CORES=2: the thread sleeps 0..10 N us before and after each job (seeded by N),
 *                       so nothing may depend on how fast core 1 is
 *   CPU1_TIMEOUT_AT=K   the K-th job is lost: the double never runs it, cpu1_wait returns -1 and core 1 is
 *                       held from then on (cpu1_on 0, cpu1_run 0): the caller's fallback path, once
 *   CPU1_UNITS=mask     the render units handed to core 1 (cpu1.h cpu1_units): 0 none, 15 all that may go
 * cpu1_ticks() is 0 here: the load figures (fx.c cpu1_load_half) are the device's.
 * cpu1_host_jobs() and cpu1_host_timeouts() tell a test what happened. Nothing of the firmware changes. */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static struct {
    int init, mode, chaos, on, pending;                  /* pending: a job handed over and not waited yet */
    uint32_t jobs, timeout_at, timeouts, seed;
    cpu1_job_t fn;
    uint32_t arg;
    pthread_t th;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int th_started, go, done;
} c1h;

static uint32_t c1h_rand(void) { c1h.seed = c1h.seed * 1664525u + 1013904223u; return c1h.seed >> 8; }

static void *cpu1_host_thread(void *p)
{
    (void)p;
    for (;;) {
        cpu1_job_t fn;
        uint32_t arg;
        pthread_mutex_lock(&c1h.mu);
        while (!c1h.go)
            pthread_cond_wait(&c1h.cv, &c1h.mu);
        c1h.go = 0;
        fn = c1h.fn;
        arg = c1h.arg;
        pthread_mutex_unlock(&c1h.mu);
        if (c1h.chaos)
            usleep(c1h_rand() % (10u * (uint32_t)c1h.chaos + 1u));
        fn(arg);
        if (c1h.chaos)
            usleep(c1h_rand() % (10u * (uint32_t)c1h.chaos + 1u));
        pthread_mutex_lock(&c1h.mu);
        c1h.done = 1;
        pthread_cond_signal(&c1h.cv);
        pthread_mutex_unlock(&c1h.mu);
    }
    return 0;
}

static void cpu1_host_init(void)
{
    const char *e;
    if (c1h.init)
        return;
    c1h.init = 1;
    e = getenv("CORES");
    c1h.mode = e && atoi(e) >= 2 ? 2 : 1;
    e = getenv("CHAOS");
    c1h.chaos = e ? atoi(e) : 0;
    c1h.seed = (uint32_t)c1h.chaos * 7919u + 1u;
    e = getenv("CPU1_TIMEOUT_AT");
    c1h.timeout_at = e ? (uint32_t)atoi(e) : 0;
    e = getenv("CPU1_UNITS");
    if (e)
        cpu1_units = atoi(e);
    c1h.on = 1;
    pthread_mutex_init(&c1h.mu, 0);
    pthread_cond_init(&c1h.cv, 0);
}

static int cpu1_on(void) { cpu1_host_init(); return c1h.on; }
static uint32_t cpu1_ticks(void) { return 0; }
static uint32_t cpu1_host_jobs(void) { return c1h.jobs; }
static uint32_t cpu1_host_timeouts(void) { return c1h.timeouts; }
static int cpu1_host_mode(void) { cpu1_host_init(); return c1h.mode; }

static int cpu1_run(cpu1_job_t fn, uint32_t arg)
{
    cpu1_host_init();
    if (!c1h.on)
        return 0;
    if (c1h.pending)
        abort();                                         /* two jobs at once: the protocol is one at a time */
    c1h.jobs++;
    c1h.pending = 1;
    c1h.fn = fn;
    c1h.arg = arg;
    if (c1h.timeout_at && c1h.jobs == c1h.timeout_at)
        return 1;                                        /* lost: cpu1_wait says so */
    if (c1h.mode == 2) {
        pthread_mutex_lock(&c1h.mu);
        if (!c1h.th_started) {
            pthread_create(&c1h.th, 0, cpu1_host_thread, 0);
            c1h.th_started = 1;
        }
        c1h.go = 1;
        c1h.done = 0;
        pthread_cond_signal(&c1h.cv);
        pthread_mutex_unlock(&c1h.mu);
    }
    return 1;
}

static int cpu1_wait(void)
{
    if (!c1h.pending)
        return 0;                                        /* nothing handed over, or already waited */
    c1h.pending = 0;
    if (c1h.timeout_at && c1h.jobs == c1h.timeout_at) {
        c1h.timeouts++;
        c1h.on = 0;                                      /* held: one core from here on */
        return -1;
    }
    if (c1h.mode == 2) {
        pthread_mutex_lock(&c1h.mu);
        while (!c1h.done)
            pthread_cond_wait(&c1h.cv, &c1h.mu);
        pthread_mutex_unlock(&c1h.mu);
    } else {
        c1h.fn(c1h.arg);                                 /* inline, deferred to here: as late as the device's */
    }
    return 0;
}
