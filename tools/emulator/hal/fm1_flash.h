/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_flash.h: the 1 MiB SPI NOR as a RAM image (emu_flash,
 * saved to build/emu/sloop_flash.bin). NOR rules: erase sets a 4 KiB sector to 0xFF, program can
 * only clear bits (a write without an erase reads back ANDed, as on the part). Same windows. */
#pragma once
#include <stdint.h>
#include "fm1_irq.h"
#include "emu_bridge.h"

#define RAMFN
#define RAMINL static inline
#define FL_FAR(fn) (fn)

#define FL_DATA_LO      0x00097000u                /* Felucca main store */
#define FL_DATA_HI      0x000E0000u
#define FL_GLOB_LO      0x000FC000u                /* Felucca superblock / globals */
#define FL_GLOB_HI      0x000FF000u
#define FL_OTA_LO       0x000E0000u                /* M-UPGRADE loader staging, ota.c */
#define FL_OTA_HI       0x000E5000u
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI))
#ifndef FL_RANGE_OK
#define FL_RANGE_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI))
#endif
#define FL_XIP(off)     ((const uint8_t *)emu_flash + (uint32_t)(off))

static uint32_t fl_jedec_ram(void) { return 0x856014u; }   /* the expected 1 MiB part */
static uint32_t fl_status_ram(void) { return 0; }

static int fl_erase4k_ram(uint32_t off, uint32_t *took_us)
{
    uint32_t i;
    if (!FL_RANGE_OK(off, 0x1000u) || (off & 0xFFFu))
        return -1;
    for (i = 0; i < 0x1000u; i++)
        emu_flash[off + i] = 0xFFu;
    if (took_us)
        *took_us = 45000u;                         /* (typical; the emulator does not wait) */
    emu_flash_written();
    return 0;
}

static int fl_prog_ram(uint32_t off, const uint8_t *src, uint32_t n, uint32_t *took_us)
{
    uint32_t i;
    if (!FL_RANGE_OK(off, n) ||
        n == 0 || n > 256u || ((off & 0xFFu) + n) > 256u) return -1;    /* one page, no wrap */
    for (i = 0; i < n; i++)
        emu_flash[off + i] &= src[i];
    if (took_us)
        *took_us = 700u;
    emu_flash_written();
    return 0;
}

static int fl_read_ram(uint32_t off, uint8_t *dst, uint32_t n)
{
    uint32_t i;
    if (off >= EMU_FLASH_SIZE || n > EMU_FLASH_SIZE - off)
        return -2;
    for (i = 0; i < n; i++)
        dst[i] = emu_flash[off + i];
    return 0;
}

static void fl_plain_window_init(void) {}

static inline uint32_t irq_save(void) { emu_irq_off(); return 0; }
static inline void irq_restore(uint32_t f) { (void)f; emu_irq_on(); }

static int fl_erase4k(uint32_t off, uint32_t *took_us)
{
    uint32_t f = irq_save();
    int rc = FL_FAR(fl_erase4k_ram)(off, took_us);
    irq_restore(f);
    return rc;
}

static int fl_write(uint32_t off, const uint8_t *src_ram, uint32_t n)
{
    uint32_t took;
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu);
        int rc;
        uint32_t f;
        if (chunk > n) chunk = n;
        f = irq_save();
        rc = FL_FAR(fl_prog_ram)(off, src_ram, chunk, &took);
        irq_restore(f);
        if (rc) return rc;
        off += chunk; src_ram += chunk; n -= chunk;
    }
    return 0;
}

static int fl_verify_xip(uint32_t off, const uint8_t *ref, uint32_t n)
{
    const volatile uint8_t *p = FL_XIP(off);
    uint32_t i;
    for (i = 0; i < n; i++) if (p[i] != ref[i]) return -(int)(i + 1);
    return 0;
}
