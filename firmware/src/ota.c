/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* M-UPGRADE update entry.
 *
 * The updater talks SysEx (F0 pack7(00 59 cmd len24 body chk) F7):
 *   cmd 0x11  handshake -> we answer our package identity (FELUCCA_ID)
 *   F0 22 24 35 7F F7  upgrade -> ota_session(): pull parts of the package
 *     with cmd 0x30 read requests, check them, stage the package's update
 *     loader at 0xE0000 (outside Felucca's store), ask 0xE0000000 ("success"),
 *     then write the UPDATA_PARM record (flash 0xE4F00 + RAM 0x01C7FD88) and
 *     reset. The SPL runs the loader, which installs the package.
 * Nothing is committed before the host's "success"; any failure erases the
 * staging area and Felucca carries on.
 *
 * The firmware (felucca.c) and the host test supply these hooks:
 *   ota_wire_send(p, n)        one complete F0..F7 message to the host
 *   ota_frame_get(&p, &n)      next received SysEx (7-bit bytes between F0/F7), 0 if none
 *   ota_frame_done()           release it
 *   ota_now_ms(), ota_idle()   time; called while waiting (watchdog, USB)
 *   ota_erase(off) ota_prog(off, p, n) ota_fread(off, p, n)   flash, 0 = ok
 *   ota_show(step, code)       progress / result for the screen
 *   ota_commit(parm)           write RAM record + reset (never returns on hardware)
 */
#ifndef UI_SMALL                 /* (2.7) compiled for size: not the audio path (gfx.c UI_SMALL); the host tests and the loader: nothing */
#define UI_SMALL
#endif
#define OTA_AREA 0x000E0000u                     /* staged loader, 5 x 4 KiB */
#define OTA_AREA_LEN 0x5000u
#define OTA_RES (OTA_AREA + OTA_AREA_LEN - 0x100u)  /* 0xE4F00 = 4K boundary - 256: SPL scan */
#define OTA_LDR_HCRC 0xEBAAu                     /* official update loader (usb_hid_ota.bin) */
#define OTA_LDR_DCRC 0x5881u
#define OTA_LDR_LEN 0x4DE1u
#define OTA_FRAME_MAX 640u                       /* 512-B response = 605 wire bytes */

static int ota_wire_send(const uint8_t *p, uint32_t n);
static int ota_frame_get(const uint8_t **p, uint32_t *n);
static void ota_frame_done(void);
static uint32_t ota_now_ms(void);
static void ota_idle(void);
static int ota_erase(uint32_t off);
static int ota_prog(uint32_t off, const void *p, uint32_t n);
static int ota_fread(uint32_t off, void *p, uint32_t n);
static void ota_show(uint32_t step, int32_t code);
static void ota_commit(const uint8_t *parm);

UI_SMALL static uint32_t ota_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
UI_SMALL static uint32_t ota_rd24(const uint8_t *p) { return ota_rd16(p) | (uint32_t)p[2] << 16; }
UI_SMALL static uint32_t ota_rd32(const uint8_t *p) { return ota_rd24(p) | (uint32_t)p[3] << 24; }
UI_SMALL static void ota_wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
UI_SMALL static void ota_wr32(uint8_t *p, uint32_t v) { ota_wr16(p, v); ota_wr16(p + 2, v >> 16); }

static uint32_t ota_crc16(const uint8_t *p, uint32_t n, uint32_t c)   /* poly 0x1021, init 0 */
{
    uint32_t i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}

static void ota_jl_enc(uint8_t *p, uint32_t n)   /* UFW header cipher, key 0xFFFF */
{
    uint32_t k = 0xFFFFu;
    while (n--) {
        *p++ ^= (uint8_t)k;
        k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
    }
}

/* 8-to-7, LSB first */
static uint32_t ota_pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8u;
        while (nb >= 7u) {
            out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}

static uint32_t ota_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7Fu) << nb;
        nb += 7u;
        if (nb >= 8u) {
            if (o < max)
                out[o] = (uint8_t)acc;
            o++;
            acc >>= 8;
            nb -= 8u;
        }
    }
    return o;
}

static uint8_t ota_msg[8 + 32], ota_wire[2 + 48];
static uint8_t ota_dec[OTA_FRAME_MAX];           /* decoded host message */

