/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ugotworms */
/* The SNES engine's bank as tools/gen_brr.py --bank writes it (PREFIX.N.hdr / PREFIX.N.bin), for the host tests
 * (snes_test.c, snes_ref.cpp): its parts into a RAM image of the user slots, and a sample found by its name: a
 * built-in wave (felucca_brr.h), else the bank's. The includer defines snes_test_slots (4 slots of 0x14000 B) and
 * includes felucca_brr.h first. C and C++ */
#define SNES_TEST_SLOT 0x14000u          /* eng_sample.c SMP_USER_SIZE; the data at +512 */

/* the bank's parts into slots first.. (the rest of the image 0xFF, as erased flash); the parts read, 0 = none */
static unsigned snes_test_load(const char *prefix, unsigned first)
{
    unsigned p;
    memset(snes_test_slots, 0xFF, sizeof snes_test_slots);
    for (p = 0; first + p < 4u; p++) {
        char path[512];
        FILE *f;
        unsigned char *slot = snes_test_slots + (first + p) * SNES_TEST_SLOT;
        snprintf(path, sizeof path, "%s.%u.hdr", prefix, p);
        if (!(f = fopen(path, "rb")))
            break;
        if (fread(slot, 1, 480, f) != 480) {
            fclose(f);
            break;
        }
        fclose(f);
        memset(slot + 480, 0, 32);
        snprintf(path, sizeof path, "%s.%u.bin", prefix, p);
        if (!(f = fopen(path, "rb")))
            break;
        fread(slot + 512, 1, SNES_TEST_SLOT - 512u, f);
        fclose(f);
    }
    return p;
}

typedef struct { const unsigned char *data; unsigned start, loop, len; } snes_test_smp_t;   /* len: to its end */

static unsigned snes_test_rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned snes_test_end(const unsigned char *d, unsigned a)   /* the byte after a sample's end block */
{
    while (!(d[a] & 1u))
        a += 9u;
    return a + 9u;
}

/* a sample by name: a built-in wave, or an instrument of the bank in slots first.. (as snes_test_load put it) */
static int snes_test_find(const char *name, unsigned first, snes_test_smp_t *o)
{
    static const char *const FWN[] = {SNES_FW_NAMES_INIT};
    unsigned i;
    const unsigned char *d0 = snes_test_slots + first * SNES_TEST_SLOT + 512u, *dir = d0;
    for (i = 0; i < SNES_NFW; i++)
        if (!strcmp(FWN[i], name)) {
            o->data = SNES_BRR;
            o->start = SNES_FW[i].start;
            o->loop = SNES_FW[i].loop;
            o->len = snes_test_end(SNES_BRR, o->start) - o->start;
            return 1;
        }
    if (snes_test_rd32(snes_test_slots + first * SNES_TEST_SLOT) != 0x52524246u)   /* "FBRR" */
        return 0;
    for (i = 0; i < (unsigned)(dir[0] | dir[1] << 8); i++) {
        const unsigned char *s = d0 + snes_test_rd32(dir + 8) + 12u * i;   /* sel_off */
        if (!s[8] && !strcmp((const char *)s, name)) {     /* (name[8], kind, idx) */
            const unsigned char *m = d0 + snes_test_rd32(dir + 12) + 12u * s[9];   /* smp_off */
            o->data = snes_test_slots + (first + m[0]) * SNES_TEST_SLOT + 512u;
            o->start = snes_test_rd32(m + 4);
            o->loop = snes_test_rd32(m + 8);
            o->len = snes_test_end(o->data, o->start) - o->start;
            return 1;
        }
    }
    return 0;
}
