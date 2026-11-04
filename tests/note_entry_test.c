/* SPDX-License-Identifier: GPL-3.0-only */
/* Real NOTES input/UI, with the existing panel/audio harness. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static void reset_entry(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song); memset(&ui, 0, sizeof ui);
    host_tracks_init();
    panel = PANEL_DEFAULT; layers_init(); palette_set(4);
    song.sel = 0; song.octave = 0; song.g[G_BPM] = 120;
    host_preset(TSEL, 0, 0);
    TSEL->p[P_TRANS] = TSEL->p[P_QUANT] = TSEL->p[P_CHORD] = TSEL->p[P_VOICE] = 0;
    TSEL->p[P_SLEN] = 16; TSEL->p[P_SDIV] = 2;
    for (i = 0; i < NSTEP; i++) step_clear(&TSEL->step[i]);
    memset(TSEL->micro, 0, sizeof TSEL->micro); memset(TSEL->fill, 0, sizeof TSEL->fill);
    for (i = 0; i < NLOCK; i++) TSEL->lock[i].step = LOCK_FREE;
    fm1_in.notes = fm1_in.buttons = 0; kb_prev = 0; notes_seen = edges_btn = 0;
    memset(kb_n, 0, sizeof kb_n); memset(kb_kind, 0, sizeof kb_kind);
    memset(mi_held, 0, sizeof mi_held); step_in_w = step_in_r = step_in_lost = 0;
    rec_wait = ft_on = ci_on = transport_req = 0; undo.valid = 0;
    step_adv_off = 0; step_entry_len = ENTRY_EIGHTH;
    ui.layer = ly_lock = LY_PLAY; ui.page = (uint8_t)page_first(FAM_SEQ);
    song.seq_mode = 1; mi_ok = 1; ui.force = 1;
}

static void keys(uint32_t mask)
{
    uint32_t pressed = mask & ~fm1_in.notes;
    fm1_in.notes = mask;
    seq_entry(pressed);
}

static void put_note(uint32_t i, uint32_t note)
{
    step_clear(&TSEL->step[i]);
    TSEL->step[i].time = ST_NOTE; TSEL->step[i].n = 1; TSEL->step[i].note[0] = (uint8_t)note;
}

static void grid_and_duration(void)
{
    /* Independent 24-tick/quarter-note grid, not the firmware's div_units table. */
    const uint32_t grid[] = {24, 12, 6, 3, 8, 4, 48, 96, 192};
    const uint32_t duration[] = {0, 12, 24, 48};
    const uint32_t tempos[] = {40, 120, 240};
    uint32_t mode, div, tempo, total = 0;
    for (mode = 0; mode < ENTRY_LEN_N; mode++)
        for (div = 0; div < NDIV_STEP; div++)
            for (tempo = 0; tempo < 3; tempo++) {
                uint32_t want = mode ? (duration[mode] % grid[div] ? 0 : duration[mode] / grid[div]) : 1;
                reset_entry(); step_entry_len = (uint8_t)mode; TSEL->p[P_SDIV] = (int16_t)div;
                song.g[G_BPM] = (int16_t)tempos[tempo];
                assert(entry_steps(TSEL) == want);
                keys(1u << 7); keys(0);
                if (want) {
                    assert(TSEL->step[0].note[0] == 60 && note_len(TSEL, 0) == want);
                    assert(ui.cursor == want % 16u);
                } else {
                    assert(!step_on(&TSEL->step[0]) && ui.cursor == 0 && !undo.valid);
                    assert(!strcmp(ui.msg, "LEN NEEDS FINER DIV"));
                }
                assert(TSEL->p[P_SDIV] == (int16_t)div && song.g[G_BPM] == (int16_t)tempos[tempo]);
                total++;
            }
    printf("note entry: %u duration/DIV/BPM cases, exact grid or no mutation\n", total);
}

