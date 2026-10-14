/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Charles Vestal: from X0X (github.com/charlesvestal/fm1-x0x, commit 00bff73),
 * after Melodee's dual-core audio by Kerem Kilic (github.com/keremimo/melodee). Brought into SLOOP
 * unchanged but for this notice (the SLOOP dualcore branch, github.com/andrerezemini/sloop-lab). */
/* The AC79's second core: its entry, as the AC79 SDK's cpu.a/startup.S.o (and Melodee's dual-core
 * audio) have it: interrupts off, no interrupt configuration, its own user and supervisor stacks,
 * then out of the start-up interrupt into fm1_c1_main (in RAM, fm1_cpu1.h), which never returns.
 * fm1_cpu1_start puts this address in the word the chip starts core 1 from (0x01C7FFF8). */
	.section .text.cpu1,"ax",@progbits
	.balign 4
	.balign 4
	.globl fm1_c1_entry
fm1_c1_entry:
	cli
	r1 = fm1_c1_trace	/* a breadcrumb: core 1 reached its entry */
	r2 = 0xC1000001
	[r1 + 0] = r2
	icfg = 0
	usp = _c1_ustack_top
	sp = _c1_sstack_top
	ssp = _c1_sstack_top
	reti = fm1_c1_main
	rti

	.section .bss.c1stack,"aw",@nobits
	.balign 8
	.globl _c1_ustack
_c1_ustack:			/* after rti the core runs on this one: the render's (filled with a mark at start) */
	.space 2816
_c1_ustack_top:
_c1_sstack:			/* exceptions only (no interrupts on this core) */
	.space 256
_c1_sstack_top:
