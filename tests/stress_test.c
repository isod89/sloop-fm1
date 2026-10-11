/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.4 stress test: the real UI and audio (the ui_pages_test harness) pushed hard on what 2.4 added.
 *   font    FONT_L is FONT_S's bitmaps drawn at 2x (no copy in flash): every L glyph, drawn by the real
 *           cv_text, is checked pixel for pixel against the same glyph enlarged from S here; then
 *           random text (every byte 1..255) at random places, partly or wholly off random canvases
 *   fuzz    FRAMES frames (16 ms each, the audio between them as on the device) of hard random use: FM6
 *           on the parts, chord steps with ratchets at up to 240 BPM, CHORD+ (white keys with black
 *           modifier keys held, changed while held), STRUM -60..60, VLEAD, the track filter swept, slicer,
 *           punch FX, the visualiser (every style), the menu, SELECT spun, layers, engines switched, MIDI in.
 *           Every block: the output bounded, the strum queue sane; every frame: every draw on the screen.
 *           At the end, everything let go and stopped: no voice, drum hit or strummed note left after 30 s,
 *           and the output back to silence
 *   bench   (BENCH=1) host time per audio block of a worst case (3 FM6 parts, 4-note chords, ratchets,
 *           240 BPM) with the 2.4 additions (strum 60 ms, voice leading, the track filter on all four
 *           tracks) against the same without them; the visualiser's draw time per style against the
 *           TRACKS screen; FONT_L text drawn from S against a stored 2x copy
 * Run it under -fsanitize=address,undefined too: any read or write out of bounds stops it.
 *   stress_test [FRAMES [SEED]]   (default 40000: about 11 minutes of use) */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include <time.h>

static uint32_t sd = 2024u;
static uint32_t rn(uint32_t n) { sd = sd * 1664525u + 1013904223u; return (sd >> 8) % (n ? n : 1u); }
static int sfails;
static void scheck(int ok, const char *what) { if (!ok) { printf("FAIL %s\n", what); sfails++; } }
static double st_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }

/* ---- font ---- */
static uint8_t big_data[64 * 24 * 32 / 2 + 64];   /* the old FONT_L: S enlarged and stored (reference only) */
static uint16_t big_off[64];
static felucca_font_t FONT_L2X;

static void font_ref_build(void)
{
    uint32_t gi, o = 0;
    for (gi = 0; gi < 64u; gi++) {
        uint32_t ws = FONT_S.bw[gi], wb = FONT_L.bw[gi], bs = (ws + 1u) / 2u, bb = (wb + 1u) / 2u, x, y;
        const uint8_t *s = FONT_S.data + FONT_S.off[gi];
        big_off[gi] = (uint16_t)o;
        for (y = 0; y < 32u; y++)
            for (x = 0; x < wb; x++) {
                uint32_t sx = x / 2u, v;
                if (FONT_S.bits == 1u) {                  /* (2.6: S at 1 bit a pixel) */
                    uint32_t i = (y / 2u) * ws + sx;
                    v = (s[i >> 3] >> (7u - (i & 7u))) & 1u ? 15u : 0u;
                } else {
                    v = s[(y / 2u) * bs + sx / 2u];
                    v = (sx & 1u) ? (v & 15u) : (v >> 4);
                }
                if (x & 1u) big_data[o + y * bb + x / 2u] |= (uint8_t)v;
                else big_data[o + y * bb + x / 2u] = (uint8_t)(v << 4);
            }
        o += 32u * bb;
        assert(o <= sizeof big_data);
    }
    FONT_L2X = FONT_L;
    FONT_L2X.sh = 0;
    FONT_L2X.bits = 0;                                /* (the reference: 4-bit, stored at 2x) */
    FONT_L2X.off = big_off;
    FONT_L2X.data = big_data;
}

