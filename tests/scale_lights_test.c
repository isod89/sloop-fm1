/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real UI and keyboard mapping; no hardware or firmware package required. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static const struct { uint8_t n, interval[12]; } SCALES_EXPECTED[] = {
    {12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {7, {0, 2, 4, 5, 7, 9, 11}}, {7, {0, 2, 3, 5, 7, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 10}}, {7, {0, 2, 4, 5, 7, 9, 10}},
    {5, {0, 2, 4, 7, 9}}, {5, {0, 3, 5, 7, 10}},
    {7, {0, 2, 3, 5, 7, 8, 11}}, {7, {0, 1, 3, 5, 7, 8, 10}},
    {7, {0, 2, 4, 6, 7, 9, 11}}, {7, {0, 1, 3, 5, 6, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 11}}, {6, {0, 3, 5, 6, 7, 10}},
    {6, {0, 2, 4, 6, 8, 10}}, {8, {0, 1, 3, 4, 6, 7, 9, 10}},
    {8, {0, 2, 3, 5, 6, 8, 9, 11}},
};
static const uint8_t WHITE_POS[] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};

static uint32_t white_mask(void)
{
    uint32_t i, mask = 0;
    for (i = 0; i < sizeof WHITE_POS; i++)
        mask |= 1u << WHITE_POS[i];
    return mask;
}

static int member(uint32_t scale, int note, int root)
{
    uint32_t j;
    for (j = 0; j < SCALES_EXPECTED[scale].n; j++)
        if ((note % 12 + 12) % 12 == (root + SCALES_EXPECTED[scale].interval[j]) % 12)
            return 1;
    return 0;
}

static void setup(void)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&ui, 0, sizeof ui);
    memset(key_lit, 0, sizeof key_lit);
    memset(pad_lit, 0, sizeof pad_lit);
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 0);
        trk[i].p[P_TRANS] = trk[i].p[P_CHORD] = trk[i].p[P_QUANT] = 0;
        trk[i].p[P_SCALE] = 1;
        trk[i].p[P_ROOT] = 0;
    }
    panel = PANEL_DEFAULT;
    layers_init();
    song.sel = song.octave = 0;
    ui.layer = ly_lock = LY_PLAY;
    kb_grid = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    lights_lvl = LIGHTS_LOW;
    lights_keys = KEYS_SCALE;
    lights_notes = 0;
    go_home();
}

static void raw_scales(void)
{
    uint32_t s, root, k, combinations = 0;
    int octave, transpose;
    setup();
    assert(NSCALES == sizeof SCALES_EXPECTED / sizeof SCALES_EXPECTED[0]);
    /* Explicit musical examples catch a shifted physical-key origin or wrong root table. */
    assert(lights_keys_mask() == white_mask());                         /* C major */
    assert(keys_guide() == ((1u << 7) | (1u << 19)));                    /* C4 and C5 */
    TSEL->p[P_ROOT] = 7;
    assert(lights_keys_mask() == ((white_mask() & ~((1u << 0) | (1u << 12) | (1u << 24))) |
                                  (1u << 1) | (1u << 13) | (1u << 25))); /* G major: F# replaces F */
    TSEL->p[P_ROOT] = 2; TSEL->p[P_SCALE] = 2;
    assert(lights_keys_mask() == ((white_mask() & ~((1u << 6) | (1u << 18))) |
                                  (1u << 5) | (1u << 17)));          /* D minor: Bb replaces B */
    for (s = 0; s < NSCALES; s++)
        for (root = 0; root < 12u; root++)
            for (octave = -3; octave <= 3; octave++)
                for (transpose = -24; transpose <= 24; transpose++) {
                    uint32_t wanted = 0, roots = 0;
                    TSEL->p[P_SCALE] = (int16_t)s;
                    TSEL->p[P_ROOT] = (int16_t)root;
                    TSEL->p[P_TRANS] = (int16_t)transpose;
                    song.octave = (int8_t)octave;
                    for (k = 0; k < 27u; k++) {
                        int note = 53 + (int)k + 12 * octave + transpose;
                        if (note < 0) note = 0;
                        if (note > 127) note = 127;
                        if (member(s, note, (int)root)) wanted |= 1u << k;
                        if (note % 12 == (int)root) roots |= 1u << k;
                    }
                    assert(lights_keys_mask() == wanted);
                    assert(keys_guide() == roots);
                    combinations++;
                }
    printf("scale lights: %u chromatic combinations (16 scales, 12 roots, octave/transpose limits) ok\n", combinations);
}

