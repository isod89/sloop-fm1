/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Copyright (C) 2007 Shay Green (snes_spc 0.9.0, SPC_DSP.cpp: the accurate S-DSP emulator)
 * C port for Felucca (one voice and one echo channel at a time, for an engine that runs the chip per voice),
 * 2026: ugotworms
 *
 * This module is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser
 * General Public License as published by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version. This module is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this module (LICENSES/LGPL-2.1-snes_spc.txt); if not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */
/* The S-DSP, the Super Nintendo's sound chip, as the SNES engine (eng_snes.c) uses it: a voice's BRR decoding,
 * its 4-point Gaussian interpolation, its ADSR and GAIN envelopes on the chip's shared rate counter, the noise
 * generator, and the echo with its 8-tap FIR, bit for bit at the chip's 32 kHz (tests/snes_test.c holds every
 * part against SPC_DSP.cpp). The sample memory is the caller's: a voice reads its sample from `ram` at the
 * directory entry (start, loop) the caller gives it at key-on (the chip's DIR and SRCN). */
#define SNES_CTR_RANGE 30720            /* the rate counter's period (2048 * 5 * 3) */
enum { SE_RELEASE, SE_ATTACK, SE_DECAY, SE_SUSTAIN };   /* the order of SPC_DSP::env_mode_t */

typedef struct {                        /* one S-DSP voice (SPC_DSP::voice_t and its registers) */
    int16_t buf[24];                    /* 12 decoded samples, twice (reads never wrap) */
    const uint8_t *ram;                 /* the sample memory; the directory entry: byte offsets in it */
    uint32_t start, loop;
    uint32_t brr_addr;                  /* the block being decoded */
    int32_t interp_pos;                 /* 0x1000 = one sample */
    int16_t env, hidden_env;            /* 0..0x7FF */
    uint8_t buf_pos, brr_offset, kon_delay, env_mode;
    uint8_t adsr0, adsr1, gain;         /* the voice's registers */
} snes_chv_t;

typedef struct {                        /* the echo (SPC_DSP echo_22 .. echo_30, one channel) */
    int16_t hist[16];                   /* the FIR's last 8 inputs, twice (reads never wrap) */
    uint8_t hpos;
    uint16_t off, len;                  /* the line's position and length (EDL x 512, taken at position 0) */
    int16_t in, wr;                     /* the last FIR output and the last sample written (idle detection) */
} snes_echo_t;

