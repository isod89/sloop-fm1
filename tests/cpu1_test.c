/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* The host double of the second core (tests/cpu1_host.c): a hand-off and its result, 20000 jobs with exact
 * totals through the caller's fallback pattern, the thread (CORES=2), the deferred job (CORES=1), the lost job
 * and the hold (CPU1_TIMEOUT_AT). tests/run_tests.sh runs it as CORES=1, CORES=2, CORES=2 CHAOS=7,
 * CORES=2 CPU1_TIMEOUT_AT=777 and, under -fsanitize=thread, CORES=2 CHAOS=3. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../firmware/src/cpu1.h"
#include "cpu1_host.c"

static int fails;
#define CHECK(c, what) do { if (c) printf("cpu1: %-70s ok\n", what); else { printf("cpu1: %-70s FAIL\n", what); fails++; } } while (0)

static volatile uint32_t out;
static uint32_t acc[64], job_calls;
static pthread_t job_thread;

static void job_sum(uint32_t arg)                        /* what a render unit does: reads arg, writes its own buffers */
{
    uint32_t i;
    job_calls++;
    job_thread = pthread_self();
    for (i = 0; i < 64u; i++)
        acc[i] += arg + i;
    out = arg * 3u + 1u;
}

int main(void)
{
    const char *e = getenv("CPU1_TIMEOUT_AT");
    uint32_t lost = e ? (uint32_t)atoi(e) : 0, n, sum0 = 0;
    int cores = cpu1_host_mode();

    /* one hand-off: the result is there after the wait */
    out = 0;
    CHECK(cpu1_run(job_sum, 5) == 1, "run: core 1 takes the job");
    if (cores == 1)
        CHECK(out == 0, "CORES=1: the job has not run before the wait (deferred, as on the device)");
    CHECK(cpu1_wait() == 0, "wait: done");
    CHECK(out == 16, "the job's result is visible after the wait");

    /* the caller's pattern, 20000 times: run; wait; on a failure do the job yourself. Exact totals either way */
    memset(acc, 0, sizeof acc);
    job_calls = 0;
    for (n = 1; n <= 20000u; n++) {
        if (!cpu1_run(job_sum, n) || cpu1_wait() != 0)
            job_sum(n);                                  /* no core 1, or it failed: here */
        sum0 += n;
    }
    CHECK(acc[0] == sum0, "20000 jobs: acc[0] is the exact sum");
    CHECK(acc[63] == sum0 + 63u * 20000u, "20000 jobs: acc[63] is the exact sum");
    CHECK(job_calls == 20000u, "every job ran exactly once (the lost one by the caller)");

    if (lost) {
        CHECK(cpu1_host_timeouts() == 1u, "CPU1_TIMEOUT_AT: one job was lost");
        CHECK(cpu1_on() == 0, "after the lost job core 1 is held");
        CHECK(cpu1_run(job_sum, 1) == 0, "held: cpu1_run gives the job back to the caller");
        CHECK(cpu1_host_jobs() == lost, "no job was handed over after the hold");
    } else {
        CHECK(cpu1_host_timeouts() == 0, "no job lost");
        CHECK(cpu1_on() == 1, "core 1 still on");
        CHECK(cpu1_host_jobs() == 20001u, "every job was handed over");
        if (cores == 2)
            CHECK(!pthread_equal(job_thread, pthread_self()), "CORES=2: the jobs ran on another thread");
        else
            CHECK(pthread_equal(job_thread, pthread_self()), "CORES=1: the jobs ran on this thread");
    }
    printf("cpu1 host double: CORES=%d CHAOS=%s jobs=%u timeouts=%u %s\n", cores, getenv("CHAOS") ? getenv("CHAOS") : "0",
           cpu1_host_jobs(), cpu1_host_timeouts(), fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
