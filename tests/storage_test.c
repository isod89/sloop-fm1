/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/storage.c against a simulated NOR flash:
 * erase -> 0xFF, program can only clear bits, page writes must not wrap. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    if ((off & 0xFFu) + n > 256u) {
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    char a[600], b[600], got[600];
    int bad = 0, n;
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    memset(b, 'B', sizeof b);
    bad += check("empty flash loads nothing", st_load(OBJ_PROJECT0, got, sizeof got) < 0);
    bad += check("save A", st_save(OBJ_PROJECT0, a, sizeof a) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save B", st_save(OBJ_PROJECT0, b, sizeof b) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns B (newer seq)", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 1;                                  /* payload page 1 written, the rest torn */
    st_save(OBJ_PROJECT0, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("torn save keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 3;                                  /* all payload pages, header torn */
    st_save(OBJ_PROJECT0, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("header not written keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("save A again", st_save(OBJ_PROJECT0, a, sizeof a) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    nor[st_sector(OBJ_PROJECT0, 0) + ST_PAYLOAD_OFF + 10] ^= 0x01;   /* bit rot in one copy */
    nor[st_sector(OBJ_PROJECT0, 1) + ST_PAYLOAD_OFF + 10] ^= 0x01;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("both copies corrupt -> nothing", n < 0);
    bad += check("other objects untouched", st_load(OBJ_PROJECT0 + 1, got, sizeof got) < 0);
    bad += check("settings save/load",
                 st_save(OBJ_SETTINGS, "hello", 5) == 0 && st_load(OBJ_SETTINGS, got, 5) == 5 && !memcmp(got, "hello", 5));
    bad += check("CRC-32 is zlib's (check value 0xCBF43926)", st_crc32("123456789", 9) == 0xCBF43926u);
    memset(nor, 0xFF, sizeof nor);
    st_save(OBJ_PROJECT0 + 2, a, sizeof a);
    st_save(OBJ_PROJECT0 + 2, b, sizeof b);              /* B is newer, in the other copy */
    {
        st_hdr_t h;
        int cur = st_current(OBJ_PROJECT0 + 2, &h);
        nor[st_sector(OBJ_PROJECT0 + 2, (uint32_t)cur) + ST_PAYLOAD_OFF + 300] ^= 0x10;   /* rot in the newer copy */
    }
    n = st_load(OBJ_PROJECT0 + 2, got, sizeof got);
    bad += check("newer copy rotten -> the older one loads", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save over the rotten copy", st_save(OBJ_PROJECT0 + 2, b, sizeof b) == 0);
    n = st_load(OBJ_PROJECT0 + 2, got, sizeof got);
    bad += check("... loads the new data", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    nor[st_sector(OBJ_PROJECT0 + 2, 0) + 8] ^= 0x01;    /* both headers broken */
    nor[st_sector(OBJ_PROJECT0 + 2, 1) + 8] ^= 0x01;
    bad += check("both headers broken -> nothing", st_load(OBJ_PROJECT0 + 2, got, sizeof got) < 0);
    {   /* every copy of every object in the Felucca regions, off the sample slots (0xA0000..0xDBFFF) and
         * the update staging (0xE0000..0xE4FFF; the FM6 bank's two sectors follow it), and no two sectors shared */
        uint32_t o, c, o2, c2, inside = 1, apart = 1;
        for (o = 0; o < OBJ_COUNT; o++)
            for (c = 0; c < 2u; c++) {
                uint32_t a = st_sector(o, c);
                int data = a >= 0x97000u && a + 4096u <= 0xA0000u, ups = a >= 0xDC000u && a + 4096u <= 0xE0000u;
                int glob = a >= 0xFC000u && a + 4096u <= 0xFF000u, fm6 = a >= 0xE5000u && a + 4096u <= 0xE7000u;
                inside &= (data || ups || glob || fm6) && !(a & 0xFFFu);
                for (o2 = 0; o2 < OBJ_COUNT; o2++)
                    for (c2 = 0; c2 < 2u; c2++)
                        if ((o2 != o || c2 != c) && st_sector(o2, c2) == a)
                            apart = 0;
            }
        bad += check("data stays in the Felucca regions (autosave too)", inside);
        bad += check("every object copy has its own sector", apart);
    }
    {   /* SLOOP 2.5: the settings in two parts (persist_t, then the SYN kits): one record, A/B as any other */
        static char p1[88], p2[1464], all[88 + 1464], got2[88 + 1464];
        memset(p1, 'P', sizeof p1);
        memset(p2, 'K', sizeof p2);
        memcpy(all, p1, sizeof p1);
        memcpy(all + sizeof p1, p2, sizeof p2);
        bad += check("2.5: save in two parts", st_save2(OBJ_SETTINGS, p1, sizeof p1, p2, sizeof p2) == 0);
        n = st_load(OBJ_SETTINGS, got2, sizeof got2);
        bad += check("2.5: ... loads as one object", n == (int)sizeof all && !memcmp(got2, all, sizeof all));
        n = st_load(OBJ_SETTINGS, got2, sizeof p1);
        bad += check("2.5: ... a 2.4 load takes its part only", n == (int)sizeof p1 && !memcmp(got2, p1, sizeof p1));
        memset(p2, 'L', sizeof p2);
        fail_after = 4;                              /* a torn second save: the first copy stays */
        st_save2(OBJ_SETTINGS, p1, sizeof p1, p2, sizeof p2);
        fail_after = -1;
        n = st_load(OBJ_SETTINGS, got2, sizeof got2);
        bad += check("2.5: ... a torn save keeps the last one", n == (int)sizeof all && !memcmp(got2, all, sizeof all));
        bad += check("2.5: ... too big for a sector: refused", st_save2(OBJ_SETTINGS, p2, 3000, p2, 1000) == -1);
    }
    {   /* 2.6: SAVE > PROJECT > ERASE (project.c project_erase), as the editor's restore of an empty slot: an object
         * of no bytes; a load reads 0 bytes (proj_fetch: no project), the next save works as ever */
        static uint8_t a2[3816], got3[3816];
        memset(a2, 'P', sizeof a2);
        bad += check("2.6 erase: a project saved", st_save(OBJ_PROJECT0 + 2u, a2, sizeof a2) == 0);
        bad += check("2.6 erase: the empty object written", st_save(OBJ_PROJECT0 + 2u, a2, 0) == 0);
        n = st_load(OBJ_PROJECT0 + 2u, got3, sizeof got3);
        bad += check("2.6 erase: loads 0 bytes (an empty slot)", n == 0);
        bad += check("2.6 erase: saved again", st_save(OBJ_PROJECT0 + 2u, a2, sizeof a2) == 0 &&
                     st_load(OBJ_PROJECT0 + 2u, got3, sizeof got3) == (int)sizeof a2 && !memcmp(got3, a2, sizeof a2));
    }
    printf("%s\n", bad ? "STORAGE TEST FAILED" : "storage test passed");
    return bad != 0;
}
