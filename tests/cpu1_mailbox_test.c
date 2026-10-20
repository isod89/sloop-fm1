/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab) */
/* (after 2.7.1) The second core's own hand-off, run on the host. hostsim.c and cpu1_test.c drive the render through
 * a double of core 1 (tests/cpu1_host.c), so the firmware's mailbox (hal/fm1_cpu1.h) never ran off the device. This
 * test compiles it as it is, runs fm1_c1_main on a POSIX thread as core 1, calls core 0's fm1_cpu1_start / run /
 * wait / hold_begin / hold_end / hold / stack_ok from this thread, and cpu1.c's state over them (cpu1_boot,
 * cpu1_wait, cpu1_late_end, cpu1_guard_half, cpu1_stack_free, the bar). The chip is a model, behind the header's two
 * seams:
 *   FM1_C1_IO(a)    a word of hw[] for each register the header may touch (any other address aborts); TIMER4 is
 *                   the host's monotonic clock at 24 ticks a microsecond
 *   FM1_C1_SYNC()   a full fence. On core 0 the model then does what the chip does with what was written: C1_CON
 *                   enabled and out of hold starts core 1 through fm1_c1_entry (if the entry word holds it), a hold
 *                   stops it at once wherever it is (SIGUSR1, then a siglongjmp to the thread's top: the next start
 *                   boots it again), DBG_WR_EN's 0xE7 toggles the lock, DBG_MSG_CLR clears. Holding a running core 1
 *                   raises a bus-invalid flag (DBG_MSG bit 21, what 2.7 saw); a hold with those traps on, or such a
 *                   flag up while its trap is on, is counted as the crash it is on the device. On core 1 the model
 *                   stops there when core 0 holds it, and plays what a case asks for (a slow job, a fault while
 *                   idle, a wrong self-test result)
 * Each case runs in a child process (fork) with a core 1 of its own:
 *   start     core 1 reports in through fm1_c1_entry, trace 0xC1000002, its EMU bank as Melodee / X0X / Felucca
 *             set it (the stack window, EMU_CON bit 3 on, bits 2 and 16..20 off, EMU_MSG cleared)
 *   jobs      20000 jobs in order, each run once on core 1, done == job after each wait, a writer job's output
 *             read right after the wait (the acquire side)
 *   misread   what core 1 must refuse: a job ahead (done + 2), a stale one, a wrong check word, a null fn, several
 *             wrong values in turn, one seen during the wait: not run, mb.bad once per wrong value, bad_job /
 *             bad_done / bad_fn, and the next wait -1 at once, C1_CON held, fm1_c1_on 0
 *   late      a job that never finishes: -1 after 4 ms of TIMER4, held where it was
 *   fault     fm1_c1_fault from a job: trace 0xC1DEAD00 | vec, fault_pc, the wait -1 at once
 *   stack     fm1_cpu1_stack_ok over the red zone and the stack's lowest 256 bytes
 *   restart   a start after a hold: alive, bad, done, fn reset; a core 1 that does not report; jobs again
 *   traps     hold_begin / hold_end in pairs and not: the traps back as they were, the lock restored only when it
 *             was locked, busflags accumulating; a hold with the traps on is seen by the model (not vacuous)
 *   soak      1000000 jobs (argv[1]) with random small delays on both cores: none lost, none run twice
 *   boot-*, wait-*, guard-*   cpu1.c: the self-test, 2ND CORE OFF, the bar, a failed boot before, no answer, a late
 *             or wrong self-test; cpu1_wait's why (1 late, 3 a fault, 2 its stack limit, 4 a misread); cpu1_guard_half's
 *             (3, 2, 4, 2)
 *   lateflag-*  a bus flag that latches after the hold's clear traps when the traps come back: cpu1.c has recorded
 *             the hold by then (guard, a wrong self-test, the late path), so the crash screen's C1 line is right
 * Plain and under ASan + UBSan (tests/cpu1_modes.sh). Not under ThreadSanitizer: the mailbox is volatile words
 * ordered by csync, as on the device, and TSan takes neither a volatile access nor a lone fence as synchronisation
 * (every hand-off would be a race report); it also defers the hold's signal while core 1 spins in its fault loop.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/cpu1_mailbox_test tests/cpu1_mailbox_test.c -lm
 *   build/host/cpu1_mailbox_test [soak jobs [case]] */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <pthread/qos.h>
#endif

/* ------------------------------------------------------------------- the chip's registers, as the header sees them */
enum { R_CON, R_CLK, R_T4, R_ENTRY, R_WR_EN, R_MSG, R_MSG_CLR, R_EN, R_EMU, R_IBANK = R_EMU + 6, R_N = R_IBANK + 32 };
enum { R_EMU_CON = R_EMU, R_EMU_MSG, R_EMU_HI0, R_EMU_LO0, R_EMU_HI1, R_EMU_LO1 };   /* 0x1EEF2D0 .. 0x1EEF2E4 */
static volatile uint32_t hw[R_N];
static uint32_t t4_reads;                                /* core 0's reads of TIMER4 through the header */
static uint32_t t4_virtual, t4_virt;                     /* (a case's) TIMER4 steps t4_virtual ticks a read of core 0's */

static uint32_t host_t4(void)                            /* TIMER4: 24 MHz, from the host's monotonic clock */
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 24000000u + (uint64_t)ts.tv_nsec * 3u / 125u);
}

static volatile uint32_t *c1hw_reg(uint32_t a)
{
    int k;
    switch (a) {
    case 0x1EEE004u: k = R_CON; break;                   /* C1_CON */
    case 0x00010008u: k = R_CLK; break;                  /* bit 3 held while core 1 starts */
    case 0x10804u: hw[R_T4] = t4_virtual ? (t4_virt += t4_virtual) : host_t4(); t4_reads++; k = R_T4; break;
    case 0x01C7FFF8u: k = R_ENTRY; break;                /* the word core 1 starts from */
    case 0x1EEE240u: k = R_WR_EN; break;
    case 0x1EEE244u: k = R_MSG; break;
    case 0x1EEE248u: k = R_MSG_CLR; break;
    case 0x1EEE340u: k = R_EN; break;
    default:
        if (a >= 0x1EEF2D0u && a <= 0x1EEF2E4u && !(a & 3u))
            k = R_EMU + (int)((a - 0x1EEF2D0u) >> 2);   /* core 1's EMU bank */
        else if (a >= 0x1EEF300u && a < 0x1EEF380u && !(a & 3u))
            k = R_IBANK + (int)((a - 0x1EEF300u) >> 2); /* core 1's interrupt bank */
        else {
            fprintf(stderr, "mailbox: the header touched 0x%08X, a register the model does not have\n", (unsigned)a);
            abort();
        }
    }
    return &hw[k];
}
static void c1hw_sync(void);
#define FM1_C1_IO(a) (*c1hw_reg(a))
#define FM1_C1_SYNC() c1hw_sync()
/* what felucca.c includes before the header (fm1_sys.h's hold registers, fm1_irq.h's DBG_MSG), at the model */
#define FM1__C1_CON FM1_C1_IO(0x1EEE004u)
#define FM1__DBG_WR_EN FM1_C1_IO(0x1EEE240u)
#define FM1_DBG_MSG FM1_C1_IO(0x1EEE244u)
#define FM1__DBG_MSG_CLR FM1_C1_IO(0x1EEE248u)
#define FM1__DBG_EN FM1_C1_IO(0x1EEE340u)
#define FM1__DBG_BUS 0x003F0030u

/* cpu1.c's surroundings: main.c's boot guard, fm1_guard.h's PC limits, fm1_time.h's ticks, fm1_sys.h's supplies */
#define FELUCCA_CPU1 1
static struct { uint8_t failed; } bootguard;
enum { FM1_GUARD_PC = 8 };
static uint32_t pc_opens, pc_arms;
static void fm1_guard_pc_open(void) { pc_opens++; }
static void fm1_guard_enable(uint32_t which) { if (which & FM1_GUARD_PC) pc_arms++; }
static uint32_t fm1_ticks(void) { return host_t4(); }
static uint32_t fm1_sysvdd(void) { return 15u; }
static uint32_t fm1_vdc14(void) { return 7u; }

#define section(x) unused                                /* .c1_text, .noinit: the device's sections (Mach-O has none) */
#include "../firmware/hal/fm1_cpu1.h"
#include "../firmware/src/cpu1.h"
#include "../firmware/src/cpu1.c"
#undef section

uint32_t _c1_red[FM1_C1_RED_WORDS], _c1_ustack[FM1_C1_STACK_WORDS], _c1_sstack_top[1];   /* app.ld's, fm1_cpu1.S's */
#define TRAPS_ON (FM1__DBG_BUS | 0x00000407u)            /* the bus traps on (fm1_guard.h), and bits the hold must keep */

