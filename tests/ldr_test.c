/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the Felucca update loader (firmware/loader/ldr_core.c): a fake
 * host serves a package over the SysEx protocol, the "device" flash starts as
 * another package's flash.bin (as if that firmware were installed).
 *   ldr_test OLD.fwsc NEW.fwsc
 * With -DBOARD_SMK37=1 (packages from fm1pkg_make.py --board smk37): the SMK-37 Pro windows (app area
 * up to 0x9D000, records 0x9D000..0xBFFFF) and the protected ranges BTIF / USRTRIM 0xC0000..0xC1FFF,
 * USR 0xF4000..0xFDFFF and key_mac 0xFF000 never touched, even with a valid-looking record in them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_OTA_DRYRUN 0
#if BOARD_SMK37
#define FELUCCA_ID "ota-SMK-37 Pro_900"
#define T_APP_HI 0x9D000u                   /* end of the app area the loader writes */
#define T_REC 0xBFF00u                      /* the update record (ota.c OTA_RES) */
#define T_ERASE_HI 0xC0000u                 /* nothing erased at or above (BTIF, USRTRIM, ...) */
#else
#define FELUCCA_ID "ota-FM-1_900"
#define T_APP_HI 0x93000u
#define T_REC 0xE4F00u
#define T_ERASE_HI 0xFC000u
#endif

static uint8_t nor[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, requests, erases, bad_range, record_cleared, f0_asked;
static uint32_t corrupt_at, corrupt_once, no_finish;   /* the host serves a damaged byte / never says "success" */

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    requests++;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xF0000000u && no_finish) {
        f0_asked++;
        return 0;                                    /* no reply */
    }
    if (addr >= 0xE0000000u) {
        f0_asked += addr == 0xF0000000u;
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && addr <= corrupt_at && corrupt_at < addr + len) {
            m[14 + corrupt_at - addr] ^= 0x08;       /* (the frame's own checksum is fine: only the CRC sees it) */
            if (corrupt_once)
                corrupt_at = 0;
        }
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/src/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static int ldr_erase(uint32_t off)
{
    if (off < 0x4000u || off >= T_ERASE_HI || (off & 0xFFFu)) bad_range++;
    if (off < 0x4000u) { printf("ERASE IN THE HEAD %#x\n", off); exit(1); }
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (off < 0x4000u || off + n > T_APP_HI || ((off & 0xFFu) + n) > 256u) bad_range++;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; }
static uint32_t flash_unknown;
static int ldr_flash_known(void) { return !flash_unknown; }
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#include "../firmware/loader/ldr_core.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}

static uint32_t flash_off(const uint8_t *lg)        /* flash.bin offset in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) return ota_rd32(e + 8);
    }
    return 0;
}

static int check(const char *what, int ok) { printf("%-56s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static void put_record_at(uint32_t at)
{
    uint8_t r[112] = {0};
    r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
    ota_wr16(r, ota_crc16(r + 2, 78, 0));
    memcpy(nor + at, r, sizeof r);
}
static void put_record(void) { put_record_at(T_REC); }   /* a valid update record (the SPL runs the loader) */

#if BOARD_SMK37
/* the SMK's protected ranges: a pattern, and a valid-looking record in the USRTRIM sector and above it */
static const uint32_t PROT[][2] = {{0xC0000u, 0xC2000u}, {0xF4000u, 0xFE000u}, {0xFF000u, 0x100000u}};
static uint8_t prot_img[sizeof nor];
static void protect_fill(void)
{
    uint32_t i, k;
    for (k = 0; k < sizeof PROT / sizeof PROT[0]; k++)
        for (i = PROT[k][0]; i < PROT[k][1]; i++)
            nor[i] = (uint8_t)(i * 13u + 5u);
    memcpy(nor + 0xC1224u, "trimdat", 8);
    put_record_at(0xC1F00u);                       /* USRTRIM sector: must never be erased */
    put_record_at(0xC0F00u);                       /* BTIF */
    put_record_at(0xF4F00u);                       /* USR */
    memcpy(prot_img, nor, sizeof nor);
}
static int protect_ok(void)
{
    uint32_t k;
    for (k = 0; k < sizeof PROT / sizeof PROT[0]; k++)
        if (memcmp(nor + PROT[k][0], prot_img + PROT[k][0], PROT[k][1] - PROT[k][0]))
            return 0;
    return 1;
}
#else
static void protect_fill(void) {}
static int protect_ok(void) { return 1; }
#endif

