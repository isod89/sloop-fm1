/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37: the stock firmware's mapping in USRFLASH and its end marker "usrdata" at 0xEAFF8 (firmware/src/storage.c
 * st_stock_*, fm1_flash_map.h FL_STOCKTAG) on a simulated 1 MiB flash, every erase going through the hook the firmware's
 * erase has (felucca.c fl_erase4k_quiet: st_stock_touch first): an intact stock mapping stays untouched at boot; the
 * marker's sector is erased before the first erase in USRFLASH, and only once; a marker over our samples (or broken
 * stock data) is released at boot; the editor's release says whether there was one. Build with -DBOARD_SMK37=1. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif
#include "fm1_flash_map.h"

static uint8_t nor[0x100000];
static int tag_erases, usr_before_tag, usr_erases, refused;
static void st_stock_touch(uint32_t off);
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)                  /* as felucca.c: the window, then fl_erase4k_quiet (hook first) */
{
    if (!FL_STORE_OK(off, 0x1000u) && !FL_STOCKTAG_OK(off, 0x1000u)) {
        refused++;
        return -8;
    }
    st_stock_touch(off);
    if (off == FL_STOCKTAG_LO)
        tag_erases++;
    else if (off >= FL_SMP_LO && off < FL_SMP_HI) {
        usr_erases++;
        usr_before_tag += nor[FL_STOCKTAG_OFF] == 'u';   /* a USRFLASH erase with the marker still there */
    }
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))
        return refused++, -8;
    memcpy(nor + off, src, n);
    return 0;
}
#include "../firmware/src/storage.c"

static int bad;
static void check(const char *what, int ok) { printf("smk stock: %-86s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void stock(void)                            /* the stock mapping: settings A3 3A, 8 presets (notes), the marker */
{
    uint32_t n, k;
    memset(nor, 0xFF, sizeof nor);
    nor[0xC2000] = 0xA3, nor[0xC2001] = 0x3A;
    for (n = 0; n < 8u; n++)
        for (k = 0; k < 0x29E3u; k++)
            nor[0xC3000u + n * 0x5000u + k] = (uint8_t)(k < 5u ? 0x10u + k : 36u + k % 37u);
    memcpy(nor + FL_STOCKTAG_OFF, "usrdata", 8);
    tag_erases = usr_before_tag = usr_erases = refused = 0;
}
static int tagged(void) { return !memcmp(nor + FL_STOCKTAG_OFF, "usrdata", 8); }

int main(void)
{
    stock();
    st_stock_boot();
    check("boot over the intact stock mapping: nothing erased, the marker kept", tag_erases == 0 && tagged() && st_stock_intact());
    st_erase(0xC2000u);                            /* SMP_BEGIN of USR1 (eng_sample.c / editor.c) */
    check("the first erase in USRFLASH: the marker's sector erased first", tag_erases == 1 && usr_before_tag == 0 && !tagged());
    st_erase(0xC3000u);
    st_erase(0xD6000u);
    check("... and only once (later erases: no marker, nothing more)", tag_erases == 1 && usr_erases == 3);
    check("the marker sector: all of it erased, nothing else above USRFLASH",
          nor[0xEA000] == 0xFF && nor[0xEAFFF] == 0xFF && refused == 0);

    stock();                                       /* SLOOP <= 2.6.5 wrote USR2 and left the marker */
    memset(nor + 0xD6000, 0xFF, 0x14000);
    memcpy(nor + 0xD6000, "FSMP", 4);
    st_stock_boot();
    check("boot, the marker over a sample slot (FSMP at USR2): released", tag_erases == 1 && !tagged());
    stock();
    memset(nor + 0xC2000, 0xFF, 0x14000);          /* USR1 erased (SMP_ERASE) */
    st_stock_boot();
    check("boot, the marker over an erased USR1 (no A3 3A): released", tag_erases == 1 && !tagged());
    stock();
    memset(nor + 0xD6000, 0xFF, 0x14000);          /* USR2 erased: preset 4 gone */
    st_stock_boot();
    check("boot, the marker over an erased USR2 (preset 4 gone): released", tag_erases == 1 && !tagged());
    stock();
    nor[0xC3000 + 6] = 0xC4;                        /* (not a note) */
    st_stock_boot();
    check("boot, preset 0 not a mapping: released", tag_erases == 1);
    stock();
    memset(nor + FL_STOCKTAG_LO, 0xFF, 0x1000);    /* no marker: the stock firmware rebuilds anyway */
    st_stock_boot();
    st_erase(0xC2000u);
    check("no marker: nothing released, ever", tag_erases == 0 && usr_erases == 1);

    stock();                                       /* the web editor's PREPARE THE RETURN */
    check("the editor's release: 1 when the marker was there, then 0",
          st_stock_release() == 1 && !tagged() && st_stock_release() == 0 && tag_erases == 1);
    stock();
    st_erase(0xBA000u);
    st_erase(0x9D000u);
    check("erases outside USRFLASH (the store) leave the marker alone", tag_erases == 0 && tagged());
    check("the guard: no page write into the marker sector, no erase past it",
          st_prog(FL_STOCKTAG_OFF, "x", 1) == -8 && st_erase(0xEB000u) == -8 && st_erase(0xC1000u) == -8);
    printf("%s\n", bad ? "SMK STOCK TEST FAILED" : "smk stock test passed");
    return bad != 0;
}