/* the hardware's Gaussian kernel (SPC_DSP.cpp gauss[]) */
static const int16_t SNES_GAUSS[512] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,
       2,    2,    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,    5,    5,    5,    5,
       6,    6,    6,    6,    7,    7,    7,    8,    8,    8,    9,    9,    9,   10,   10,   10,
      11,   11,   11,   12,   12,   13,   13,   14,   14,   15,   15,   15,   16,   16,   17,   17,
      18,   19,   19,   20,   20,   21,   21,   22,   23,   23,   24,   24,   25,   26,   27,   27,
      28,   29,   29,   30,   31,   32,   32,   33,   34,   35,   36,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,
      58,   59,   60,   61,   62,   64,   65,   66,   67,   69,   70,   71,   73,   74,   76,   77,
      78,   80,   81,   83,   84,   86,   87,   89,   90,   92,   94,   95,   97,   99,  100,  102,
     104,  106,  107,  109,  111,  113,  115,  117,  118,  120,  122,  124,  126,  128,  130,  132,
     134,  137,  139,  141,  143,  145,  147,  150,  152,  154,  156,  159,  161,  163,  166,  168,
     171,  173,  175,  178,  180,  183,  186,  188,  191,  193,  196,  199,  201,  204,  207,  210,
     212,  215,  218,  221,  224,  227,  230,  233,  236,  239,  242,  245,  248,  251,  254,  257,
     260,  263,  267,  270,  273,  276,  280,  283,  286,  290,  293,  297,  300,  304,  307,  311,
     314,  318,  321,  325,  328,  332,  336,  339,  343,  347,  351,  354,  358,  362,  366,  370,
     374,  378,  381,  385,  389,  393,  397,  401,  405,  410,  414,  418,  422,  426,  430,  434,
     439,  443,  447,  451,  456,  460,  464,  469,  473,  477,  482,  486,  491,  495,  499,  504,
     508,  513,  517,  522,  527,  531,  536,  540,  545,  550,  554,  559,  563,  568,  573,  577,
     582,  587,  592,  596,  601,  606,  611,  615,  620,  625,  630,  635,  640,  644,  649,  654,
     659,  664,  669,  674,  678,  683,  688,  693,  698,  703,  708,  713,  718,  723,  728,  732,
     737,  742,  747,  752,  757,  762,  767,  772,  777,  782,  787,  792,  797,  802,  806,  811,
     816,  821,  826,  831,  836,  841,  846,  851,  855,  860,  865,  870,  875,  880,  884,  889,
     894,  899,  904,  908,  913,  918,  923,  927,  932,  937,  941,  946,  951,  955,  960,  965,
     969,  974,  978,  983,  988,  992,  997, 1001, 1005, 1010, 1014, 1019, 1023, 1027, 1032, 1036,
    1040, 1045, 1049, 1053, 1057, 1061, 1066, 1070, 1074, 1078, 1082, 1086, 1090, 1094, 1098, 1102,
    1106, 1109, 1113, 1117, 1121, 1125, 1128, 1132, 1136, 1139, 1143, 1146, 1150, 1153, 1157, 1160,
    1164, 1167, 1170, 1174, 1177, 1180, 1183, 1186, 1190, 1193, 1196, 1199, 1202, 1205, 1207, 1210,
    1213, 1216, 1219, 1221, 1224, 1227, 1229, 1232, 1234, 1237, 1239, 1241, 1244, 1246, 1248, 1251,
    1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269, 1270, 1272, 1274, 1275, 1277, 1279, 1280,
    1282, 1283, 1284, 1286, 1287, 1288, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1297, 1298,
    1299, 1300, 1300, 1301, 1302, 1302, 1303, 1303, 1303, 1304, 1304, 1304, 1304, 1304, 1305, 1305,
};

/* the rate counter: rate r (0..31) fires every SNES_CTR_RATE[r] chip samples, rate 0 never */
static const uint16_t SNES_CTR_RATE[32] = {
    SNES_CTR_RANGE + 1, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1};
static const uint16_t SNES_CTR_OFS[32] = {
    1, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536,
    0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 0, 0};
static inline uint32_t snes_ctr(int32_t counter, uint32_t rate)   /* 0: rate fires on this sample */
{
    return ((uint32_t)counter + SNES_CTR_OFS[rate]) % SNES_CTR_RATE[rate];
}
/* the rates that fire (bit r) at the rate counter's value `counter`: SPC_DSP read_counter as it is. The
 * engine counts each rate down instead (eng_snes.c snes_block: the counter's period is a multiple of
 * every rate), tests/snes_test.c holds the two together */
static uint32_t snes_fire_of(int32_t counter)
{
    uint32_t r, m = 0;
    for (r = 1; r < 32u; r++)
        m |= (uint32_t)!snes_ctr(counter, r) << r;
    return m;
}

static inline int32_t snes_clamp16(int32_t x) { return (int16_t)x != x ? (x >> 31) ^ 0x7FFF : x; }

static inline void snes_noise_step(int32_t *noise)     /* SPC_DSP misc_30: the noise, when its rate fires */
{
    int32_t fb = (*noise << 13) ^ (*noise << 14);
    *noise = (fb & 0x4000) ^ (*noise >> 1);
}