/* ------------------------------------------------------------------- core 1: a thread the model starts and holds */
enum { C1_BOOTING = -1, C1_PARKED, C1_GO, C1_RUNNING };
static _Atomic int c1_state = C1_BOOTING, c1_stop, c1_in_code;
static _Atomic uint32_t hw_boots;                        /* entries into fm1_c1_entry */
static _Atomic uint32_t c1_stall, c1_fault_idle, c1_flip_test;   /* what core 1 plays at its csync, once */
static _Atomic uint32_t hw_emu_clears;                   /* EMU_MSG written all ones (it clears what is latched, as the chip) */
static _Atomic uint32_t c1_pend_max;                     /* the most csyncs core 1 made with one job pending: 3 when it
                                                          * takes each job at its first look (a lost one: thousands) */
static uint32_t c1_pend_done, c1_pend;
static pthread_t c1_th;
static sigjmp_buf c1_jb;
static __thread int on_core1;

static uint32_t hw_starts, hw_holds_running, hw_trap_at_hold, hw_trap_flag, hw_bad_entry, hw_noclk, hw_ibank_dirty;
static uint32_t hw_lost, hw_toggles, hw_en_locked_writes, hw_wr_en_odd, hw_clk_at_start, hw_en_seen;
static int hw_unlocked, hw_en_prev, hw_refuse_start, hw_pc_open_at_start;
static uint32_t hw_hold_flag = 1u << 21;                 /* the flag a hold of a running core 1 raises (2.7: bit 21) */
static int hw_late_flag;                                 /* (after 2.7.1) that flag latches again after DBG_MSG_CLR, once */
static uint32_t hw_trap_state = 0xFFu, hw_trap_why = 0xFFu, hw_trap_bar; /* cpu1.c's record when a trap came (the crash screen's C1 line) */

void fm1_c1_entry(void)                                  /* fm1_cpu1.S on the device: the breadcrumb, then the loop */
{
    fm1_c1_trace = 0xC1000001u;
    atomic_fetch_add(&hw_boots, 1u);
    fm1_c1_main();
}

static void c1_park(void)                                /* held: back to the thread's top, wherever it was */
{
    atomic_store(&c1_in_code, 0);
    siglongjmp(c1_jb, 1);
}

static void c1_sig(int s)
{
    (void)s;
    if (atomic_load(&c1_in_code) && atomic_load(&c1_stop))
        c1_park();
}

static void *c1_thread(void *p)
{
    (void)p;
    on_core1 = 1;
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    sigsetjmp(c1_jb, 1);
    atomic_store(&c1_in_code, 0);
    atomic_store(&c1_state, C1_PARKED);
    for (;;) {
        int s = C1_GO;
        if (atomic_compare_exchange_weak(&c1_state, &s, C1_RUNNING))
            break;
        sched_yield();
    }
    atomic_store(&c1_in_code, 1);
    if (atomic_load(&c1_stop))
        c1_park();
    fm1_c1_entry();
    abort();                                             /* (fm1_c1_main never returns) */
    return 0;
}

static void c1_side(void)                                /* core 1 at its csync */
{
    uint32_t v, j = fm1_c1_mb.job, d = fm1_c1_mb.done;
    if (atomic_load(&c1_stop))
        c1_park();
    if (hw[R_EMU_MSG] == 0xFFFFFFFFu) {                  /* (fm1_c1_main) a write of ones clears its EMU_MSG: nothing latched */
        hw[R_EMU_MSG] = 0;
        atomic_fetch_add(&hw_emu_clears, 1u);
    }
    if (j == d)
        c1_pend = 0;
    else {                                               /* a job pending: the csync after reading it, the one after
                                                          * running it, and one more if core 0 handed the next at once */
        if (d != c1_pend_done)
            c1_pend_done = d, c1_pend = 0;
        if (++c1_pend > atomic_load(&c1_pend_max))
            atomic_store(&c1_pend_max, c1_pend);
    }
    if (atomic_load(&c1_fault_idle) && fm1_c1_mb.job == fm1_c1_mb.done && (v = atomic_exchange(&c1_fault_idle, 0u)))
        fm1_c1_fault(v, 0x01C0FA17u);                    /* a fault taken in its waiting loop (fm1_irq.h, on core 1) */
    if (atomic_load(&c1_stall) && fm1_c1_mb.job != fm1_c1_mb.done && (v = atomic_exchange(&c1_stall, 0u))) {
        uint32_t t0 = host_t4();
        while (host_t4() - t0 < v)                       /* a slow core 1: this job takes v ticks more */
            if (atomic_load(&c1_stop))
                c1_park();
    }
    if (atomic_load(&c1_flip_test) && cpu1_test_out && atomic_exchange(&c1_flip_test, 0u))
        cpu1_test_out ^= 1u;                             /* a wrong self-test result */
}

static void hw_hold(void)                                /* the chip holds core 1: at once, wherever it is */
{
    uint32_t t0, last, now;
    for (;;) {
        int s = atomic_load(&c1_state);
        if (s == C1_PARKED)
            return;
        if (s == C1_GO) {                                /* released but not yet running: it never starts */
            if (atomic_compare_exchange_strong(&c1_state, &s, C1_PARKED))
                return;
            continue;
        }
        break;
    }
    hw_holds_running++;
    if (hw[R_EN] & FM1__DBG_BUS)
        hw_trap_at_hold++;                               /* the device: core 0 takes the bus-invalid exception */
    atomic_store(&c1_stop, 1);
    t0 = host_t4();
    last = t0 - 24000u;
    while (atomic_load(&c1_state) != C1_PARKED) {
        now = host_t4();
        if (now - last >= 24000u) {                      /* every 1 ms until it is held */
            pthread_kill(c1_th, SIGUSR1);
            last = now;
        }
        if (now - t0 > 24000u * 2000u) {
            hw_lost++;
            fprintf(stderr, "mailbox: the model could not hold core 1\n");
            return;
        }
    }
    atomic_store(&c1_stop, 0);
    hw[R_MSG] |= hw_hold_flag;                           /* the cut access, flagged some instructions later */
}

static void hw_start_edge(void)                          /* C1_CON: enabled and out of hold */
{
    uint32_t i;
    hw_clk_at_start = hw[R_CLK];
    if (!(hw[R_CLK] & 8u))
        hw_noclk++;
    for (i = 0; i < 32u; i++)
        if (hw[R_IBANK + i]) {
            hw_ibank_dirty++;
            break;
        }
    hw_pc_open_at_start = pc_opens > pc_arms;
    if (hw[R_ENTRY] != (uint32_t)(uintptr_t)&fm1_c1_entry) {
        hw_bad_entry++;                                  /* it would start somewhere else and never report */
        return;
    }
    if (hw_refuse_start)                                 /* a core 1 that does not come up */
        return;
    if (atomic_load(&c1_state) != C1_PARKED) {
        hw_lost++;
        return;
    }
    hw_starts++;
    atomic_store(&c1_state, C1_GO);
}

static void hw_follow(void)                              /* core 0's csync: what the chip did with what was written */
{
    uint32_t con = hw[R_CON];
    int en = (con & 8u) && !(con & 2u), toggled = 0;
    if (hw[R_WR_EN] == 0xE7u) {
        hw_unlocked = !hw_unlocked;
        hw_toggles++;
        toggled = 1;
    } else if (hw[R_WR_EN] != (uint32_t)hw_unlocked)
        hw_wr_en_odd++;
    hw[R_WR_EN] = (uint32_t)hw_unlocked;
    if (hw[R_EN] != hw_en_seen) {
        if (!hw_unlocked && !toggled)
            hw_en_locked_writes++;                       /* written while locked: the chip would ignore it */
        hw_en_seen = hw[R_EN];
    }
    if (hw[R_MSG_CLR]) {
        hw[R_MSG] &= ~hw[R_MSG_CLR];
        if (hw_late_flag && (hw[R_MSG_CLR] & hw_hold_flag) && hw_holds_running) {
            hw[R_MSG] |= hw_hold_flag;                   /* a flag that latched after the clear: it traps once the traps are on */
            hw_late_flag = 0;
        }
        hw[R_MSG_CLR] = 0;
    }
    if (!en)
        hw_hold();
    else if (!hw_en_prev)
        hw_start_edge();
    hw_en_prev = en;
    if (hw[R_EN] & hw[R_MSG] & FM1__DBG_BUS) {
        if (!hw_trap_flag++) {                           /* the device: fm1_fault draws the C1 line from these */
            hw_trap_state = cpu1_state;
            hw_trap_why = cpu1_why;
            hw_trap_bar = cpu1_bar;
        }
    }
}

