/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducers of what tests/editor_fuzz.c found, on the same harness (tests/editor_host.c):
 *   editor_repro 1   a user sample slot whose zone root lies outside the MIDI notes, played as a drum kit (KIT USR1):
 *                    drums.c drum_on -> eng_sample.c pow2_q16 shifts by more than 31 (UB; a garbage playback step)
 *   editor_repro 2   MIDI Bank Select 8 + Program Change (ui.c pc_poll, PC_USER_BANK): an empty user slot says
 *                    "T1 PROG n", a user preset that loads says nothing (the test of up_load_to's result is inverted)
 *   editor_repro 3   a backup restore (BK_PUT object 0) of a project whose steps carry a velocity > 127 (project.c
 *                    proj_apply keeps it): the notes play up to 2x louder than the loudest a step can be
 * Build as tests/editor_fuzz.sh builds editor_fuzz.c. */
#include "editor_host.c"

static void ed(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint8_t f[700];
    f[0] = ED_HDR0; f[1] = ED_HDR1; f[2] = ED_HDR2; f[3] = (uint8_t)cmd;
    memcpy(f + 4, a, na);
    usb_sysex(f, 4 + na);                               /* through usb.c's SysEx assembly, as the editor sends it */
}
static uint32_t pack7(uint8_t *o, const uint8_t *p, uint32_t n)
{
    uint32_t w = 0;
    while (n) {
        uint32_t k = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < k; i++) m |= (uint32_t)(p[i] >> 7) << i;
        o[w++] = (uint8_t)m;
        for (i = 0; i < k; i++) o[w++] = p[i] & 127u;
        p += k; n -= k;
    }
    return w;
}
static void midi(uint8_t st, uint8_t d1, uint8_t d2)
{
    uint8_t b[4] = {(uint8_t)(st >> 4), st, d1, d2};
    ep1_take(b, 4);
}

static int repro_root(void)
{
    uint8_t a[700], d[256], hdr[sizeof(smp_user_hdr_t)];
    smp_user_hdr_t *h = (smp_user_hdr_t *)hdr;
    uint32_t n, i;
    a[0] = 0; ed(ED_SMP_BEGIN, a, 1);                   /* slot USR1 */
    for (i = 0; i < 256; i++) d[i] = (uint8_t)(i * 37u);
    n = 0; a[n++] = 0; a[n++] = 512 & 127; a[n++] = 512 >> 7; a[n++] = 0;
    n += pack7(a + n, d, 256);
    ed(ED_SMP_WRITE, a, n);
    memset(hdr, 0, sizeof hdr);
    h->magic = SMP_USER_MAGIC; h->version = 1; h->nz = 1; memcpy(h->name, "ROOT", 4);
    h->data_len = 256; h->crc = st_crc32(d, 256);
    h->zone[0].off = 0; h->zone[0].n = 512; h->zone[0].ls = 0; h->zone[0].le = 511; h->zone[0].rate = 0x8000;
    h->zone[0].lo = 0; h->zone[0].hi = 127;
    h->zone[0].root16 = 32767;                         /* every check of the header passes: smp_zone_ok has no root check */
    n = 0; a[n++] = 0; n += pack7(a + n, hdr, sizeof hdr);
    ed(ED_SMP_END, a, n);
    printf("SMP_END rc %u, usr_nz[0] %u\n", rx_n > 6 ? rx_last[6] : 99u, usr_nz[0]);
    a[0] = TRK_DRUM; a[1] = P_E0; a[2] = (uint8_t)((DRUM_USR + 8192) & 127); a[3] = (uint8_t)((DRUM_USR + 8192) >> 7);
    ed(ED_TRACK_PARAM, a, 4);                           /* the drum track's KIT: USR1 */
    printf("drum kit %d (USR1 = %u); a kick on MIDI channel 10:\n", TDRUM->p[P_E0], DRUM_USR);
    midi(0x99, 36, 100);
    audio_blocks(1);                                    /* -> drums.c drum_on -> pow2_q16(36 * 16 - 32767) */
    printf("drum voice step %d (a sane one: ~0x8000 for rate 0x8000 at its root)\n", drums.v[0].s[5]);
    {   uint32_t v, ok = 1;                             /* (any voice: the kick's step bounded, 8 octaves down at most) */
        for (v = 0; v < sizeof drums.v / sizeof drums.v[0]; v++)
            if (drums.v[v].s[5] < 0 || drums.v[v].s[5] > (int32_t)(16u << 16))
                ok = 0;
        printf("repro 1 (a user kit zone with root16 32767): %s\n", ok ? "ok" : "FAIL");
        return !ok; }
}

