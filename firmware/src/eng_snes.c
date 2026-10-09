/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ugotworms */
/* SNES: an engine of the S-DSP, the Super Nintendo's sound chip (snes_dsp.c, ported from snes_spc). Each part is
 * one S-DSP: its voices share the rate counter and the noise as on the chip, and it has its echo. The chip runs at
 * its 32 kHz; its samples are linearly interpolated to 44.1 kHz.
 * The keys drive the chip as a game's sound driver did: the pitch register from the note (14 bits: at most two
 * octaves above an instrument's root, and coarse in the bass, as on the hardware), the voice volume from the
 * velocity, ATK DEC SUS into ADSR (AR, DR, SL), SR its sustain rate. REL 0 is the chip's key-off (a linear fade
 * of 8 ms); above it a key-off switches the voice to GAIN's exponential decrease at that rate, the drivers'
 * longer release. A kit's pieces play to their end. The echo: one per part, mono, at most 96 ms (it lives in
 * the part's engine arena: snes_eline).
 *
 * The instruments: INST 0 .. SNES_NFW - 1 are built in (felucca_brr.h: a few single-cycle waves, tools/gen_brr.py);
 * the rest come from a bank in the user sample slots (snes_bank_scan: the CC0 instruments, the kits, your own .spc
 * and .brr samples; tools/gen_brr.py --bank, uploaded by tools/fm1_sample_upload.py snes). With no bank those INST
 * values are silent, and the INST knob stops at the built-in ones.
 * Not here: pitch modulation, the voices' stereo volumes (a part is mono). */
#include "snes_dsp.c"                   /* the chip (LGPL-2.1-or-later) */
#define SNES_PRESETS                    /* (felucca_brr.h: the preset table too) */
#include "felucca_brr.h"                /* tools/gen_brr.py: the built-in waves, every preset */

#define SNES_RATE_Q16 47555u            /* 32000 / 44100, Q16: chip samples per output sample */
#define SNES_ONE (1u << 16)             /* one chip sample in that phase, Q16 */
/* the voice volume register: the velocity / 4 (0..32), as drivers kept it low: the chip sums the voices into
 * the echo in 16 bits, and 8 voices at full volume clip it hard (a crunch on every chord). The mix (32 bits)
 * makes the level up after the chip */
#define SNES_VOL_SH 2
#define SNES_NSEL_MAX 127u              /* INST values: a preset keeps INST in a signed byte */

typedef struct {                        /* a voice of the engine: the chip's, and what the driver keeps of it */
    snes_chv_t c;
    int16_t root16;                     /* the note of pitch 0x1000 of its sample, 1/16 st */
    uint8_t down;                       /* the key held (the driver's view) */
    uint8_t kit;                        /* 1: a kit's piece, at its own pitch (+ ktune) whatever the key */
    int8_t ktune;
    uint8_t bank;                       /* 1: it plays from the bank in the user slots */
    int16_t o0, o1;                     /* the last two chip samples (to 44.1 kHz) */
} snes_voice_t;

#define SNES_TICKS 24u                  /* chip samples in a block of CTL, at most (32 x 0.7256 = 23.2) */
typedef struct {                        /* a part's S-DSP: what its voices share */
    int32_t noise;                      /* the noise shift register */
    int16_t rc[32];                     /* the rate counter, as one countdown per rate: 0 = it fires */
    uint32_t ph, ph0;                   /* the 32 kHz clock against the output's, Q16; .. as the block began */
    uint32_t fire[SNES_TICKS];          /* this block's chip samples: the rates that fire (bit r), */
    int16_t nz[SNES_TICKS];             /* .. the noise; the voices read them in step */
    int16_t ein[SNES_TICKS];            /* .. and add their outputs here: the echo's input */
    snes_echo_t ec;
    int16_t e0, e1;                     /* the echo's last two outputs (to 44.1 kHz) */
    uint16_t quiet;                     /* chip samples the echo has stayed flat (its FIR output and what it */
    int16_t qlo[2], qhi[2];             /* writes within 8 of a level, the range in qlo .. qhi) */
    uint8_t init, eown, edirty;         /* the echo line is ours and cleared; it holds sound */
} snes_dsp_t;