static uint32_t pub_j, pub_f, pub_a, pub_c, pub_bad, pub_off;   /* the mailbox at core 0's last csync */
static void hw_pub(void)                                 /* a new job: its fn, arg, check seen a csync before, and right */
{
    uint32_t j = fm1_c1_mb.job, f = (uint32_t)(uintptr_t)fm1_c1_mb.fn, a = fm1_c1_mb.arg, c = fm1_c1_mb.check;
    if (!pub_off && j != pub_j && (f != pub_f || a != pub_a || c != pub_c || c != FM1_C1_CHECK(fm1_c1_mb.fn, a, j)))
        pub_bad++;
    pub_j = j, pub_f = f, pub_a = a, pub_c = c;
}
static void c1hw_sync(void)
{
    __asm__ volatile("" ::: "memory");
    atomic_thread_fence(memory_order_seq_cst);           /* csync: each core's words in order */
    if (on_core1)
        c1_side();
    else {
        hw_pub();
        hw_follow();
    }
    atomic_thread_fence(memory_order_seq_cst);
}

static void hw_set_traps(uint32_t en, int unlocked)
{
    hw[R_EN] = hw_en_seen = en;
    hw_unlocked = unlocked;
    hw[R_WR_EN] = (uint32_t)unlocked;
}

static void hw_init(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = c1_sig;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, 0);
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    hw_set_traps(TRAPS_ON, 0);                           /* the bus traps on, locked: as fm1_guard_enable leaves them */
    hw[R_CLK] = 0x101u;
    hw[R_CON] = 0x2u;                                    /* held */
    if (pthread_create(&c1_th, 0, c1_thread, 0))
        abort();
    while (atomic_load(&c1_state) != C1_PARKED)
        sched_yield();
}

/* ------------------------------------------------------------------- the checks */
static uint32_t fails, checks, soak_n = 1000000u;
static int host_stalled;                                 /* an unexpected 4 ms timeout or 20 ms start: the host took a
                                                          * thread off its CPU (a loaded machine); the case is run again */
#define CHECK(c, ...) do { char w_[200]; snprintf(w_, sizeof w_, __VA_ARGS__); checks++; \
    if (c) printf("mailbox: %-100s ok\n", w_); else { printf("mailbox: %-100s FAIL\n", w_); fails++; } } while (0)
#define UNTIL(cond, ms) do { uint32_t u0_ = host_t4(); while (!(cond) && host_t4() - u0_ < 24000u * (ms)) sched_yield(); } while (0)

static void spin_ms(uint32_t ms)
{
    uint32_t t0 = host_t4();
    while (host_t4() - t0 < 24000u * ms)
        sched_yield();
}

static volatile uint32_t job_runs, job_last, job_off_core1, spin_count, spin_started;
static uint32_t job_out[256];
static void job_note(uint32_t arg)                       /* a job: which, in what order, on which thread */
{
    if (!on_core1)
        job_off_core1++;
    job_last = arg;
    job_runs++;
}
static void job_write(uint32_t arg)                      /* a writer: core 0 reads this right after the wait */
{
    uint32_t i;
    for (i = 0; i < 256u; i++)
        job_out[i] = arg * 2654435761u + i;
    job_note(arg);
}
static void job_spin(uint32_t arg)                       /* never finishes */
{
    (void)arg;
    spin_started = 1;
    for (;;)
        spin_count++;
}
static void job_fault(uint32_t arg) { fm1_c1_fault(arg, 0x01C0BEEFu); }   /* a fault in a job (fm1_irq.h, core 1) */

static int start_ok(void)
{
    uint32_t starts = hw_starts;
    int r = fm1_cpu1_start();
    if (r != 0 || !fm1_c1_on) {
        if (hw_starts != starts && !hw_refuse_start)
            host_stalled = 1;                            /* released, but not scheduled within 20 ms */
        printf("mailbox: fm1_cpu1_start failed (%d)\n", r);
        fails++;
        return 0;
    }
    return 1;
}

static int wait_ok(void)                                 /* fm1_cpu1_wait where no timeout is expected */
{
    int r = fm1_cpu1_wait();
    if (r != 0 && !fm1_c1_mb.bad && (fm1_c1_trace & 0xFFFFFF00u) != FM1_C1_FAULT)
        host_stalled = 1;
    return r;
}

static uint32_t jobs_ok(uint32_t n)                      /* n jobs through run and wait: how many went wrong */
{
    uint32_t k, bad = 0;
    for (k = 0; k < n; k++) {
        uint32_t runs = job_runs, arg = 0x5EED0000u + k;
        if (fm1_cpu1_run(job_note, arg) != 1 || wait_ok() != 0 || job_runs != runs + 1u || job_last != arg)
            bad++;
    }
    if (bad) {
        printf("mailbox: %u of %u jobs went wrong\n", bad, n);
        fails++;
    }
    return bad;
}

/* core 0's words as fm1_cpu1_run writes them, with values a misread would give core 1 */
static void mb_write(uint32_t job, void (*fn)(uint32_t), uint32_t arg, uint32_t check)
{
    pub_off = 1;                                         /* (the cases' own wrong values) */
    fm1_c1_mb.fn = fn;
    fm1_c1_mb.arg = arg;
    fm1_c1_mb.check = check;
    FM1_C1_SYNC();
    fm1_c1_mb.job = job;
    FM1_C1_SYNC();
    pub_off = 0;
}

static int c1_held(void) { return (hw[R_CON] & 2u) && !(hw[R_CON] & 8u) && atomic_load(&c1_state) == C1_PARKED; }

static void expect_held(const char *what)
{
    CHECK(c1_held() && !fm1_c1_on, "%s: C1_CON held (bit 1 set, bit 3 clear), core 1 stopped, fm1_c1_on 0", what);
}

static void expect_traps_back(const char *what)
{
    CHECK(!fm1_c1_traps.open && hw[R_EN] == TRAPS_ON && !hw_unlocked && !(hw[R_MSG] & FM1__DBG_BUS),
          "%s: the traps back on, locked again, the bus flags cleared", what);
    CHECK(hw_trap_at_hold == 0u && hw_trap_flag == 0u && hw_en_locked_writes == 0u && hw_wr_en_odd == 0u && hw_lost == 0u,
          "%s: no trap taken (none on at the hold, no flag up under one)", what);
}

static void held_at_once(const char *what)               /* core 1 refused or faulted: the next wait */
{
    int r;
    t4_reads = 0;
    r = fm1_cpu1_wait();
    CHECK(r == -1 && t4_reads == 1u, "%s: the wait returns -1 at once (%u TIMER4 read)", what, t4_reads);
    expect_held(what);
    CHECK(fm1_cpu1_run(job_note, 1u) == 0, "%s: fm1_cpu1_run 0 from then on (one core)", what);
    CHECK(fm1_c1_traps.open && !(hw[R_EN] & FM1__DBG_BUS) && hw_unlocked,
          "%s: the bus traps off until hold_end (the caller renders first)", what);
    fm1_cpu1_hold_end();
    expect_traps_back(what);
    CHECK((fm1_c1_busflags & FM1__DBG_BUS) == hw_hold_flag, "%s: cpu1_busflags: the flag the hold raised", what);
    CHECK(fm1_c1_busflags_cut == hw_hold_flag, "%s: cpu1_busflags_cut: the same, right after the hold", what);
}