int main(int argc, char **argv)
{
    size_t olen;
    uint8_t *old, head[0x4000];
    uint32_t ofo, nfo;
    int bad = 0, rc;
    if (argc < 3) { printf("usage: ldr_test OLD.fwsc NEW.fwsc\n"); return 2; }
    old = load_logical(argv[1], &olen);
    logical = load_logical(argv[2], &logical_len);
    ofo = flash_off(old);
    nfo = flash_off(logical);
    /* device: OLD installed, plus a valid update record at 0xE4F00 */
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, old + ofo, T_APP_HI);
    protect_fill();
    memcpy(head, nor, sizeof head);
    put_record();
    rc = ldr_session();
    printf("  rc %d, %u requests, %u sector erases\n", rc, requests, erases);
    bad += check("install completes", rc == 0);
    bad += check("app area == the new package's flash.bin", !memcmp(nor + 0x4000, logical + nfo + 0x4000, T_APP_HI - 0x4000));
    bad += check("flash head [0, 0x4000) untouched", !memcmp(nor, head, sizeof head));
    bad += check("finish asked (0xF0000000)", f0_asked >= 1);
    bad += check("update record cleared (RAM + flash)", record_cleared && nor[T_REC + 6u] == 0xFF);
    bad += check("no write outside the allowed windows", bad_range == 0);
    /* same package again: nothing to erase */
    erases = 0; requests = 0;
    rc = ldr_session();
    bad += check("re-run with the same package erases nothing", rc == 0 && erases == 0);
    /* a package for another chip key is refused before any erase */
    {
        uint8_t save = logical[nfo + 0x4000 + 5];
        logical[nfo + 0x4000 + 5] ^= 0x5A;           /* app area head no longer decrypts */
        memcpy(nor, old + ofo, T_APP_HI);
        erases = 0;
        rc = ldr_session();
        bad += check("foreign key / damaged app head refused, nothing erased", rc == -6 && erases == 0);
        logical[nfo + 0x4000 + 5] = save;
    }
    /* one bit damaged on the way, once: the CRC sees it, the next pass rewrites that sector */
    {
        const uint8_t r6 = 0x41;                     /* byte 6 of a valid record ("AT"); 0xFF once erased */
        memcpy(nor, old + ofo, T_APP_HI);
        put_record();
        record_cleared = 0;
        corrupt_at = nfo + 0x30123u;
        corrupt_once = 1;
        rc = ldr_session();
        bad += check("a byte damaged once: caught by the CRC, fixed by a 2nd pass", rc == 0 && record_cleared
                     && nor[T_REC + 6u] == 0xFF && !memcmp(nor + 0x4000, logical + nfo + 0x4000, T_APP_HI - 0x4000));
        /* the package itself damaged (every pass): the record stays, nothing is booted */
        memcpy(nor, old + ofo, T_APP_HI);
        put_record();
        record_cleared = 0;
        corrupt_at = nfo + 0x30123u;
        corrupt_once = 0;
        rc = ldr_session();
        bad += check("a damaged package: refused after 3 passes, the record stays", rc == -12 && !record_cleared
                     && nor[T_REC + 6u] == r6);
        corrupt_at = nfo + 0x100u;                   /* in the head: never written, still checked */
        rc = ldr_session();
        bad += check("... also when the damage is in the head", rc == -12 && !record_cleared);
        corrupt_at = 0;
        /* no "success" from the host: the record stays (the host can finish later) */
        no_finish = 1;
        rc = ldr_session();
        bad += check("no \"success\" from the host: the record stays", rc == -13 && !record_cleared && nor[T_REC + 6u] == r6);
        no_finish = 0;
        rc = ldr_session();
        bad += check("... and the next session finishes", rc == 0 && record_cleared && nor[T_REC + 6u] == 0xFF);
    }
    /* another flash chip: nothing written, the records go (the old firmware starts again) */
    {
        memcpy(nor, old + ofo, T_APP_HI);
        put_record();
        record_cleared = 0;
        erases = 0;
        flash_unknown = 1;
        rc = ldr_session();
        bad += check("unknown flash chip: nothing written, record dropped", rc == -14 && record_cleared && erases == 1
                     && nor[T_REC + 6u] == 0xFF && !memcmp(nor, old + ofo, T_APP_HI));
        flash_unknown = 0;
    }
    bad += check("protected ranges untouched (SMK: BTIF, USRTRIM, USR, key_mac)", protect_ok());
    printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
    return bad != 0;
}
