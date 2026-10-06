/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 SLOOP contributors.
 * The operator, envelope, pitch-envelope and LFO models follow Google's music-synthesizer-for-android
 * (Copyright 2012 Google Inc., Apache-2.0: fm_core.cc, env.cc, pitchenv.cc, lfo.cc, dx7note.cc), written
 * again here in Felucca's fixed-point conventions. See LICENSING.md. */
/* DX7: six-operator FM that plays Yamaha DX7 voices as they are — the 128-byte packed voice of a
 * bulk dump (VMEM), 32 algorithms, per-operator 4-stage envelopes with keyboard level and rate
 * scaling, velocity, fixed-frequency operators, detune, feedback, the pitch envelope and the LFO
 * (six waves, delay, key sync, pitch and amplitude modulation). Format-compatible with the DX7 and
 * with Dexed; not a bit-exact emulator (the sine is Felucca's interpolated table, levels are Q14).
 *
 * Where the voices come from (P_E0 PTCH, 0..127): 128 slots in flash at DX_USER_BASE (four sectors of 32
 * voices, the banks 1..4), written by the editor (backup objects 8..11, web/EDITOR_PROTOCOL.md v7) or by a
 * Yamaha dump on the MIDI input (dx_syx_parse, editor.c). An empty or damaged slot plays INIT VOICE. Read
 * through the XIP window; a part copies its voice into RAM when PTCH changes or after a bank is written
 * (dx_gen), so a bank rewrite never changes a voice under a sounding note. No voice data is compiled in:
 * the collections such voices come from are not SLOOP's to redistribute.
 * The other EDIT knobs are performance macros over the loaded voice, never stored in it: BRGT (the
 * modulators' level, the classic brightness control), ENVS (every envelope faster or slower), LFOD
 * (more or less pitch LFO), TRN (semitones), ALG and FB (0 = the voice's own, else an override).
 *
 * Felucca's ADSR stays in front as a VCA (the voice's own envelopes are inside the operators): the
 * presets open it (ATK 0, SUS 127, REL long), and dx7_amp frees the voice itself as soon as every
 * carrier has finished its release, so a long REL costs no polyphony. The SLOOP LFO / filter / pitch
 * modulations of the track apply as on any engine (the pitch one through m->inc). */
#define DX_VOICE 128u                       /* a packed voice */
#define DX_BANK_N 32u                       /* voices per bank (a flash sector) */
#define DX_USER_BANKS 4u
#define DX_USER_SLOTS (DX_USER_BANKS * DX_BANK_N)
#define DX_USER_BASE 0xE5000u               /* flash: 0xE5000..0xE8FFF, after the OTA staging area (fm1_flash.h FL_DX_*) */
#define DX_USER_SIZE (DX_USER_BANKS * 0x1000u)
#ifndef DX_USER_XIP                         /* host tests: a RAM image of the store */
#define DX_USER_XIP fm1_xip_ptr(DX_USER_BASE)
#endif

static uint32_t dx_gen;                     /* bumped after a user bank is written: parts re-read their voice */

/* INIT VOICE as the DX7 ships it: OP1 alone at level 99, ratio 1, the others silent; pitch envelope
 * flat at 50; LFO 35 / sine / PMS 3; transpose C3; operator key sync on */
#define DX_OP_SILENT 99, 99, 99, 99, 99, 99, 99, 0, 39, 0, 0, 0, 7 << 3, 0, 0, 2, 0
static const uint8_t DX_INIT[DX_VOICE] = {
    DX_OP_SILENT, DX_OP_SILENT, DX_OP_SILENT, DX_OP_SILENT, DX_OP_SILENT,
    99, 99, 99, 99, 99, 99, 99, 0, 39, 0, 0, 0, 7 << 3, 0, 99, 2, 0,          /* OP1 */
    99, 99, 99, 99, 50, 50, 50, 50,                                            /* pitch envelope */
    0, 1 << 3, 35, 0, 0, 0, 1 | 3 << 4, 24,                                   /* ALG 1, OKS, LFO, PMS 3, C3 */
    'I', 'N', 'I', 'T', ' ', 'V', 'O', 'I', 'C', 'E',
};

/* a packed voice is usable: 7-bit bytes and a printable name. An erased slot (0xFF) or a zeroed one is
 * BLANK; an 8-bit byte or a control character in the name is damaged. Fields beyond their DX7 range are
 * NOT refused: real dumps carry 127s in rates, fine and LFO bytes (7 of the FM-1's own 128 factory voices
 * do) and a DX7, like Dexed, clamps them — so does every table index here. */