static void t_font(void)
{
    static uint16_t a[CV_MAX];
    uint32_t c, i, bad = 0, n = 0;
    char s[48];
    font_ref_build();
    scheck(FONT_L.data == FONT_S.data && FONT_L.sh == 1u && FONT_L.h == 2u * FONT_S.h && FONT_L.first == FONT_S.first,
           "font: FONT_L draws FONT_S's bitmaps at 2x");
    for (c = 32; c < 128u; c++) {                     /* every character, lower case folding to capitals */
        s[0] = (char)c; s[1] = 0;
        cv_begin(40, 40, C_BLACK); cv_text(6, 3, &FONT_L2X, s, C_HI);
        memcpy(a, cv_px, sizeof(uint16_t) * 40u * 40u);
        cv_begin(40, 40, C_BLACK); cv_text(6, 3, &FONT_L, s, C_HI);
        for (i = 0; i < 40u * 40u; i++) { n++; bad += a[i] != cv_px[i]; }
        scheck(text_w(&FONT_L, s) == text_w(&FONT_L2X, s) && text_w(&FONT_L, s) == 2 * text_w(&FONT_S, c < 'a' || c > 'z' ? s : "A"),
               "font: L advance = 2 x S");
    }
    printf("font: %u characters, %u pixels compared, %u different\n", 96u, n, bad);
    scheck(bad == 0, "font: FONT_L from S at 2x = the stored 2x font, pixel for pixel");
    for (i = 0; i < 20000u; i++) {                    /* random text anywhere, partly or wholly off the canvas */
        uint32_t len = rn(40), k, w = 1u + rn(240), h = 1u + rn(124);
        const felucca_font_t *f = rn(2) ? &FONT_L : &FONT_S;
        for (k = 0; k < len; k++) s[k] = (char)(1u + rn(255));
        s[len] = 0;
        cv_begin(w, h, C_BLACK);
        cv_oy = (int32_t)rn(3) - 1;
        cv_text((int32_t)rn(600) - 300, (int32_t)rn(260) - 80, f, s, (uint16_t)rn(65536));
        cv_oy = 0;
    }
    printf("font: 20000 random strings drawn on and off random canvases ok\n");
}

/* ---- fuzz ---- */
static int32_t peak;
static uint32_t loud_blocks, play_blocks, stq_max, vis_frames, menu_seen;
static double blk_ns;
static uint32_t blk_hist[2001];                       /* host time per block, 1 us buckets */
static uint32_t blk_n;
static void sframe(void)                              /* frame(), the output checked */
{
    uint32_t q, i;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++) {
        double t0 = st_ns(), d;
        mix_block(o, CTL);
        d = st_ns() - t0;
        blk_ns += d; blk_n++;
        blk_hist[d >= 2000000.0 ? 2000u : (uint32_t)(d / 1000.0)]++;
        for (i = 0; i < CTL * 2u; i++) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > peak) peak = v;
        }
        for (i = 1; i < CTL; i += 2u) {
            scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * i + 1u];
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * i];
        }
        {
            int32_t bp = 0;
            uint32_t on = 0;
            for (i = 0; i < CTL * 2u; i++) if ((o[i] < 0 ? -o[i] : o[i]) > bp) bp = o[i] < 0 ? -o[i] : o[i];
            loud_blocks += bp > 256;
            play_blocks += song.playing;
            for (i = 0; i < STQ; i++) on += stq[i].on;
            if (on > stq_max) stq_max = on;
        }
        for (i = 0; i < STQ; i++)                     /* a strummed note waits at most 3 x 60 ms */
            if (stq[i].on && (stq[i].left > 3u * 60u * FS / 1000u || stq[i].trk >= NPART)) {
                scheck(0, "fuzz: strum queue entry out of range");
                stq[i].on = 0;
            }
    }
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
    vis_frames += vis_shown();
    menu_seen += ui.menu != 0;
}

static uint32_t white[16], nwhite, black_keys[11], nblack;

static void chord_steps(uint32_t t)
{
    uint32_t i, k;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &trk[t].step[i];
        memset(s, 0, sizeof *s);
        if (rn(3) == 0) continue;
        s->n = (uint8_t)(1u + rn(4));
        for (k = 0; k < s->n; k++) s->note[k] = (uint8_t)(36 + rn(48));
        s->time = (uint8_t)(rn(8) ? ST_NOTE : ST_TIE);
        s->vel = (uint8_t)(40 + rn(88));
        s->rat = (uint8_t)rn(256);
        s->lvl = (uint8_t)rn(256);
        s->flags = (uint8_t)(rn(6) == 0 ? SF_SLIDE : 0u);
    }
}