static snes_voice_t snes_v[NPART][NVOICE];   /* (not the pool: tools/build.py keeps 8 KiB of it spare) */
static snes_dsp_t snes_dsp[NPART];
static uint32_t snes_eclips;            /* chip samples the echo's input clipped (16 bits, as on the chip): tests */

/* the echo line: the part's engine arena (engines.c eng_arena_of: a part plays one engine at a time), 6 KB: up to
 * EDL 6 (96 ms; the chip's 15, 240 ms, would need 15 KB a part). It fits in GRAIN's share of the arena, so the
 * pool stays as it was */
#define SNES_EDL_MAX 6u
#define SNES_ELINE (SNES_EDL_MAX * 512u)
static int16_t *snes_eline(uint32_t part) { return (int16_t *)eng_arena_of(&trk[part % NPART], ENGI_SNES); }


/* ---------------------------------------------------- the bank in the user slots --- */
/* A bank is one to four user slots (eng_sample.c), each a part: the slot's header as a sample set's (the same
 * size, data_len and crc at the same places, the data at +SMP_USER_DATA) with its own magic; part 0's data starts
 * with the directory. A sample lies within its part, so the parts need not be neighbours. Built by
 * tools/gen_brr.py --bank; the editor's SMP_* commands write it (editor.c ed_smp_end takes either magic) */
#define SNES_SLOT_MAGIC 0x52524246u     /* "FBRR" */
typedef struct {
    uint32_t magic;
    uint16_t version;                   /* 1 */
    uint8_t part, nparts;               /* this slot's part of the bank, of how many (1 .. 4) */
    char name[8];
    uint32_t data_len, crc;             /* as smp_user_hdr_t: the CRC (zlib) of the data, checked at the upload */
    uint32_t bank;                      /* the bank's id: the same in each of its parts */
    uint32_t rsv;
} snes_slot_hdr_t;
typedef struct {                        /* part 0's data at 0 */
    uint16_t nsel, nsmp, nkit, rsv;     /* INST values (SNES_NFW + k), samples, kits */
    uint32_t sel_off, smp_off, kit_off; /* byte offsets in part 0's data */
} snes_dir_t;
typedef struct { char name[8]; uint8_t kind, idx, rsv[2]; } snes_sel_t;   /* kind 0: sample idx, 1: kit idx;
                                                                           * name: 0-terminated */
typedef struct { uint8_t part, rsv; int16_t root16; uint32_t start, loop; } snes_smp_t;   /* byte offsets in the
                                                                                           * part's data */
typedef struct { uint8_t smp, tune; } snes_cell_t;   /* a kit's note: its sample + 1 (0: none), semitones (int8) */
_Static_assert(sizeof(snes_slot_hdr_t) <= sizeof(smp_user_hdr_t), "an SNES slot header in a slot's header");
_Static_assert(__builtin_offsetof(snes_slot_hdr_t, data_len) == __builtin_offsetof(smp_user_hdr_t, data_len) &&
               __builtin_offsetof(snes_slot_hdr_t, crc) == __builtin_offsetof(smp_user_hdr_t, crc), "slot header");

static struct {
    uint32_t nsel;                      /* INST values from the bank (SNES_NFW ..), 0 = no bank */
    const snes_dir_t *dir;
    const uint8_t *data[SMP_USER_SLOTS];   /* each part's data, by part */
    uint32_t len[SMP_USER_SLOTS];
    uint32_t slots;                     /* bit k: user slot k holds a part of the bank */
} snes_bank;
static const char *snes_names[SNES_NSEL_MAX] = {SNES_FW_NAMES_INIT, [SNES_NFW ... SNES_NSEL_MAX - 1] = "-"};   /* INST */
static const uint8_t SNES_SILENT[9] = {0x03};   /* one block of silence that loops: a voice whose bank went */