/* (1) start */
static void case_start(int unused)
{
    uint32_t i, lo, hi, marks = 0;
    (void)unused;
    hw[R_EMU_CON] = 0x801F0005u;                         /* as a reset or a firmware before left it: bits 0, 2, 16..20, 31 */
    hw[R_EMU_MSG] = 0x00000123u;
    for (i = 0; i < 32u; i++)
        hw[R_IBANK + i] = 0xFFFFFFFFu;                   /* its interrupt bank as left: off before it starts */
    hw[R_ENTRY] = 0xDEADBEEFu;
    fm1_c1_trace = 0x5A5A5A5Au;
    fm1_c1_fault_emu = 0xA5A5A5A5u;                      /* (a fault before: start clears it) */
    CHECK(fm1_cpu1_start() == 0 && fm1_c1_on == 1u, "start: 0, fm1_c1_on 1");
    CHECK(fm1_c1_mb.alive == 1u, "start: core 1 reported alive");
    UNTIL(fm1_c1_trace == 0xC1000002u, 100);
    CHECK(fm1_c1_trace == 0xC1000002u, "start: trace 0xC1000002 (its loop), through fm1_c1_entry (0x%08X)", (unsigned)fm1_c1_trace);
    CHECK(hw_starts == 1u && atomic_load(&hw_boots) == 1u, "start: core 1 started once, through fm1_c1_entry");
    CHECK(hw_bad_entry == 0u && hw[R_ENTRY] == (uint32_t)(uintptr_t)&fm1_c1_entry, "start: the entry word holds fm1_c1_entry");
    CHECK(hw_noclk == 0u && (hw_clk_at_start & 8u) && hw[R_CLK] == 0x101u, "start: 0x10008 bit 3 set while it starts, restored after");
    CHECK(hw_ibank_dirty == 0u, "start: its interrupt bank all off before it started");
    lo = (uint32_t)(uintptr_t)_c1_ustack + 256u;
    hi = (uint32_t)(uintptr_t)_c1_sstack_top - 1u;
    CHECK(hw[R_EMU_HI0] == hi && hw[R_EMU_LO0] == lo && hw[R_EMU_HI1] == hi && hw[R_EMU_LO1] == lo,
          "start: EMU stack limits lo = _c1_ustack + 256, hi = _c1_sstack_top - 1 (both pairs)");
    CHECK(hw[R_EMU_CON] == 0x80000009u, "start: EMU_CON bit 3 on, bits 2 and 16..20 off, the rest kept (0x%08X)", (unsigned)hw[R_EMU_CON]);
    UNTIL(atomic_load(&hw_emu_clears), 100);
    CHECK(atomic_load(&hw_emu_clears) == 1u && hw[R_EMU_MSG] == 0u, "start: EMU_MSG cleared (all ones written: what was left goes)");
    CHECK(fm1_c1_fault_emu == 0u, "start: fault_emu 0");
    CHECK((hw[R_CON] & 8u) && !(hw[R_CON] & 2u), "start: C1_CON enabled, out of hold");
    CHECK(fm1_c1_mb.done == fm1_c1_mb.job && fm1_c1_mb.bad == 0u && fm1_c1_mb.fn == 0, "start: done == job, bad 0, fn 0");
    for (i = 0; i < FM1_C1_STACK_WORDS; i++)
        marks += _c1_ustack[i] == FM1_C1_MARK;
    for (i = 0; i < FM1_C1_RED_WORDS; i++)
        marks += _c1_red[i] == FM1_C1_MARK;
    CHECK(marks == FM1_C1_STACK_WORDS + FM1_C1_RED_WORDS && fm1_cpu1_stack_ok(), "start: the stack and its red zone marked, stack_ok");
    CHECK(hw_holds_running == 0u && hw[R_MSG] == 0u && fm1_c1_busflags == 0u && fm1_c1_busflags_cut == 0u,
          "start: its hold found core 1 stopped (no flag)");
    expect_traps_back("start");
    CHECK(fm1_cpu1_start() == 0 && atomic_load(&hw_boots) == 1u, "start again while on: 0, no new boot");
}

/* (2) many jobs in order */
static void case_jobs(int unused)
{
    uint32_t k, i, n = 20000u, job0, order = 0, ret = 0, out = 0, stalls = 0;
    (void)unused;
    if (!start_ok())
        return;
    job0 = fm1_c1_mb.job;
    atomic_store(&c1_pend_max, 0u);
    for (k = 1; k <= n; k++) {
        uint32_t arg = k * 0x9E3779B9u, runs = job_runs;
        if (fm1_cpu1_run(k & 1u ? job_write : job_note, arg) != 1)
            ret++;
        if (fm1_c1_mb.job != job0 + k)
            order++;
        if (fm1_cpu1_wait() != 0) {                      /* a timeout: the host's (a loaded machine), as in the soak */
            if (fm1_c1_mb.bad || (fm1_c1_trace & 0xFFFFFF00u) == FM1_C1_FAULT || ++stalls > 20u) {
                ret++;
                break;
            }
            fm1_cpu1_hold_end();
            if (!start_ok())
                break;
            job0 = fm1_c1_mb.job - k;
            continue;                                    /* (the job may have run or not: not counted) */
        }
        if (k & 1u)                                      /* the acquire side: read right after the wait */
            for (i = 0; i < 256u; i++)
                out += job_out[i] != arg * 2654435761u + i;
        if (fm1_c1_mb.done != fm1_c1_mb.job || job_runs != runs + 1u || job_last != arg)
            order++;
    }
    CHECK(ret == 0u && k == n + 1u, "jobs: %u runs took, the waits returned 0 (%u host stalls over 4 ms skipped)", n, stalls);
    CHECK(order == 0u, "jobs: each ran exactly once, in order, done == job after its wait (%u wrong)", order);
    CHECK(out == 0u, "jobs: a writer job's 256 words all seen right after the wait (%u wrong)", out);
    CHECK(job_off_core1 == 0u, "jobs: every job ran on core 1's thread");
    CHECK(pub_bad == 0u, "jobs: fn, arg and the check word published a csync before the job (%u not)", pub_bad);
    CHECK(atomic_load(&c1_pend_max) <= 3u, "jobs: core 1 took each job at its first look (at most %u csyncs with it pending)",
          (unsigned)atomic_load(&c1_pend_max));
    CHECK(fm1_c1_mb.bad == 0u && fm1_c1_trace == 0xC1000002u && fm1_c1_on, "jobs: no misread, no fault, still on");
}

/* (3) misreads seen from core 1 */
enum { MR_AHEAD, MR_STALE, MR_CHECK, MR_NULL, MR_SEQ, MR_DURING };
static void case_misread(int kind)
{
    static const char *const NAME[] = {"misread, a job ahead (done + 2)", "misread, a stale job (done - 1)",
                                       "misread, a wrong check word", "misread, a null fn",
                                       "misread, wrong values in turn", "misread, seen during the wait"};
    const char *w = NAME[kind];
    uint32_t d, runs0, want_job = 0, want_fn = 0xB0B0B0B0u;
    if (!start_ok() || jobs_ok(5))
        return;
    d = fm1_c1_mb.done;
    fm1_c1_mb.bad_fn = 0xB0B0B0B0u;                      /* (not written unless the job was the next one) */
    runs0 = job_runs;
    switch (kind) {
    case MR_AHEAD:
        want_job = d + 2u;
        mb_write(want_job, job_note, 7u, FM1_C1_CHECK(job_note, 7u, want_job));
        break;
    case MR_STALE:
        want_job = d - 1u;
        mb_write(want_job, job_note, 7u, FM1_C1_CHECK(job_note, 7u, want_job));
        break;
    case MR_CHECK:
        want_job = d + 1u;
        want_fn = (uint32_t)(uintptr_t)job_note;
        mb_write(want_job, job_note, 7u, FM1_C1_CHECK(job_note, 7u, want_job) ^ 0x100u);
        break;
    case MR_NULL:
        want_job = d + 1u;
        want_fn = 0;
        mb_write(want_job, 0, 7u, FM1_C1_CHECK(0, 7u, want_job));
        break;
    case MR_SEQ: {
        static const uint32_t step[] = {2u, 3u, 3u, 2u, 0u};   /* job = done + step: 0 is not wrong (idle) */
        static const uint32_t want_bad[] = {1u, 2u, 2u, 3u, 3u};
        uint32_t s;
        for (s = 0; s < 5u; s++) {
            mb_write(d + step[s], job_note, 7u, FM1_C1_CHECK(job_note, 7u, d + step[s]));
            UNTIL(fm1_c1_mb.bad >= want_bad[s], 1000);
            spin_ms(2);
            CHECK(fm1_c1_mb.bad == want_bad[s] && (step[s] == 0u || fm1_c1_mb.bad_job == d + step[s]),
                  "%s: job done + %u, mb.bad %u (want %u)", w, step[s], (unsigned)fm1_c1_mb.bad, want_bad[s]);
        }
        mb_write(d + 1u, 0, 7u, FM1_C1_CHECK(0, 7u, d + 1u));
        UNTIL(fm1_c1_mb.bad >= 4u, 1000);
        CHECK(fm1_c1_mb.bad == 4u && fm1_c1_mb.bad_fn == 0u && fm1_c1_mb.bad_job == d + 1u, "%s: then a null fn: mb.bad 4, bad_fn 0", w);
        CHECK(job_runs == runs0, "%s: none of them ran", w);
        held_at_once(w);
        return;
    }
    default: {                                           /* MR_DURING: core 0 already waiting */
        uint32_t t0, el;
        int r;
        mb_write(d + 2u, job_note, 7u, FM1_C1_CHECK(job_note, 7u, d + 2u));
        t0 = host_t4();
        r = fm1_cpu1_wait();
        el = host_t4() - t0;
        if (el > 24000u * 4u && !fm1_c1_mb.bad)
            host_stalled = 1;                            /* core 1 never ran in those 4 ms */
        CHECK(r == -1 && el < 24000u * 4u && fm1_c1_mb.bad == 1u, "%s: -1 before the 4 ms timeout (%u us), mb.bad 1", w, el / 24u);
        CHECK(job_runs == runs0, "%s: not run", w);
        expect_held(w);
        fm1_cpu1_hold_end();
        expect_traps_back(w);
        return;
    }
    }
    UNTIL(fm1_c1_mb.bad, 1000);
    spin_ms(3);                                          /* core 1 sees the same wrong job on every pass meanwhile */
    CHECK(fm1_c1_mb.bad == 1u, "%s: counted once (mb.bad %u)", w, (unsigned)fm1_c1_mb.bad);
    CHECK(fm1_c1_mb.bad_job == want_job && fm1_c1_mb.bad_done == d, "%s: bad_job, bad_done recorded", w);
    CHECK(fm1_c1_mb.bad_fn == want_fn, "%s: bad_fn 0x%08X (want 0x%08X)", w, (unsigned)fm1_c1_mb.bad_fn, (unsigned)want_fn);
    CHECK(job_runs == runs0 && fm1_c1_mb.done == d, "%s: not run, done unchanged", w);
    held_at_once(w);
}