enum { DXV_OK, DXV_BLANK, DXV_BAD };
static uint32_t dx_voice_check(const uint8_t *p)
{
    uint32_t i, ff = 1, zz = 1;
    for (i = 0; i < DX_VOICE; i++) {
        ff &= p[i] == 0xFFu;
        zz &= p[i] == 0u;
    }
    if (ff || zz)
        return DXV_BLANK;
    for (i = 0; i < DX_VOICE; i++)
        if (p[i] > 127u || (i >= 118u && p[i] < 32u))
            return DXV_BAD;
    return DXV_OK;
}
static inline uint32_t dx99(uint32_t v) { return v > 99u ? 99u : v; }   /* a 0..99 field as the DX7 reads it */

static const uint8_t *dx_user_xip(void) { return DX_USER_XIP; }
static const uint8_t *dx_user_slot(uint32_t k) { return dx_user_xip() + (k % DX_USER_SLOTS) * DX_VOICE; }
static int dx_user_ok(uint32_t k) { return dx_voice_check(dx_user_slot(k)) == DXV_OK; }
static uint32_t dx_bank_used(uint32_t b)             /* voices in user bank b that are OK */
{
    uint32_t i, n = 0;
    for (i = 0; i < DX_BANK_N; i++)
        n += (uint32_t)dx_user_ok(b * DX_BANK_N + i);
    return n;
}

/* the voice slot k plays; INIT VOICE for an empty or damaged one */
static const uint8_t *dx_source(uint32_t slot) { return dx_user_ok(slot) ? dx_user_slot(slot) : DX_INIT; }

/* the loaded voices as a bitmap, for the PRESETS browser (ui.c): recomputed after a bank was written
 * (dx_gen), in the main loop — a scan of the 128 slots, never from the audio ISR */
static uint8_t dx_map[DX_USER_SLOTS / 8u];
static uint32_t dx_map_gen = 0xFFFFFFFFu;
static void dx_map_refresh(void)
{
    uint32_t k;
    if (dx_map_gen == dx_gen)
        return;
    for (k = 0; k < DX_USER_SLOTS; k++) {
        if (dx_user_ok(k))
            dx_map[k >> 3] |= (uint8_t)(1u << (k & 7u));
        else
            dx_map[k >> 3] &= (uint8_t)~(1u << (k & 7u));
    }
    dx_map_gen = dx_gen;
}
static int dx_loaded(uint32_t k) { dx_map_refresh(); return k < DX_USER_SLOTS && ((dx_map[k >> 3] >> (k & 7u)) & 1u); }
static uint32_t dx_count(void)                       /* loaded voices */
{
    uint32_t k, n = 0;
    for (k = 0; k < DX_USER_SLOTS; k++)
        n += (uint32_t)dx_loaded(k);
    return n;
}
static uint32_t dx_rank(uint32_t slot)               /* loaded voices before slot */
{
    uint32_t k, n = 0;
    for (k = 0; k < slot && k < DX_USER_SLOTS; k++)
        n += (uint32_t)dx_loaded(k);
    return n;
}
static uint32_t dx_nth(uint32_t n)                   /* slot of the n-th loaded voice (n < dx_count()) */
{
    uint32_t k;
    for (k = 0; k < DX_USER_SLOTS; k++)
        if (dx_loaded(k) && !n--)
            return k;
    return 0;
}
/* the name of slot k's voice (INIT VOICE when empty), trimmed; b holds 13 */
static void dx_slot_name(uint32_t k, char *b)
{
    const uint8_t *p = dx_source(k & 127u);
    uint32_t i, n = 10;
    while (n && p[118 + n - 1u] == ' ')
        n--;
    for (i = 0; i < n; i++)
        b[i] = p[118 + i] > 126u ? ' ' : (char)p[118 + i];
    b[i] = 0;
}

/* ---- Yamaha bulk dumps on the MIDI input (editor.c ed_dx_native), as a DX7 and the stock FM-1 take them:
 * a single voice (F0 43 0n 00 01 1B, 155 bytes VCED, checksum) or a 32-voice bank (F0 43 0n 09 20 00, 4096
 * bytes VMEM, checksum). p / n: the 7-bit bytes between F0 and F7. */
