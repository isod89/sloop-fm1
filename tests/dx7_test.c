/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the DX7 engine (firmware/src/eng_dx7.c) through the host simulator: the voice check
 * (INIT VOICE, blank and damaged slots), the user store read through the host's RAM image, every
 * algorithm rendered (bounded, audible, finite, the voice freed after the release), a fixed-frequency
 * operator, the macros (BRGT, TRN, ALG, FB), and a store rewrite reaching a sounding part (dx_gen).
 *   build/host/dx7_test [OUT.wav] */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t outbuf[2 * CTL];
static uint32_t dx_engine(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_DX7)
            return e;
    return NENGINES;
}

/* render s seconds of the mix; returns the peak, *rms the RMS of the left channel; hf_rms: the RMS of
 * its first difference (high-frequency content: FM keeps a sine's power, so RMS alone cannot see
 * brightness) */
static double hf_rms;
static int32_t render(double s, double *rms, FILE *wav)
{
    uint32_t nb = (uint32_t)(s * FS) / CTL, k, i;
    int64_t sq = 0, dq = 0;
    int32_t peak = 0, last = 0;
    for (k = 0; k < nb; k++) {
        mix_block(outbuf, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t x = outbuf[i], a = x < 0 ? -x : x;
            if (a > peak)
                peak = a;
            if (!(i & 1u)) {
                sq += (int64_t)x * x;
                dq += (int64_t)(x - last) * (x - last);
                last = x;
            }
        }
        if (wav)
            for (i = 0; i < CTL; i++)
                wav_put(wav, outbuf[2 * i], outbuf[2 * i + 1]);
    }
    *rms = sqrt((double)sq / (nb * CTL));
    hf_rms = sqrt((double)dq / (nb * CTL));
    return peak;
}

static uint32_t part_voices(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}

/* a packed voice: INIT VOICE with operator op (1..6) at output level ol, ratio coarse, algorithm alg */
static void make_voice(uint8_t *v, uint32_t alg, uint32_t op, uint32_t ol, uint32_t coarse)
{
    uint8_t *o = v + (6u - op) * 17u;
    memcpy(v, DX_INIT, DX_VOICE);
    v[110] = (uint8_t)alg;
    o[14] = (uint8_t)ol;
    o[15] = (uint8_t)(coarse << 1);
}