/* (3b) its done word misread: core 1 keeps done in a register (fm1_c1_main), so a wrong value in memory never makes it
 * run the last job again (its fn, arg and check still match that job) */
static void case_done_word(int unused)
{
    uint32_t d, runs0;
    (void)unused;
    if (!start_ok() || jobs_ok(5))
        return;
    d = fm1_c1_mb.done;
    runs0 = job_runs;
    fm1_c1_mb.done = d - 1u;                             /* as a misread of its own word would give it */
    FM1_C1_SYNC();
    spin_ms(3);
    CHECK(job_runs == runs0 && fm1_c1_mb.bad == 0u, "done word: the last job not run again, no misread counted");
    fm1_c1_mb.done = d;
    FM1_C1_SYNC();
    CHECK(jobs_ok(5) == 0u, "done word: the next jobs each once");
}

/* (4) a job that never finishes */
static void case_late(int unused)
{
    uint32_t d, t0, el, c;
    int r;
    (void)unused;
    if (!start_ok() || jobs_ok(3))
        return;
    d = fm1_c1_mb.done;
    CHECK(fm1_cpu1_run(job_spin, 0u) == 1, "late: core 1 takes a job that never finishes");
    t0 = host_t4();
    r = fm1_cpu1_wait();
    el = host_t4() - t0;
    if (el >= 24000u * 20u)
        host_stalled = 1;                                /* (core 0 off its CPU, or the hold's signal late) */
    CHECK(r == -1 && el > 24000u * 4u && el < 24000u * 20u, "late: the wait returns -1 after 4 ms of TIMER4 (%u us)", el / 24u);
    expect_held("late");
    CHECK(spin_started && fm1_c1_mb.done == d && fm1_c1_mb.job == d + 1u, "late: the job started, not finished: done one behind");
    CHECK(fm1_c1_mb.bad == 0u && fm1_c1_trace == 0xC1000002u, "late: no misread, no fault (cpu1_why 1)");
    c = spin_count;
    spin_ms(5);
    CHECK(spin_count == c, "late: held where it was, its loop no longer runs");
    CHECK(hw_holds_running == 1u && hw_trap_at_hold == 0u, "late: held while running, with the bus traps off");
    CHECK(fm1_c1_traps.open && !(hw[R_EN] & FM1__DBG_BUS), "late: the traps stay off until hold_end");
    fm1_cpu1_hold_end();
    expect_traps_back("late");
    CHECK(fm1_c1_busflags == hw_hold_flag, "late: cpu1_busflags: the flag the hold raised");
    CHECK(fm1_c1_busflags_cut == hw_hold_flag, "late: cpu1_busflags_cut: the same, right after the hold (before core 0 wrote)");
    if (!start_ok())                                     /* the bound itself: TIMER4 virtual, 1000 ticks a read of core 0's */
        return;
    fm1_cpu1_run(job_spin, 0u);
    t4_reads = 0;
    t4_virtual = 1000u;
    r = fm1_cpu1_wait();
    t4_virtual = 0;
    CHECK(r == -1 && t4_reads == 98u, "late: -1 at the first TIMER4 read past 96000 ticks (%u reads, want 98)", t4_reads);
    fm1_cpu1_hold_end();
}

/* (5) the fault park */
static void case_fault(int unused)
{
    (void)unused;
    if (!start_ok() || jobs_ok(2))
        return;
    CHECK(fm1_cpu1_run(job_fault, 0x1234u) == 1, "fault: a job that faults (fm1_c1_fault, vector 0x1234)");
    UNTIL(fm1_c1_trace == 0xC1DEAD34u, 1000);
    CHECK(fm1_c1_trace == 0xC1DEAD34u, "fault: trace 0xC1DEAD00 | (vec & 0xFF) (0x%08X)", (unsigned)fm1_c1_trace);
    CHECK(fm1_c1_fault_pc == 0x01C0BEEFu, "fault: fault_pc");
    CHECK(fm1_c1_fault_emu == 0u, "fault: fault_emu, its EMU_MSG then: no class latched (0x%08X)", (unsigned)fm1_c1_fault_emu);
    held_at_once("fault");
    CHECK(hw_holds_running == 1u, "fault: parked in its loop, held from there");
}

/* (6) the stack check */
static void case_stack(int unused)
{
    uint32_t i, marks = 0;
    (void)unused;
    if (!start_ok())
        return;
    CHECK(fm1_cpu1_stack_ok(), "stack: ok after start");
    _c1_red[17] = 0;
    CHECK(!fm1_cpu1_stack_ok(), "stack: a word of the red zone written: not ok");
    _c1_red[17] = FM1_C1_MARK;
    _c1_red[FM1_C1_RED_WORDS - 1u] ^= 1u;
    CHECK(!fm1_cpu1_stack_ok(), "stack: the red zone's top word: not ok");
    _c1_red[FM1_C1_RED_WORDS - 1u] = FM1_C1_MARK;
    _c1_ustack[0] = 0;
    CHECK(!fm1_cpu1_stack_ok(), "stack: the stack's lowest word: not ok");
    _c1_ustack[0] = FM1_C1_MARK;
    _c1_ustack[63] = FM1_C1_MARK - 1u;
    CHECK(!fm1_cpu1_stack_ok(), "stack: the 64th word (within its lowest 256 bytes): not ok");
    _c1_ustack[63] = FM1_C1_MARK;
    CHECK(fm1_cpu1_stack_ok(), "stack: marks back: ok");
    _c1_ustack[64] = 0;
    _c1_ustack[FM1_C1_STACK_WORDS - 1u] = 0;
    CHECK(fm1_cpu1_stack_ok(), "stack: words above its lowest 256 bytes (the jobs' depth): ok");
    _c1_red[5] = 1u;
    _c1_ustack[10] = 2u;
    fm1_cpu1_hold();
    CHECK(start_ok() && fm1_cpu1_stack_ok(), "stack: after a hold and a start again: ok");
    for (i = 0; i < FM1_C1_STACK_WORDS; i++)
        marks += _c1_ustack[i] == FM1_C1_MARK;
    for (i = 0; i < FM1_C1_RED_WORDS; i++)
        marks += _c1_red[i] == FM1_C1_MARK;
    CHECK(marks == FM1_C1_STACK_WORDS + FM1_C1_RED_WORDS, "stack: start marked every word again");
}