enum { DXS_NONE, DXS_VOICE, DXS_BANK, DXS_BAD };
static void dx_pack_vced(const uint8_t *d, uint8_t *out)   /* 155-byte single voice -> the 128-byte bank form */
{
    uint32_t j, i;
    for (j = 0; j < 6u; j++) {
        const uint8_t *s = d + j * 21u;
        uint8_t *o = out + j * 17u;
        for (i = 0; i < 11u; i++)
            o[i] = s[i];
        o[11] = (uint8_t)(s[11] | s[12] << 2);
        o[12] = (uint8_t)(s[13] | s[20] << 3);
        o[13] = (uint8_t)(s[14] | s[15] << 2);
        o[14] = s[16];
        o[15] = (uint8_t)(s[17] | s[18] << 1);
        o[16] = s[19];
    }
    for (i = 0; i < 8u; i++)
        out[102 + i] = d[126 + i];
    out[110] = d[134];
    out[111] = (uint8_t)(d[135] | d[136] << 3);
    for (i = 0; i < 4u; i++)
        out[112 + i] = d[137 + i];
    out[116] = (uint8_t)(d[141] | d[142] << 1 | d[143] << 4);
    out[117] = d[144];
    for (i = 0; i < 10u; i++)
        out[118 + i] = d[145 + i];
}
/* DXS_VOICE: *voice (128 bytes) filled; DXS_BANK: *bank points at the 4096 bytes in the frame; DXS_BAD: a
 * Yamaha dump whose checksum fails; DXS_NONE: not a DX7 dump */
static uint32_t dx_syx_parse(const uint8_t *p, uint32_t n, uint8_t *voice, const uint8_t **bank)
{
    uint32_t i, s = 0, len;
    if (n < 6u || p[0] != 0x43u || (p[1] & 0xF0u) != 0u)
        return DXS_NONE;
    if (p[2] == 0x00u && p[3] == 0x01u && p[4] == 0x1Bu && n == 161u)
        len = 155;
    else if (p[2] == 0x09u && p[3] == 0x20u && p[4] == 0x00u && n == 4102u)
        len = 4096;
    else
        return DXS_NONE;
    for (i = 0; i < len; i++)
        s += p[5 + i];
    if (((0u - s) & 0x7Fu) != p[5 + len])
        return DXS_BAD;
    if (len == 155u) {
        dx_pack_vced(p + 5, voice);
        return DXS_VOICE;
    }
    *bank = p + 5;
    return DXS_BANK;
}
/* a bank arrived: the screen asks "DX7 BANK n? SAVE=YES" and ui_input.c answers (SAVE: 1, any other
 * button: 2); editor.c writes or drops the staged bank. pending = bank + 1 (0: nothing asked) */
static struct { uint8_t pending, decided; uint32_t ms, eat; } dx_ask;   /* eat: the button bits that answered it */

/* ---------------------------------------------------------------- tables --- */
/* algorithm j of 32: per operator (OP6 first, as in the voice) a flag byte: bits 0-1 the bus it writes
 * (0 = the output), bit 2 add to the bus instead of replacing, bits 4-5 the bus it reads, bit 6 takes
 * the feedback, bit 7 writes the feedback (music-synthesizer-for-android fm_core.cc) */
static const uint8_t DX_ALG[32][6] = {
    {0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x11, 0x14, 0xc1, 0x14}, {0xc1, 0x11, 0x14, 0x01, 0x11, 0x14},
    {0x41, 0x11, 0x94, 0x01, 0x11, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}, {0x41, 0x94, 0x01, 0x14, 0x01, 0x14},
    {0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}, {0x01, 0x11, 0xc5, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x05, 0x14, 0xc1, 0x14},
    {0x01, 0x05, 0x14, 0xc1, 0x11, 0x14}, {0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x05, 0x14, 0xc1, 0x14},
    {0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}, {0xc1, 0x05, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x05, 0x11, 0x14, 0xc1, 0x14},
    {0xc1, 0x11, 0x02, 0x25, 0x05, 0x14}, {0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}, {0x01, 0x11, 0x11, 0xc5, 0x05, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x14}, {0x01, 0x14, 0x14, 0xc1, 0x14, 0x14},
    {0xc1, 0x14, 0x14, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}, {0xc1, 0x14, 0x14, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}, {0xc1, 0x05, 0x14, 0x01, 0x14, 0x04}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x04},
    {0x04, 0xc1, 0x11, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}, {0xc4, 0x04, 0x04, 0x04, 0x04, 0x04},
};
/* log2 of the COARSE ratio (0 = 0.5), Q24 */
static const int32_t DX_COARSE[32] = {
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600, 50331648, 53182516, 55732705,
    58039632, 60145690, 62083076, 63876816, 65546747, 67108864, 68576247, 69959732, 71268397, 72509921, 73690858,
    74816848, 75892776, 76922906, 77910978, 78860292, 79773775, 80654032, 81503396, 82323963, 83117622};