static const snes_slot_hdr_t *snes_slot_hdr(uint32_t k) { return (const snes_slot_hdr_t *)smp_user_xip(k); }
static int snes_slot_ok(const snes_slot_hdr_t *h)
{
    return h->magic == SNES_SLOT_MAGIC && h->version == 1u && h->nparts && h->nparts <= SMP_USER_SLOTS &&
           h->part < h->nparts && h->data_len <= SMP_USER_SIZE - SMP_USER_DATA;
}

/* a sample's blocks from a, 9 bytes each, reach an end block within len (its loop too): the chip never reads past */
static int snes_brr_ok(const uint8_t *d, uint32_t len, uint32_t a)
{
    for (; a + 9u <= len; a += 9u)
        if (d[a] & 1u)
            return 1;
    return 0;
}

static const snes_smp_t *snes_smp(uint32_t i)
{
    return (const snes_smp_t *)((const uint8_t *)snes_bank.dir + snes_bank.dir->smp_off) + i;
}

/* the voices that play from the bank: silent at once (a slot of it is about to change). Called with the IRQs off */
static void snes_bank_voices_off(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            snes_voice_t *sv = &snes_v[p][i];
            if (!sv->bank)
                continue;
            sv->c.ram = SNES_SILENT;
            sv->c.start = sv->c.loop = sv->c.brr_addr = 0;
            sv->c.brr_offset = 1;
            sv->c.kon_delay = 0;
            sv->c.env = sv->c.hidden_env = 0;
            sv->c.env_mode = SE_RELEASE;
            sv->down = 0;
            sv->bank = 0;
        }
}

/* user slot k is about to be erased or written (editor.c): a bank it is part of goes */
static void snes_bank_drop(uint32_t k)
{
    uint32_t i;
    if (!(snes_bank.slots >> k & 1u))
        return;
    fm1_irq_off();
    snes_bank_voices_off();
    snes_bank.nsel = 0;
    snes_bank.slots = 0;
    fm1_irq_on();
    for (i = SNES_NFW; i < SNES_NSEL_MAX; i++)
        snes_names[i] = "-";
}

/* find a bank in the user slots and check all of it (at boot, after an upload): its parts, its directory, every
 * sample's blocks and every kit's notes; a bank that fails any of it is left out whole */
static void snes_bank_scan(void)
{
    const uint8_t *data[SMP_USER_SLOTS];
    uint32_t len[SMP_USER_SLOTS], slots = 0, k, p, i, n;
    const snes_slot_hdr_t *h0 = 0;
    const snes_dir_t *dir;
    for (k = 0; k < SMP_USER_SLOTS && !h0; k++)
        if (snes_slot_ok(snes_slot_hdr(k)) && !snes_slot_hdr(k)->part)
            h0 = snes_slot_hdr(k);
    fm1_irq_off();
    snes_bank_voices_off();
    snes_bank.nsel = 0;
    snes_bank.slots = 0;
    fm1_irq_on();
    for (i = SNES_NFW; i < SNES_NSEL_MAX; i++)
        snes_names[i] = "-";
    if (!h0)
        return;
    for (p = 0; p < h0->nparts; p++) {                  /* each part, by the bank's id */
        for (k = 0; k < SMP_USER_SLOTS; k++) {
            const snes_slot_hdr_t *h = snes_slot_hdr(k);
            if (snes_slot_ok(h) && h->bank == h0->bank && h->part == p && h->nparts == h0->nparts)
                break;
        }
        if (k == SMP_USER_SLOTS)
            return;                                     /* (a part not uploaded yet) */
        data[p] = smp_user_xip(k) + SMP_USER_DATA;
        len[p] = snes_slot_hdr(k)->data_len;
        slots |= 1u << k;
    }
    dir = (const snes_dir_t *)data[0];
    if (len[0] < sizeof *dir || !dir->nsel || dir->nsel > SNES_NSEL_MAX - SNES_NFW ||
        dir->sel_off > len[0] || dir->nsel * sizeof(snes_sel_t) > len[0] - dir->sel_off ||
        dir->smp_off > len[0] || dir->nsmp * sizeof(snes_smp_t) > len[0] - dir->smp_off ||
        dir->kit_off > len[0] || dir->nkit * 128u * sizeof(snes_cell_t) > len[0] - dir->kit_off ||
        (dir->sel_off | dir->smp_off) & 3u)
        return;
    for (i = 0; i < dir->nsmp; i++) {
        const snes_smp_t *s = (const snes_smp_t *)(data[0] + dir->smp_off) + i;
        if (s->part >= h0->nparts || !snes_brr_ok(data[s->part], len[s->part], s->start) ||
            !snes_brr_ok(data[s->part], len[s->part], s->loop))
            return;
    }
    for (i = 0, n = dir->nkit * 128u; i < n; i++)
        if (((const snes_cell_t *)(data[0] + dir->kit_off))[i].smp > dir->nsmp)
            return;
    for (i = 0; i < dir->nsel; i++) {
        const snes_sel_t *s = (const snes_sel_t *)(data[0] + dir->sel_off) + i;
        if (s->name[7] || s->idx >= (s->kind ? dir->nkit : dir->nsmp) || s->kind > 1u)
            return;
    }
    for (i = 0; i < dir->nsel; i++)
        snes_names[SNES_NFW + i] = ((const snes_sel_t *)(data[0] + dir->sel_off))[i].name;
    fm1_irq_off();
    for (p = 0; p < h0->nparts; p++) {
        snes_bank.data[p] = data[p];
        snes_bank.len[p] = len[p];
    }
    snes_bank.dir = dir;
    snes_bank.slots = slots;
    snes_bank.nsel = dir->nsel;
    fm1_irq_on();
}