/* (7) start again after a hold */
static void case_restart(int unused)
{
    uint32_t d, x, t0, el;
    int r;
    (void)unused;
    if (!start_ok() || jobs_ok(10))
        return;
    d = fm1_c1_mb.done;
    x = d + 2u;
    mb_write(x, job_note, 3u, FM1_C1_CHECK(job_note, 3u, x));
    UNTIL(fm1_c1_mb.bad, 1000);
    CHECK(fm1_cpu1_wait() == -1, "restart: a misread, the wait -1");
    fm1_cpu1_hold_end();
    CHECK(fm1_c1_mb.alive == 1u && fm1_c1_mb.bad == 1u && fm1_c1_mb.done != fm1_c1_mb.job && fm1_c1_mb.fn,
          "restart: before: alive 1, bad 1, done != job, fn set");
    hw_refuse_start = 1;
    t0 = host_t4();
    r = fm1_cpu1_start();
    el = host_t4() - t0;
    CHECK(r == -1 && el >= 24000u * 20u, "restart: a core 1 that does not report: -1 after 20 ms (%u us): alive was reset first", el / 24u);
    CHECK(fm1_c1_mb.alive == 0u && hw[R_CLK] == 0x101u, "restart: alive 0, 0x10008 restored");
    expect_held("restart, no report");
    expect_traps_back("restart, no report");
    hw_refuse_start = 0;
    CHECK(start_ok(), "restart: start again: 0");
    CHECK(fm1_c1_mb.alive == 1u && fm1_c1_mb.bad == 0u && fm1_c1_mb.done == fm1_c1_mb.job && fm1_c1_mb.fn == 0,
          "restart: alive 1, bad 0, done == job, fn 0");
    UNTIL(fm1_c1_trace == 0xC1000002u, 100);
    CHECK(fm1_c1_trace == 0xC1000002u && atomic_load(&hw_boots) == 2u, "restart: a new boot through fm1_c1_entry, in its loop");
    CHECK(jobs_ok(100) == 0u, "restart: 100 jobs again, each once");
    mb_write(x, job_note, 3u, FM1_C1_CHECK(job_note, 3u, x));   /* the old wrong value, now stale */
    UNTIL(fm1_c1_mb.bad, 1000);
    CHECK(fm1_c1_mb.bad == 1u && fm1_c1_mb.bad_job == x, "restart: the same wrong job value counted again (a new loop)");
    held_at_once("restart");
}

/* (8) the hold's two halves */
static void case_traps(int unused)
{
    uint32_t en0, bf, tg;
    (void)unused;
    if (!start_ok())
        return;
    tg = hw_toggles;                                     /* (the start's own hold unlocked and locked once) */
    fm1_cpu1_hold_begin();                               /* locked, core 1 running (in its loop) */
    CHECK(c1_held() && !fm1_c1_on && hw_holds_running == 1u && hw_trap_at_hold == 0u, "traps: hold_begin holds a running core 1, traps off at the hold");
    CHECK(hw[R_EN] == (TRAPS_ON & ~FM1__DBG_BUS) && hw_unlocked && fm1_c1_traps.open && fm1_c1_traps.locked,
          "traps: hold_begin: the bus traps off (others kept), unlocked, open");
    fm1_cpu1_hold_begin();                               /* again (fm1_cpu1_wait's, then cpu1.c's fm1_cpu1_hold) */
    CHECK(fm1_c1_traps.en == TRAPS_ON && fm1_c1_traps.locked && hw_toggles == tg + 1u, "traps: a second hold_begin keeps the first's saved state");
    hw[R_MSG] |= 1u;                                     /* a flag that is not a bus trap's */
    fm1_cpu1_hold_end();
    expect_traps_back("traps, locked");
    CHECK(fm1_c1_busflags == hw_hold_flag && hw[R_MSG] == 1u, "traps: busflags the bus flag only; DBG_MSG: bit 0 left");
    CHECK(hw_toggles == tg + 2u, "traps: unlocked once, locked once");
    hw[R_MSG] = 0;
    en0 = hw[R_EN];
    bf = fm1_c1_busflags;
    fm1_cpu1_hold_end();                                 /* not open: nothing */
    CHECK(hw[R_EN] == en0 && !hw_unlocked && hw_toggles == tg + 2u && fm1_c1_busflags == bf, "traps: hold_end without hold_begin: nothing");
    hw_set_traps(TRAPS_ON, 1);                           /* unlocked to begin with */
    if (!start_ok())
        return;
    tg = hw_toggles;
    fm1_cpu1_hold_begin();
    hw[R_MSG] |= 1u << 16;
    fm1_cpu1_hold_end();
    CHECK(hw_unlocked && hw_toggles == tg && hw[R_EN] == TRAPS_ON && !fm1_c1_traps.open, "traps: unlocked before: not locked after, no 0xE7 written");
    CHECK(fm1_c1_busflags == (hw_hold_flag | 1u << 16), "traps: busflags accumulate across holds (0x%08X)", (unsigned)fm1_c1_busflags);
    hw_set_traps(TRAPS_ON, 0);
    hw_hold_flag = 1u << 5;
    if (!start_ok())
        return;
    fm1_cpu1_hold();                                     /* both halves at once */
    expect_held("traps, fm1_cpu1_hold");
    expect_traps_back("traps, fm1_cpu1_hold");
    CHECK(fm1_c1_busflags == (1u << 21 | 1u << 16 | 1u << 5), "traps: fm1_cpu1_hold: its flag kept too");
    if (!start_ok())                                     /* not vacuous: a hold with the traps on, as 2.7 held it */
        return;
    FM1__C1_CON |= 0x2u;
    FM1__C1_CON &= ~0x8u;
    FM1_C1_SYNC();
    CHECK(hw_trap_at_hold == 1u && hw_trap_flag == 1u, "traps: (control) a hold with the traps on is the model's crash");
}

/* (9) the soak */
static volatile uint32_t soak_last, soak_order, soak_runs, soak_sink;
static uint32_t soak_seed1 = 1u, soak_out[8];
static void soak_job(uint32_t arg)
{
    uint32_t i, d;
    soak_seed1 = soak_seed1 * 1664525u + 1013904223u;
    d = soak_seed1 >> 26;
    for (i = 0; i < d; i++)                              /* a random small time on core 1 */
        soak_sink++;
    if (arg != soak_last + 1u)                           /* in order, each once */
        soak_order++;
    soak_out[arg & 7u] = arg ^ 0xA5A5A5A5u;
    soak_runs++;
    soak_last = arg;                                     /* last: core 0 checks it after the wait */
}
static void case_soak(int unused)
{
    uint32_t k, i, d, seed0 = 12345u, lost = 0, wrong = 0, stalls = 0, here = 0, t0, el;
    volatile uint32_t sink0 = 0;
    (void)unused;
    if (!start_ok())
        return;
    atomic_store(&c1_pend_max, 0u);
    t0 = host_t4();
    for (k = 1; k <= soak_n; k++) {
        seed0 = seed0 * 1664525u + 1013904223u;
        for (d = seed0 >> 26, i = 0; i < d; i++)         /* before the hand-off */
            sink0++;
        if (fm1_cpu1_run(soak_job, k) != 1) {
            wrong++;
            break;
        }
        for (d = (seed0 >> 20) & 63u, i = 0; i < d; i++) /* core 0's own units meanwhile */
            sink0++;
        if (fm1_cpu1_wait() != 0) {                      /* the host took a thread off its CPU for 4 ms */
            stalls++;
            if (fm1_c1_mb.bad || (fm1_c1_trace & 0xFFFFFF00u) == FM1_C1_FAULT)
                wrong++;
            if (soak_last != k) {                        /* held before its last store: here, once (fx.c's late path) */
                soak_out[k & 7u] = k ^ 0xA5A5A5A5u;
                soak_last = k;
                here++;
            }
            fm1_cpu1_hold_end();
            if (fm1_cpu1_start() != 0) {
                wrong++;
                break;
            }
            continue;
        }
        if (soak_last != k)
            lost++;
        if (soak_out[k & 7u] != (k ^ 0xA5A5A5A5u))
            wrong++;
    }
    el = host_t4() - t0;
    CHECK(k == soak_n + 1u && lost == 0u && wrong == 0u, "soak: %u jobs, none lost, every result seen (%u lost, %u wrong)", soak_n, lost, wrong);
    CHECK(soak_order == 0u, "soak: none run twice or out of order on core 1 (%u)", (unsigned)soak_order);
    CHECK(soak_runs + here >= soak_n && soak_runs <= soak_n, "soak: core 1 ran %u, core 0 %u after a stall", (unsigned)soak_runs, here);
    CHECK(atomic_load(&c1_pend_max) <= 3u, "soak: core 1 took each job at its first look (at most %u csyncs with it pending)",
          (unsigned)atomic_load(&c1_pend_max));
    CHECK(stalls <= soak_n / 1000u + 5u, "soak: %u waits over 4 ms, the host's (each: held, the job here once if unfinished, "
          "started again)", stalls);
    CHECK(fm1_c1_mb.bad == 0u && fm1_c1_trace == 0xC1000002u && fm1_c1_on, "soak: no misread, no fault, still on");
    printf("mailbox: soak: %u jobs in %u ms, %.2f us a hand-off\n", soak_n, el / 24000u, (double)el / 24.0 / (double)soak_n);
}

