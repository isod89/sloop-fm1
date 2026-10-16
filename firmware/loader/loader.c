/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader (device side). Runs from RAM, started by the SPL from
 * an update record, with interrupts off: USB is polled from the main loop.
 * It is a USB-MIDI device "Felucca Update" (1209:0002) that answers the
 * M-UPGRADE update protocol as "ota-FM-1_900" (SMK: "ota-SMK-37 Pro_900", or the
 * SMK_MODEL build's name; hosts look for "ota-"), and
 * writes the app area only (ldr_core.c). The UBOOT soft key works here too.
 * Single compilation unit. */
#include <stdint.h>
#define FELUCCA_LOADER 1
#define FELUCCA_CDC 0
#define FELUCCA_OTA 1
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_USB_PID 0x0002
#include "fm1_cc.h"                             /* BOARD_SMK37 */
#if BOARD_SMK37
#define FELUCCA_ID "ota-" SMK_MODEL "_900"
#else
#define FELUCCA_ID "ota-FM-1_900"
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "fm1_time.h"
#include "fm1_sys.h"
/* the loader may write the app area and erase update records; never the head */
#if BOARD_SMK37
/* SMK: app area 0x4000..0x9CFFF, records in the VM 0x9D000..0xBFFFF only: never BTIF / USRTRIM
 * 0xC0000..0xC1FFF, USR 0xF4000.., key_mac 0xFF000 (FL_APP / FL_REC, asserted in fm1_flash.h; the RAM
 * driver also refuses every protected range on its own, FL_GUARD) */
#define FL_RANGE_OK(off, n) FL_LDR_RANGE_OK(off, n)
#else
#define FL_RANGE_OK(off, n) (FL_IN(off, n, 0x4000u, 0x93000u) || FL_IN(off, n, 0x93000u, 0xFC000u))
#endif
#include "fm1_flash.h"
#include "../src/libc.c"
#include "../src/usb.c"

static uint32_t ldr_ms(void)   /* monotonic ms; survives the TIMER4 wrap (178.9 s) */
{
    static uint32_t last, acc, ms;
    uint32_t now = fm1_ticks();
    acc += now - last;
    last = now;
    ms += acc / (1000u * FM1_TICKS_PER_US);
    acc %= 1000u * FM1_TICKS_PER_US;
    return ms;
}

static void ldr_poll(void)                          /* USB at ~2 kHz, watchdog */
{
    static uint32_t last;
    uint32_t now = fm1_ticks();
    fm1_wdt_feed();
    if (now - last >= 500u * FM1_TICKS_PER_US) {
        last = now;
        usb_poll();
    }
}

/* ---- ota.c hooks (the SysEx frame hooks are in usb.c) ---- */
static uint32_t ota_now_ms(void) { return ldr_ms(); }
static void ota_idle(void) { ldr_poll(); }
/* step-1 hooks of ota.c: not used by the loader */
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { return fl_read_ram(off, p, n); }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../src/ota.c"

/* ---- ldr_core.c hooks: interrupts are off for the whole loader ---- */
static int ldr_fread(uint32_t off, void *p, uint32_t n) { return fl_read_ram(off, p, n); }
static int ldr_erase(uint32_t off)
{
    uint32_t took;
    return fl_erase4k_ram(off, &took);
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t took;
    return fl_prog_ram(off, p, n, &took);
}
static void ldr_record_clear(void)
{
    fm1_updata_parm_clear();                        /* CRC 0: the SPL ignores it on a warm reset */
}
static int ldr_flash_known(void) { return FL_JEDEC_OK(fl_jedec_ram()); }   /* as the app (project.c) */
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; ldr_poll(); }
#include "ldr_core.c"

extern uint32_t _bss_start[], _bss_end[];

void ldr_main(void)
{
    fm1_wdt_arm(0x0D);                              /* ~8 s, fed by ldr_poll */
    fm1_time_init();
    usb_start();
    for (;;) {
        ldr_poll();
        usb_retry(ldr_ms());
        if (usb.uboot_req) {                        /* soft key: mask-ROM UBOOT, as in Felucca */
            usb_detach();
            fm1_delay_ms(30);
            fm1_enter_uboot();
        }
        ota_service();                              /* handshake */
        if (usb.ota_req) {
            int rc;
            usb.ota_req = 0;
            rc = ldr_session();
            if (rc == 0 || rc == LDR_EFLASH) {      /* done, or another flash chip: the old firmware again */
                fm1_delay_ms(200);                  /* the "success" reply leaves */
                usb_detach();
                fm1_delay_ms(30);
                fm1_core_reset();                   /* the SPL boots the new app */
            }
        }
    }
}

void ldr_cstart(void)
{
    uint32_t *p;
    ldr_record_clear();                             /* first thing: a crash below cannot loop through a
                                                     * warm reset; resume after power loss uses the flash record */
    for (p = _bss_start; p < _bss_end; p++)
        *p = 0;
    ldr_main();
}