static int repro_pc(void)
{
    uint8_t a[700];
    uint32_t n = 0, i, bad = 0;
    /* a user preset in slot 0 (UP_PUT), slot 5 empty */
    a[n++] = 0; a[n++] = 0; memcpy(a + n, "MY LEAD", 7); n += 7; a[n++] = 0;
    for (i = 0; i < P_COUNT; i++) { uint32_t u = (uint32_t)(TP[i].def + 8192); if (i >= P_E0) u = (uint32_t)(ENGINES[0]->edit[i - P_E0].def + 8192); a[n++] = u & 127; a[n++] = (u >> 7) & 127; }
    for (i = 0; i < 32; i++) a[n++] = 0;
    ed(ED_UP_PUT, a, n);
    printf("UP_PUT slot 0 rc %u, used %d, slot 5 used %d\n", rx_n > 6 ? rx_last[6] : 99u, up_used(0), up_used(5));
    /* channel 1 plays track 1: Bank Select 8 (user presets straight), then Program Change 5 (an empty slot) */
    ui.msg[0] = 0;
    midi(0xB0, 0, 8); midi(0xC0, 5, 0);
    ui_frame(); ui_frame();
    printf("PC 5 in bank 8 (slot 5 empty): message \"%s\", track 1 user preset %u\n", ui.msg, trk[0].user);
    bad += ui.msg[0] != 0 || trk[0].user != 0;
    ui.msg[0] = 0;
    midi(0xB0, 0, 8); midi(0xC0, 0, 0);
    ui_frame(); ui_frame();
    printf("PC 0 in bank 8 (slot 0 = MY LEAD): message \"%s\", track 1 user preset %u\n", ui.msg, trk[0].user);
    bad += strcmp(ui.msg, "T1 PROG 0") != 0 || trk[0].user != 1;
    ui.msg[0] = 0;
    midi(0xB0, 0, 0); midi(0xC0, 3, 0);
    ui_frame(); ui_frame();
    printf("PC 3 in bank 0 (a factory sound, for comparison): message \"%s\"\n", ui.msg);
    printf("repro 2 (MIDI bank 8 + program change: the message of an empty and of a stored slot): %s\n", bad ? "FAIL" : "ok");
    return bad != 0;
}

static int repro_vel(void)
{
    static project_t p;
    uint8_t a[700];
    uint32_t n, pos = 0, crc, i, k, bad = 0;
    int32_t pk0 = 0, pk1 = 0;
    for (k = 0; k < 2; k++) {
        /* track 1: a note on every step, its velocity 127 (k 0) or 255 (k 1), accent off; the rest silent */
        proj_capture(&p);
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &p.t[0].step[i];
            memset(s, 0, sizeof *s);
            s->n = 1; s->note[0] = 48; s->time = ST_NOTE; s->vel = k ? 255 : 127;
        }
        for (i = 1; i < NTRK; i++) memset(p.t[i].step, 0, sizeof p.t[i].step);
        p.t[0].p[P_LEVEL] = 100;
        p.sum = proj_sum(&p);
        crc = st_crc32(&p, sizeof p);
        n = 0; a[n++] = 0; a[n++] = 0;
        for (i = 0; i < 5; i++) a[n++] = ((uint32_t)sizeof p >> (7 * i)) & 127;
        for (i = 0; i < 5; i++) a[n++] = (crc >> (7 * i)) & 127;
        ed(ED_BK_PUT, a, n);
        for (pos = 0; pos < sizeof p; pos += 256) {
            uint32_t c = sizeof p - pos > 256 ? 256 : (uint32_t)sizeof p - pos;
            n = 0; a[n++] = 1; a[n++] = 0;
            for (i = 0; i < 5; i++) a[n++] = (pos >> (7 * i)) & 127;
            n += pack7(a + n, (const uint8_t *)&p + pos, c);
            ed(ED_BK_PUT, a, n);
        }
        a[0] = 2; a[1] = 0; ed(ED_BK_PUT, a, 2);
        printf("restore (step velocity %u): commit rc %u; track 1 step 0 velocity now %u\n", k ? 255 : 127,
               rx_n > 7 ? rx_last[7] : 99u, trk[0].step[0].vel);
        bad += trk[0].step[0].vel > 127u;
        audio_peak = 0;
        transport_req = 1;
        audio_blocks(2000);
        transport_req = 2;
        audio_blocks(400);
        if (k) pk1 = audio_peak; else pk0 = audio_peak;
        {
            uint32_t v; int mx = 0;
            for (v = 0; v < NVOICE; v++) if (trk[0].v[v].vel > mx) mx = trk[0].v[v].vel;
        }
    }
    printf("output peak: velocity 127 -> %d, velocity 255 -> %d\n", pk0, pk1);
    bad += pk1 > pk0 + pk0 / 16;
    printf("repro 3 (a restored step of velocity 255): %s\n", bad ? "FAIL" : "ok");
    return bad != 0;
}

int main(int argc, char **argv)
{
    int which = argc > 1 ? atoi(argv[1]) : 1;
    harness_init();
    return which == 1 ? repro_root() : which == 2 ? repro_pc() : repro_vel();
}
