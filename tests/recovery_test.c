/* SPDX-License-Identifier: GPL-3.0-only */
/* Real recovery control flow with hardware hooks mocked. No claim of USB
 * enumeration on hardware: the separate OTA suite exercises the real protocol. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include "../firmware/src/bootguard.h"
static bootguard_t bootguard;
static uint32_t ticks, fm1_ms, polls, feeds, retries, services, sessions, rom, detached, scans;
static uint32_t jedec = 0x856014u, jedec_reads, lcd_calls, font;
static uint8_t recovery_active, flash_ok;
static struct { uint8_t ota_req, uboot_req; } usb;
static struct { uint32_t buttons; } fm1_in;
static uint32_t input_buttons, stop_at_retry;
static jmp_buf exit_loop;
#define FM1_TICKS_PER_US 24u
#define FL_FAR(x) x
#define C_BLACK 0
#define C_WHITE 65535
#define FONT_S font
static uint32_t fm1_ticks(void) { return ticks; }
static void fm1_wdt_feed(void) { feeds++; }
static void usb_poll(void) { polls++; }
static void usb_retry(uint32_t t)
{
    (void)t; retries++;
    if (stop_at_retry && retries >= stop_at_retry) longjmp(exit_loop, 1);
}
static void usb_detach(void) { detached++; }
static void fm1_delay_ms(uint32_t n) { ticks += n * 24000u; }
static void fm1_enter_uboot(void) { rom++; }
static void ota_service(void) { services++; }
static void recovery_poll(void);
static int ota_session(void)
{
    unsigned i;
    sessions++;
    for (i = 0; i < 200; i++) { ticks += 12000u; recovery_poll(); }
    return -13;  /* failed update must leave rescue available for another try */
}
static void fm1_input_init(void) { fm1_in.buttons = 0; }
static unsigned keys_on, scans_before_on;
#define RECOVERY_KEYS_ON() (keys_on++, scans_before_on += scans)   /* (2.7.2: PA2 driven low before the scan) */
static void fm1_input_scan(void) { scans++; ticks += 24000u; fm1_in.buttons = keys_on ? input_buttons : 0u; }
static uint32_t fl_jedec_ram(void) { jedec_reads++; return jedec; }
static void lcd_init(void) { lcd_calls++; }
static void lcd_fill(int x, int y, int w, int h, int c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
static void draw_text_box(int x, int y, int w, const void *f, const char *s, int c, int a)
{ (void)x;(void)y;(void)w;(void)f;(void)s;(void)c;(void)a; }
static void lcd_sync(void) {}
static void usb_start(void) {}
#include "../firmware/src/recovery.c"

int main(void)
{
    bootguard_t b = {0};
    assert(bootguard_begin(&b) == BOOT_NORMAL);
    assert(bootguard_begin(&b) == BOOT_NORMAL && b.failed == 1);
    assert(bootguard_begin(&b) == BOOT_RECOVERY && b.pending == 2);
    assert(bootguard_begin(&b) == BOOT_ROM && b.pending == 0);
    assert(bootguard_begin(&b) == BOOT_NORMAL);
    bootguard_clear(&b);                     /* healthy boot / intentional update */
    assert(bootguard_begin(&b) == BOOT_NORMAL && b.failed == 0);
    b.pending = 2; bootguard_clear(&b);
    assert(bootguard_begin(&b) == BOOT_NORMAL);
    b.failed = 0xFFFFFFFFu;
    assert(bootguard_begin(&b) == BOOT_NORMAL && b.failed == 0);
    assert(bootguard_manual(1) && !bootguard_manual(0) && !bootguard_manual(2) && !bootguard_manual(3));

    input_buttons = 1; assert(recovery_key());
    assert(keys_on == 1 && scans_before_on == 0);   /* (2.7.2) the matrix powered (PA2 low) before the first scan */
    input_buttons = 3; assert(!recovery_key());
    input_buttons = 0; assert(!recovery_key());
    assert(scans >= 150);

    fm1_ms = 0; recovery_fraction = 0;
    recovery_last = recovery_usb_last = ticks = 0xFFFFF000u;
    ticks += 12000u; recovery_poll();         /* wrap + half a millisecond */
    assert(fm1_ms == 0 && polls == 1);
    ticks += 12000u; recovery_poll();
    assert(fm1_ms == 1 && polls == 2);
    recovery_poll(); assert(polls == 2);     /* never busy-poll USB faster than 2 kHz */

    flash_ok = 0; usb.ota_req = 1; recovery_step();
    assert(sessions == 0 && usb.ota_req == 0);
    flash_ok = 1; usb.ota_req = 1; recovery_step();
    assert(sessions == 1 && fm1_ms == 101 && polls == 202);
    usb.ota_req = 1; recovery_step();
    assert(sessions == 2 && fm1_ms == 201 && polls == 402);
    assert(feeds > 400 && services == 3);
    usb.uboot_req = 1; bootguard.pending = 2; recovery_step();
    assert(rom == 1 && detached == 1 && bootguard.pending == 0);
    usb.uboot_req = 0;

    retries = 0; stop_at_retry = 5;
    if (!setjmp(exit_loop)) recovery_main();
    assert(recovery_active && bootguard.pending == 2 && flash_ok);
    assert(jedec_reads == 1 && lcd_calls == 1);
    assert(services == 7 && sessions == 2);  /* idle startup starts no update */
    assert(bootguard_begin(&bootguard) == BOOT_ROM);
    puts("recovery: boot failures, manual key, timer wrap, polled USB during OTA, retry, flash refusal and ROM fallback passed");
    return 0;
}