static void t_fuzz(uint32_t nframes)
{
    uint32_t home_hold = 0, f, nf, k, i, held_btn = 0, vis_seen = 0, menu_frames = 0, mods = 0, strums = 0;
    for (k = 0; k < 27u; k++) {
        if (chord_mod_of_key(k)) black_keys[nblack++] = k;
        else if (nwhite < 16u) white[nwhite++] = k;
    }
    song.sel = 0; go_home(); ui.force = 1;
    song.g[G_BPM] = 140;
    for (k = 0; k < NPART; k++) {
        host_preset_req(&trk[k], ENGI_FM6, rn(ENGINES[ENGI_FM6]->npresets));
        chord_steps(k);
        trk[k].p[P_CHORD] = 3;
        trk[k].p[P_STRUM] = 30;
    }
    transport_req = 1;
    for (f = 0; f < nframes; f++) {
        uint32_t r = rn(100);
        if (r < 14) {                                 /* a button */
            uint32_t bt = rn(NB);
            edges_btn |= 1u << panel.btn[bt];
            if (bt != B_HOME && rn(3) == 0) held_btn ^= 1u << panel.btn[bt];
        } else if (r < 30) {
            encs[rn(NE)] += (int32_t)rn(9) - 4;      /* knobs, SELECT, ALGO, PRESETS */
        } else if (r < 44) {                          /* the keys: a chord, with modifiers */
            uint32_t m = 0;
            if (rn(4)) m |= 1u << white[rn(nwhite)];
            for (k = 0; k < nblack; k++) if (rn(5) == 0) m |= 1u << black_keys[k];
            if (rn(3) == 0) m = fm1_in.notes ^ (1u << black_keys[rn(nblack)]);   /* a modifier while held */
            fm1_in.notes = m;
            mods += (m & ~0u) != 0;
        } else if (r < 47) {
            fm1_in.notes = 0;
        } else if (r < 49) {
            uint32_t t = rn(NPART);
            trk[t].p[P_STRUM] = (int16_t)((int)rn(121) - 60); strums++;
        } else if (r < 50) {
            trk[rn(NPART)].p[P_VLEAD] = (int16_t)rn(2);
        } else if (r < 53) {
            trk[rn(NTRK)].p[P_TFLT] = (int16_t)((int)rn(128) - 64);
        } else if (r < 54) {
            trk[rn(NPART)].p[P_CHORD] = (int16_t)rn(6);
        } else if (r < 55) {
            uint32_t t = rn(NPART), e = rn(3) ? ENGI_FM6 : rn(NENGINES);
            host_preset_req(&trk[t], e, rn(ENGINES[e]->npresets));
        } else if (r < 56) {
            trk[rn(NTRK)].p[P_SDIV] = (int16_t)rn(NDIV_STEP);
        } else if (r < 57) {
            song.g[G_BPM] = (int16_t)(60 + rn(181));
        } else if (r < 58) {
            chord_steps(rn(NPART));
        } else if (r < 59) {
            trk[rn(NTRK)].p[P_SLCR] = (int16_t)rn(3);
        } else if (r < 60) {
            punch.req = (int8_t)(rn(3) ? -1 : (int)rn(PUNCH_NFX));
        } else if (r < 61) {
            song.g[G_FILT] = (int16_t)((int)rn(128) - 64);
        } else if (r < 62) {
            uint32_t ch = rn(16), nt = 24 + rn(80);
            if (mi_w - mi_r < MQ) { midi_in_q[mi_w % MQ] = ((rn(2) ? 0x9u : 0x8u)) | (((rn(2) ? 0x90u : 0x80u) | ch) << 8) | nt << 16 | (1u + rn(127)) << 24; mi_w++; }
        } else if (r < 63) {
            if (!song.rec) transport_req = song.playing ? 2 : 1;
        } else if (r < 64) {
            vis_style = (uint8_t)rn(VIS_N);
            if (rn(8) == 0 && !ui.menu) { go_home(); vis_open(); }   /* (back to TRACKS, the visualiser open) */
        } else if (r < 65) {
            trk[rn(NTRK)].p[P_MUTE] = (int16_t)(rn(4) == 0);
        }
        if (rn(40) == 0) held_btn = 0;
        if (rn(900) == 0) ly_lock = (uint8_t)rn(LY_COUNT);
        if (rn(600) == 0) ly_lock = 0;
        fm1_in.buttons = held_btn & ~(1u << panel.btn[B_HOME]);
        if (rn(700) == 0) home_hold = 60u;          /* HOME held: the menu, then fuzzed like the rest */
        if (home_hold) { home_hold--; fm1_in.buttons |= 1u << panel.btn[B_HOME]; }
        sframe();
        vis_seen |= vis_shown() ? 1u << vis_style : 0u;
        if (ui.menu && ++menu_frames % 300u == 0u) { ui.menu = 0; ui.force = 1; }
        scheck(ui.page < NPAGES && vis_style < VIS_N && song.sel < NTRK, "fuzz: UI state in range");
        if (sfails > 20) break;
    }
    nf = f;
    /* the end: everything let go, MIDI notes off, stop, 5 s */
    fm1_in.notes = 0; fm1_in.buttons = 0; held_btn = 0; ly_lock = 0; ui.menu = 0;
    punch.hold = 0; punch.req = -1;
    sframe();
    for (k = 0; k < 16u; k++)
        for (i = 24; i < 104u; i++) {
            midi_in_q[mi_w % MQ] = 0x8u | ((0x80u | k) << 8) | i << 16; mi_w++;
            if (mi_w - mi_r >= MQ - 4u) sframe();
        }
    song.rec = 0; rec_wait = 0;
    transport_req = 2;
    for (k = 0; k < NTRK; k++) trk[k].p[P_AMODE] = 0;
    for (f = 0; f < 30u * 1000u / 16u; f++) {           /* (a slow FM6 release, R4 35: several seconds) */
        uint32_t a = 0;
        sframe();
        for (k = 0; k < NPART; k++) for (i = 0; i < NVOICE; i++) a += trk[k].v[i].active;
        for (i = 0; i < NDRUM; i++) a += drums.v[i].active;
        for (i = 0; i < STQ; i++) a += stq[i].on;
        if (!a && f >= 2u * 1000u / 16u) break;
    }
    for (i = 0; i < 3u * 1000u / 16u; i++) sframe();      /* (the echo and reverb tails) */
    printf("end: everything let go and stopped: all voices free after %.1f s (at most 30)\n", f * 0.016);
    scheck(!song.playing, "end: stopped");
    for (k = 0; k < NPART; k++)
        for (i = 0; i < NVOICE; i++)
            scheck(!trk[k].v[i].active, "end: no synth voice left sounding");
    for (i = 0; i < NDRUM; i++)
        scheck(!drums.v[i].active, "end: no drum hit left");
    for (i = 0; i < STQ; i++)
        scheck(!stq[i].on, "end: no strummed note left waiting");
    {
        static int32_t o[CTL * 2];
        int32_t q = 0;
        uint32_t b;
        for (b = 0; b < FS / CTL; b++) {             /* then a second of silence */
            mix_block(o, CTL);
            for (i = 0; i < CTL * 2u; i++) if ((o[i] < 0 ? -o[i] : o[i]) > q) q = o[i] < 0 ? -o[i] : o[i];
        }
        printf("fuzz: %u frames (%.1f min), peak %d, tail %d, visualiser styles seen %u/12, chords %u, strum changes %u\n",
               nf, nf * 16.0 / 60000.0, peak, q, (unsigned)__builtin_popcount(vis_seen), mods, strums);
        scheck(q < 64, "end: silence after stop");
    }
    scheck(peak < (1 << 24), "fuzz: output bounded");
    if (nframes >= 30000u)                            /* (a short run may not reach them all) */
        scheck(__builtin_popcount(vis_seen) == (int)VIS_N, "fuzz: every visualiser style drawn");
}