UI_SMALL static int ota_send_msg(uint32_t cmd, const uint8_t *body, uint32_t n)   /* n <= 32 */
{
    uint32_t i, s = 0, w;
    ota_msg[0] = 0;
    ota_msg[1] = 0x59;
    ota_msg[2] = (uint8_t)cmd;
    ota_msg[3] = (uint8_t)n;
    ota_msg[4] = 0;
    ota_msg[5] = 0;
    for (i = 0; i < n; i++) {
        ota_msg[6 + i] = body[i];
        s += body[i];
    }
    ota_msg[6 + n] = (uint8_t)~s;
    ota_wire[0] = 0xF0;
    w = 1u + ota_pack7(ota_msg, 7u + n, ota_wire + 1);
    ota_wire[w++] = 0xF7;
    return ota_wire_send(ota_wire, w);
}

UI_SMALL static void ota_reply_identity(void)
{
#ifdef OTA_IDENTITY
    const char *id_text = OTA_IDENTITY;
    uint8_t id[27];
    uint32_t i;
    for (i = 0; i < sizeof id; i++) id[i] = 0;
    for (i = 0; i < sizeof id && id_text[i]; i++) id[i] = (uint8_t)id_text[i];
#else
    static const char ID[] = FELUCCA_ID;         /* loader / unmodified builds */
    uint8_t id[27];
    uint32_t i;
    for (i = 0; i < sizeof id; i++)
        id[i] = i < sizeof ID - 1u ? (uint8_t)ID[i] : 0;
#endif
    ota_send_msg(0x11, id, sizeof id);
}

/* decode the pending frame; returns its decoded length (0 = not ours) */
UI_SMALL static uint32_t ota_take(void)
{
    const uint8_t *p;
    uint32_t n, d, s = 0, i;
    if (!ota_frame_get(&p, &n))
        return 0;
    d = ota_unpack7(p, n, ota_dec, sizeof ota_dec);
    ota_frame_done();
    if (d < 7u || d > sizeof ota_dec || ota_dec[0] != 0 || ota_dec[1] != 0x59 || ota_rd24(ota_dec + 3) + 7u != d)
        return 0;
    for (i = 6; i + 1u < d; i++)
        s += ota_dec[i];
    if ((uint8_t)~s != ota_dec[d - 1u])
        return 0;
    return d;
}

/* main loop, outside a session: answer handshakes */
UI_SMALL static void ota_service(void)
{
    uint32_t d = ota_take();
    if (d == 7u && ota_dec[2] == 0x11)
        ota_reply_identity();
}

/* one read request (3 tries x 5 s); data -> dst */
static uint32_t ota_deadline;                    /* 0 = none; else ota_now_ms() limit of the session */

static int ota_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    uint8_t q[8];
    uint32_t tries, t, d, i;
    if (ota_deadline && (int32_t)(ota_now_ms() - ota_deadline) > 0)
        return -1;                                  /* the whole session took too long: give up */
    q[0] = 0;
    ota_wr32(q + 1, addr);
    q[5] = (uint8_t)len;
    q[6] = (uint8_t)(len >> 8);
    q[7] = 0;
    for (tries = 0; tries < 3u; tries++) {
        if (ota_send_msg(0x30, q, 8))
            return -1;
        for (t = ota_now_ms(); ota_now_ms() - t < 5000u;) {
            ota_idle();
            d = ota_take();
            if (!d)
                continue;
            if (d == 7u && ota_dec[2] == 0x11) {
                ota_reply_identity();
                continue;
            }
            if (d == len + 15u && ota_dec[2] == 0x30 && ota_rd24(ota_dec + 3) == len + 8u &&
                ota_rd32(ota_dec + 7) == addr && ota_rd24(ota_dec + 11) == len) {
                for (i = 0; i < len; i++)
                    dst[i] = ota_dec[14 + i];
                return 0;
            }
        }
    }
    return -1;
}

static int ota_memeq(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    while (n--)
        if (*a++ != *b++)
            return 0;
    return 1;
}

