/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/ota.c: a fake M-UPGRADE serves a real .fwsc over
 * the SysEx protocol (pack7, cmd 0x30 reads, 0xE0000000 -> "success") and a
 * simulated NOR plays the flash.
 *   cc -o ota_test tests/ota_test.c && ./ota_test FILE.fwsc
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_OTA_DRYRUN 0
static int recovery_active;
#if BOARD_SMK37                          /* -DBOARD_SMK37=1: the SMK-37 Pro build of ota.c */
#if T_MODEL == 1                         /* -DT_MODEL=1 / 2: the MODEL=elite / mke build */
#define SMK_MODEL "SMK-37 Elite"
#elif T_MODEL == 2
#define SMK_MODEL "MKE-P37"
#else
#define SMK_MODEL "SMK-37 Pro"
#endif
#define FELUCCA_ID SMK_MODEL "_900"
#define T_RECOVERY SMK_MODEL "_000"
#define T_AREA 0xBB000u
#define T_PARM_ID "ota-" SMK_MODEL "_016"
#else
#define FELUCCA_ID "FM-1_900"
#define T_RECOVERY "FM-1_000"
#define T_AREA 0xE0000u
#define T_PARM_ID "ota-FM-1_015"
#endif
#define T_AREA_HI (T_AREA + 0x5000u)
#define OTA_IDENTITY (recovery_active ? T_RECOVERY : FELUCCA_ID)

static uint8_t nor[0x100000];
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024];                 /* one pending host frame (7-bit bytes) */
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, ident_replies, mute_after = 0xFFFFFFFFu, bad_erase;
static int committed;
static uint8_t committed_parm[112];
static int32_t last_code;
static uint32_t erases;                  /* ota_erase calls */
static uint8_t *corrupt_at;              /* flip a served byte (loader body) */

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

static void host_reply(const uint8_t *msg, uint32_t n)       /* queue msg as a device-bound frame */
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

/* ---- hooks ---- */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) { ident_replies++; return 0; }
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg cmd %02x len %u\n", u[2], d); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (++requests > mute_after)
        return 0;                                         /* host gone */
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0;
    m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && corrupt_at >= logical + addr && corrupt_at < logical + addr + len)
            m[14 + (corrupt_at - (logical + addr))] ^= 0x01;
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    host_reply(m, 15 + len);
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len;
    return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms += 1; }
static int ota_erase(uint32_t off)
{
    erases++;
    if (off < T_AREA || off >= T_AREA_HI || (off & 0xFFFu)) { bad_erase++; return -8; }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < T_AREA || off + n > T_AREA_HI) { bad_erase++; return -8; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm) { committed = 1; memcpy(committed_parm, parm, 112); }

#include "../firmware/src/ota.c"

static uint32_t ota_body_off(void)                  /* ota.bin data + 0x20 in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, logical, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 100) return ota_rd32(e + 8) + 0x20u;
    }
    return 0;
}

#if BOARD_SMK37
/* edit the package's UFW list in place (deciphered, CRCs redone, enciphered again): flash.bin's length, or an
 * entry 50 (a USR image) at target / size; returns the deciphered header in h */
static void pkg_edit(uint32_t fl_len, int usr, uint32_t target, uint32_t size, uint8_t *h)
{
    uint32_t i, n;
    memcpy(h, logical, 0x400);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0 && fl_len)
            ota_wr32(e + 12, fl_len);
    }
    if (usr) {                                         /* one more entry: a USR image, as the stock packer writes it */
        uint8_t *e = h + 0x40 + n * 0x50u;
        memset(e, 0, 0x50);
        ota_wr16(e, 50);
        ota_wr32(e + 8, 0x400u);
        ota_wr32(e + 0x1C, target);
        ota_wr32(e + 0x20, size);
        memcpy(e + 0x24, "USR", 3);
        memcpy(e + 0x40, "USR", 3);
        ota_wr16(h + 8, ++n);
    }
    {
        uint8_t w[0x400];
        memcpy(w, h, 0x400);
        for (i = 0; i < n; i++)
            ota_jl_enc(w + 0x40 + i * 0x50u, 0x50);
        ota_wr16(h + 2, ota_crc16(w + 0x40, n * 0x50u, 0));    /* (the list's CRC: over the enciphered entries) */
        ota_wr16(h, ota_crc16(h + 2, 0x3E, 0));
        memcpy(w, h, 0x40);
        ota_jl_enc(w, 0x40);
        memcpy(logical, w, 0x40u + n * 0x50u);
    }
}
#endif

static int check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = T_AREA; i < T_AREA_HI; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + 0x400, 0x4000);                 /* device flash head = package flash.bin head */
#if BOARD_SMK37
    memcpy(nor + 0x4060, logical + 0x400 + 0x4060, 0xC0);  /* and its region table: it runs this build */