static inline int32_t snes_interp(const snes_chv_t *v)   /* SPC_DSP::interpolate */
{
    uint32_t off = (uint32_t)v->interp_pos >> 4 & 0xFFu;
    const int16_t *fwd = SNES_GAUSS + 255 - off, *rev = SNES_GAUSS + off;
    const int16_t *in = &v->buf[(v->interp_pos >> 12) + v->buf_pos];
    int32_t o = (fwd[0] * in[0]) >> 11;
    o += (fwd[256] * in[1]) >> 11;
    o += (rev[256] * in[2]) >> 11;
    o = (int16_t)o;
    o += (rev[0] * in[3]) >> 11;
    return snes_clamp16(o) & ~1;
}

static void snes_envelope(snes_chv_t *v, uint32_t fire)     /* SPC_DSP::run_envelope */
{
    int32_t env = v->env, rate, data = v->adsr1;
    if (v->env_mode == SE_RELEASE) {
        env -= 0x8;
        v->env = (int16_t)(env < 0 ? 0 : env);
        return;
    }
    if (v->adsr0 & 0x80) {                              /* ADSR */
        if (v->env_mode >= SE_DECAY) {
            env--;
            env -= env >> 8;
            rate = data & 0x1F;
            if (v->env_mode == SE_DECAY)
                rate = (v->adsr0 >> 3 & 0x0E) + 0x10;
        } else {
            rate = (v->adsr0 & 0x0F) * 2 + 1;
            env += rate < 31 ? 0x20 : 0x400;
        }
    } else {                                            /* GAIN */
        int32_t mode;
        data = v->gain;
        mode = data >> 5;
        if (mode < 4) {                                 /* direct */
            env = data * 0x10;
            rate = 31;
        } else {
            rate = data & 0x1F;
            if (mode == 4)                              /* linear decrease */
                env -= 0x20;
            else if (mode < 6) {                        /* exponential decrease */
                env--;
                env -= env >> 8;
            } else {                                    /* linear increase; 7: two slopes */
                env += 0x20;
                if (mode > 6 && (uint32_t)v->hidden_env >= 0x600u)
                    env += 0x8 - 0x20;
            }
        }
    }
    if ((env >> 8) == (data >> 5) && v->env_mode == SE_DECAY)   /* sustain level */
        v->env_mode = SE_SUSTAIN;
    v->hidden_env = (int16_t)env;
    if ((uint32_t)env > 0x7FFu) {                       /* (a linear decrease below 0 too) */
        env = env < 0 ? 0 : 0x7FF;
        if (v->env_mode == SE_ATTACK)
            v->env_mode = SE_DECAY;
    }
    if (fire >> rate & 1u)                              /* nothing else waits for the counter */
        v->env = (int16_t)env;
}

static void snes_decode(snes_chv_t *v, int32_t header, int32_t byte)   /* SPC_DSP::decode_brr */
{
    int32_t nyb = byte * 0x100 + v->ram[v->brr_addr + v->brr_offset + 1u];
    int32_t shift = header >> 4, filter = header & 0x0C;
    int16_t *pos = &v->buf[v->buf_pos], *end = pos + 4;
    v->buf_pos = (uint8_t)(v->buf_pos + 4u >= 12u ? 0u : v->buf_pos + 4u);
    for (; pos < end; pos++, nyb <<= 4) {
        int32_t s = (int16_t)nyb >> 12, p1 = pos[11], p2 = pos[10] >> 1;
        s = (s * (1 << shift)) >> 1;
        if (shift >= 0xD)                               /* invalid shifts */
            s = s < 0 ? -0x800 : 0;
        if (filter >= 8) {
            s += p1;
            s -= p2;
            if (filter == 8) {                          /* p1 * 0.953125 - p2 * 0.46875 */
                s += p2 >> 4;
                s += (p1 * -3) >> 6;
            } else {                                    /* p1 * 0.8984375 - p2 * 0.40625 */
                s += (p1 * -13) >> 7;
                s += (p2 * 3) >> 4;
            }
        } else if (filter) {                            /* p1 * 0.46875 */
            s += p1 >> 1;
            s += (-p1) >> 5;
        }
        s = (int16_t)(snes_clamp16(s) * 2);
        pos[12] = pos[0] = (int16_t)s;
    }
}