/* ---------------------------------------------------- the engine (the driver) --- */
/* FIR settings games used often: none, a low-pass, a high-pass, a band-pass (C0 .. C7) */
static const int8_t SNES_FIRS[4][8] = {
    {127, 0, 0, 0, 0, 0, 0, 0},
    {0x0C, 0x21, 0x2B, 0x2B, 0x13, -0x02, -0x0D, -0x07},
    {0x58, -0x41, -0x25, -0x10, -0x02, 0x07, 0x0C, 0x0C},
    {0x34, 0x33, 0x00, -0x27, -0x1B, 0x01, -0x04, -0x15},
};
static const char *const N_SNES_FIR[] = {"FLAT", "LOW", "HIGH", "BAND"};
static const char *const N_SNES_EDL[] = {"0", "16", "32", "48", "64", "80", "96"};
_Static_assert(NELEM(N_SNES_EDL) == SNES_EDL_MAX + 1u, "a DELAY name per EDL");

static snes_voice_t *snes_vo(const track_t *t, const voice_t *v)
{
    return &snes_v[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE];
}

/* ATK DEC SUS (0..127) -> ADSR's AR (15 = at once), DR, SL; SR (P_E2) as it is */
static void snes_adsr(const track_t *t, snes_voice_t *sv)
{
    const int16_t *p = t->p;
    uint32_t ar = 15u - ((uint32_t)p[P_ATK] * 15u + 63u) / 127u, dr = 7u - ((uint32_t)p[P_DEC] * 7u + 63u) / 127u;
    uint32_t sl = ((uint32_t)p[P_SUS] * 7u + 63u) / 127u;
    sv->c.adsr0 = (uint8_t)(0x80u | dr << 4 | ar);
    sv->c.adsr1 = (uint8_t)(sl << 5 | ((uint32_t)p[P_E2] & 31u));
}

/* the pitch register of the note: 0x1000 at the sample's root, 14 bits. A kit's piece: key 60 plays it at its
 * own rate (gen_brr.py), the kit's tune of the key and the voice's bend / glide / LFO on top */
static uint32_t snes_pitch(const track_t *t, const snes_voice_t *sv, const vmod_t *m, uint32_t note)
{
    int32_t p16 = sv->kit ? (60 + sv->ktune) * 16 + m->pitch16 - (int32_t)note * 16 : m->pitch16;
    int32_t d16 = clamp(p16 + t->p[P_E1] * 16 - sv->root16, -3072, 384);
    int32_t pr = (int32_t)(pow2_q16(d16) >> 4);
    if (m->fine)
        pr += (pr * m->fine) >> 12;
    return (uint32_t)clamp(pr, 0, 0x3FFF);
}

