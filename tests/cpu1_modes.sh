#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Andre Rezemini (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab)
# The render split over the two cores (fx.c mix_units) under every mode of the host double of core 1
# (tests/cpu1_host.c). The golden renders (tests/regress.c) and tests/mix_split_test.c must give the same hashes
# with CORES=1 (the job inline, deferred to the wait), CORES=2 (a thread), CORES=2 CHAOS=7 (a slow, erratic
# thread), CORES=2 CPU1_TIMEOUT_AT=K (a lost job: the fallback, then one core), CPU1_UNITS=0 (no split) and
# CPU1_UNITS=15 (every unit that may go, goes); and mix_split_test must be clean under ThreadSanitizer. A hold in the
# middle of a job (tests/cpu1_hold_test.c, the device's late job) must leave no state wrong for good. (after 2.7.1) The
# firmware's own hand-off (hal/fm1_cpu1.h) must pass tests/cpu1_mailbox_test.c, its core 1 a thread.
# Of regress only the golden renders, health, voices and crashes count here: its CPU budget is the default
# run's business (tests/run_tests.sh), and a thread's hand-off is not in the baseline.
# From the repo root, after tests/run_tests.sh built build/host/regress (it calls this script), or alone:
#   sh tests/cpu1_modes.sh
set -u
cd "$(dirname "$0")/.."
OUT=build/host
CC="${CC:-cc} -O2 -w -Ibuild/gen -Ifirmware/src"
fail=0
[ -x "$OUT/regress" ] || $CC -o "$OUT/regress" tests/regress.c -lm || exit 1
$CC -o "$OUT/mix_split_test" tests/mix_split_test.c -lm || exit 1

golden() {   # $1 = the mode's name, the rest its env: regress's golden renders, health, voices, no crash
    name=$1; shift
    if env "$@" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt > "$OUT/regress_mode.log" 2>&1; then :; fi
    if grep -E "golden renders \(0 changed, 0 gone\), 0 health failures, 0 voice / routing checks failed, [0-9]+ CPU entries over budget \([0-9]+ notes\), 0 crashes" "$OUT/regress_mode.log" >/dev/null; then
        echo "   golden renders, $name: same hashes"
    else
        echo "   golden renders, $name: FAIL"; grep -E "CHANGED|GONE|CRASH|HEALTH|FAIL|regress: [0-9]+ golden" "$OUT/regress_mode.log" | head -20; fail=1
    fi
}
split() {    # $1 = the mode's name, the rest its env: every split of the units against none
    name=$1; shift
    if env "$@" "$OUT/mix_split_test" > "$OUT/split_mode.log" 2>&1; then
        echo "   every split of the units, $name: $(tail -n 1 "$OUT/split_mode.log" | sed 's/.*: //')"
    else
        echo "   every split of the units, $name: FAIL"; grep -v " same$" "$OUT/split_mode.log" | head -20; fail=1
    fi
}
echo "== the render split, the second core as the host double has it"
golden "CORES=1 (the job inline, at the wait)" CORES=1
golden "CORES=2 (a thread)" CORES=2
golden "CORES=2 CHAOS=7 (a slow, erratic thread)" CORES=2 CHAOS=7
golden "CORES=2 CPU1_TIMEOUT_AT=300 (a lost job: the fallback, then one core)" CORES=2 CPU1_TIMEOUT_AT=300
golden "CPU1_UNITS=0 (no split)" CORES=1 CPU1_UNITS=0
golden "CORES=2 CPU1_UNITS=15 (every unit that may go, goes)" CORES=2 CPU1_UNITS=15
split "CORES=1" CORES=1
split "CORES=2" CORES=2
split "CORES=2 CHAOS=7" CORES=2 CHAOS=7
split "CORES=2 CPU1_TIMEOUT_AT=700" CORES=2 CPU1_TIMEOUT_AT=700
# (after 2.7.1) a hold in the middle of a job (tests/cpu1_hold_test.c): the state it could corrupt for good is the same
$CC -o "$OUT/cpu1_hold_test" tests/cpu1_hold_test.c -lm || exit 1
if "$OUT/cpu1_hold_test" > "$OUT/hold.log" 2>&1; then
    echo "   $(tail -n 1 "$OUT/hold.log")"
else
    echo "   a hold in the middle of a job: FAIL"; grep -v "same state$" "$OUT/hold.log" | head -20; fail=1