/* one chip sample of a voice (SPC_DSP voice_V2 .. V4 in their order); returns its output before the
 * volume. fire: the rates that fire on it (snes_fire_of); noise: the noise register; pitch: the 14-bit
 * register; non: NON (noise instead of the sample); koff: a key-off now */
static int32_t snes_tick(snes_chv_t *v, uint32_t fire, int32_t noise, uint32_t pitch, int non, int koff)
{
    uint32_t next = v->kon_delay ? v->start : v->loop; /* V2: the directory entry */
    int32_t header = v->ram[v->brr_addr], byte = v->ram[v->brr_addr + v->brr_offset], out;   /* V3b */
    if (v->kon_delay) {                                 /* V3c: key-on, 5 samples */
        if (v->kon_delay == 5u) {
            v->brr_addr = next;
            v->brr_offset = 1;
            v->buf_pos = 0;
            header = 0;                                 /* ignored on this sample */
        }
        v->env = v->hidden_env = 0;
        v->interp_pos = (--v->kon_delay & 3u) ? 0x4000 : 0;   /* decode in the last three */
        pitch = 0;
    }
    out = non ? (int16_t)(noise * 2) : snes_interp(v);
    out = (out * v->env) >> 11 & ~1;
    if ((header & 3) == 1) {                            /* the end of a sample that does not loop */
        v->env_mode = SE_RELEASE;
        v->env = 0;
    }
    if (koff)
        v->env_mode = SE_RELEASE;
    if (!v->kon_delay)
        snes_envelope(v, fire);
    if (v->interp_pos >= 0x4000) {                      /* V4: four more samples */
        snes_decode(v, header, byte);
        if ((v->brr_offset += 2u) >= 9u) {
            v->brr_addr = (header & 1) ? next : v->brr_addr + 9u;
            v->brr_offset = 1;
        }
    }
    v->interp_pos = (v->interp_pos & 0x3FFF) + (int32_t)pitch;
    if (v->interp_pos > 0x7FFF)
        v->interp_pos = 0x7FFF;
    return out;
}

/* one chip sample of the echo (SPC_DSP echo_22 .. echo_30, one channel): read the line into the FIR (C0 on
 * the oldest of its 8 samples), feed the voices' sum (ein, EON) plus the FIR's output at EFB back into the
 * same place, step on. Returns the FIR's output at EVOL (what the DAC adds); buf: the line; edl: 0..15
 * (taken when the position comes round to 0); wr: FLG's echo write enable */
static int32_t snes_echo_tick(snes_echo_t *e, int16_t *buf, int32_t ein, int32_t evol, int32_t efb, const int8_t *fir,
                              uint32_t edl, int wr)
{
    int16_t *h;
    int32_t l, w;
    uint32_t off = e->off;
    e->hpos = (uint8_t)((e->hpos + 1u) & 7u);
    h = &e->hist[e->hpos];
    h[0] = h[8] = (int16_t)(buf[off] >> 1);
#define SNES_FIR(i) ((h[(i) + 1] * fir[i]) >> 6)
    l = SNES_FIR(0) + SNES_FIR(1) + SNES_FIR(2) + SNES_FIR(3) + SNES_FIR(4) + SNES_FIR(5) + SNES_FIR(6);
    l = (int16_t)l;
    l += (int16_t)SNES_FIR(7);
#undef SNES_FIR
    l = snes_clamp16(l) & ~1;
    w = snes_clamp16(ein + (int16_t)((l * efb) >> 7)) & ~1;
    if (!off)
        e->len = (uint16_t)(edl * 512u);
    if (++off >= e->len)
        off = 0;
    if (wr)
        buf[e->off] = (int16_t)w;
    e->off = (uint16_t)off;
    e->in = (int16_t)l;
    e->wr = (int16_t)(wr ? w : 0);
    return (int16_t)((l * evol) >> 7);
}