static void snes_note_on(track_t *t, voice_t *v)       /* KON */
{
    snes_voice_t *sv = snes_vo(t, v);
    uint32_t sel = (uint32_t)t->p[P_E0];
    sv->kit = 0;
    sv->bank = 0;
    if (sel < SNES_NFW) {                               /* built in */
        sv->c.ram = SNES_BRR;
        sv->c.start = SNES_FW[sel].start;
        sv->c.loop = SNES_FW[sel].loop;
        sv->root16 = SNES_FW[sel].root16;
    } else {                                            /* the bank's: an instrument, or a kit's piece of the key */
        const snes_sel_t *s = sel - SNES_NFW < snes_bank.nsel ?
                              (const snes_sel_t *)((const uint8_t *)snes_bank.dir + snes_bank.dir->sel_off) +
                              (sel - SNES_NFW) : 0;
        const snes_smp_t *smp = 0;
        if (s && !s->kind) {
            smp = snes_smp(s->idx);
        } else if (s) {
            const snes_cell_t *c = (const snes_cell_t *)((const uint8_t *)snes_bank.dir + snes_bank.dir->kit_off) +
                                   s->idx * 128u + (v->note & 127u);
            if (c->smp) {
                smp = snes_smp(c->smp - 1u);
                sv->kit = 1;
                sv->ktune = (int8_t)c->tune;
            }
        }
        if (!smp) {                                     /* no bank, or no piece on the key: silent */
            sv->c.kon_delay = 0;
            sv->c.env = 0;
            sv->c.env_mode = SE_RELEASE;                /* (snes_done ends the voice) */
            sv->down = 0;
            return;
        }
        sv->c.ram = snes_bank.data[smp->part];
        sv->c.start = smp->start;
        sv->c.loop = smp->loop;
        sv->root16 = smp->root16;
        sv->bank = 1;
    }
    snes_adsr(t, sv);
    sv->c.kon_delay = 5;
    sv->c.env_mode = SE_ATTACK;
    sv->down = 1;
}

/* the part's chip samples of this block (SPC_DSP misc_30, once for all its voices): the rate counter
 * (one countdown per rate, from SPC_DSP's reset value 0), the rates firing, the noise at NOISE's rate */
static void snes_block(track_t *t)
{
    snes_dsp_t *d = &snes_dsp[(uint32_t)(t - trk) % NPART];
    uint32_t i, r, k = 0, nrate = (uint32_t)t->p[P_E3] & 31u;
    if (!d->init) {
        for (r = 0; r < 32u; r++)
            d->rc[r] = (int16_t)snes_ctr(0, r);
        d->noise = 0x4000;
        d->init = 1;
    }
    if (!eng_arena_held(t, ENGI_SNES))                  /* another engine had the arena: the line was theirs */
        d->eown = 0;                                    /* (snes_post takes it back, cleared) */
    for (i = 0; i < SNES_TICKS; i++)
        d->ein[i] = 0;
    d->ph0 = d->ph;
    for (i = 0; i < CTL; i++)
        if ((d->ph += SNES_RATE_Q16) >= SNES_ONE) {
            uint32_t m = 0;
            d->ph -= SNES_ONE;
            for (r = 1; r < 32u; r++) {                  /* (rate 0 never fires) */
                d->rc[r] = (int16_t)(d->rc[r] ? d->rc[r] - 1 : SNES_CTR_RATE[r] - 1);
                m |= (uint32_t)!d->rc[r] << r;
            }
            if (m >> nrate & 1u)
                snes_noise_step(&d->noise);
            if (k < SNES_TICKS) {
                d->fire[k] = m;
                d->nz[k++] = (int16_t)d->noise;
            }
        }
}

