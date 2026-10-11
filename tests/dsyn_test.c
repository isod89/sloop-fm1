/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.5: the drum track's SYN1..SYN4, your own synthesised kits (drum_synth.c dsu), and the editor's DRUM
 * SYNTH commands (editor_dsyn.c, protocol v10), through hostsim.c:
 *   the kits list ends with SYN1..SYN4; never stored they are copies of 808, 909, TRAP, TECHNO
 *   SYN1 (a copy of 808) plays every lane bit for bit as the 808 kit; an edit is heard at once, undone it is the same
 *   any byte the editor (or a file, or flash) sends is put into range: 2000 random kits play bounded and finite
 *   DSYN_LIST / GET / PUT (a sound, the name, a factory copy) / PLAY (queued, played by the audio block) / STORE
 *   (no flash here: rc 4) */
#define main hostsim_main
#include "hostsim.c"
#undef main
static struct { uint8_t force; } ui;
/* the editor's reply helpers (editor.c), on a buffer */
static uint8_t ed_out[600];
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out) ed_out[ed_n++] = (uint8_t)(v & 0x7Fu); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        for (j = 0; j < 7u && na && n < max; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return n;
}
static void ed_pack7(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < k; i++)
            m |= (uint32_t)(p[i] >> 7) << i;
        ed_b(m);
        for (i = 0; i < k; i++)
            ed_b(p[i] & 127u);
        p += k;
        n -= k;
    }
}
#include "../firmware/src/editor_dsyn.c"