static int boot_ok(const char *w)                        /* cpu1_boot to CPU1_OK */
{
    cpu1_boot();
    if (cpu1_state == CPU1_OK)
        return 1;
    if (cpu1_state == CPU1_NOSTART || (cpu1_state == CPU1_FAILED && cpu1_why == 1u))
        host_stalled = 1;                                /* not scheduled within 20 ms, or its self-test past 4 ms */
    CHECK(0, "%s: cpu1_boot (state %u, why %u)", w, cpu1_state, cpu1_why);
    return 0;
}

/* cpu1.c: cpu1_boot */
enum { B_OK, B_OFF, B_BAR, B_GUARD, B_NOANSWER, B_LATE, B_WRONG };
static void case_boot(int v)
{
    static const char *const NAME[] = {"boot", "boot, 2ND CORE OFF", "boot, barred (cpu1_bar)", "boot, the boot before failed",
                                       "boot, core 1 does not report", "boot, the self-test late", "boot, the self-test wrong"};
    const char *w = NAME[v];
    uint32_t t0, el, k, ok = 0;
    switch (v) {
    case B_OFF: cpu1_off = 1; break;
    case B_BAR: cpu1_bar = CPU1_BAR; break;
    case B_GUARD: bootguard.failed = 1; break;
    case B_NOANSWER: hw_refuse_start = 1; break;
    case B_LATE: atomic_store(&c1_stall, 24000u * 6u); break;
    case B_WRONG: atomic_store(&c1_flip_test, 1u); break;
    }
    t0 = host_t4();
    cpu1_boot();
    el = host_t4() - t0;
    if (v == B_OK && (cpu1_state == CPU1_NOSTART || (cpu1_state == CPU1_FAILED && cpu1_why == 1u)))
        host_stalled = 1;
    switch (v) {
    case B_OK:
        CHECK(cpu1_state == CPU1_OK && cpu1_on() && cpu1_why == 0u && cpu1_bar != CPU1_BAR, "%s: CPU1_OK, cpu1_why 0, not barred", w);
        CHECK(!strcmp(cpu1_label(), "2 CORES") && cpu1_units == -1, "%s: ABOUT \"2 CORES\", the firmware's share", w);
        CHECK(hw_starts == 1u && hw_pc_open_at_start && pc_opens == 1u && pc_arms == 1u, "%s: started with the PC limits open, armed again after", w);
        CHECK(cpu1_test_out == cpu1_lcg(20261010u) && fm1_c1_mb.job == 1u && fm1_c1_mb.done == 1u, "%s: the self-test ran once on core 1, right", w);
        CHECK(cpu1_stack_free() == FM1_C1_STACK_WORDS * 4u, "%s: cpu1_stack_free 2816 (the host's core 1 has its own stack)", w);
        _c1_ustack[700] = 0;
        CHECK(cpu1_stack_free() == 2800u, "%s: cpu1_stack_free counts from the bottom to the first word written", w);
        _c1_ustack[700] = FM1_C1_MARK;
        for (k = 0; k < 1000u; k++) {                    /* the render's pattern (fx.c mix_block) */
            uint32_t runs = job_runs;
            if (!cpu1_run(job_note, k) || cpu1_wait() != 0) {
                host_stalled |= !fm1_c1_mb.bad;
                job_note(k);
            }
            ok += job_runs == runs + 1u && job_last == k;
        }
        CHECK(ok == 1000u && job_off_core1 == 0u && cpu1_timeouts == 0u, "%s: 1000 jobs through cpu1_run / cpu1_wait, all on core 1", w);
        expect_traps_back(w);
        break;
    case B_OFF:
        CHECK(cpu1_state == CPU1_OFF && cpu1_units == 0 && hw_starts == 0u && pc_opens == 0u && cpu1_why == 0u, "%s: never started, cpu1_units 0", w);
        CHECK(!strcmp(cpu1_label(), "1 CORE") && !cpu1_on() && cpu1_run(job_note, 1u) == 0 && cpu1_stack_free() == 2816u,
              "%s: \"1 CORE\", cpu1_run 0, stack_free 2816", w);
        break;
    case B_BAR:
    case B_GUARD:
        CHECK(cpu1_state == CPU1_OFF && hw_starts == 0u && pc_opens == 0u && cpu1_why == (v == B_BAR ? 5u : 6u) && cpu1_units == -1,
              "%s: never started, cpu1_why %u", w, cpu1_why);
        CHECK(!strcmp(cpu1_label(), "1 CORE") && !cpu1_on(), "%s: \"1 CORE\"", w);
        break;
    case B_NOANSWER:
        CHECK(cpu1_state == CPU1_NOSTART && cpu1_why == 0u && cpu1_bar != CPU1_BAR && el >= 24000u * 20u,
              "%s: CPU1_NOSTART after 20 ms (%u us), not barred", w, el / 24u);
        CHECK(pc_opens == 1u && pc_arms == 1u && !cpu1_on() && !strcmp(cpu1_label(), "1 CORE"), "%s: PC limits armed again, \"1 CORE\"", w);
        expect_held(w);
        expect_traps_back(w);
        break;
    default:
        CHECK(cpu1_state == CPU1_FAILED && cpu1_why == (v == B_WRONG ? 4u : 1u) && cpu1_bar == CPU1_BAR,
              "%s: CPU1_FAILED, cpu1_why %u (1 late, 4 in time but wrong), barred until power-off", w, cpu1_why);
        CHECK(!cpu1_on() && cpu1_run(job_note, 1u) == 0 && !strcmp(cpu1_label(), "1 CORE"), "%s: \"1 CORE\", cpu1_run 0", w);
        if (v == B_WRONG)
            CHECK(fm1_c1_mb.done == fm1_c1_mb.job && fm1_c1_mb.bad == 0u, "%s: answered (done == job, no misread), the result wrong", w);
        else
            CHECK(el > 24000u * 4u, "%s: the wait's 4 ms (%u us)", w, el / 24u);
        expect_held(w);
        expect_traps_back(w);
        break;
    }
}

/* cpu1.c: cpu1_wait and cpu1_late_end */
enum { W_LATE, W_FAULT, W_MISREAD, W_STACKLIM };
static void case_wait(int v)
{
    static const char *const NAME[] = {"cpu1_wait, a late job", "cpu1_wait, a fault", "cpu1_wait, a misread",
                                       "cpu1_wait, its stack limit"};
    static const uint8_t WHY[] = {1u, 3u, 4u, 2u};
    const char *w = NAME[v];
    int r;
    if (!boot_ok(w))
        return;
    CHECK(cpu1_run(job_note, 5u) == 1 && wait_ok() == 0 && job_last == 5u, "%s: a job first", w);
    if (v == W_LATE)
        cpu1_run(job_spin, 0u);
    else if (v == W_FAULT) {
        cpu1_run(job_fault, 0x107u);
        UNTIL(fm1_c1_trace == 0xC1DEAD07u, 1000);
    } else if (v == W_STACKLIM) {                        /* (after 2.7.1) the EMU stack limit trapped: its EMU_MSG bit 3 */
        hw[R_EMU_MSG] = 1u << 3;
        cpu1_run(job_fault, 0x101u);
        UNTIL(fm1_c1_trace == 0xC1DEAD01u, 1000);
        CHECK(fm1_c1_fault_emu == 1u << 3, "%s: fault_emu bit 3", w);
    } else {
        uint32_t d = fm1_c1_mb.done;
        mb_write(d + 2u, job_note, 1u, FM1_C1_CHECK(job_note, 1u, d + 2u));
        UNTIL(fm1_c1_mb.bad, 1000);
    }
    r = cpu1_wait();
    CHECK(r == -1 && cpu1_timeouts == 1u && cpu1_why == WHY[v] && cpu1_state == CPU1_FAILED && cpu1_bar == CPU1_BAR,
          "%s: -1, cpu1_timeouts 1, cpu1_why %u, CPU1_FAILED, barred", w, cpu1_why);
    CHECK(!cpu1_on() && cpu1_run(job_note, 1u) == 0 && !strcmp(cpu1_label(), "1 CORE"), "%s: one core from then on", w);
    expect_held(w);
    CHECK(fm1_c1_traps.open, "%s: the traps off through the re-render", w);
    cpu1_late_end();
    expect_traps_back(w);
    cpu1_guard_half();
    CHECK(cpu1_state == CPU1_FAILED && cpu1_why == WHY[v], "%s: cpu1_guard_half after it: nothing more", w);
}