/* log2(1 + FINE / 100), Q24 */
static const uint32_t DX_FINE[100] = {
    0, 240842, 479311, 715453, 949314, 1180937, 1410364, 1637637, 1862796, 2085880, 2306926, 2525972, 2743054,
    2958205, 3171461, 3382855, 3592418, 3800183, 4006179, 4210437, 4412985, 4613853, 4813067, 5010655, 5206643,
    5401057, 5593922, 5785262, 5975102, 6163464, 6350371, 6535847, 6719911, 6902587, 7083894, 7263853, 7442485,
    7619807, 7795840, 7970602, 8144111, 8316385, 8487441, 8657298, 8825970, 8993475, 9159829, 9325048, 9489146,
    9652139, 9814042, 9974869, 10134635, 10293353, 10451037, 10607700, 10763356, 10918018, 11071697, 11224407,
    11376159, 11526966, 11676839, 11825789, 11973829, 12120969, 12267219, 12412591, 12557096, 12700743, 12843542,
    12985504, 13126637, 13266953, 13406460, 13545168, 13683085, 13820221, 13956584, 14092183, 14227027, 14361124,
    14494482, 14627109, 14759014, 14890203, 15020685, 15150468, 15279559, 15407964, 15535692, 15662750, 15789144,
    15914881, 16039969, 16164413, 16288221, 16411399, 16533954, 16655890};
static const uint8_t DX_PRATE[100] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47, 49, 51, 53, 54, 56,
    58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82, 85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141,
    147, 153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255};
static const int8_t DX_PLEVEL[100] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43, -41, -39, -37, -35, -33, -32, -31, -30, -29,
    -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5,
    -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73, 82, 92, 103, 115, 127};
static const uint8_t DX_VELOCITY[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163, 166, 170, 173, 174, 178, 181, 184,
    186, 189, 190, 194, 196, 198, 200, 202, 205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230,
    232, 233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251, 252, 253, 254};
static const uint8_t DX_EXPSCALE[33] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66, 80,
                                        94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250};