static int fails;
static void check(int ok, const char *what) { printf("dsyn: %-78s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

/* lane l of the 16 sounds snd, 120 blocks of the voice alone, into o; returns the samples */
static int32_t outbuf[2][120 * CTL];
static void voice_render(int32_t *o, const dsnd_t *snd, uint32_t crush, uint32_t l)
{
    static dsv_t v;
    uint32_t blk;
    rng_state = 0x1234567u;                             /* (the noise's seed) */
    ds_on(&v, snd, crush, DS_LANE_NOTE[l], 110);
    for (blk = 0; blk < 120u; blk++)
        if (!ds_render(&v, o + blk * CTL, CTL))
            memset(o + blk * CTL, 0, CTL * sizeof(int32_t));
}
static int lanes_same(const dsnd_t *a, uint32_t ca, const dsnd_t *b, uint32_t cb, uint32_t *ndiff)
{
    uint32_t l, n = 0, i;
    for (l = 0; l < DS_LANES; l++) {
        voice_render(outbuf[0], a, ca, l);
        voice_render(outbuf[1], b, cb, l);
        for (i = 0; i < 120u * CTL; i++)
            n += outbuf[0][i] != outbuf[1][i];
        if (ndiff) ndiff[l] = n;
    }
    return n == 0;
}
static uint32_t put(uint32_t k, uint32_t part, const uint8_t *d, uint32_t nd)
{
    uint8_t a[64];
    uint32_t na = 2, n = nd, m;
    const uint8_t *p = d;
    a[0] = (uint8_t)k;
    a[1] = (uint8_t)part;
    while (n) {                                         /* pack7 by hand: the editor's side */
        uint32_t c = n > 7u ? 7u : n, i;
        m = 0;
        for (i = 0; i < c; i++)
            m |= (uint32_t)(p[i] >> 7) << i;
        a[na++] = (uint8_t)m;
        for (i = 0; i < c; i++)
            a[na++] = p[i] & 127u;
        p += c;
        n -= c;
    }
    ed_n = 0;
    ed_dsyn_handle(ED_DSYN_PUT, a, na);
    return ed_n == 3u ? ed_out[2] : 99u;
}

int main(void)
{
    uint32_t i, k, ok;
    uint64_t rng_s = 12345;
    host_tracks_init();
    song.g[G_DRREV] = 0;
    song.g[G_BPM] = 120;
    check(DRUM_SYN == DRUM_PAIR + 1u && DRUM_KITS == DRUM_SYN + 4u && !strcmp(DRUM_KIT_NAMES[DRUM_SYN], "SYN1") &&
          !strcmp(DRUM_KIT_NAMES[DRUM_KITS - 1u], "SYN4") && !strcmp(DRUM_KIT_STYLES[DRUM_SYN], "YOUR SYNTH"),
          "the kit list ends with SYN1..SYN4, after USR3+4 (older projects keep their kit)");
    (void)dsu_kit(0);
    ok = 1;
    for (k = 0; k < DSU_N; k++)
        ok &= !memcmp(dsu.k[k].s, DS_KITS[DSU_DEF[k]].s, sizeof dsu.k[k].s) && dsu.k[k].crush == DS_KITS[DSU_DEF[k]].crush &&
              !strncmp(dsu.k[k].name, DS_KITS[DSU_DEF[k]].name, 8) && dsu.k[k].src == DSU_DEF[k];
    check(ok && !strcmp(DS_KITS[DSU_DEF[0]].name, "808") && !strcmp(DS_KITS[DSU_DEF[3]].name, "TECHNO"),
          "never stored: SYN1..4 = 808, 909, TRAP, TECHNO");
    {
        uint32_t d[DS_LANES], act = 0;
        check(lanes_same(DS_KITS[0].s, DS_KITS[0].crush, dsu.k[0].s, dsu.k[0].crush, 0),
              "SYN1 (808) plays the 16 sounds bit for bit as the 808 kit");
        dsu.k[0].s[0].decay = (uint8_t)(dsu.k[0].s[0].decay - 30u);
        lanes_same(DS_KITS[0].s, DS_KITS[0].crush, dsu.k[0].s, dsu.k[0].crush, d);
        check(d[0] > 1000u && d[DS_LANES - 1u] == d[0], "an edit (the kick's decay) is heard on that sound only");
        dsu.k[0].s[0].decay = (uint8_t)(dsu.k[0].s[0].decay + 30u);
        check(lanes_same(DS_KITS[0].s, DS_KITS[0].crush, dsu.k[0].s, dsu.k[0].crush, 0), "... undone: bit for bit again");
        for (i = 0; i < NDRUM; i++)
            drums.v[i].active = 0;
        TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + 2u);
        drum_on(38, 100);
        for (i = 0; i < NDRUM; i++)
            act += drums.v[i].active && drums.synth[i] && drums.ds[i].d == &dsu.k[2].s[1] && drums.kit[i] == DRUM_SYN + 2u;
        TDRUM->p[P_E0] = (int16_t)(DRUM_SAMPLED + 5u);
        drum_on(38, 100);
        for (i = 0; i < NDRUM; i++)
            act += drums.v[i].active && drums.synth[i] && drums.ds[i].d == &DS_KITS[5].s[1];
        check(act == 2u, "KIT = SYN3 plays its own sounds (RAM); the factory kits theirs");
    }
    /* the editor's commands */
    ed_n = 0;
    ed_dsyn_handle(ED_DSYN_LIST, 0, 0);
    check(ed_out[0] == DS_NKITS && ed_out[1] == DSU_N && ed_out[2] == 1 && !strcmp((char *)ed_out + 3, "808"),
          "DSYN_LIST: 32 factory kits, 4 of yours, stored; the names");
    {
        uint8_t a = (uint8_t)(ED_DSYN_USER + 1u), got[DS_LANES * 22u + 2u];
        uint32_t p;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_GET, &a, 1);
        p = 2u + (uint32_t)strlen((char *)ed_out + 2) + 1u;
        k = ed_unpack7(ed_out + p, ed_n - p, got, sizeof got);
        check(ed_out[0] == a && ed_out[1] == 0 && !strcmp((char *)ed_out + 2, "909") && k == sizeof got - 0u &&
              got[0] == DS_KITS[1].crush && got[1] == 1 && !memcmp(got + 2, DS_KITS[1].s, sizeof DS_KITS[1].s),
              "DSYN_GET SYN2: name, crush, the kit it came from, its 16 sounds");
        a = 99;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_GET, &a, 1);
        check(ed_n == 2u && ed_out[1] == 1, "DSYN_GET of no kit: rc 1");
    }
    {
        uint8_t ff[22], hdr[10] = {'M', 'Y', ' ', 'K', 'I', 'T', 0, 0, 0x21, 3}, src = 5;
        const dsnd_t *d = &dsu.k[2].s[4];
        memset(ff, 0xFF, sizeof ff);
        check(put(2, 4, ff, 22) == 0 && dsu_dirty && d->wave <= DW_BELL && (d->src & 15u) <= DN_CHIP && d->pitch <= 127u &&
              d->fine <= 15u && d->bend <= 96u && d->btime <= 127u && d->decay <= 127u && d->tlev <= 127u && d->fcut <= 127u &&
              d->hpf <= 127u && d->chip <= 127u && d->drive <= 127u && d->ndec <= 127u,
              "DSYN_PUT a sound of 0xFF bytes: written, every value in range, not stored");
        check(put(2, 16, hdr, 10) == 0 && !memcmp(dsu.k[2].name, "MY KIT\0\0", 8) && dsu.k[2].crush == 0x21 && dsu.k[2].src == 3,
              "DSYN_PUT the name, the crush, its source");
        check(put(3, 17, &src, 1) == 0 && !memcmp(dsu.k[3].s, DS_KITS[5].s, sizeof dsu.k[3].s) && !strcmp(dsu.k[3].name, "TRAP"),
              "DSYN_PUT 17: SYN4 = a copy of a factory kit");
        check(put(4, 0, ff, 22) == 1 && put(0, 0, ff, 21) == 1 && put(0, 18, ff, 1) == 1, "DSYN_PUT: no such kit / part, a short sound: rc 1");
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_STORE, 0, 0);
        check(ed_n == 1u && ed_out[0] == 4u, "DSYN_STORE without flash: rc 4");
    }
    {
        uint8_t a[3] = {1, 3, 100};
        uint32_t act = 0;
        for (i = 0; i < NDRUM; i++)
            drums.v[i].active = 0;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_PLAY, a, 3);
        events_block(CTL);
        for (i = 0; i < NDRUM; i++)
            act += drums.v[i].active && drums.synth[i] && drums.kit[i] == DRUM_SYN + 1u && drums.v[i].note == 42u;
        check(ed_out[2] == 0 && act == 1u && !dsu_aud_vel, "DSYN_PLAY: the hat of SYN2 plays on the next audio block");
        a[1] = 16;
        ed_n = 0;
        ed_dsyn_handle(ED_DSYN_PLAY, a, 3);
        check(ed_out[2] == 1 && !dsu_aud_vel, "DSYN_PLAY of no lane: rc 1, nothing queued");
    }
    {   /* any bytes: bounded and finite */
        static int32_t b[CTL * 2];
        int32_t pk = 0;
        uint32_t t, l, blk;
        for (t = 0; t < 2000u; t++) {
            uint8_t raw[22];
            for (l = 0; l < DS_LANES; l++) {
                for (i = 0; i < 22u; i++) {
                    rng_s = rng_s * 6364136223846793005ull + 1442695040888963407ull;
                    raw[i] = (uint8_t)(rng_s >> 56);
                }
                put(t & 3u, l, raw, 22);
            }
            dsu.k[t & 3u].crush = (uint8_t)(rng_s >> 40);
            TDRUM->p[P_E0] = (int16_t)(DRUM_SYN + (t & 3u));
            drum_on(DS_LANE_NOTE[t % DS_LANES], 30u + t % 98u);
            for (blk = 0; blk < 6u; blk++) {
                mix_block(b, CTL);
                for (i = 0; i < CTL * 2u; i++) {
                    int32_t a = b[i] < 0 ? -b[i] : b[i];
                    if (a > pk) pk = a;
                }
            }
        }
        check(pk > 0 && pk < (1 << 24), "2000 random kits (any byte the editor could send): bounded, finite");
    }
    {   /* 2.6 (a whistle on the kicks): the levels glide into the next block without a step. The kick of every
         * factory kit (not the square-wave ones), alone: its curvature (third difference) across the block edges no bigger than inside the
         * blocks (a step of up to 31 at every edge, before: 2 to 5 times) */
        uint32_t kk, worst_k = 0;
        double worst = 0;
        for (kk = 0; kk < DS_NKITS; kk++) {
            double edge = 0, in = 0;
            uint32_t ne = 0, ni = 0, j;
            if (DS_KITS[kk].s[0].wave == DW_SQUARE || DS_KITS[kk].s[0].wave == DW_BELL)
                continue;                                   /* (square waves: their own edges everywhere) */
            voice_render(outbuf[0], DS_KITS[kk].s, 0, 0);   /* (no crush: its hold is a step of its own) */
            for (j = 10u * CTL; j < 110u * CTL; j++) {
                double d3 = fabs((double)outbuf[0][j] - 3.0 * outbuf[0][j - 1] + 3.0 * outbuf[0][j - 2] - outbuf[0][j - 3]);
                if (j % CTL < 3u) edge += d3, ne++;
                else in += d3, ni++;
            }
            if (in > 0 && edge / ne / (in / ni) > worst)
                worst = edge / ne / (in / ni), worst_k = kk;
        }
        printf("  kick, curvature at the block edges / inside: at most %.2f (%s)\n", worst, DS_KITS[worst_k].name);
        check(worst < 1.6, "the kicks of the synthesised kits: no step at the block edges (no whistle at 1.4 / 2.8 kHz)");
    }
    printf(fails ? "dsyn: FAILED\n" : "dsyn: all checks ok\n");
    return fails;
}