/* JLFS head "LOADER.BIN" in front of the staged loader (vm_update_defrag), written last */
UI_SMALL static int ota_stage_head(uint32_t len)
{
    static const char NAME[16] = "LOADER.BIN";
    uint8_t h[32];
    uint32_t i;
    for (i = 0; i < 32u; i++)
        h[i] = 0;
    ota_wr16(h + 2, 0xFFFFu);
    ota_wr32(h + 4, OTA_AREA);
    ota_wr32(h + 8, len + 32u);
    h[12] = 0x41u;
    h[13] = 0xFFu;
    ota_wr16(h + 14, 0xFFFFu);
    for (i = 0; i < 16u; i++)
        h[16 + i] = (uint8_t)NAME[i];
    ota_wr16(h, ota_crc16(h + 2, 30, 0));
    return ota_prog(OTA_AREA, h, 32);
}

/* UFW header + entry list (hdr: 1 KiB) from the host -> offset / length of
 * flash.bin (entry type 0) and ota.bin (type 100), 0 if absent, and the CRC16 the
 * package stores for flash.bin (the loader checks what it was served against it);
 * -1 read, -2 CRC */
UI_SMALL static int ota_ufw(uint8_t *hdr, uint32_t *fl_off, uint32_t *fl_len, uint32_t *ota_off, uint32_t *ota_len,
                   uint32_t *fl_crc)
{
    uint32_t i, nent;
    *fl_off = *fl_len = *ota_off = *ota_len = *fl_crc = 0;
    if (ota_read(0, hdr, 512) || ota_read(512, hdr + 512, 512))
        return -1;
    ota_jl_enc(hdr, 0x40);
    nent = ota_rd16(hdr + 8);
    if (ota_crc16(hdr + 2, 0x3E, 0) != ota_rd16(hdr) || nent == 0 || nent > 11u ||
        ota_crc16(hdr + 0x40, nent * 0x50u, 0) != ota_rd16(hdr + 2))
        return -2;
    for (i = 0; i < nent; i++) {
        uint8_t *e = hdr + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) {                             /* flash.bin */
            *fl_crc = ota_rd16(e + 4);
            *fl_off = ota_rd32(e + 8);
            *fl_len = ota_rd32(e + 12);
        }
        if (ota_rd16(e) == 100) {                           /* ota.bin */
            *ota_off = ota_rd32(e + 8);
            *ota_len = ota_rd32(e + 12);
        }
    }
    return 0;
}

UI_SMALL static int ota_stage(void)                       /* steps 1..6; 0 = host said success */
{
    static uint8_t hdr[0x400], b[512], mine[512];
    uint32_t i, n, ota_off, ota_len, fl_off, fl_len, fl_crc, c, len, own, official;
    int rc;
    /* 1. UFW header + entry list */
    ota_show(1, 0);
    if ((rc = ota_ufw(hdr, &fl_off, &fl_len, &ota_off, &ota_len, &fl_crc)) != 0)
        return rc;
    if (!fl_off || !ota_off)
        return -3;
    /* 2. which loader: the official one (known CRCs; it rewrites the whole flash.bin
     *    including the head) or Felucca's own (app area only, checks the chip key) */
    if (ota_read(ota_off, b, 512))
        return -6;
    len = ota_rd32(b + 8);
    if (ota_crc16(b + 2, 30, 0) != ota_rd16(b) || ota_rd32(b + 4) != 0x20u || b[12] != 0x41u ||
        ota_len != len + 0x20u || len > OTA_AREA_LEN - 0x200u || ota_crc16(b + 0x22, 30, 0) != ota_rd16(b + 0x20) ||
        ota_rd32(b + 0x20 + 8) != 0x01C0A800u)
        return -7;
    official = ota_rd16(b) == OTA_LDR_HCRC && ota_rd16(b + 2) == OTA_LDR_DCRC && len == OTA_LDR_LEN;
    for (i = 0x48, own = 0; i + 16u <= 0x48u + 64u && !own; i++)    /* first LZ4 literals: crt0_ldr.S */
        own = ota_memeq(b + i, (const uint8_t *)"FELUCCA-LOADER-1", 16) && ota_rd32(b + 0x20 + 4) <= 0x14000u;
    if (!official && !own)
        return -7;
    if (official) {                    /* the official loader writes the head: it must equal ours */
        ota_show(2, 0);
        for (i = 0; i < 0x4000u; i += 512u) {
            if (ota_read(fl_off + i, b, 512) || ota_fread(i, mine, 512))
                return -4;
            if (!ota_memeq(b, mine, 512))
                return -5;
        }
    }
    /* 4. stage the package's loader: erase, body, verify, head last */
    ota_show(3, 0);
    if (ota_read(ota_off, b, 512))
        return -6;
    c = ota_rd16(b + 2);                                    /* outer dcrc of the body */
    for (i = 0; i < OTA_AREA_LEN; i += 0x1000u)
        if (ota_erase(OTA_AREA + i))
            return -8;
    {
        uint32_t crc = 0;
        for (i = 0; i < len; i += n) {
            n = len - i > 512u ? 512u : len - i;
            if (ota_read(ota_off + 0x20u + i, b, n))
                return -9;
            crc = ota_crc16(b, n, crc);
            if (ota_prog(OTA_AREA + 32u + i, b, n))
                return -10;
        }
        if (crc != c)
            return -11;
        for (i = 0, crc = 0; i < len; i += n) {             /* read back from flash */
            n = len - i > 512u ? 512u : len - i;
            if (ota_fread(OTA_AREA + 32u + i, b, n))
                return -11;
            crc = ota_crc16(b, n, crc);
        }
        if (crc != c)
            return -11;
    }
    if (ota_stage_head(len))
        return -12;
    /* 6. the host confirms: nothing is committed without it */
    ota_show(4, 0);
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xE0000000u, b, 8) && ota_memeq(b, (const uint8_t *)"success", 8))
            return 0;
    return -13;
}