int main(int argc, char **argv)
{
    FILE *wav = argc > 1 ? fopen(argv[1], "wb") : 0;
    uint32_t e = dx_engine(), a, i;
    track_t *t = &trk[0];
    double rms, rms2, r1, r2;
    int32_t peak;
    uint8_t v[DX_VOICE];
    if (wav)
        wav_hdr(wav, 0);
    check("the DX7 engine is built and comes after GRAIN", e == ENG_DX7_IDX && e < NENGINES);
    memset(dx_host_store, 0xFF, sizeof dx_host_store);

    /* ---- the voice check */
    check("INIT VOICE passes the voice check", dx_voice_check(DX_INIT) == DXV_OK);
    memset(v, 0xFF, sizeof v);
    check("an erased slot is BLANK", dx_voice_check(v) == DXV_BLANK);
    memset(v, 0, sizeof v);
    check("a zeroed slot is BLANK", dx_voice_check(v) == DXV_BLANK);
    memcpy(v, DX_INIT, sizeof v);
    v[3] = 127;
    v[105] = 127;
    v[113] = 127;
    v[5 * 17 + 16] = 127;
    check("127s in a rate, the pitch envelope, the LFO delay and fine are accepted (a DX7 clamps them)", dx_voice_check(v) == DXV_OK);
    memcpy(dx_host_store + 60 * DX_VOICE, v, DX_VOICE);   /* ... and render without reading past a table (below) */
    memcpy(v, DX_INIT, sizeof v);
    v[120] = 0x80;
    check("an 8-bit byte is damaged", dx_voice_check(v) == DXV_BAD);
    memcpy(v, DX_INIT, sizeof v);
    v[121] = 7;
    check("a control character in the name is damaged", dx_voice_check(v) == DXV_BAD);
    check("an empty user slot plays INIT VOICE", dx_source(DXB_USR, 5) == DX_INIT && dx_bank_used(0) == 0);
    make_voice(v, 4, 1, 99, 2);
    memcpy(dx_host_store + 5 * DX_VOICE, v, DX_VOICE);
    check("a written slot is read from the store", dx_source(DXB_USR, 5) == dx_host_store + 5 * DX_VOICE && dx_bank_used(0) == 1);

    /* ---- every algorithm: a note, held, released; bounded, audible, finite, freed */
    host_tracks_init();
    host_preset(t, e, 0);
    t->p[P_VOICE] = V_POLY;
    t->p[P_REV] = 0;                                      /* no reverb tail: silence after a release is the voices' */
    for (a = 0; a < 32u; a++) {
        char what[96];
        uint32_t j;
        memcpy(v, DX_INIT, DX_VOICE);
        v[110] = (uint8_t)a;
        for (j = 0; j < 6u; j++)                          /* every operator at 90: modulators and carriers alike */
            v[j * 17 + 14] = 90;
        memcpy(dx_host_store + a * DX_VOICE, v, DX_VOICE);
        t->p[P_E0] = (int16_t)a;
        t->p[P_E1] = DXB_USR;
        trk_note_on(t, 60, 100);
        peak = render(0.5, &rms, wav);
        trk_note_off(t, 60);
        render(1.5, &rms2, wav);
        snprintf(what, sizeof what, "algorithm %2u: peak %5d, RMS %6.0f, free after release (%u voices left)", a + 1, peak, rms,
                 part_voices(t));
        check(what, peak > 500 && peak < 30000 && rms > 100 && rms2 < rms && part_voices(t) == 0);
    }

    /* ---- Yamaha dumps as Dexed sends them (dx_syx_parse): a bank, a single voice, a bad checksum */
    {
        static uint8_t fr[4102];
        uint8_t vo[DX_VOICE];
        const uint8_t *bk = 0;
        uint32_t s = 0, k;
        fr[0] = 0x43; fr[1] = 0x00; fr[2] = 0x09; fr[3] = 0x20; fr[4] = 0x00;
        for (k = 0; k < 32u; k++)
            memcpy(fr + 5 + k * DX_VOICE, DX_INIT, DX_VOICE);
        for (k = 0; k < 4096u; k++)
            s += fr[5 + k];
        fr[4101] = (uint8_t)((0u - s) & 0x7Fu);
        check("a 32-voice bulk dump parses as a bank", dx_syx_parse(fr, 4102, vo, &bk) == DXS_BANK && bk == fr + 5);
        fr[4101] ^= 1u;
        check("... and fails its checksum when damaged", dx_syx_parse(fr, 4102, vo, &bk) == DXS_BAD);
        {   /* a single voice (VCED) of INIT VOICE packs to DX_INIT */
            static uint8_t sv[161];
            uint32_t j;
            memset(sv, 0, sizeof sv);
            sv[0] = 0x43; sv[1] = 0x00; sv[2] = 0x00; sv[3] = 0x01; sv[4] = 0x1B;
            for (j = 0; j < 6u; j++) {
                uint8_t *o = sv + 5 + j * 21;
                static const uint8_t op[21] = {99, 99, 99, 99, 99, 99, 99, 0, 39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 7};
                memcpy(o, op, 21);
                o[16] = j == 5u ? 99 : 0;
            }
            {
                static const uint8_t tail[19] = {99, 99, 99, 99, 50, 50, 50, 50, 0, 0, 1, 35, 0, 0, 0, 1, 0, 3, 24};
                memcpy(sv + 5 + 126, tail, 19);
                memcpy(sv + 5 + 145, "INIT VOICE", 10);
            }
            for (s = 0, k = 0; k < 155u; k++)
                s += sv[5 + k];
            sv[160] = (uint8_t)((0u - s) & 0x7Fu);
            check("a single-voice dump packs to INIT VOICE's bank form", dx_syx_parse(sv, 161, vo, &bk) == DXS_VOICE && memcmp(vo, DX_INIT, DX_VOICE) == 0);
            sv[0] = 0x7D;
            check("an editor frame is not a DX7 dump", dx_syx_parse(sv, 161, vo, &bk) == DXS_NONE);
        }
    }

    /* ---- the voice with 127s: plays, bounded, freed */
    t->p[P_E0] = 60;
    trk_note_on(t, 60, 100);
    peak = render(0.5, &rms, 0);
    trk_note_off(t, 60);
    render(1.5, &rms2, 0);
    check("a voice with 127s beyond the DX7 ranges plays, bounded and freed", peak > 500 && peak < 30000 && part_voices(t) == 0);

    /* ---- a fixed-frequency operator: the note does not change the pitch */
    {
        int32_t p1, p2;
        make_voice(v, 0, 1, 99, 1);
        v[5 * 17 + 15] = 1 | (2 << 1);                    /* OP1 fixed, 100 Hz */
        memcpy(dx_host_store + 40 * DX_VOICE, v, DX_VOICE);
        t->p[P_E0] = 40;
        trk_note_on(t, 36, 100);
        p1 = render(0.3, &r1, 0);
        trk_note_off(t, 36);
        render(1.0, &rms2, 0);
        trk_note_on(t, 84, 100);
        p2 = render(0.3, &r2, 0);
        trk_note_off(t, 84);
        render(1.0, &rms2, 0);
        check("fixed-frequency operator: the same level at C2 and C6", p1 > 1000 && p2 > 1000 && (p1 > p2 ? p1 - p2 : p2 - p1) < p1 / 8);
    }

    /* ---- macros: BRGT makes a two-operator stack brighter (more energy above the carrier) */
    {
        make_voice(v, 0, 1, 99, 1);
        v[4 * 17 + 14] = 70;                              /* OP2 modulates OP1 (algorithm 1) at 70 */
        memcpy(dx_host_store + 41 * DX_VOICE, v, DX_VOICE);
        t->p[P_E0] = 41;
        t->p[P_E2] = -64;
        trk_note_on(t, 60, 100);
        render(0.5, &rms, 0);
        r1 = hf_rms;
        trk_note_off(t, 60);
        render(1.5, &rms2, 0);
        t->p[P_E2] = 63;
        trk_note_on(t, 60, 100);
        render(0.5, &rms2, 0);
        r2 = hf_rms;
        trk_note_off(t, 60);
        render(1.5, &rms2, 0);
        t->p[P_E2] = 0;
        check("BRGT -64 is a near sine, BRGT +63 a brighter one (more high-frequency energy)",
              rms > 100 && r2 > r1 * 1.3);
        t->p[P_E5] = 12;
        trk_note_on(t, 60, 100);
        render(0.5, &rms, 0);
        trk_note_off(t, 60);
        render(1.5, &rms2, 0);
        t->p[P_E5] = 0;
        check("TRN +12 still renders (bounded, freed)", rms > 100 && part_voices(t) == 0);
        t->p[P_E6] = 32;                                  /* ALG override: 32 = six carriers */
        t->p[P_E7] = 8;                                   /* FB 7 */
        trk_note_on(t, 60, 100);
        peak = render(0.5, &rms, 0);
        trk_note_off(t, 60);
        render(1.5, &rms2, 0);
        t->p[P_E6] = t->p[P_E7] = 0;
        check("ALG 32 + FB 7 override: bounded and freed", peak > 500 && peak < 30000 && part_voices(t) == 0);
    }

    /* ---- a store rewrite reaches a sounding part through dx_gen (the part re-reads its voice) */
    {
        const dx_part_t *d = dx_part(t);
        t->p[P_E0] = 41;
        trk_note_on(t, 60, 100);
        render(0.2, &rms, 0);
        check("the part holds a copy of its voice", memcmp(d->p, dx_host_store + 41 * DX_VOICE, DX_VOICE) == 0);
        make_voice(v, 0, 1, 99, 4);
        memcpy(dx_host_store + 41 * DX_VOICE, v, DX_VOICE);
        render(0.2, &rms, 0);
        check("... unchanged by a store write until dx_gen moves", d->p[5 * 17 + 15] == 2);
        dx_gen++;
        render(0.2, &rms, 0);
        check("... and re-read after it", d->p[5 * 17 + 15] == (4 << 1));
        trk_note_off(t, 60);
        render(1.5, &rms, 0);
    }

    /* ---- 8 notes held on one part: the shared budget, all freed */
    {
        t->p[P_E0] = 0;
        for (i = 0; i < 8u; i++)
            trk_note_on(t, 48 + 2 * i, 100);
        peak = render(0.5, &rms, wav);
        check("8 held notes: bounded", peak > 500 && peak < 32767 && part_voices(t) == 8);
        for (i = 0; i < 8u; i++)
            trk_note_off(t, 48 + 2 * i);
        render(1.0, &rms, wav);                           /* the releases themselves */
        render(1.0, &rms, wav);
        check("8 notes released: every voice free, silence a second later", part_voices(t) == 0 && rms < 50);
    }

    if (wav) {
        long n = (ftell(wav) - 44) / 4;
        fseek(wav, 0, SEEK_SET);
        wav_hdr(wav, (uint32_t)n);
        fclose(wav);
    }
    printf(bad ? "DX7 TEST FAILED (%d)\n" : "DX7 TEST PASSED\n", bad);
    return bad ? 1 : 0;
}