/* cpu1.c: cpu1_guard_half, core 1 idle */
enum { G_NONE, G_FAULT, G_MISREAD, G_STACK, G_BOTH, G_STACKLIM };
static void case_guard(int v)
{
    static const char *const NAME[] = {"cpu1_guard_half, all well", "cpu1_guard_half, a fault while idle",
                                       "cpu1_guard_half, a misread while idle", "cpu1_guard_half, the red zone written",
                                       "cpu1_guard_half, a misread and the stack", "cpu1_guard_half, its stack limit while idle"};
    static const uint8_t WHY[] = {0u, 3u, 4u, 2u, 4u, 2u};
    const char *w = NAME[v];
    uint32_t d;
    if (!boot_ok(w))
        return;
    d = fm1_c1_mb.done;
    switch (v) {
    case G_FAULT:
        atomic_store(&c1_fault_idle, 0x21u);
        UNTIL(fm1_c1_trace == 0xC1DEAD21u, 1000);
        CHECK(fm1_c1_trace == 0xC1DEAD21u && fm1_c1_fault_pc == 0x01C0FA17u, "%s: trace 0xC1DEAD21, fault_pc", w);
        break;
    case G_MISREAD:
        mb_write(d - 1u, job_note, 1u, FM1_C1_CHECK(job_note, 1u, d - 1u));
        UNTIL(fm1_c1_mb.bad, 1000);
        break;
    case G_STACK:
        _c1_red[3] = 0;
        break;
    case G_BOTH:
        mb_write(d + 2u, job_note, 1u, FM1_C1_CHECK(job_note, 1u, d + 2u));
        UNTIL(fm1_c1_mb.bad, 1000);
        _c1_ustack[10] = 0;
        break;
    case G_STACKLIM:                                     /* (after 2.7.1) its stack limit trapped, parked: why 2, not 3 */
        hw[R_EMU_MSG] = 1u << 3;
        atomic_store(&c1_fault_idle, 0x01u);
        UNTIL(fm1_c1_trace == 0xC1DEAD01u, 1000);
        CHECK(fm1_c1_trace == 0xC1DEAD01u && fm1_c1_fault_emu == 1u << 3, "%s: trace 0xC1DEAD01, fault_emu bit 3", w);
        break;
    }
    cpu1_guard_half();
    if (v == G_NONE) {
        CHECK(cpu1_state == CPU1_OK && cpu1_on() && hw_holds_running == 0u && cpu1_why == 0u, "%s: nothing done, still on", w);
        CHECK(cpu1_run(job_note, 9u) == 1 && wait_ok() == 0 && job_last == 9u, "%s: a job after it", w);
        return;
    }
    CHECK(cpu1_state == CPU1_FAILED && cpu1_why == WHY[v] && cpu1_bar == CPU1_BAR && !cpu1_on(), "%s: held for good, cpu1_why %u", w, cpu1_why);
    expect_held(w);
    expect_traps_back(w);
}

/* (after 2.7.1) a bus flag that latches after hold_end's clear traps as soon as the traps are back on: cpu1.c must have
 * recorded the hold by then, so the crash screen's C1 line says why (not "C1 3 W0", running, no reason) */
enum { LF_GUARD, LF_BOOT, LF_WAIT };
static void case_lateflag(int v)
{
    static const char *const NAME[] = {"a late flag, cpu1_guard_half", "a late flag, a wrong self-test",
                                       "a late flag, cpu1_wait and cpu1_late_end"};
    static const uint8_t WHY[] = {4u, 4u, 4u};
    const char *w = NAME[v];
    uint32_t d;
    if (v == LF_BOOT) {
        atomic_store(&c1_flip_test, 1u);
        hw_late_flag = 1;
        cpu1_boot();
    } else {
        if (!boot_ok(w))
            return;
        d = fm1_c1_mb.done;
        if (v == LF_GUARD) {
            mb_write(d - 1u, job_note, 1u, FM1_C1_CHECK(job_note, 1u, d - 1u));   /* a stale misread while idle */
            UNTIL(fm1_c1_mb.bad, 1000);
            hw_late_flag = 1;
            cpu1_guard_half();
        } else {
            mb_write(d + 2u, job_note, 1u, FM1_C1_CHECK(job_note, 1u, d + 2u));
            UNTIL(fm1_c1_mb.bad, 1000);
            hw_late_flag = 1;
            CHECK(cpu1_wait() == -1, "%s: -1", w);
            cpu1_late_end();
        }
    }
    CHECK(cpu1_state == CPU1_FAILED && cpu1_why == WHY[v] && cpu1_bar == CPU1_BAR, "%s: CPU1_FAILED, cpu1_why %u, barred", w, cpu1_why);
    CHECK(hw_trap_flag == 1u, "%s: the late flag trapped once the traps were back (as the device would)", w);
    CHECK(hw_trap_state == CPU1_FAILED && hw_trap_why == WHY[v] && hw_trap_bar == CPU1_BAR,
          "%s: at the trap cpu1.c had recorded it (state %u, why %u): the C1 line right", w, hw_trap_state, hw_trap_why);
    expect_held(w);
}

typedef struct { const char *name; void (*fn)(int); int arg; } case_t;
static const case_t CASES[] = {
    {"start", case_start, 0}, {"jobs", case_jobs, 0}, {"misread-ahead", case_misread, MR_AHEAD},
    {"misread-stale", case_misread, MR_STALE}, {"misread-check", case_misread, MR_CHECK}, {"misread-null", case_misread, MR_NULL},
    {"misread-sequence", case_misread, MR_SEQ}, {"misread-during", case_misread, MR_DURING}, {"done-word", case_done_word, 0},
    {"late", case_late, 0},
    {"fault", case_fault, 0}, {"stack", case_stack, 0}, {"restart", case_restart, 0}, {"traps", case_traps, 0},
    {"soak", case_soak, 0}, {"boot", case_boot, B_OK}, {"boot-off", case_boot, B_OFF}, {"boot-bar", case_boot, B_BAR},
    {"boot-guard", case_boot, B_GUARD}, {"boot-no-answer", case_boot, B_NOANSWER}, {"boot-late", case_boot, B_LATE},
    {"boot-wrong", case_boot, B_WRONG}, {"wait-late", case_wait, W_LATE}, {"wait-fault", case_wait, W_FAULT},
    {"wait-misread", case_wait, W_MISREAD}, {"wait-stack-limit", case_wait, W_STACKLIM}, {"guard-none", case_guard, G_NONE},
    {"guard-fault", case_guard, G_FAULT}, {"guard-misread", case_guard, G_MISREAD}, {"guard-stack", case_guard, G_STACK},
    {"guard-both", case_guard, G_BOTH}, {"guard-stack-limit", case_guard, G_STACKLIM},
    {"lateflag-guard", case_lateflag, LF_GUARD}, {"lateflag-boot", case_lateflag, LF_BOOT}, {"lateflag-wait", case_lateflag, LF_WAIT},
};

int main(int argc, char **argv)
{
    unsigned i, ran = 0, failed = 0, stalled_runs = 0;
    if (argc > 1 && atoi(argv[1]) > 0)
        soak_n = (uint32_t)strtoul(argv[1], 0, 10);
    for (i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        pid_t pid;
        int st = 0, attempt = 0;
        if (argc > 2 && strcmp(argv[2], CASES[i].name))
            continue;
        ran++;
again:
        fflush(stdout);
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {                                  /* a case: a process, core 1 a thread of it */
            alarm(600);
            hw_init();
            CASES[i].fn(CASES[i].arg);
            printf("mailbox: case %-20s %3u checks, %s\n", CASES[i].name, checks,
                   !fails ? "pass" : host_stalled ? "FAIL after a host stall" : "FAIL");
            fflush(stdout);
            _exit(!fails ? 0 : host_stalled ? 3 : 1);
        }
        if (waitpid(pid, &st, 0) >= 0 && WIFEXITED(st) && WEXITSTATUS(st) == 3 && ++attempt < 5) {
            printf("mailbox: case %s: a timeout no case asked for, a thread off its CPU over 4 ms (load): run again\n",
                   CASES[i].name);
            stalled_runs++;
            goto again;
        }
        if (!WIFEXITED(st) || WEXITSTATUS(st)) {
            failed++;
            if (WIFSIGNALED(st))
                printf("mailbox: case %s: FAIL, signal %d%s\n", CASES[i].name, WTERMSIG(st), WTERMSIG(st) == SIGALRM ? " (it hung)" : "");
        }
    }
    if (failed || !ran) {
        printf("the real hand-off (hal/fm1_cpu1.h): %u of %u cases FAILED\n", failed, ran);
        return 1;
    }
    printf("the real hand-off (hal/fm1_cpu1.h), fm1_c1_main on a thread as core 1: %u cases, %u soak jobs, all pass%s\n", ran,
           (unsigned)soak_n, stalled_runs ? " (a case run again after a host stall)" : "");
    return 0;
}