static void consecutive_and_chords(void)
{
    const uint8_t physical[] = {7, 9, 11, 12};
    uint32_t k;
    reset_entry();
    for (k = 0; k < 4; k++) {
        keys(1u << physical[k]);
        assert(note_len(TSEL, k * 2u) == 2u);          /* written immediately, not measured from held time */
        seq_entry(0); seq_entry(0); assert(ui.cursor == k * 2u);
        keys(0); assert(ui.cursor == (k + 1u) * 2u);
    }
    assert(undo_swap(0) && !step_on(&TSEL->step[6]) && TSEL->step[7].time == ST_REST);
    assert(TSEL->step[0].note[0] == 60 && TSEL->step[2].note[0] == 62);
    assert(undo_swap(1) && note_len(TSEL, 6) == 2u);
    reset_entry(); step_entry_len = ENTRY_QUARTER;
    keys((1u << 7) | (1u << 11) | (1u << 14));      /* C E G, one entry */
    assert(TSEL->step[0].n == 3 && note_len(TSEL, 0) == 4u);
    keys(1u << 7); assert(ui.cursor == 0); keys(0); assert(ui.cursor == 4);
    reset_entry(); step_entry_len = ENTRY_HALF; TSEL->p[P_CHORD] = 1; TSEL->p[P_SCALE] = 1;
    keys(1u << 7); keys(0);
    assert(TSEL->step[0].n == 3 && note_len(TSEL, 0) == 8u && ui.cursor == 8u);
    reset_entry(); step_adv_off = 1;
    keys(1u << 7); keys(0); assert(ui.cursor == 0 && !ui.entry_open);
    keys(1u << 9); keys(0); assert(TSEL->step[0].note[0] == 62 && ui.cursor == 0);
    step_edit(2, 2); assert(note_len(TSEL, 0) == 4u && step_entry_len == ENTRY_EIGHTH);
    step_adv_off = 0; cursor_set(4); keys(1u << 11); keys(0);
    assert(note_len(TSEL, 4) == 2u && ui.cursor == 6u);
    puts("note entry: repeated notes, chords, release, undo/redo, manual LEN and advance OFF ok");
}

static void boundaries(void)
{
    step_t saved[NSTEP];
    int8_t micro[NSTEP]; uint8_t fill[sizeof TSEL->fill]; plock_t locks[NLOCK];
    uint32_t k;
    reset_entry(); step_entry_len = ENTRY_QUARTER; put_note(2, 72);
    TSEL->micro[1] = 7; step_fill_set(TSEL, 1, FC_NOFILL);
    TSEL->lock[0].step = 1; TSEL->lock[0].param = P_LEVEL; TSEL->lock[0].val = 12;
    memcpy(micro, TSEL->micro, sizeof micro); memcpy(fill, TSEL->fill, sizeof fill); memcpy(locks, TSEL->lock, sizeof locks);
    keys(1u << 7); assert(!strcmp(ui.msg, "ENTRY: NOTE AHEAD")); keys(0);
    assert(note_len(TSEL, 0) == 2 && ui.cursor == 2 && TSEL->step[2].note[0] == 72);
    assert(!memcmp(micro, TSEL->micro, sizeof micro) && !memcmp(fill, TSEL->fill, sizeof fill) && !memcmp(locks, TSEL->lock, sizeof locks));
    reset_entry(); cursor_set(15); keys(1u << 7); assert(!strcmp(ui.msg, "ENTRY: PATTERN END")); keys(0);
    assert(note_len(TSEL, 15) == 1 && ui.cursor == 0 && !step_on(&TSEL->step[0]));
    reset_entry(); cursor_set(14); keys(1u << 7); keys(0); assert(ui.cursor == 0 && note_len(TSEL, 14) == 2);
    reset_entry(); put_note(0, 60); note_span_set(TSEL, 0, 8); put_note(8, 72);
    memcpy(saved, TSEL->step, sizeof saved);
    keys(1u << 9); keys(0);
    assert(TSEL->step[0].note[0] == 62 && note_len(TSEL, 0) == 2 && TSEL->step[8].note[0] == 72);
    for (k = 2; k < 8; k++) assert(TSEL->step[k].time == ST_REST);
    assert(undo_swap(0) && !memcmp(saved, TSEL->step, sizeof saved));
    reset_entry(); put_note(0, 60); put_note(2, 64); note_span_set(TSEL, 0, 6);
    keys(1u << 2); keys(0);                        /* replace held C by short G3; keep later E4 */
    assert(TSEL->step[2].n == 1 && TSEL->step[2].note[0] == 64);
    reset_entry(); put_note(0, 60); note_span_set(TSEL, 0, 4); cursor_set(1);
    memcpy(saved, TSEL->step, sizeof saved); keys(1u << 9); keys(0);
    assert(!memcmp(saved, TSEL->step, sizeof saved) && ui.cursor == 1 && !strcmp(ui.msg, "SELECT NOTE START"));
    for (k = 1; k <= 2; k++) {                       /* a foreign tie inside or exactly at the desired end */
        reset_entry(); TSEL->step[k].time = ST_TIE;
        memcpy(saved, TSEL->step, sizeof saved); keys(1u << 7); keys(0);
        assert(!memcmp(saved, TSEL->step, sizeof saved) && ui.cursor == 0 && !undo.valid);
        assert(!strcmp(ui.msg, "ENTRY: TIE AHEAD"));
    }
    reset_entry(); TSEL->p[P_SDIV] = 1; put_note(0, 60);
    TSEL->step[0].note[1] = 64; TSEL->step[0].n = 2; TSEL->step[1].time = ST_TIE;
    put_note(2, 60); TSEL->step[2].flags = SF_CONT0; TSEL->step[3].time = ST_TIE;
    memcpy(saved, TSEL->step, sizeof saved); keys(1u << 9); keys(0);
    assert(!memcmp(saved, TSEL->step, sizeof saved) && !undo.valid && !strcmp(ui.msg, "USE NOTE EDIT KNOBS"));
    puts("note entry: occupied steps, end/wrap, replacement tails, continued notes and metadata preservation ok");
}