static const uint8_t DX_OLLUT[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
static const uint8_t DX_PMS[8] = {0, 10, 20, 33, 55, 92, 153, 255};
static const int32_t DX_AMS[4] = {0, 4342338, 7171437, 16777216};   /* Q24 */
static const uint16_t DX_CARNORM[7] = {8192, 8192, 5793, 4730, 4096, 3664, 3345};   /* 0.5 / sqrt(carriers), Q14: a carrier at
                                                                                        * level 99 (gain 2.0) is full scale */

#define DX_LFO_UNIT 18279u                   /* CTL x 2^32 / (FS x 15.5): the LFO's phase step scale */
#define DX_PENV_UNIT 572u                    /* CTL x 2^24 / (21.3 x FS): the pitch envelope's step scale */
#define DX_LOG10 557327                      /* log2(10) / 100, Q24 */
#define DX_HZ_INC 97392u                     /* 2^32 / FS */
#define DX_SEMI 1398101                      /* 2^24 / 12 */
#define DX_LOG_ONE (1 << 24)

static inline int32_t dx_scale_ol(uint32_t v) { return v > 99u ? 127 : v >= 20u ? 28 + (int32_t)v : DX_OLLUT[v]; }
/* 2^(x / 2^24): Q14 (x <= 1 << 24 for the gains) and Q16 (ratios up to 2^14) */
static inline int32_t dx_exp2_q14(int32_t x)
{
    int32_t e = x >> 24, m = EXP2_FRAC[(x >> 16) & 255];
    if (e >= 0)
        return m << e;
    e = -e;
    return e > 30 ? 0 : m >> e;
}
static inline uint32_t dx_exp2_q16(int32_t x)
{
    int32_t e = x >> 24;
    uint32_t m = (uint32_t)EXP2_FRAC[(x >> 16) & 255] << 2;
    if (e >= 0)
        return e > 14 ? 0x3FFFFFFFu : m << e;
    e = -e;
    return e > 30 ? 0 : m >> e;
}

/* ----------------------------------------------------------------- state --- */
typedef struct {                             /* one operator's envelope (env.cc) */
    uint32_t ph;
    int32_t level, target, inc;              /* level: the DX7's own log scale, Q16 */
    int32_t gain, gain0;                     /* Q14, now and at the last control tick */
    int16_t ol;                              /* output level after level scaling and velocity (x32) */
    uint8_t rs, ix, rising;
} dx_op_t;
typedef struct {
    dx_op_t op[6];
    int32_t fb0, fb1;                        /* feedback history (Q15, 1.0 = one cycle) */
    int32_t penv, ptarget, pinc;             /* pitch envelope, Q24 octaves */
    uint8_t pix, prising, down, quiet;       /* quiet: control ticks every carrier stayed silent */
} dx_voice_t;
typedef struct {                             /* a part: its voice and its LFO (lfo.cc) */
    uint8_t p[DX_VOICE];
    uint32_t key, gen;                       /* slot + 1 the copy was made for (0: none), and dx_gen then */
    uint32_t lfo_ph, lfo_delta, dly_st, dly_inc, dly_inc2;
    int32_t lfo, dly;                        /* this block's LFO (Q24, 0..1) and delay ramp (Q24) */
    uint8_t rnd;
    uint8_t ncar;                            /* carriers of the algorithm in use */
    const uint8_t *alg;
} dx_part_t;
static dx_voice_t dx_v[NPART][NVOICE];
static dx_part_t dx_t[NPART];
static dx_part_t *dx_part(const track_t *t) { return &dx_t[(uint32_t)(t - trk) % NPART]; }
static dx_voice_t *dx_voice(track_t *t, voice_t *v) { return &dx_v[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE]; }

static uint32_t dx_alg_index(const track_t *t, const uint8_t *p)   /* ALG macro 1..32 overrides the voice's */
{
    int32_t a = t->p[P_E6];
    return a > 0 ? (uint32_t)(a - 1) & 31u : p[110] & 31u;
}
static uint32_t dx_fb(const track_t *t, const uint8_t *p)
{
    int32_t f = t->p[P_E7];
    return f > 0 ? (uint32_t)(f - 1) & 7u : p[111] & 7u;
}

/* the part's voice: copied when PTCH changes or a bank was written; the LFO follows */
static void dx_part_load(track_t *t)
{
    dx_part_t *d = dx_part(t);
    uint32_t key = (uint32_t)t->p[P_E0] & 127u, sr, a;
    if (d->key == key + 1u && d->gen == dx_gen)
        return;
    memcpy(d->p, dx_source(key), DX_VOICE);
    d->key = key + 1u;
    d->gen = dx_gen;
    d->alg = DX_ALG[dx_alg_index(t, d->p)];              /* (dx7_block refreshes it; never 0 for a note before it) */
    sr = d->p[112] ? (165u * dx99(d->p[112])) >> 6 : 1u;   /* LFO speed */
    sr *= sr < 160u ? 11u : 11u + ((sr - 160u) >> 4);
    d->lfo_delta = DX_LFO_UNIT * sr;
    a = 99u - dx99(d->p[113]);                           /* LFO delay */
    if (a == 99u) {
        d->dly_inc = d->dly_inc2 = 0xFFFFFFFFu;
    } else {
        a = (16u + (a & 15u)) << (1u + (a >> 4));
        d->dly_inc = DX_LFO_UNIT * a;
        a &= 0xFF80u;
        d->dly_inc2 = DX_LFO_UNIT * (a < 0x80u ? 0x80u : a);
    }
}

static void dx7_block(track_t *t)                        /* once per block and part, before its voices */
{
    dx_part_t *d;
    uint32_t ph, j, n = 0;
    int32_t x;
    dx_part_load(t);
    d = dx_part(t);
    d->alg = DX_ALG[dx_alg_index(t, d->p)];
    for (j = 0; j < 6u; j++)
        n += (d->alg[j] & 3u) == 0u;
    d->ncar = (uint8_t)n;
    ph = d->lfo_ph += d->lfo_delta;
    switch ((d->p[116] >> 1) & 7u) {                      /* LFO wave: 0..1 in Q24 */
    case 0:
        x = (int32_t)(ph >> 7);
        x ^= -(int32_t)(ph >> 31);
        x &= DX_LOG_ONE - 1;
        break;
    case 1:
        x = (int32_t)((~ph ^ 0x80000000u) >> 8);
        break;
    case 2:
        x = (int32_t)((ph ^ 0x80000000u) >> 8);
        break;
    case 3:
        x = (int32_t)(((~ph) >> 7) & DX_LOG_ONE);
        break;
    case 4:
        x = (1 << 23) + (sine_i(ph) << 8);
        break;
    case 5:
        if (ph < d->lfo_delta)
            d->rnd = (uint8_t)(d->rnd * 179u + 17u);
        x = ((int32_t)(d->rnd ^ 0x80u) + 1) << 16;
        break;
    default:
        x = 1 << 23;
        break;
    }
    d->lfo = x;
    {   /* the delay ramp */
        uint32_t delta = d->dly_st < 0x80000000u ? d->dly_inc : d->dly_inc2, s = d->dly_st + delta;
        if (s < d->dly_inc) {
            d->dly = DX_LOG_ONE;                          /* wrapped: fully in */
        } else {
            d->dly_st = s;
            d->dly = s < 0x80000000u ? 0 : (int32_t)((s >> 7) & (DX_LOG_ONE - 1));
        }
    }
}

/* ------------------------------------------------------------- envelopes --- */
static void dx_env_advance(const track_t *t, dx_op_t *e, const uint8_t *o, uint32_t ix)
{
    e->ix = (uint8_t)ix;
    if (ix < 4u) {
        int32_t act = ((dx_scale_ol(o[4 + ix]) >> 1) << 6) + e->ol - 4256, qr;
        if (act < 16)
            act = 16;
        e->target = act << 16;
        e->rising = e->target > e->level;
        qr = ((int32_t)o[ix] * 41) >> 6;
        qr += e->rs + (t->p[P_E3] >> 3);                  /* ENVS: -8 .. +7 quarter-octave steps of speed */
        qr = clamp(qr, 0, 63);
        e->inc = (4 + (qr & 3)) << (2 + CTL_LOG2 + (qr >> 2));
    }
}
static void dx_env_tick(const track_t *t, dx_op_t *e, const uint8_t *o, int down)
{
    if (e->ix < 3u || (e->ix < 4u && !down)) {
        if (e->rising) {
            if (e->level < (1716 << 16))
                e->level = 1716 << 16;
            e->level += (((17 << 24) - e->level) >> 24) * e->inc;
            if (e->level >= e->target) {
                e->level = e->target;
                dx_env_advance(t, e, o, e->ix + 1u);
            }
        } else {
            e->level -= e->inc;
            if (e->level <= e->target) {
                e->level = e->target;
                dx_env_advance(t, e, o, e->ix + 1u);
            }
        }
    }
}
static void dx_penv_advance(dx_voice_t *s, const uint8_t *p, uint32_t ix)
{
    s->pix = (uint8_t)ix;
    if (ix < 4u) {
        s->ptarget = (int32_t)DX_PLEVEL[dx99(p[106 + ix])] << 19;
        s->prising = s->ptarget > s->penv;
        s->pinc = (int32_t)DX_PRATE[dx99(p[102 + ix])] * (int32_t)DX_PENV_UNIT;
    }
}
static void dx_penv_tick(dx_voice_t *s, const uint8_t *p)
{
    if (s->pix < 3u || (s->pix < 4u && !s->down)) {
        if (s->prising) {
            s->penv += s->pinc;
            if (s->penv >= s->ptarget) {
                s->penv = s->ptarget;
                dx_penv_advance(s, p, s->pix + 1u);
            }
        } else {
            s->penv -= s->pinc;
            if (s->penv <= s->ptarget) {
                s->penv = s->ptarget;
                dx_penv_advance(s, p, s->pix + 1u);
            }
        }
    }
}

/* keyboard level scaling (dx7note.cc ScaleLevel / ScaleCurve), in output-level units */
static int32_t dx_scale_curve(int32_t group, int32_t depth, uint32_t curve)
{
    int32_t s;
    if (curve == 0u || curve == 3u)
        s = (group * depth * 329) >> 12;                  /* linear */
    else
        s = ((int32_t)DX_EXPSCALE[group > 32 ? 32 : group] * depth * 329) >> 15;   /* exponential */
    return curve < 2u ? -s : s;
}

static void dx7_note_on(track_t *t, voice_t *v)
{
    dx_part_t *d;
    dx_voice_t *s = dx_voice(t, v);
    const uint8_t *p;
    uint32_t j, others = 0;
    int32_t vel = (int32_t)DX_VELOCITY[(v->vel & 127u) >> 1] - 239;
    dx_part_load(t);                                      /* (a note before the first block of the part) */
    d = dx_part(t);
    p = d->p;
    for (j = 0; j < NVOICE; j++)
        others |= (uint32_t)(&t->v[j] != v && t->v[j].active && t->v[j].gate);
    if (!others) {                                        /* the first key of a phrase: LFO key sync and delay */
        if (p[116] & 1u)
            d->lfo_ph = 0x7FFFFFFFu;
        d->dly_st = 0;
    }
    for (j = 0; j < 6u; j++) {
        dx_op_t *e = &s->op[j];
        const uint8_t *o = p + j * 17u;
        int32_t ol = dx_scale_ol(o[14]), off = (int32_t)v->note - (int32_t)dx99(o[8]) - 17;
        ol += off >= 0 ? dx_scale_curve(off / 3, (int32_t)dx99(o[10]), (o[11] >> 2) & 3u)
                       : dx_scale_curve(-off / 3, (int32_t)dx99(o[9]), o[11] & 3u);
        if (ol > 127)
            ol = 127;
        ol <<= 5;
        ol += ((((int32_t)(o[13] >> 2) & 7) * vel + 7) >> 3) << 4;   /* velocity sensitivity */
        e->ol = (int16_t)clamp(ol, 0, 4095);
        e->rs = (uint8_t)(((int32_t)(o[12] & 7u) * clamp((int32_t)v->note / 3 - 7, 0, 31)) >> 3);
        if (p[111] & 8u)                                  /* operator key sync */
            e->ph = 0;
        e->level = 0;
        e->gain = e->gain0 = 0;
        dx_env_advance(t, e, o, 0);
    }
    s->fb0 = s->fb1 = 0;
    s->down = 1;
    s->quiet = 0;
    s->penv = (int32_t)DX_PLEVEL[dx99(p[109])] << 19;
    dx_penv_advance(s, p, 0);
}

/* control rate: the operator envelopes, the LFO's amplitude modulation, the pitch envelope. Returns
 * Felucca's ADSR value (a VCA in front); frees the voice when every carrier has finished. */
static int32_t dx7_amp(track_t *t, voice_t *v, int32_t adsr)
{
    dx_part_t *d = dx_part(t);
    dx_voice_t *s = dx_voice(t, v);
    const uint8_t *p = d->p;
    uint32_t j, alive = 0;
    int32_t amd = ((int32_t)dx99(p[115]) * 165) >> 6, amod, bright = dx_exp2_q14((int32_t)t->p[P_E2] << 19);
    if (!v->gate && s->down) {                            /* key up: every envelope to its release */
        s->down = 0;
        for (j = 0; j < 6u; j++)
            dx_env_advance(t, &s->op[j], p + j * 17u, 3);
        dx_penv_advance(s, p, 3);
    }
    amod = (int32_t)(((int64_t)amd * d->dly) >> 8);       /* Q24 */
    amod = (int32_t)(((int64_t)amod * (DX_LOG_ONE - d->lfo)) >> 24);
    for (j = 0; j < 6u; j++) {
        dx_op_t *e = &s->op[j];
        const uint8_t *o = p + j * 17u;
        int32_t level, g;
        dx_env_tick(t, e, o, s->down);
        level = e->level - (int32_t)(((int64_t)DX_AMS[o[13] & 3u] * amod) >> 24);
        g = dx_exp2_q14(level - (14 << 24));
        if (g > 65536)
            g = 65536;
        if ((d->alg[j] & 3u) != 0u)                       /* a modulator: BRGT scales it */
            g = (int32_t)(((int64_t)g * bright) >> 14);
        else if (e->ix < 4u || g > 8)
            alive = 1;
        e->gain0 = e->gain;
        e->gain = g;
    }
    dx_penv_tick(s, p);
    if (!s->down) {
        s->quiet = alive ? 0u : (uint8_t)(s->quiet < 255u ? s->quiet + 1u : 255u);
        if (s->quiet >= 4u) {                             /* released and silent: the voice is free */
            v->active = 0;
            v->stage = 0;
            v->env = 0;
            return 0;
        }
    }
    return adsr;
}

static void dx7_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    dx_part_t *d = dx_part(t);
    dx_voice_t *s = dx_voice(t, v);
    const uint8_t *p = d->p, *alg = d->alg;
    uint32_t inc[6], j, i, fbs = dx_fb(t, p);
    int32_t gstep[6], g[6], pm_lfo, base, norm = DX_CARNORM[d->ncar], fb0 = s->fb0, fb1 = s->fb1;
    fbs = fbs ? 8u - fbs : 16u;
    {   /* the pitch of this block: transpose, TRN, the pitch envelope, the LFO (Q24 octaves) */
        int32_t pmd = clamp((int32_t)p[114] + t->p[P_E4], 0, 99), sens = DX_PMS[(p[116] >> 4) & 7u];
        pmd = (int32_t)(((int64_t)((pmd * 165) >> 6) * d->dly) >> 8);   /* Q24 (255 x 2^24 needs 64 bits) */
        pm_lfo = (int32_t)(((int64_t)pmd * (sens * (d->lfo - (1 << 23)))) >> 31);
        base = ((int32_t)(p[117] > 48u ? 48u : p[117]) - 24 + t->p[P_E5]) * DX_SEMI + s->penv + pm_lfo;
    }
    for (j = 0; j < 6u; j++) {
        const uint8_t *o = p + j * 17u;
        uint32_t coarse = (o[15] >> 1) & 31u, k;
        int32_t det = (int32_t)(o[12] >> 3) - 7, g0 = s->op[j].gain0, g1 = s->op[j].gain;
        if ((alg[j] & 3u) == 0u) {                        /* a carrier: the output scale in its gain */
            g0 = (g0 * norm) >> 14;
            g1 = (g1 * norm) >> 14;
        }
        if (o[15] & 1u) {                                 /* fixed frequency: 10^(coarse%4 + fine/100) Hz */
            uint32_t hz16 = dx_exp2_q16((int32_t)(((coarse & 3u) * 100u + dx99(o[16])) * DX_LOG10) + (det > 0 ? det * 13457 : 0));
            k = (uint32_t)(((uint64_t)hz16 * DX_HZ_INC) >> 16);
        } else {
            int32_t lg = base + DX_COARSE[coarse] + (int32_t)DX_FINE[dx99(o[16])] + det * 12606;   /* 7.2 Hz a step at 9.6 kHz */
            k = (uint32_t)(((uint64_t)m->inc * dx_exp2_q16(lg)) >> 16);
        }
        inc[j] = k > 0x73000000u ? 0x73000000u : k;
        g[j] = g0;
        gstep[j] = (g1 - g0) / (int32_t)n;
    }
    for (i = 0; i < n; i++) {
        int32_t bus[3] = {0, 0, 0}, fbnew = fb0, sum;
        for (j = 0; j < 6u; j++) {
            uint32_t f = alg[j], ib = (f >> 4) & 3u, ob = f & 3u;
            int32_t pm = ib ? bus[ib] : 0, y;
            if (f & 0x40u)
                pm += (fb0 + fb1) >> (fbs + 1u);
            y = (sine_i(s->op[j].ph + ((uint32_t)pm << 17)) * g[j]) >> 14;   /* Q15, up to +-2.0 */
            s->op[j].ph += inc[j];
            g[j] += gstep[j];
            if (f & 0x80u)
                fbnew = y;
            if (f & 4u)
                bus[ob] += y;
            else
                bus[ob] = y;
        }
        fb1 = fb0;
        fb0 = fbnew;
        sum = clamp(bus[0], -32767, 32767);                /* (the carriers' gains carry the scale) */
        out[i] += mulq15(mulq15(sum, amp_at(m, i)), VOICE_FS);
    }
    s->fb0 = fb0;
    s->fb1 = fb1;
}