/* the voice has gone silent for good: its envelope at 0 after the key-on, not rising (voice.c, done) */
static int snes_done(track_t *t, voice_t *v)
{
    const snes_voice_t *sv = snes_vo(t, v);
    return !sv->c.kon_delay && !sv->c.env && (sv->c.env_mode != SE_ATTACK || !sv->down);
}

static void snes_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    snes_voice_t *sv = snes_vo(t, v);
    snes_dsp_t *d = &snes_dsp[(uint32_t)(t - trk) % NPART];
    int32_t vol = (v->vel + 2) >> SNES_VOL_SH, non = t->p[P_E3] != 0, eon = t->p[P_E4] != 0, koff = 0;
    uint32_t i, k = 0, ph = d->ph0, pitch = snes_pitch(t, sv, m, v->note);
    if (n != CTL)                                       /* (snes_block clocked CTL samples) */
        return;
    if (!v->gate && sv->down) {                         /* key-off: the chip's, or GAIN's decrease at REL */
        int32_t rel = t->p[P_REL];
        sv->down = 0;
        if (sv->kit)                                    /* a drum plays on (a cymbal: SR decays it) */
            ;
        else if (!rel)
            koff = 1;
        else {
            sv->c.adsr0 &= 0x7Fu;
            sv->c.gain = (uint8_t)(0xA0 | (31 - ((rel - 1) * 30 + 63) / 126));
        }
    } else if (sv->down) {
        snes_adsr(t, sv);                               /* the knobs move a held note's envelope */
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        if ((ph += SNES_RATE_Q16) >= SNES_ONE) {        /* a chip sample: the block's k-th */
            ph -= SNES_ONE;
            if (k < SNES_TICKS) {
                sv->o0 = sv->o1;
                sv->o1 = (int16_t)((snes_tick(&sv->c, d->fire[k], d->nz[k], pitch, non, koff) * vol) >> 7);
                if (eon) {                              /* EON: into the echo (SPC_DSP voice_output) */
                    int32_t e = d->ein[k] + sv->o1;
                    snes_eclips += (int16_t)e != e;
                    d->ein[k] = (int16_t)snes_clamp16(e);
                }
                k++;
                koff = 0;
            }
        }
        s = sv->o0 + (((sv->o1 - sv->o0) * (int32_t)(ph >> 1)) >> 15);
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) << SNES_VOL_SH;   /* (the headroom, made up) */
    }
}

static void snes_echo_clear(snes_dsp_t *d, uint32_t part)
{
    int16_t *b = snes_eline(part);
    uint32_t i;
    for (i = 0; i < SNES_ELINE; i++)
        b[i] = 0;
    memset(&d->ec, 0, sizeof d->ec);
    d->e0 = d->e1 = 0;
    d->quiet = 0;
    d->edirty = 0;
}

/* after an echo sample: is it still moving? Its FIR output and the sample written are watched; within a range of
 * 8 (and below 1024) they are flat, a window that a real tail leaves within a line (16 ms) even at 50 Hz */
static void snes_echo_flat(snes_dsp_t *d)
{
    int32_t x[2] = {d->ec.in, d->ec.wr}, k, moved = 0;
    if (x[0] || x[1])
        d->edirty = 1;
    for (k = 0; k < 2; k++) {
        if (!d->quiet) {
            d->qlo[k] = d->qhi[k] = (int16_t)x[k];
            continue;
        }
        if (x[k] < d->qlo[k])
            d->qlo[k] = (int16_t)x[k];
        if (x[k] > d->qhi[k])
            d->qhi[k] = (int16_t)x[k];
        moved |= d->qhi[k] - d->qlo[k] > 8 || x[k] >= 1024 || x[k] <= -1024;
    }
    if (moved) {                                        /* a new window from here */
        d->qlo[0] = d->qhi[0] = (int16_t)x[0];
        d->qlo[1] = d->qhi[1] = (int16_t)x[1];
        d->quiet = 1;
    } else if (d->quiet < 0xFFFFu) {
        d->quiet++;
    }
}

/* the part's echo, after its voices: ECHO (EVOL; 0 = off, the line forgotten), DELAY (EDL), FDBK (EFB), FIR; the
 * voices' sum came in ein. 1 while it still sounds */
