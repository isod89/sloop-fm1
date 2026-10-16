/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; the flash head
 *      (SPL, isd_config) is never written;
 *   4. checks the CRC16 the package stores for flash.bin against everything it
 *      was served (the head too, which it never writes; a mismatch: up to two
 *      more passes, rewriting what differs), asks 0xF0000000 ("success"), then
 *      invalidates the update record (RAM and flash) so the SPL boots the new
 *      app, and resets. A damaged package or no "success" leaves the record:
 *      the loader stays (also after a power cycle) and the host can resume.
 * Power loss during 3: the record is still there, the SPL runs the loader
 * again on the next power-on and the host can resume.
 *
 * Hooks from the platform (loader.c, tests/ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_flash_known()     the flash chip is the 1 MiB part SLOOP's storage expects (else nothing is
 *                         written: the update records go and the old firmware starts again)
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#if BOARD_SMK37
/* SMK-37 Pro: app area up to the stock PRCT end; records only in the VM, so ldr_records_drop can never
 * erase BTIF / USRTRIM (0xC0000..0xC1FFF, the key calibration) or anything above (see the fm1-to-smk37 guide) */
#define LDR_APP_HI 0x9D000u
#define LDR_REC_LO 0x9D000u
#define LDR_REC_HI 0xC0000u
_Static_assert(LDR_APP_HI <= LDR_REC_LO && LDR_REC_HI <= 0xC0000u && LDR_APP_LO >= 0x4000u,
               "SMK loader windows must stay below BTIF / USRTRIM and above the head");
#else
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals */
#endif

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static int ldr_flash_known(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

#define LDR_EBADPKG (-12)                       /* flash.bin as served does not match the package's CRC16 */
#define LDR_ENOFIN (-13)                        /* no "success" from the host */
#define LDR_EFLASH (-14)                        /* another flash chip: nothing written, the records dropped */

/* the update records (RAM, and flash: a 4K boundary - 256) go: the SPL boots the app */
static void ldr_records_drop(void)
{
    uint32_t s;
    ldr_record_clear();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {
        uint8_t r[80];
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0))
            ldr_erase(s - 0x1000u);
    }
}

static int ldr_pass(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, n, want, crc = 0, done = 0;
    int rc;
    /* 1. UFW header + entry list (ota.c) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len, &want)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI)
        return -3;
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    if (!ldr_flash_known()) {                            /* SLOOP would run without its storage and could not */
        ldr_records_drop();                              /* be updated again: keep the firmware there is */
        return LDR_EFLASH;
    }
    /* the head of flash.bin is never written, but it is part of the package's CRC */
    for (s = 0; s < LDR_APP_LO; s += 512u) {
        if (ota_read(fl_off + s, sec, 512))
            return -7;
        crc = ota_crc16(sec, 512, crc);
    }
    /* 3. app area, sector by sector */
    for (s = LDR_APP_LO; s < LDR_APP_HI; s += 0x1000u) {
        for (k = 0; k < 0x1000u; k += 512u)
            if (ota_read(fl_off + s + k, sec + k, 512))
                return -7;
        crc = ota_crc16(sec, 0x1000u, crc);
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (!ota_memeq(sec, cur, 0x1000u)) {
            uint32_t tries;
            for (tries = 0; tries < 2u; tries++) {
                if (ldr_erase(s))
                    return -9;
                for (k = 0; k < 0x1000u; k += 256u)
                    if (ldr_prog(s + k, sec + k, 256))
                        return -10;
                if (ldr_fread(s, cur, 0x1000u))
                    return -8;
                if (ota_memeq(sec, cur, 0x1000u))
                    break;
            }
            if (tries == 2u)
                return -11;                              /* verify failed twice */
        }
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO);
    }
    for (s = LDR_APP_HI; s < fl_size; s += n) {          /* (a flash.bin longer than the app area) */
        n = fl_size - s < 512u ? fl_size - s : 512u;
        if (ota_read(fl_off + s, sec, n))
            return -7;
        crc = ota_crc16(sec, n, crc);
    }
    /* 4. finish: the whole package arrived as it was built, the host confirms, the record goes,
     *    the SPL boots the app */
    if (crc != want)
        return LDR_EBADPKG;
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    if (i == 4u)
        return LDR_ENOFIN;
    ldr_records_drop();
    return 0;
}

/* a pass that does not add up (a byte damaged on the way) is run again: the host answers any read, so it
 * just serves the image once more and the sectors that differ are rewritten; a package that is damaged
 * itself fails every pass and the record stays */
static int ldr_session(void)
{
    int rc = LDR_EBADPKG;
    uint32_t pass;
    for (pass = 0; pass < 3u && rc == LDR_EBADPKG; pass++)
        rc = ldr_pass();
    return rc;
}