static void midi_event(uint32_t tr, uint32_t note, int on)
{
    step_in_q[step_in_w++ % STEP_IN_Q] = note | (tr << 8) | (on ? 0x80u : 0u);
    midi_entry(); seq_entry(0);
}

static void midi_and_isolation(void)
{
    step_t saved[NSTEP]; uint32_t mode;
    reset_entry(); midi_event(0, 60, 1); midi_event(0, 64, 1);
    assert(TSEL->step[0].n == 2 && note_len(TSEL, 0) == 2);
    midi_event(0, 60, 0); assert(ui.cursor == 0); midi_event(0, 64, 0); assert(ui.cursor == 2);
    midi_event(1, 72, 1); midi_event(1, 72, 0); assert(ui.cursor == 2 && !step_on(&TSEL->step[2]));
    reset_entry(); TSEL->p[P_SDIV] = 0; midi_event(0, 60, 1); midi_event(0, 60, 0);
    assert(ui.cursor == 0 && !step_on(&TSEL->step[0]));
    for (mode = 0; mode < 3; mode++) {
        reset_entry(); memcpy(saved, TSEL->step, sizeof saved);
        if (mode == 0) song.rec = 1; else if (mode == 1) rec_wait = 1; else ft_on = 1;
        keys(1u << 7); keys(0);
        assert(!memcmp(saved, TSEL->step, sizeof saved) && !ui.entry_open && ui.cursor == 0);
    }
    reset_entry(); song.sel = TRK_DRUM; memcpy(saved, TSEL->step, sizeof saved);
    keys(1u << 7); keys(0); assert(!memcmp(saved, TSEL->step, sizeof saved));
    reset_entry(); go_home(); memcpy(saved, TSEL->step, sizeof saved); key(7);
    assert(!memcmp(saved, TSEL->step, sizeof saved));
    reset_entry(); keys(1u << 7); go_home(); fm1_in.notes = 0; frame();
    assert(note_len(TSEL, 0) == 2 && ui.cursor == 0 && !ui.entry_open);
    puts("note entry: MIDI chords, routing, grid rejection, live-record/drum/HOME isolation and leaving page ok");
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : "build/note-entry";
    grid_and_duration(); consecutive_and_chords(); boundaries(); midi_and_isolation();
    reset_entry(); ui.menu = 1; ui.menu_sel = MI_ENTRYLEN;
    mi_set(MI_ENTRYLEN, 1); assert(step_entry_len == ENTRY_QUARTER);
    mi_set(MI_ENTRYLEN, -1); assert(step_entry_len == ENTRY_EIGHTH);
    assert(mi_row(MI_ENTRYLEN) == 2 && mi_sec(MI_ENTRYLEN) == MS_SEQ);
    ui.force = 1; ui_draw(); ppm("entry-menu");
    ui.menu = 0; keys(1u << 7); keys(0); keys(1u << 9); keys(0); keys(1u << 11); keys(0);
    ui.force = 1; ui_draw(); ppm("entry-notes");
    puts("NOTE_ENTRY_OK");
    return 0;
}
