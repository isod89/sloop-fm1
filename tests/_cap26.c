/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.6 screenshots for the README (not a test): the UI test's harness, a pattern, then the new pages as RGB888
 * 240x240 frames into DIR/<name>.rgb */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
static void shot(const char *name)
{
    char p[512];
    FILE *f;
    if (ui.msg_t) { ui.msg_t = 0; ui.force = 1; frame(); }
    snprintf(p, sizeof p, "%s/%s.rgb", outdir, name);
    f = fopen(p, "wb");
    for (unsigned i = 0; i < 240 * 240; i++) {
        uint16_t q = swap16(screen[i]);
        uint8_t rgb[3] = {(uint8_t)((q >> 11) * 255 / 31), (uint8_t)(((q >> 5) & 63) * 255 / 63), (uint8_t)((q & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void shot_keep(const char *name)
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/%s.rgb", outdir, name);
    f = fopen(p, "wb");
    for (unsigned i = 0; i < 240 * 240; i++) {
        uint16_t q = swap16(screen[i]);
        uint8_t rgb[3] = {(uint8_t)((q >> 11) * 255 / 31), (uint8_t)(((q >> 5) & 63) * 255 / 63), (uint8_t)((q & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static int find_preset_g(const char *n, uint32_t *e, uint32_t *p)
{ for (uint32_t i = 0; i < NENGINES; i++) for (uint32_t k = 0; k < ENGINES[i]->npresets; k++) if (!strcmp(ENGINES[i]->presets[k].name, n)) { *e = i; *p = k; return 1; } return 0; }
static int find_kit_g(const char *n) { for (int k = 0; k < DRUM_KITS; k++) if (!strcmp(DRUM_KIT_NAMES[k], n)) return k; return 0; }
static void load(int t, const char *n) { uint32_t e, p; if (find_preset_g(n, &e, &p)) { set_engine_of(&trk[t], e); apply_preset_to(&trk[t], p); trk[t].engine = trk[t].eng_req; } else { fprintf(stderr, "? %s\n", n); exit(1); } }
static void put(int t, int st, int n, const uint8_t *notes) { step_t *s = &trk[t].step[st]; memset(s, 0, sizeof *s); s->n = (uint8_t)n; memcpy(s->note, notes, (size_t)n); s->time = ST_NOTE; s->vel = 100; }
static void open_page(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].title, title)) break;
    if (i == NPAGES) { fprintf(stderr, "? page %s\n", title); exit(1); }
    open_family(PAGES[i].fam); ui.page = (uint8_t)i; ui.fam_last[PAGES[i].fam] = (uint8_t)i; page_entered(); ui.force = 1; frames(2);
}
int main(int argc, char **argv)
{
    uint32_t i, j;
    outdir = argc > 1 ? argv[1] : "/tmp";
    panel = PANEL_DEFAULT; layers_init(); settings.palette = 4; palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    load(0, "ACID 303"); load(1, "RHODES"); load(2, "KALIMBA");
    TDRUM->p[P_E0] = (int16_t)find_kit_g("BOOMBAP");
    song.g[G_BPM] = 92;
    for (j = 0; j < NTRK; j++) { steps_clear(&trk[j]); trk[j].p[P_SLEN] = 32; }
    {
        static const uint8_t R[2] = {43, 41};
        static const uint8_t EP[2][4] = {{59, 62, 66, 69}, {57, 60, 64, 67}};
        static const uint8_t MEL[32] = {83,0,81,0,78,0,74,0, 0,0,76,0,0,0,0,0, 84,0,81,0,79,0,76,0, 0,0,0,0,0,0,0,0};
        for (j = 0; j < 32u; j++) {
            uint32_t h = j / 16u; uint8_t n;
            if ((j % 4u) == 0u || (j % 8u) == 3u) { n = (uint8_t)(R[h] + ((j % 8u) == 3u ? 12 : 0)); put(0, (int)j, 1, &n); }
            if ((j % 16u) == 0u || (j % 16u) == 10u) put(1, (int)j, 4, EP[h]);
            if (MEL[j]) { n = MEL[j]; put(2, (int)j, 1, &n); }
            memset(&TDRUM->dstep[j], 0, sizeof(dstep_t));
            if ((j % 16u) == 0u || (j % 16u) == 7u || (j % 16u) == 10u) dstep_set(&TDRUM->dstep[j], 0, LV_NORM, 0);
            if ((j % 8u) == 4u) dstep_set(&TDRUM->dstep[j], 2, LV_NORM, 0);
            if (!(j & 1u)) dstep_set(&TDRUM->dstep[j], 4, (j & 2u) ? LV_SOFT : LV_NORM, 0);
        }
    }
    for (j = 0; j < NTRK; j++) trk[j].seq_active = 1;
    song.g[G_TXSRC] = 1, song.g[G_TXLVL] = 80, song.g[G_TXTONE] = -12, song.g[G_TXMOVE] = 70, song.g[G_TXRATE] = 2;
    transport_req = 1; frames(150);
    song.sel = 0; go_home(); frames(3); shot("tracks");
    /* punch-in FX: FREEZE on a black key, REV THROW (FX + OCT-) */
    press(B_FX); frames(12);
    punch.req = PX_FREEZE;
    fm1_in.buttons |= BT(B_OCTDN); edges_btn |= BT(B_OCTDN); frames(3);
    ui.force = 1; frames(2); shot("punch");
    fm1_in.buttons &= ~BT(B_OCTDN); punch.req = -1; frames(2); release(B_FX); frames(6);
    /* INSERT on the ACID 303 */
    trk[0].p[P_ITYPE] = 6, trk[0].p[P_IA] = 40, trk[0].p[P_IB] = 90, trk[0].p[P_IC] = 70, trk[0].p[P_IMIX] = 100;
    open_page("INSERT"); frames(4); shot("insert");
    /* LFO 2 */
    song.sel = 1;
    trk[1].p[P_LSYNC] = 5, trk[1].p[P_LTRIG] = 1, trk[1].p[P_LPOL] = 1;
    open_page("LFO 2"); frames(4); shot("lfo2");
    /* the STEP page, LEN on a note */
    song.sel = 2;
    open_page("STEP"); ui.cursor = 0; note_set_len(&trk[2], 0, 2); ui.force = 1; frames(3); shot("step_len");
    /* TEXTURE */
    song.sel = 0;
    open_page("TEXTURE"); frames(4); shot("texture");
    /* GLO > MIDI, MIDI 2 */
    song.g[G_CH1] = 2, song.g[G_CH2] = 11, song.g[G_CH3] = 8, song.g[G_DRCH] = 10, frames(2);
    open_page("MIDI"); frames(4); shot("midi");
    song.g[G_CHOTH] = 1, song.g[G_MIDI] = 1, song.g[G_BEND] = 12;
    open_page("MIDI 2"); frames(4); shot("midi2");
    /* MIDI LEARN */
    for (i = 0; i < ML_N; i++) ml_tab[i] = 0;
    song.sel = 0;
    open_page("FX"); frames(2);
    press(B_GLO); frames(10); key(1); release(B_GLO); frames(16);
    encs[panel.enc[EN_K1]] = 1; frames(4);
    ui.force = 1; frame();
    {   /* (the message kept: "T1 DST: SEND A CC") */
        char p[512]; FILE *f;
        snprintf(p, sizeof p, "%s/learn.rgb", outdir); f = fopen(p, "wb");
        for (unsigned k = 0; k < 240 * 240; k++) { uint16_t q = swap16(screen[k]);
            uint8_t rgb[3] = {(uint8_t)((q >> 11) * 255 / 31), (uint8_t)(((q >> 5) & 63) * 255 / 63), (uint8_t)((q & 31) * 255 / 31)}; fwrite(rgb, 1, 3, f); }
        fclose(f);
    }
    press(B_GLO); frames(10); key(1); release(B_GLO); frames(16);   /* (LEARN off) */
    /* the second sheet: what came before 2.6, in today's look */
    song.sel = 0; go_home(); frames(4);
    song.sel = TRK_DRUM; studio_open(SC_DRUM); frames(4);
    drum_page = 0; drum_lane = 0; drum_cursor = 0; ui.force = 1; frames(2); shot("b_drums_grid");
    drum_page = 1; ui.force = 1; frames(2); shot("b_drums_kit");
    drum_page = 0; song.sel = 0; go_home(); frames(4);
    ui.lock_par = P_E4;
    press(B_SEQ); frames(10);
    fm1_in.notes = 1u << key_of_white(4); frames(2);
    encs[panel.enc[EN_PRESET]] = -9; frame(); encs[panel.enc[EN_PRESET]] = -9; frame();
    ui.force = 1; frames(2); shot_keep("b_steps_lock");
    fm1_in.notes = 0; frames(2); release(B_SEQ); frames(4);
    press(B_GLO); frames(10); ui.force = 1; frames(2); shot("b_mix");
    release(B_GLO); frames(4);
    press(B_SAVE); frames(15);
    key(key_of_white(0)); key(key_of_white(1)); key(key_of_white(1)); ui.force = 1; frame(); shot("b_chain");
    release(B_SAVE); frames(4);
    song.sel = 2; load(2, "KOTO"); open_page("EDIT 1"); frames(3); shot("b_phys");
    song.sel = 1; load(1, "TINE EP"); open_page("EDIT 1"); frames(3); shot("b_fm6");
    trk[1].p[P_CHORD] = 3; trk[1].p[P_ROOT] = 2;
    go_home(); frames(2);
    press(B_SCL); frames(12); ui.force = 1; frames(2); shot("b_scl"); release(B_SCL); frames(4);
    trk[1].p[P_CHORD] = 0;
    go_home(); frames(2); vis_open(); frames(4); vis_style = 3; ui.force = 1; frames(90); shot("b_vis");
    vis_on = 0; go_home(); frames(4);
    return 0;
}