fi
HASAN="${CC_UB:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src"
if $HASAN -o "$OUT/cpu1_hold_asan" tests/cpu1_hold_test.c -lm 2>/dev/null; then   # (what a re-render after a hold reads)
    if "$OUT/cpu1_hold_asan" > "$OUT/hold_asan.log" 2>&1; then
        echo "   the same under ASan + UBSan: no access out of bounds"
    else
        echo "   a hold in the middle of a job, under ASan + UBSan: FAIL"; grep -v "same state$" "$OUT/hold_asan.log" | head -20; fail=1
    fi
else
    echo "   (a hold in the middle of a job under ASan + UBSan: this compiler has none, skipped)"
fi
# (after 2.7.1) the real hand-off (hal/fm1_cpu1.h, tests/cpu1_mailbox_test.c): fm1_c1_main on a thread as core 1 against
# core 0's start, run, wait, hold and stack check over a model of the chip's registers, and cpu1.c's state over them;
# plain and under ASan + UBSan (not ThreadSanitizer: the mailbox is volatile words and csync, as on the device)
$CC -o "$OUT/cpu1_mailbox_test" tests/cpu1_mailbox_test.c -lm || exit 1
if "$OUT/cpu1_mailbox_test" > "$OUT/mailbox.log" 2>&1; then
    echo "   $(tail -n 1 "$OUT/mailbox.log")"
else
    echo "   the real hand-off on a thread: FAIL"; grep -v " ok$" "$OUT/mailbox.log" | grep -v " pass$" | head -20; fail=1
fi
if $HASAN -o "$OUT/cpu1_mailbox_asan" tests/cpu1_mailbox_test.c -lm 2>/dev/null; then
    if "$OUT/cpu1_mailbox_asan" > "$OUT/mailbox_asan.log" 2>&1; then
        echo "   the same under ASan + UBSan: no access out of bounds"
    else
        echo "   the real hand-off under ASan + UBSan: FAIL"; grep -v " ok$" "$OUT/mailbox_asan.log" | grep -v " pass$" | head -20; fail=1
    fi
else
    echo "   (the real hand-off under ASan + UBSan: this compiler has none, skipped)"
fi
# (after 2.7.1) the split under ASan + UBSan (CORES=1, the splits 0, 15, -1): what a unit reads past its buffers, as
# 2.7.1's phys_render_draws did (array-bounds off: the host rebases the user sample slots on SMP_DATA, tests/hostsim.c)
if $HASAN -fno-sanitize=array-bounds -o "$OUT/mix_split_asan" tests/mix_split_test.c -lm 2>/dev/null; then
    if env CORES=1 "$OUT/mix_split_asan" QUICK > "$OUT/split_asan.log" 2>&1; then
        echo "   every split of the units under ASan + UBSan (CORES=1, the splits 0, 15, -1): no access out of bounds"
    else
        echo "   every split of the units under ASan + UBSan: FAIL"; grep -E "ERROR|SUMMARY|runtime error" "$OUT/split_asan.log" | head; grep -v " same$" "$OUT/split_asan.log" | head; fail=1
    fi
else
    echo "   (every split of the units under ASan + UBSan: this compiler has none, skipped)"
fi
# (after 2.7.1) ThreadSanitizer with the firmware's own byte-loop memcpy / memset (HOST_OWN_MEM, tests/hostsim.c): this
# toolchain's TSan does not check libc's, so a race on a copied buffer (mix_part's preset scratch) went unseen
TSAN="${CC_UB:-cc} -O1 -g -w -fsanitize=thread -fno-builtin -DHOST_OWN_MEM -Ibuild/gen -Ifirmware/src"
if $TSAN -o "$OUT/mix_split_tsan" tests/mix_split_test.c -lm 2>/dev/null; then
    if env CORES=2 CHAOS=3 TSAN_OPTIONS="halt_on_error=1" "$OUT/mix_split_tsan" QUICK > "$OUT/split_tsan.log" 2>&1; then
        echo "   under ThreadSanitizer (CORES=2 CHAOS=3, the splits 0, 15, -1): $(tail -n 1 "$OUT/split_tsan.log" | sed 's/.*: //'), no race"
    else
        echo "   under ThreadSanitizer: FAIL"; grep -A 12 "WARNING: ThreadSanitizer" "$OUT/split_tsan.log" | head -40; grep -v " same$" "$OUT/split_tsan.log" | head; fail=1
    fi
else
    echo "   (under ThreadSanitizer: this compiler has none, skipped)"
fi
[ $fail -eq 0 ] && echo "   the render split: every mode gives the same output" || { echo "   the render split: FAILED"; exit 1; }