UI_SMALL static void ota_unstage(void)
{
    uint32_t i;
    for (i = 0; i < OTA_AREA_LEN; i += 0x1000u)
        ota_erase(OTA_AREA + i);
}

/* returns only when nothing was committed (refused, failed, or dry run) */
UI_SMALL static int ota_session(void)
{
    static uint8_t parm[112];
    uint8_t back[112];
    uint32_t i;
    int rc;
    ota_deadline = ota_now_ms() + 120000u;          /* step 1 normally takes ~5 s; a stalled host gives up after 2 min */
    rc = ota_stage();
    if (rc) {
        ota_unstage();
        ota_show(9, rc);
        return rc;
    }
    /* 5./7. UPDATA_PARM (update_mode_api_v2 layout), flash copy first */
    for (i = 0; i < sizeof parm; i++)
        parm[i] = 0;
    ota_wr16(parm + 2, 0x5A0Du);
    ota_wr16(parm + 4, 0x5A01u);
    ota_wr16(parm + 6, 0x5441u);
    for (i = 0; i < 12u; i++)
        parm[8 + i] = (uint8_t)"ota-FM-1_015"[i];    /* the loader's own USB identity */
    ota_wr32(parm + 72, OTA_AREA);
    ota_wr16(parm, ota_crc16(parm + 2, 78, 0));
#if FELUCCA_OTA_DRYRUN
    ota_unstage();
    ota_show(9, 1);                                 /* 1 = dry run complete */
    return 1;
#endif
#ifndef FELUCCA_OTA_RAMONLY
#define FELUCCA_OTA_RAMONLY 0                       /* 1: RAM record only, no power-loss resume (a power
                                                     * cycle leaves a broken loader behind) */
#endif
    if (!FELUCCA_OTA_RAMONLY && (ota_prog(OTA_RES, parm, sizeof parm) || ota_fread(OTA_RES, back, sizeof back) ||
        !ota_memeq(back, parm, sizeof parm))) {
        ota_unstage();
        ota_show(9, -14);
        return -14;
    }
    ota_show(5, 0);
    ota_commit(parm);
    return 0;
}

/* boot: a staging area left over (we are running, so the update finished or
 * never started): erase it so the SPL does not re-enter the loader */
UI_SMALL static int ota_boot_cleanup(void)
{
    uint8_t a[4], r[8];
    uint32_t i, used = 0;
    if (ota_fread(OTA_AREA, a, 4) || ota_fread(OTA_RES, r, 8))
        return -1;
    for (i = 0; i < 4u; i++)
        used |= a[i] != 0xFFu;
    for (i = 0; i < 8u; i++)
        used |= r[i] != 0xFFu;
    if (used)
        ota_unstage();
    return (int)used;
}