static uint32_t snes_post(track_t *t, int32_t *out, uint32_t n)
{
    uint32_t part = (uint32_t)(t - trk) % NPART, i, k = 0, ph, any = 0;
    snes_dsp_t *d = &snes_dsp[part];
    const int16_t *p = t->p;
    int32_t evol = clamp(p[P_E4], 0, 127), efb = clamp(p[P_E6] * 2, -128, 127);
    const int8_t *fir = SNES_FIRS[(uint32_t)p[P_E7] & 3u];
    uint32_t edl = (uint32_t)clamp(p[P_E5], 0, SNES_EDL_MAX);
    int16_t *line = snes_eline(part);
    if (n != CTL)
        return 0;
    if (!d->eown) {                                     /* the line from silence: ours now */
        snes_echo_clear(d, part);
        d->eown = 1;
    }
    if (!evol) {
        if (d->edirty)
            snes_echo_clear(d, part);
        return 0;
    }
    for (i = 0; i < SNES_TICKS; i++)
        any |= (uint32_t)(d->ein[i] != 0);
    if (!any && !d->edirty)                             /* nothing in, nothing in the line: idle */
        return 0;
    for (i = 0, ph = d->ph0; i < n; i++) {
        int32_t s;
        if ((ph += SNES_RATE_Q16) >= SNES_ONE) {
            ph -= SNES_ONE;
            if (k < SNES_TICKS) {
                d->e0 = d->e1;
                d->e1 = (int16_t)snes_echo_tick(&d->ec, line, d->ein[k++], evol, efb, fir, edl, 1);
                snes_echo_flat(d);
            }
        }
        s = d->e0 + (((d->e1 - d->e0) * (int32_t)(ph >> 1)) >> 15);
        out[i] += mulq15(s, VOICE_FS) << SNES_VOL_SH;
    }
    /* flat for a whole line and the FIR's history: done. The chip's echo rounds towards -inf, so with feedback
     * a decayed echo settles on a few units of DC rather than 0; that is let go, the line cleared */
    if (d->edirty && d->quiet > (uint32_t)d->ec.len + 16u)
        snes_echo_clear(d, part);
    return d->edirty;
}

/* INST: the built-in waves and the bank's entries (the knob stops at the last there is) */
static param_desc_t snes_inst_desc;
static const param_desc_t *snes_desc(const struct track *t, uint32_t k)
{
    (void)t;
    if (k)
        return 0;
    snes_inst_desc = (param_desc_t){"INST", F_ENUM, 0, (int16_t)(SNES_NFW + snes_bank.nsel - 1u), 0, snes_names, 0};
    return &snes_inst_desc;
}

static const engine_t ENG_SNES = {
    "SNES", {"VOICE", "ECHO"},
    {
        {"INST", F_ENUM, 0, SNES_NSEL_MAX - 1, 0, snes_names, 0},   /* built in, then the bank's (snes_desc) */
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"SR", F_INT, 0, 31, 0, 0, 0},                  /* ADSR's sustain rate: the decay while held, 0 = none */
        {"NOISE", F_INT, 0, 31, 0, 0, 0},               /* 0 = the sample, else the chip's noise at this rate */
        {"ECHO", F_PCT, 0, 127, 0, 0, 0},               /* EVOL; 0 = no echo (EON off) */
        {"DELAY", F_ENUM, 0, SNES_EDL_MAX, 4, N_SNES_EDL, "ms"},   /* EDL */
        {"FDBK", F_BIPCT, -64, 63, 0, 0, 0},            /* EFB / 2 */
        {"FIR", F_ENUM, 0, 3, 1, N_SNES_FIR, 0},
    },
    SNES_PRESET_TABLE, NELEM(SNES_PRESET_TABLE), -1, snes_note_on, snes_render,
    0x9A7F, {P_E0, P_E2, P_ATK, P_REL}, 0, 0, snes_desc, snes_block,
    .ownenv = 1, .done = snes_done, .post = snes_post,
};