static void mapped_keys(void)
{
    uint32_t s, root, mode, k;
    setup();
    /* WHITE and chord keys are evaluated after mapping, not by their printed key names. */
    TSEL->p[P_QUANT] = 2;
    TSEL->p[P_SCALE] = 2;
    TSEL->p[P_ROOT] = 2;
    assert(lights_keys_mask() == white_mask());                         /* D minor on white keys */
    assert(keys_guide() == ((1u << 7) | (1u << 19)));                    /* C4/C5 now play D */
    TSEL->p[P_ROOT] = 0; TSEL->p[P_SCALE] = 1; TSEL->p[P_TRANS] = 1;
    assert(lights_keys_mask() == ((1u << 6) | (1u << 11) | (1u << 18) | (1u << 23)));
    assert(keys_guide() == ((1u << 6) | (1u << 18)));                    /* printed B now sounds C */
    TSEL->p[P_QUANT] = 1;
    assert(lights_keys_mask() == 0x7FFFFFFu);                          /* SNAP: all keys land in C major */
    for (s = 0; s < NSCALES; s++)
        for (root = 0; root < 12u; root++)
            for (mode = 0; mode < 3u; mode++) {
                uint32_t wanted = 0, roots = 0;
                TSEL->p[P_SCALE] = (int16_t)s;
                TSEL->p[P_ROOT] = (int16_t)root;
                TSEL->p[P_QUANT] = (int16_t)mode;
                TSEL->p[P_TRANS] = 0;
                for (k = 0; k < 27u; k++) {
                    uint32_t note = kb_map(TSEL, k);
                    if (note != KB_SILENT && member(s, (int)note, (int)root)) wanted |= 1u << k;
                    if (note != KB_SILENT && note % 12u == root) roots |= 1u << k;
                }
                assert(lights_keys_mask() == wanted);
                assert(keys_guide() == roots);
            }
    for (mode = 1; mode <= 5u; mode++) {
        TSEL->p[P_ROOT] = 0; TSEL->p[P_SCALE] = 0; TSEL->p[P_TRANS] = 0;
        TSEL->p[P_QUANT] = 0; TSEL->p[P_CHORD] = (int16_t)mode;
        assert(lights_keys_mask() == white_mask());
        assert(keys_guide() == ((1u << 7) | (1u << 19)));
        /* CHR chords really use minor, including after transpose; do not use CHR's all-note mask. */
        TSEL->p[P_TRANS] = 1;
        {
            uint32_t wanted = 0;
            for (k = 0; k < 27u; k++) {
                uint32_t note = kb_map(TSEL, k);
                if (note != KB_SILENT && member(2, (int)note, 0)) wanted |= 1u << k;
            }
            assert(wanted != white_mask() && lights_keys_mask() == wanted);
        }
    }
    puts("scale lights: SNAP, WHITE, chord modifiers and CHR chord fallback ok");
}

static uint32_t led_keys(const uint8_t *plane)
{
    uint32_t k, mask = 0;
    for (k = 0; k < 27u; k++) {
        uint8_t pos = led_pos[14u + k];
        assert(pos != 0xFF);
        if (plane[pos >> 3] & (1u << (pos & 7u))) mask |= 1u << k;
    }
    return mask;
}