#endif
    rx_full = 0; requests = 0; ident_replies = 0; committed = 0; last_code = 0;
    mute_after = 0xFFFFFFFFu; corrupt_at = NULL; bad_erase = 0; erases = 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw;
    size_t n, i;
    int bad = 0, rc;
    if (argc < 2) { printf("usage: ota_test FILE.fwsc\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    logical = malloc(n);
    for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);   /* drop the marker bytes */
    memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
    logical_len = n - 20;

    /* handshake outside a session */
    reset_dev();
    {
        uint8_t m[7] = {0, 0x59, 0x11, 0, 0, 0, 0xFF};
        host_reply(m, 7);
        ota_service();
        bad += check("handshake answered", ident_replies == 1);
        bad += check("identity frame is " FELUCCA_ID,
                     ota_msg[2] == 0x11 && !memcmp(ota_msg + 6, FELUCCA_ID, sizeof FELUCCA_ID - 1));
        recovery_active = 1;
        host_reply(m, 7);
        ota_service();
        bad += check("recovery identity is distinct from a successful normal boot",
                     ident_replies == 2 && !memcmp(ota_msg + 6, T_RECOVERY, sizeof T_RECOVERY));
        recovery_active = 0;
    }

    /* full session */
    reset_dev();
    rc = ota_session();
    printf("  %u requests\n", requests);
    bad += check("session commits", rc == 0 && committed);
    bad += check("headers + loader + confirm, no head reads", requests >= 10 && requests <= 30);
    {
        uint8_t *h = nor + T_AREA, *p = committed_parm;
        uint32_t k, zero = 1;
        for (k = 8u + sizeof T_PARM_ID - 1u; k < 0x28u; k++)
            zero &= p[k] == 0;
        bad += check("staged body == the package's loader", !memcmp(nor + T_AREA + 0x20u, logical + ota_body_off(), 64));
        bad += check("LOADER.BIN head CRC",
                     (h[0] | h[1] << 8) == ota_crc16(h + 2, 30, 0) && !memcmp(h + 16, "LOADER.BIN", 10));
        bad += check("record at the area end - 256 == RAM record", !memcmp(nor + T_AREA_HI - 0x100u, p, 112));
        bad += check("parm identity " T_PARM_ID ", zero to +0x27",
                     !memcmp(p + 8, T_PARM_ID, sizeof T_PARM_ID - 1u) && zero);
        bad += check("record CRC / type / magic / ota_addr",
                     (p[0] | p[1] << 8) == ota_crc16(p + 2, 78, 0) && (p[2] | p[3] << 8) == 0x5A0D &&
                     (p[6] | p[7] << 8) == 0x5441 && ota_rd32(p + 72) == T_AREA);
        bad += check("nothing written outside the staging area", bad_erase == 0);
    }
    /* boot cleanup erases it */
    bad += check("boot cleanup erases the area", ota_boot_cleanup() == 1 && area_erased());
    bad += check("second boot: nothing to do", ota_boot_cleanup() == 0);

    /* host disappears in the middle */
    reset_dev();
    mute_after = 4;
    rc = ota_session();
    bad += check("host gone -> no commit, area clean", rc < 0 && !committed && area_erased());

    /* host never confirms */
    reset_dev();
    mute_after = 0xFFFFFFFFu;
    {
        uint32_t total;
        rc = ota_session();
        total = requests;                                  /* requests of a full run incl. confirm */
        reset_dev();
        mute_after = total - 1;                            /* answer everything but the confirmation */
        rc = ota_session();
        bad += check("no 'success' -> -13, no commit, area clean", rc == -13 && !committed && area_erased());
    }

#if BOARD_SMK37
    /* the board check: another board's package is refused before anything is erased */
    {
        static uint8_t keep[0x400], h[0x400];
        uint32_t fo, fl, oo, ol, fc;
        memcpy(keep, logical, sizeof keep);
        reset_dev();
        bad += check("board check: this package (0x9D000, this flash's table) passes",
                     !ota_ufw(h, &fo, &fl, &oo, &ol, &fc) && fl == 0x9D000u &&
                     ota_board_check(h, fo, fl, 0) == 0 && ota_board_check(h, fo, fl, 1) == 0);
        pkg_edit(0x93000u, 0, 0, 0, h);                    /* the FM-1's flash.bin length */
        reset_dev();
        rc = ota_session();
        bad += check("flash.bin 0x93000 (our loader) -> -15, nothing erased", rc == -15 && !committed && erases == 0 &&
                     area_erased() && bad_erase == 0);
        bad += check("... the official loader -> -15", ota_board_check(h, 0x400u, 0x93000u, 1) == -15);
        memcpy(logical, keep, sizeof keep);
        reset_dev();
        nor[0x4070] ^= 0x01;                               /* another region table on the device */
        bad += check("official loader, another region table -> -16",
                     ota_board_check(h, 0x400u, 0x9D000u, 1) == -16 && ota_board_check(h, 0x400u, 0x9D000u, 0) == 0);
        reset_dev();
        pkg_edit(0, 1, 0xEA000u, 0x12000u, h);             /* the FM-1's USR image (0xEA000) */
        bad += check("official loader, a USR image for 0xEA000 -> -17", ota_board_check(h, 0x400u, 0x9D000u, 1) == -17);
        memcpy(logical, keep, sizeof keep);
        pkg_edit(0, 1, 0xF4000u, 0xA000u, h);
        bad += check("... a USR image for 0xF4000 / 0xA000 passes", ota_board_check(h, 0x400u, 0x9D000u, 1) == 0);
        memcpy(logical, keep, sizeof keep);
        reset_dev();
        rc = ota_session();
        bad += check("the package unchanged commits again", rc == 0 && committed);
    }
#endif

    printf("%s\n", bad ? "OTA TEST FAILED" : "ota test passed");
    return bad != 0;
}
