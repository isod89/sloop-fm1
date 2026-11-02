/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C07F00 holds .ram_text (code that must run while the flash
 * is busy: the flash driver), .c1_text (the second core's waiting loop, fm1_cpu1.h) and, after them, .lowram
 * (sloop-lab: the reverb lines, the second core's stacks and the render split's scratch; before this patch that
 * SRAM sat unused: Felucca kept 0x01C06000..0x01C08000 for RAM-run debug, which SLOOP has no build for). crt0.S
 * writes the SPL's state at 0x01C07F20: the region stops at 0x01C07F00. main.c proves and zeroes .lowram at boot.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x7F00
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .c1_text : { _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1_end = .; } > RAMTEXT AT > XIP
    _c1_load = LOADADDR(.c1_text);
    _rt_end = _c1_end;                  /* the guard's RAM code window covers both (fm1_cpu1.h) */
    /* the SRAM left below the RAM region: section(".lowram") buffers and the second core's stacks (fm1_cpu1.S
     * .bss.c1stack), outside the guard's RAM code window; zeroed at boot as .pool is (main.c lowram_test) */
    .lowram (NOLOAD) : { . = ALIGN(8); _lr_start = .; *(.bss.c1stack) *(.lowram*) . = ALIGN(4); _lr_end = .; } > RAMTEXT
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    /* (SLOOP 2.8.3) .noinit.c1bar (fm1_cpu1.h, one core until power-off) after the others: none of them moves */
    .noinit (NOLOAD) : { *(.noinit) *(.noinit.*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