static void overlays(void)
{
    uint32_t k, level, layer;
    setup();
    ui_leds();
    /* ui_pages_test's empty hardware map cannot check LED planes. Give all IDs unique positions. */
    for (k = 0; k < 41u; k++) led_pos[k] = (uint8_t)((k / 4u) << 3 | (k % 4u + 1u));
    for (level = LIGHTS_LOW; level <= LIGHTS_HIGH; level++) {
        uint32_t full, dim, back;
        lights_lvl = (uint8_t)level;
        fm1_in.notes = (1u << 7) | (1u << 1);                          /* pressed root and out-of-scale F# */
        TSEL->v[0].active = TSEL->v[0].gate = 1;
        TSEL->v[0].stage = 1; TSEL->v[0].note = 64;                     /* sequence/held E4, NOTES OFF */
        ui_leds();
        full = led_keys(fm1_led); dim = led_keys(fm1_led_root); back = led_keys(fm1_led_scale);
        assert(full == ((1u << 7) | (1u << 1) | (1u << 11)));
        assert(dim == (1u << 19));                                    /* other root stays brighter */
        assert(back == (white_mask() & ~full & ~dim));
        assert(!(full & dim) && !(full & back) && !(dim & back));
        assert(led_keys(fm1_led_dim) == 0u && led_keys(fm1_led_bg) == 0u);
        assert(fm1_led_scale_level == level);
        assert(fm1_led_bg_ns == LIGHTS_NS[level] && fm1_led_bg_ns < 4000u);
    }
    /* Release and out-of-view activity must leave the hints intact. */
    fm1_in.notes = 0; TSEL->v[0].gate = 0; ui_leds();
    assert(led_keys(fm1_led) == 0);
    assert(led_keys(fm1_led_root) == ((1u << 7) | (1u << 19)));
    for (layer = LY_FX; layer < LY_COUNT; layer++) {
        uint8_t full[FM1_NCOL], dim[FM1_NCOL], back[FM1_NCOL];
        ui.layer = ly_lock = (uint8_t)layer;
        lights_keys = KEYS_OFF; ui_leds();
        memcpy(full, fm1_led, sizeof full); memcpy(dim, fm1_led_dim, sizeof dim); memcpy(back, fm1_led_bg, sizeof back);
        lights_keys = KEYS_SCALE; ui_leds();
        assert(lights_keys_mask() == 0 && lights_scale_mask(1) == 0);
        assert(fm1_led_scale_level == 0u && led_keys(fm1_led_scale) == 0u && led_keys(fm1_led_root) == 0u);
        assert(!memcmp(full, fm1_led, sizeof full) && !memcmp(dim, fm1_led_dim, sizeof dim) &&
               !memcmp(back, fm1_led_bg, sizeof back));                  /* same layer, no new scale LEDs */
    }
    ui.layer = ly_lock = LY_PLAY;
    kb_grid = 1;
    assert(lights_keys_mask() == 0 && lights_scale_mask(1) == 0);
    kb_grid = 0;
    song.sel = TRK_DRUM;
    assert(lights_keys_mask() == 0 && lights_scale_mask(1) == 0);
    song.sel = 0;
    TSEL->eng_req = 4; TSEL->p[P_E0] = (int16_t)drum_set();
    assert(drum_set() >= 0 && !kb_scaled(TSEL) && lights_keys_mask() == 0);
#if FELUCCA_SLICE
    TSEL->eng_req = ENGI_SLICE;
    assert(!kb_scaled(TSEL) && lights_keys_mask() == 0);
#endif
    puts("scale lights: LED brightness planes, played-note priority and layer/pad suppression ok");
}

static void settings_and_selection(void)
{
    uint32_t mode, word, k;
    static track_t before[NTRK];
    song_t before_song;
    setup();
    assert(KEYS_OFF == 0 && KEYS_C == 1 && KEYS_WHITE == 2 && KEYS_ALL == 3 && KEYS_SCALE == 4);
    for (mode = KEYS_OFF; mode < KEYS_N; mode++) {
        lights_keys = (uint8_t)mode;
        word = lights_word();
        lights_keys = KEYS_OFF;
        lights_from_word(word);
        assert(lights_keys == mode && lights_word() == word);
    }
    lights_from_word((word & ~(15u << 4)) | 15u << 4);
    assert(lights_keys == KEYS_OFF);                                    /* malformed mode stays safe */
    lights_keys = KEYS_SCALE; lights_lvl = LIGHTS_OFF;
    assert(lights_keys_mask() == 0 && keys_guide() == 0);
    ui_leds();
    assert(fm1_led_scale_level == 0u);
    ui.menu = 1; ui.menu_sel = MI_KEYS; lights_keys = KEYS_ALL;
    mi_set(MI_KEYS, 1);
    assert(lights_keys == KEYS_SCALE && lights_lvl == LIGHTS_LOW);
    mi_set(MI_KEYS, 1); assert(lights_keys == KEYS_SCALE);
    mi_set(MI_KEYS, 0); assert(lights_keys == KEYS_OFF);
    lights_keys = KEYS_SCALE; ui.menu = 0;
    trk[1].p[P_ROOT] = 7;
    trk[2].p[P_ROOT] = 2; trk[2].p[P_SCALE] = 2;
    for (k = 0; k < NPART; k++) {
        uint32_t key, want = 0;
        song.sel = (uint8_t)k;
        for (key = 0; key < 27u; key++)
            if (member((uint32_t)TSEL->p[P_SCALE], 53 + (int)key, TSEL->p[P_ROOT])) want |= 1u << key;
        assert(lights_keys_mask() == want);
    }
    memcpy(before, trk, sizeof trk); before_song = song;
    for (k = 0; k < 500u; k++) { lights_keys_mask(); keys_guide(); ui_leds(); }
    assert(!memcmp(before, trk, sizeof trk) && !memcmp(&before_song, &song, sizeof song));
    puts("scale lights: settings compatibility, menu, selected track and no musical state mutation ok");
}

int main(void)
{
    raw_scales();
    mapped_keys();
    overlays();
    settings_and_selection();
    puts("SCALE_LIGHTS_OK");
    return 0;
}