/* the sound's name on the screen: the voice's own (ui_draw.c trk_short_name) */
static const char *dx7_sound(const track_t *t, char *b)
{
    dx_slot_name((uint32_t)t->p[P_E0] & 127u, b);
    return b;
}

static const preset_t DX7_PRESETS[] = {
    /* PTCH - BRGT ENVS LFOD TRN ALG FB ; the ADSR open: the voice's envelopes shape the sound */
    {"DX7", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 127, 127, 110}, 0, 0, FX(0, 0, 0, 20)},
};

static const engine_t ENG_DX7 = {
    "DX7", {"PATCH", "TONE"},
    {
        {"PTCH", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"BRGT", F_BIPCT, -64, 63, 0, 0, 0},
        {"ENVS", F_BIPCT, -64, 63, 0, 0, 0},
        {"LFOD", F_BIPCT, -64, 63, 0, 0, 0},
        {"TRN", F_SEMI, -24, 24, 0, 0, 0},
        {"ALG", F_INT, 0, 32, 0, 0, 0},
        {"FB", F_INT, 0, 8, 0, 0, 0},
    },
    DX7_PRESETS, sizeof(DX7_PRESETS) / sizeof(DX7_PRESETS[0]), -1, dx7_note_on, dx7_render,
    0x9DBF, {P_E0, P_E2, P_E3, P_REL}, 0, dx7_amp, 0, dx7_block, dx7_sound,
};