/* ---- bench ---- */
static double bench_blocks(uint32_t on, uint32_t nb)
{
    static int32_t o[CTL * 2];
    uint32_t k, b;
    double t0, best = 1e30;
    int rep;
    for (rep = 0; rep < 5; rep++) {
        sd = 99u;
        for (k = 0; k < NPART; k++) {
            uint32_t i;
            trk[k].p[P_STRUM] = on ? 60 : 0;
            trk[k].p[P_VLEAD] = (int16_t)on;
            trk[k].p[P_CHORD] = 0;
            trk[k].p[P_SDIV] = 3;                     /* 1/16 */
            trk[k].p[P_MUTE] = 0;
            for (i = 0; i < NSTEP; i++) {
                step_t *s = &trk[k].step[i];
                s->n = 4; s->time = ST_NOTE; s->flags = 0; s->vel = 100; s->lvl = 0; s->rat = 0x55;
                s->note[0] = (uint8_t)(48 + i % 7); s->note[1] = (uint8_t)(52 + i % 5);
                s->note[2] = (uint8_t)(55 + i % 3); s->note[3] = (uint8_t)(59 + i % 4);
            }
        }
        for (k = 0; k < NTRK; k++) trk[k].p[P_TFLT] = on ? (int16_t)(k & 1u ? 40 : -40) : 0;
        song.g[G_BPM] = 240;
        if (!song.playing) { transport_req = 1; mix_block(o, CTL); }
        for (b = 0; b < 2000u; b++) mix_block(o, CTL);   /* (the voices up) */
        t0 = st_ns();
        for (b = 0; b < nb; b++) mix_block(o, CTL);
        t0 = (st_ns() - t0) / nb;
        if (t0 < best) best = t0;
    }
    return best;
}

