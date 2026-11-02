/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's knobs (after 2.8.3; the SMK-37 version: SLOOP Pro): every page, every knob turned alone changes one value at
 * most (sloop-pro #6: on REC READY, K3 START also moved K2 LENGTH). A turn is a detent at the read and one the scan ISR counts
 * right after it (as on the device, where the scan runs while a UI pass does: tests/ui_pages_test.c enc_late); the page,
 * HOME, the REC READY screen over each page, the synth and the drum track. What a turn changed: the global and track
 * parameters, the steps, the REC screen's MODE / LENGTH / START / QUANT, the track, the page.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal tests/knob_walk_test.c -lm */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

typedef struct {
    int16_t g[G_COUNT], p[NTRK][P_COUNT];
    step_t step[NTRK][NSTEP];
    uint8_t eng[NTRK], preset[NTRK];
    int32_t x[12];
} snap_t;
static snap_t s0, s1;
static void snap(snap_t *s)
{
    uint32_t k;
    memcpy(s->g, song.g, sizeof s->g);
    for (k = 0; k < NTRK; k++) {
        memcpy(s->p[k], trk[k].p, sizeof s->p[k]);
        memcpy(s->step[k], trk[k].step, sizeof s->step[k]);
        s->eng[k] = trk[k].eng_req, s->preset[k] = trk[k].preset;
    }
    s->x[0] = rec_tempo, s->x[1] = rec_count, s->x[2] = rec_quant_off, s->x[3] = song.sel, s->x[4] = ui.page;
    s->x[5] = ui.home, s->x[6] = ui.step_page, s->x[7] = ui.cursor, s->x[8] = drum_page, s->x[9] = drum_cursor;
    s->x[10] = rec_wait, s->x[11] = ui.zoom;
}
/* the values a turn changed (a new sound, PRESETS: one), their names into what */
static uint32_t changed(char *what, size_t n)
{
    static const char *const XN[12] = {"MODE", "START", "QUANT", "track", "page", "home", "step page", "cursor",
                                       "drum page", "drum cursor", "rec arm", "zoom"};
    uint32_t c = 0, k, i;
    what[0] = 0;
#define NOTE(...) do { size_t l = strlen(what); c++; if (l + 24u < n) snprintf(what + l, n - l, __VA_ARGS__); } while (0)
    for (i = 0; i < G_COUNT; i++)
        if (s0.g[i] != s1.g[i])
            NOTE(" g%u", i);
    for (k = 0; k < NTRK; k++) {
        if (s0.eng[k] != s1.eng[k] || s0.preset[k] != s1.preset[k]) {
            NOTE(" t%u sound", k);
            continue;
        }
        for (i = 0; i < P_COUNT; i++)
            if (s0.p[k][i] != s1.p[k][i])
                NOTE(" t%u p%u%s", k, i, i == P_SLEN ? "(LEN)" : "");
        for (i = 0; i < NSTEP; i++)
            if (memcmp(&s0.step[k][i], &s1.step[k][i], sizeof s0.step[k][i])) {
                NOTE(" t%u steps", k);            /* (the pattern: one value; a note's LEN is its TIE steps) */
                break;
            }
    }
    for (i = 0; i < 12u; i++)
        if (s0.x[i] != s1.x[i] && !(i >= 4u && i <= 9u && s0.x[3] != s1.x[3]))
            NOTE(" %s", XN[i]);                   /* (ALGORITHM: the new track's page, ui.c page_fix, is the same move) */
#undef NOTE
    return c;
}
static uint32_t enc_of(uint32_t role)             /* the FM-1: the encoder of a role (EN_SELECT .. EN_K4) */
{
    return panel.enc[role] % 7u;
}
static uint32_t bad;
/* knob K(kn + 1) one detent (and a late one), on the screen as it is now; where: what is up */
static void turn(uint32_t kn, int32_t d, const char *where)
{
    char what[256];
    uint32_t e = enc_of(kn), c, i;
    for (i = 0; i < 7u; i++)
        encs[i] = enc_late[i] = 0;
    ui.hot_t = 0, ui.bpm_t = 0, ui.msg_t = 0, ui.arm = 0;
    frame();                                      /* (what the last turn left settles: GLO > MIDI's channels, ui.c
                                                   * midi_chan_fix, a frame later) */
    snap(&s0);
    encs[e] = d, enc_late[e] = d;
    frame();
    frame();                                      /* (a detent left for the next pass: read there) */
    snap(&s1);
    c = changed(what, sizeof what);
    if (c > 1u) {
        bad++;
        if (bad <= 60u)
            printf("knob_walk: %-26s encoder role %u %+d changed %u values:%s\n", where, kn, d, c, what);
    }
}

int main(int argc, char **argv)
{
    uint32_t i, pg, kn, t, rec;
    static track_t keep[NTRK];
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    for (t = 0; t < NTRK; t++)                    /* a pattern on every track: the REC screen's LENGTH and START */
        for (i = 0; i < 16u; i += 4u) {
            if (t == TRK_DRUM) {
                dstep_set(&trk[t].dstep[i], 0, LV_NORM, 0);
            } else {
                trk[t].step[i].time = ST_NOTE; trk[t].step[i].n = 1; trk[t].step[i].note[0] = (uint8_t)(48 + i);
            }
        }
    memcpy(keep, trk, sizeof keep);
    go_home(); ui.force = 1; frame();
    {   /* sloop-pro #6, on the FM-1: REC READY (a pattern: LENGTH, START, QUANT), on TRACKS: KNOB 3 (START) */
        uint32_t ok;
        rec_count = 0; trk[0].p[P_SLEN] = 32; song.sel = 0;
        go_home(); rec_wait = 1; ui.force = 1; frame();
        encs[enc_of(EN_K3)] = 1; enc_late[enc_of(EN_K3)] = 1; frame(); frame();
        ok = rec_count == 1u && trk[0].p[P_SLEN] == 32;
        check(ok, "#6 REC READY, K3 START -> count: K2 LENGTH stays 2 bars");
        printf("knob_walk: (START %u, LENGTH %d steps)\n", rec_count, trk[0].p[P_SLEN]);
        rec_wait = 0; rec_count = 0; frame();
    }
    {   /* the count-in, a free take: KNOB 1..4 edit nothing (not the page under them either) */
        char what[256];
        uint32_t n = 0, w;
        for (w = 0; w < 2u; w++)
            for (kn = 0; kn < 4u; kn++) {
                go_home(); song.sel = 0; frame();
                rec_wait = (uint8_t)!w, ci_on = (uint8_t)!w, ft_on = (uint8_t)w;
                for (i = 0; i < 7u; i++)
                    encs[i] = enc_late[i] = 0;
                snap(&s0);
                encs[enc_of(EN_K1 + kn)] = 1; enc_late[enc_of(EN_K1 + kn)] = 1; frame(); frame();
                snap(&s1);
                n += changed(what, sizeof what) != 0u;
                rec_wait = 0, ci_on = 0, ft_on = 0; frame();
            }
        check(!n, "the count-in, a free take: K1..K4 change nothing");
        memcpy(trk, keep, sizeof keep);
    }
    for (rec = 0; rec < 2u; rec++)
        for (t = 0; t < 2u; t++)
            for (pg = 0; pg <= NPAGES; pg++) {
                char where[48];
                const char *name;
                song.sel = (uint8_t)(t ? TRK_DRUM : 0u);
                song.playing = 0, transport_req = 0, song.rec = 0, ft_on = 0, ci_on = 0;
                ui.menu = 0; ui.layer = LY_PLAY; ui.sheet = 0; vis_on = 0;
                if (pg == NPAGES) {
                    go_home();
                    name = "HOME";
                } else {
                    ui.page = (uint8_t)pg; ui.home = 0; song.seq_mode = 0;
                    name = PAGES[pg].title;
                }
                rec_wait = (uint8_t)rec;
                ui.force = 1; frame();
                if (pg < NPAGES && (ui.page != pg || ui.home))
                    continue;                     /* (a page this track does not have) */
                if (rec && !rec_wait)
                    continue;                     /* (the song page: no arm) */
                snprintf(where, sizeof where, "%s%s %s", rec ? "REC READY over " : "", name, t ? "(drums)" : "(synth)");
                for (kn = 0; kn < NE; kn++) {
                    uint32_t r = kn;
                    if (pg < NPAGES && !rec && r >= EN_K1 && PAGES[pg].scope == SC_GLOBAL &&
                        (PAGES[pg].id[r - EN_K1] == G_LOAD || PAGES[pg].id[r - EN_K1] == G_SAVE ||
                         PAGES[pg].id[r - EN_K1] == G_CLRSEQ || PAGES[pg].id[r - EN_K1] == G_INITSND ||
                         PAGES[pg].id[r - EN_K1] == G_NEWPRJ))
                        continue;                 /* (a GO: a detent arms, a second one acts on the whole pattern / project) */
                    turn(kn, 1, where);
                    turn(kn, -1, where);
                    song.sel = (uint8_t)(t ? TRK_DRUM : 0u);   /* (ALGORITHM: back to the track) */
                    if (pg < NPAGES && (ui.page != pg || ui.home))
                        break;                    /* (SELECT walked the pages) */
                }
                memcpy(trk, keep, sizeof keep);
                rec_wait = 0;
            }
    check(!bad, "every page, HOME, REC READY (synth, drums): each knob alone changes one value at most");
    printf("knob_walk: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