static double bench_draw(int vis, uint32_t style, uint32_t n)
{
    uint32_t i;
    double t = 0;
    vis_style = (uint8_t)style;
    if (vis && !vis_on) vis_open();
    if (!vis) { vis_on = 0; go_home(); }
    for (i = 0; i < 8u; i++) frame();
    for (i = 0; i < n; i++) {
        double t0;
        uint32_t q;
        static int32_t o[CTL * 2];
        for (q = 0; q < 22u; q++) {
            uint32_t j;
            mix_block(o, CTL);
            for (j = 1; j < CTL; j += 2u) { scope_bufr[scope_w & (SCOPE_N - 1u)] = vis_tap[2u * j + 1u]; scope_buf[scope_w++ & (SCOPE_N - 1u)] = vis_tap[2u * j]; }
        }
        ui_input(); ui_leds();
        t0 = st_ns(); ui_draw(); t += st_ns() - t0;
        fm1_ms += 16;
    }
    return t / n;
}

static void t_bench(void)
{
    double off, on, home, v[VIS_N], a = 0, b = 0;
    uint32_t s, i;
    static const char *const TXT[] = {"120.0", "-64", "FILTER", "SLOOP", "C#m7", "2BAR"};
    for (i = 0; i < NPART; i++) host_preset(&trk[i], ENGI_FM6, i);
    off = bench_blocks(0, 6000);
    on = bench_blocks(1, 6000);
    printf("bench: worst case (3 FM6 parts, 4-note chords x2 ratchets, 1/16 at 240 BPM): %.2f us a block without "
           "the 2.4 additions, %.2f us with strum 60 ms + VLEAD + the filter on 4 tracks (%+.1f %%)\n",
           off / 1000, on / 1000, (on / off - 1) * 100);
    home = bench_draw(0, 0, 300);
    for (s = 0; s < VIS_N; s++) v[s] = bench_draw(1, s, 300);
    printf("bench: draw a frame: TRACKS screen %.1f us;", home / 1000);
    for (s = 0; s < VIS_N; s++) printf(" %s %.1f%s", VIS_NAME[s], v[s] / 1000, s + 1u < VIS_N ? "," : " us\n");
    vis_on = 0; go_home(); frames(4);
    for (i = 0; i < 3000u; i++) {                     /* FONT_L from S (shifts) against the stored 2x copy */
        double t0 = st_ns();
        cv_begin(240, 32, C_BLACK); cv_text(2, 0, &FONT_L, TXT[i % 6u], C_HI);
        a += st_ns() - t0;
        t0 = st_ns();
        cv_begin(240, 32, C_BLACK); cv_text(2, 0, &FONT_L2X, TXT[i % 6u], C_HI);
        b += st_ns() - t0;
    }
    printf("bench: a large value drawn: %.2f us from S at 2x, %.2f us from a stored 2x copy (%+.0f %%)\n",
           a / 3000 / 1000, b / 3000 / 1000, (a / b - 1) * 100);
    transport_req = 2; frames(4);
}

int main(int argc, char **argv)
{
    uint32_t nframes = argc > 1 ? (uint32_t)atoi(argv[1]) : 40000u;
    if (argc > 2) sd = (uint32_t)atoi(argv[2]);       /* (another seed: another run) */
    wdt_tick_ms = 1;                                  /* (the menu's PANEL setup waits for a press: as on the FM-1, its
                                                       * clock runs while it waits, and it gives up after 30 s) */
    uint32_t i;
    bank_resolve();
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_SAMPLED;
    song.g[G_BPM] = 120;
    go_home(); ui.force = 1;
    frames(4);
    t_font();
    t_fuzz(nframes);
    printf("fuzz: audio playing in %.0f %% of blocks, sounding in %.0f %%; up to %u strummed notes waiting at once; "
           "visualiser on %.0f %% of frames, menu %.0f %%\n", 100.0 * play_blocks / blk_n, 100.0 * loud_blocks / blk_n,
           stq_max, 100.0 * vis_frames / nframes, 100.0 * menu_seen / nframes);
    {   /* (a max on a shared host is its scheduler's, not ours: the 99.9 and 99.99 percentiles) */
        uint32_t i, acc = 0, p3 = 0, p4 = 0;
        for (i = 0; i <= 2000u; i++) {
            acc += blk_hist[i];
            if (!p3 && acc >= blk_n - blk_n / 1000u) p3 = i + 1u;
            if (!p4 && acc >= blk_n - blk_n / 10000u) p4 = i + 1u;
        }
        printf("fuzz: host time per audio block (%u blocks): mean %.2f us, 99.9 %% under %u us, 99.99 %% under %u us\n",
               blk_n, blk_ns / blk_n / 1000, p3, p4);
    }
    if (getenv("BENCH")) t_bench();
    printf("stress: %s\n", sfails ? "FAILED" : "font, fuzz with every 2.4 addition, clean stop PASS");
    return sfails != 0;
}
