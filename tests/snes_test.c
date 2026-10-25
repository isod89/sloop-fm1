/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ugotworms */
/* The SNES engine (eng_snes.c), on the host, with its bank (tools/gen_brr.py --bank BANK) in a RAM image of the
 * user slots:
 *   build/host/snes_test BANK REF.bin OUTDIR
 * 0. the bank: found and checked as at boot (snes_bank_scan), INST's names and range with and without it; a bank
 *    with a part missing or a sample running off its part is left out; a slot of it erased silences its voices.
 * 1. the voice against snes_spc: every scenario of snes_cases.h on eng_snes.c's chip voice, compared
 *    sample for sample with REF.bin (tests/snes_ref.cpp: the same scenarios on SPC_DSP.cpp). The key-on
 *    and key-off are polled every other sample on the chip and the DAC is a sample behind, so the
 *    key-on sample, the key-off's and the output's lag are searched (a few samples); the rate counter
 *    runs from the reset on both sides, so a match is exact or nothing.
 *    The echo cases run eng_snes.c's echo too (line, FIR, feedback) against the chip's.
 * 2. the engine in the mix (voice.c, fx.c): every preset, a chord held 1 s then released: its peak,
 *    no clipping, every voice free after the release, the echo idle after its tail; the pitch register
 *    of the keys; the cost of 8 voices next to other engines'. WAVs in OUTDIR. The echo line lives in
 *    the engine arena: SNES after GRAIN, and GRAIN after SNES, sound as from boot, sample for sample.
 * 3. a 4-part demo in OUTDIR/snes_demo.wav: strings, bass, lead and the noise hat. */
#include <stdint.h>
static unsigned char snes_test_slots[4 * 0x14000];     /* USR1..4 (a RAM image) */
#define SMP_USER_XIP(k) ((const uint8_t *)snes_test_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main
#undef inst                                     /* hostsim's part 1: here, the instrument fields */
#include "snes_cases.h"
#include "snes_bankfile.h"

#define ENGI_GRAIN 8u
static int bad;
static void check(const char *what, int ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    bad += !ok;
}

/* ------------------------------------------------- 1. against snes_spc --- */
static void run_case(const snes_case_t *c, int kon_s, int koff_s, int16_t *o)
{
    snes_chv_t v;
    snes_echo_t ec;
    static int16_t line[15u * 512u];             /* (EDL up to the chip's 15 here)*/
    int32_t counter = 0, noise = 0x4000, s;     /* as SPC_DSP::reset leaves them */
    memset(&v, 0, sizeof v);
    memset(&ec, 0, sizeof ec);
    memset(line, 0, sizeof line);
    {                                           /* the instrument's directory entry */
        snes_test_smp_t sm;
        if (!snes_test_find(c->inst, 1, &sm)) {   /* (the bank in USR2..: bank()) */
            fprintf(stderr, "snes_test: no instrument %s\n", c->inst);
            exit(2);
        }
        v.ram = sm.data;
        v.start = sm.start;
        v.loop = sm.loop;
    }
    v.adsr0 = c->adsr0;
    v.adsr1 = c->adsr1;
    v.gain = c->gain;
    v.env_mode = SE_RELEASE;
    for (s = 0; s < c->n; s++) {
        int koff = 0;
        if (s == koff_s) {
            if (c->rel_gain) {
                v.adsr0 &= 0x7Fu;
                v.gain = c->rel_gain;
            } else {
                koff = 1;
            }
        }
        uint32_t fire;
        if (--counter < 0)                          /* SPC_DSP misc_30 */
            counter = SNES_CTR_RANGE - 1;
        fire = snes_fire_of(counter);
        if (fire >> c->nrate & 1u)
            snes_noise_step(&noise);
        o[s] = (int16_t)snes_tick(&v, fire, noise, c->pitch, c->non, koff);
        if (c->echo) {                              /* the reference's voice volume is -128: -o into the echo */
            int32_t e = snes_echo_tick(&ec, line, (o[s] * -128) >> 7, c->evol, c->efb, c->fir, c->edl, 1);
            o[s] = (int16_t)snes_clamp16(o[s] + e);
        }
        if (s == kon_s) {                           /* KON, seen at the end of this sample's V3c */
            v.kon_delay = 5;
            v.env_mode = SE_ATTACK;
        }
    }
}

/* the engine's rate countdowns and noise (snes_block) against SPC_DSP's counter, 3 of its periods */
static void counters(void)
{
    uint32_t b, k, ticks = 0, bad_fire = 0, bad_noise = 0;
    int32_t counter = 0, noise = 0x4000;
    char msg[160];
    memset(snes_dsp, 0, sizeof snes_dsp);
    trk[2].p[P_E3] = 17;
    for (b = 0; ticks < 3u * SNES_CTR_RANGE; b++) {
        uint32_t ph = snes_dsp[2].ph, n = 0;
        for (k = 0; k < CTL; k++)                   /* the chip samples of this block */
            if ((ph += SNES_RATE_Q16) >= 0x10000u) {
                ph -= 0x10000u;
                n++;
            }
        snes_block(&trk[2]);
        for (k = 0; k < n; k++, ticks++) {
            uint32_t fire;
            if (--counter < 0)
                counter = SNES_CTR_RANGE - 1;
            fire = snes_fire_of(counter);
            if (fire >> 17 & 1u)
                snes_noise_step(&noise);
            bad_fire += snes_dsp[2].fire[k] != fire;
            bad_noise += snes_dsp[2].nz[k] != (int16_t)noise;
        }
    }
    snprintf(msg, sizeof msg, "rate countdowns and noise = SPC_DSP's counter over %u chip samples (%u, %u differ)", ticks,
             bad_fire, bad_noise);
    check(msg, !bad_fire && !bad_noise);
}

static void against_ref(const char *path)
{
    FILE *f = fopen(path, "rb");
    static int16_t ref[1 << 17], mine[1 << 17];
    uint32_t k;
    printf("1. the voice against snes_spc (%s)\n", path);
    if (!f) {
        check("reference file readable", 0);
        return;
    }
    for (k = 0; k < SNES_NCASES; k++) {
        const snes_case_t *c = &SNES_CASES[k];
        int32_t n = 0, kon, kd, lag, best = 0x7FFFFFFF, bk = 0, bd = 0, bl = 0, nz = 0, peak = 0, i;
        char msg[200];
        if (fread(&n, 4, 1, f) != 1 || n != c->n || fread(ref, 2, (size_t)n, f) != (size_t)n) {
            check("reference file matches snes_cases.h", 0);
            break;
        }
        for (i = 0; i < n; i++) {
            nz += ref[i] != 0;
            peak = ref[i] > peak ? ref[i] : -ref[i] > peak ? -ref[i] : peak;
        }
        for (kon = 0; kon < 3 && best; kon++)
            for (kd = 0; kd < 3 && best; kd++) {
                run_case(c, kon, c->koff_at < 0 ? -1 : c->koff_at + kd, mine);
                for (lag = 0; lag < 4 && best; lag++) {
                    int32_t diff = 0;
                    for (i = 0; i + lag < n; i++)
                        diff += mine[i] != ref[i + lag];
                    if (diff < best) {
                        best = diff;
                        bk = kon;
                        bd = kd;
                        bl = lag;
                    }
                }
            }
        snprintf(msg, sizeof msg, "%-58s %s (key-on %d, key-off +%d, lag %d; %d of %d samples sound, peak %d)",
                 c->what, best ? "DIFFERS" : "exact", bk, bd, bl, nz, n, peak);
        if (best)
            printf("       %d samples differ\n", best);
        check(msg, !best && nz > n / 20);
    }
    fclose(f);
}

/* ------------------------------------------------- 2. the engine in the mix --- */
static FILE *wav_open(const char *dir, const char *name, uint32_t frames)
{
    char p[512];
    FILE *w;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    if ((w = fopen(p, "wb")))
        wav_hdr(w, frames);
    return w;
}

static uint32_t voices_on(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active;
    return n;
}

static int snes_is_kit(uint32_t sel)                   /* INST value sel is one of the bank's kits */
{
    const snes_sel_t *s = (const snes_sel_t *)((const uint8_t *)snes_bank.dir + snes_bank.dir->sel_off);
    return sel >= SNES_NFW && sel - SNES_NFW < snes_bank.nsel && s[sel - SNES_NFW].kind;
}

static uint32_t snes_preset(const char *name)           /* a SNES preset's index by its name */
{
    uint32_t i;
    for (i = 0; i < ENGINES[ENGI_SNES]->npresets; i++)
        if (!strcmp(ENGINES[ENGI_SNES]->presets[i].name, name))
            return i;
    fprintf(stderr, "snes_test: no preset %s\n", name);
    exit(2);
}

static void presets(const char *dir)
{
    uint32_t pi;
    printf("2. the engine in the mix\n");
    for (pi = 0; pi < ENGINES[ENGI_SNES]->npresets; pi++) {
        const preset_t *pr = &ENGINES[ENGI_SNES]->presets[pi];
        static const uint8_t CHORD[3] = {60, 64, 67}, DRUMS[3] = {36, 38, 42};   /* a kit: kick, snare, hat */
        const uint8_t *CH = snes_is_kit(pr->e[0]) ? DRUMS : CHORD;
        uint32_t f, i, frames = 6u * FS, nk = pr->mono ? 1u : 3u, rel = FS / CTL * CTL, freed = 0, eidle = 0;   /* (a block's start) */
        int32_t peak = 0, clip = 0;
        char nm[64], msg[200];
        FILE *w;
        for (i = 0; pr->name[i] && i < 40u; i++)
            nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
        nm[i] = 0;
        strcat(nm, ".wav");
        memset(trk, 0, sizeof trk);
        memset(snes_v, 0, sizeof snes_v);
        memset(snes_dsp, 0, sizeof snes_dsp);
        host_tracks_init();
        host_preset(&trk[0], ENGI_SNES, pi);
        w = wav_open(dir, nm, frames);
        for (i = 0; i < nk; i++)
            trk_note_on(&trk[0], CH[i], 100);
        for (f = 0; f < frames; f += CTL) {
            int32_t o[2 * CTL];
            if (f == rel)
                for (i = 0; i < nk; i++)
                    trk_note_off(&trk[0], CH[i]);
            mix_block(o, CTL);
            for (i = 0; i < 2u * CTL; i++) {
                int32_t a = o[i] < 0 ? -o[i] : o[i];
                peak = a > peak ? a : peak;
                clip += a >= 30000;
            }
            if (w)
                for (i = 0; i < CTL; i++)
                    wav_put(w, o[2 * i], o[2 * i + 1]);
            if (f > rel && !freed && !voices_on())
                freed = f - rel;
            if (freed && !eidle && !snes_dsp[0].edirty)
                eidle = f - rel;
        }
        if (w)
            fclose(w);
        if (!eidle) {                                   /* what keeps the echo on */
            const snes_dsp_t *d = &snes_dsp[0];
            printf("       echo: in %d wr %d quiet %u len %u dirty %u own %u; line[0..7]:", d->ec.in, d->ec.wr, d->quiet,
                   d->ec.len, d->edirty, d->eown);
            for (i = 0; i < 8u; i++)
                printf(" %d", snes_eline(0)[i]);
            printf("\n");
        }
        for (i = 0; i < NVOICE && !freed; i++)          /* what holds a voice */
            if (trk[0].v[i].active) {
                const snes_voice_t *sv = &snes_v[0][i];
                printf("       voice %u: note %u gate %u stage %u | env %d mode %u kon %u down %u adsr0 %02X gain %02X\n",
                       i, trk[0].v[i].note, trk[0].v[i].gate, trk[0].v[i].stage, sv->c.env, sv->c.env_mode,
                       sv->c.kon_delay, sv->down, sv->c.adsr0, sv->c.gain);
            }
        snprintf(msg, sizeof msg, "%-10s peak %5d, %d >= 30000, voices free %4u ms, echo idle %4u ms after the release -> %s",
                 pr->name, peak, clip, freed * 1000u / FS, eidle * 1000u / FS, nm);
        check(msg, peak > 250 && !clip && freed && freed < 3u * FS && eidle);
    }
    {                                               /* the pitch register of the keys (SQUARE) */
        vmod_t m;
        snes_voice_t sv;
        int32_t root = SNES_FW[0].root16;
        memset(&m, 0, sizeof m);
        memset(&sv, 0, sizeof sv);
        sv.root16 = (int16_t)root;
        host_preset(&trk[0], ENGI_SNES, snes_preset("SQUARE"));
        trk[0].p[P_E1] = 0;
        m.pitch16 = root;
        {
            uint32_t p0 = snes_pitch(&trk[0], &sv, &m, 60);
            uint32_t p12, p24, pm24;
            m.pitch16 = root + 192;
            p12 = snes_pitch(&trk[0], &sv, &m, 60);
            m.pitch16 = root + 384 + 16;
            p24 = snes_pitch(&trk[0], &sv, &m, 60);
            m.pitch16 = root - 384;
            pm24 = snes_pitch(&trk[0], &sv, &m, 60);
            char msg[160];
            snprintf(msg, sizeof msg, "pitch register: root 0x%04X, +1 oct 0x%04X, +2 oct+ 0x%04X (14-bit cap), -2 oct 0x%04X",
                     p0, p12, p24, pm24);
            check(msg, p0 >= 0xFF8 && p0 <= 0x1008 && p12 >= 0x1FF0 && p12 <= 0x2010 && p24 == 0x3FFF &&
                           pm24 >= 0x3FC && pm24 <= 0x404);
        }
    }
    {                                               /* cost: 8 sustained voices, next to the other engines' */
        static const struct { uint32_t e, p; const char *nm; } B[] = {
            {4u, 2, "SAMPLE FLUTE (loops)"}, {0, 0, "ANALOG SAW LEAD"}, {ENGI_FM6, 4, "FM6 PAD"},
            {ENGI_SNES, 0, "SNES SYN STR"}, {ENGI_SNES, 0, "SNES SQUARE"}};
        uint32_t k;
        for (k = 0; k < NELEM(B); k++) {
            uint64_t t0, best = ~0ull;
            uint32_t r, f;
            memset(trk, 0, sizeof trk);
            host_tracks_init();
            host_preset(&trk[0], B[k].e, B[k].e == ENGI_SNES ? snes_preset(B[k].nm + 5) : B[k].p);
            trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
            trk[0].p[P_VOICE] = V_POLY;
            trk[0].p[P_SUS] = 127;
            for (r = 0; r < 8u; r++)
                trk_note_on(&trk[0], 48 + r * 3, 100);
            for (r = 0; r < 5; r++) {
                int32_t o[2 * CTL];
                t0 = now_ns();
                for (f = 0; f < FS / 4u; f += CTL)
                    mix_block(o, CTL);
                if (now_ns() - t0 < best)
                    best = now_ns() - t0;
            }
            printf("  info %-22s %u voices sounding at the end, host ns per output sample %.1f\n", B[k].nm,
                   voices_on(), (double)best / (FS / 4u));
        }
    }
}

/* ------------------------------------------------- the echo line in the engine arena --- */
/* part 1's own output (no FX buses, no master: they keep their own state) for `blocks` blocks, a note at 0
 * released at the middle */
static void part_blocks(int32_t *dst, uint32_t blocks, uint32_t note)
{
    uint32_t b;
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < blocks; b++) {
        if (b == blocks / 2u)
            trk_note_off(&trk[0], note);
        track_render(&trk[0], dst + b * CTL, CTL);
    }
}

static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(snes_v, 0, sizeof snes_v);
    memset(snes_dsp, 0, sizeof snes_dsp);
    memset(eng_arena, 0, sizeof eng_arena);
    memset(eng_arena_own, 0, sizeof eng_arena_own);
    host_tracks_init();
}

#define GRAIN_P 0u                              /* a GRAIN preset */
static void sharing(void)
{
    enum { NB = 2000 };                             /* 1.45 s */
    static int32_t a[NB * CTL], b[NB * CTL], junk[CTL * NB];
    uint32_t i, diff, dirty;
    int32_t pk;
    char msg[160];
    /* SNES (echo) after GRAIN has filled the memory: the same as SNES from boot */
    fresh();
    host_preset(&trk[0], ENGI_SNES, snes_preset("SYN STR"));   /* echo, feedback */
    trk[0].p[P_E5] = SNES_EDL_MAX;                  /* (the whole line)*/
    part_blocks(a, NB, 60);
    fresh();
    host_preset(&trk[0], ENGI_GRAIN, GRAIN_P);
    for (i = 0; i < 6u; i++)
        trk_note_on(&trk[0], 48 + i * 5, 120);
    for (i = 0; i < 400u; i++) {
        track_render(&trk[0], junk, CTL);
    }
    for (i = dirty = 0; i < sizeof eng_arena[0] / 2u; i++)
        dirty += ((const int16_t *)&eng_arena[0])[i] != 0;
    host_preset(&trk[0], ENGI_SNES, snes_preset("SYN STR"));   /* as a project load: the engine at once */
    trk[0].p[P_E5] = SNES_EDL_MAX;
    memset(trk[0].v, 0, sizeof trk[0].v);
    memset(&snes_dsp[0], 0, sizeof snes_dsp[0]);    /* (the chip's clock from 0, as the first run's) */
    snes_dsp[0].init = 0;
    part_blocks(b, NB, 60);
    for (i = 0, pk = 0, diff = 0; i < NB * CTL; i++) {
        diff += a[i] != b[i];
        pk = a[i] > pk ? a[i] : -a[i] > pk ? -a[i] : pk;
    }
    snprintf(msg, sizeof msg, "SNES with echo after GRAIN left %u words in the memory = SNES from boot (%u differ, peak %d)",
             dirty, diff, pk);
    check(msg, !diff && dirty > 500u && pk > 1000);
    /* GRAIN after the echo has filled the memory: the same as GRAIN from boot */
    fresh();
    host_preset(&trk[0], ENGI_GRAIN, GRAIN_P);
    part_blocks(a, NB, 52);
    fresh();
    host_preset(&trk[0], ENGI_SNES, snes_preset("CHOIR"));   /* echo on */
    trk[0].p[P_E5] = SNES_EDL_MAX;
    trk[0].p[P_E6] = 60;
    part_blocks(junk, NB, 60);
    for (i = diff = 0; i < SNES_ELINE; i++)
        diff += eng_arena[0].snes[i] != 0;
    host_preset(&trk[0], ENGI_GRAIN, GRAIN_P);
    memset(trk[0].v, 0, sizeof trk[0].v);
    part_blocks(b, NB, 52);
    {
        uint32_t filled = diff;
        for (i = 0, pk = 0, diff = 0; i < NB * CTL; i++) {
            diff += a[i] != b[i];
            pk = a[i] > pk ? a[i] : -a[i] > pk ? -a[i] : pk;
        }
        snprintf(msg, sizeof msg, "GRAIN after the echo line filled %u samples = GRAIN from boot (%u differ, peak %d)", filled,
                 diff, pk);
        check(msg, !diff && filled > 1000u && pk > 1000);
    }
}

/* ------------------------------------------------- 3. a demo --- */
typedef struct { uint16_t step; uint8_t part, note, len; } ev_t;
static void demo(const char *dir)
{
    /* 12 bars, 16th steps at 120 BPM: 0 SYN STR (the chords), 1 SYNBASS, 2 SQUARE (the tune from bar 5), 3 E.KIT
     * (kick, snare, hats; a crash at each section, a tom fill before the tune) */
    static const char *const PRE[4] = {"SYN STR", "SYNBASS", "SQUARE", "E.KIT"};
    static const uint8_t CHORD[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 59, 62}, {52, 55, 59}};   /* Am F G Em */
    static const uint8_t BASS[4] = {45, 41, 43, 40};
    static const uint8_t LEAD[64] = {76, 0, 0, 72, 0, 74, 76, 0, 79, 0, 77, 0, 76, 0, 72, 0,
                                     74, 0, 0, 71, 0, 72, 74, 0, 76, 0, 0, 0, 0, 0, 0, 0,
                                     72, 0, 0, 76, 0, 79, 81, 0, 79, 0, 77, 0, 76, 0, 74, 0,
                                     76, 0, 74, 0, 72, 0, 71, 0, 69, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t FILL[8] = {50, 50, 48, 48, 45, 45, 41, 41};
    ev_t ev[1200];
    uint32_t nev = 0, bar, s, f, i, step_len = FS * 60u / 120u / 4u, steps = 12u * 16u, frames = (steps + 32u) * step_len;
    int32_t peak = 0;
    FILE *w;
    for (bar = 0; bar < 12u; bar++) {
        uint32_t c = bar % 4u, b0 = bar * 16u;
        for (i = 0; i < 3u; i++)
            ev[nev++] = (ev_t){(uint16_t)b0, 0, CHORD[c][i], 15};
        for (s = 0; s < 16u; s += 2u)                   /* bass: root, its octave on the off-beats (TUNE -12) */
            ev[nev++] = (ev_t){(uint16_t)(b0 + s), 1, (uint8_t)(BASS[c] + ((s & 6u) == 4u ? 12u : 0u)), 1};
        if (bar == 3u) {                                /* a tom fill into the tune */
            for (s = 8; s < 16u; s++)
                ev[nev++] = (ev_t){(uint16_t)(b0 + s), 3, FILL[s - 8u], 1};
        } else {
            for (s = 0; s < 16u; s += 2u)
                ev[nev++] = (ev_t){(uint16_t)(b0 + s), 3, 42, 1};
            for (s = 0; s < 16u; s += 8u)
                ev[nev++] = (ev_t){(uint16_t)(b0 + s), 3, 36, 1};
            ev[nev++] = (ev_t){(uint16_t)(b0 + 6u), 3, 36, 1};
            for (s = 4; s < 16u; s += 8u)
                ev[nev++] = (ev_t){(uint16_t)(b0 + s), 3, 38, 1};
        }
        if (bar == 0u || bar == 4u || bar == 8u)
            ev[nev++] = (ev_t){(uint16_t)b0, 3, 49, 1};
        if (bar >= 4u)
            for (s = 0; s < 16u; s++) {
                uint32_t n = LEAD[((bar - 4u) % 4u) * 16u + s], len = 1;
                if (!n)
                    continue;
                while (s + len < 16u && !LEAD[((bar - 4u) % 4u) * 16u + s + len])
                    len++;
                ev[nev++] = (ev_t){(uint16_t)(b0 + s), 2, (uint8_t)n, (uint8_t)len};
            }
    }
    memset(trk, 0, sizeof trk);
    memset(snes_v, 0, sizeof snes_v);
    memset(snes_dsp, 0, sizeof snes_dsp);
    host_tracks_init();
    for (i = 0; i < 4u; i++) {
        host_preset(&trk[i], ENGI_SNES, snes_preset(PRE[i]));
        trk[i].p[P_LEVEL] = i == 0 ? 75 : i == 3 ? 85 : 100;
    }
    trk[0].p[P_PAN] = -20;
    trk[2].p[P_PAN] = 15;
    w = wav_open(dir, "snes_demo.wav", frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t st = f / step_len;
        if (f % step_len < CTL)                     /* this block starts a step */
            for (i = 0; i < nev; i++) {
                if (ev[i].step == st)
                    trk_note_on(&trk[ev[i].part], ev[i].note, ev[i].part == 2 ? 110 : 100);
                if (ev[i].step + ev[i].len == st)
                    trk_note_off(&trk[ev[i].part], ev[i].note);
            }
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            peak = o[i] > peak ? o[i] : -o[i] > peak ? -o[i] : peak;
        if (w)
            for (i = 0; i < CTL; i++)
                wav_put(w, o[2 * i], o[2 * i + 1]);
    }
    if (w)
        fclose(w);
    {
        char msg[120];
        snprintf(msg, sizeof msg, "4-part demo, %u s, peak %d -> snes_demo.wav", frames / FS, peak);
        check(msg, peak > 1000 && peak < 32000);
    }
}

/* ------------------------------------------------- 0. the bank in the user slots --- */
static void bank(const char *prefix)
{
    uint32_t parts, i, n;
    char msg[200];
    const param_desc_t *d;
    printf("0. the bank in the user slots (%s)\n", prefix);
    parts = snes_test_load(prefix, 1);              /* into USR2.. (not the first slot: any slot will do) */
    snes_bank_scan();
    d = ENG_SNES.desc(&trk[0], 0);
    snprintf(msg, sizeof msg, "%u parts in USR2..: %u INST values after the %u built in (%u standard), %s .. %s, knob to %d",
             parts, snes_bank.nsel, SNES_NFW, SNES_NSTD, snes_names[SNES_NFW], snes_names[SNES_NFW + snes_bank.nsel - 1],
             d->max);
    check(msg, parts >= 2u && snes_bank.nsel >= SNES_NSTD && snes_bank.slots == (((1u << parts) - 1u) << 1) &&
               d->max == (int16_t)(SNES_NFW + snes_bank.nsel - 1u) && !strcmp(snes_names[SNES_NFW], "SYN STR"));
    for (i = n = 0; i < SNES_NSTD; i++)              /* the presets' names = INST's (the layout they assume) */
        n += !strcmp(ENG_SNES.presets[SNES_NFW + i].name, snes_names[SNES_NFW + i]);
    snprintf(msg, sizeof msg, "the standard bank's %u entries are where the presets expect them (%u match)", SNES_NSTD, n);
    check(msg, n == SNES_NSTD);
    {                                               /* a part missing: no bank */
        unsigned char keep[0x14000];
        memcpy(keep, snes_test_slots + 2u * 0x14000u, sizeof keep);
        memset(snes_test_slots + 2u * 0x14000u, 0xFF, 0x14000u);
        snes_bank_scan();
        d = ENG_SNES.desc(&trk[0], 0);
        check("a part missing: no bank, INST stops at the built-in waves", !snes_bank.nsel && d->max == SNES_NFW - 1u &&
                                                                         !strcmp(snes_names[SNES_NFW], "-"));
        memcpy(snes_test_slots + 2u * 0x14000u, keep, sizeof keep);
    }
    {                                               /* a sample that runs off its part: no bank */
        unsigned char *d0 = snes_test_slots + 1u * 0x14000u + 512u, *m = d0 + snes_test_rd32(d0 + 12);   /* (sample 0: smp_off) */
        unsigned char keep[4];
        unsigned len = snes_test_rd32(snes_test_slots + 1u * 0x14000u + 16u);
        memcpy(keep, m + 4, 4);
        m[4] = (unsigned char)(len - 9u);
        m[5] = (unsigned char)((len - 9u) >> 8);
        m[6] = (unsigned char)((len - 9u) >> 16);
        m[7] = 0;
        snes_test_slots[1u * 0x14000u + 512u + len - 9u] = 0x00;   /* (a block with no end flag, at the end) */
        snes_bank_scan();
        check("a sample whose blocks run off its part: the bank is left out", !snes_bank.nsel);
        memcpy(m + 4, keep, 4);
        snes_test_load(prefix, 1);
        snes_bank_scan();
    }
    {                                               /* a slot of it erased while a note plays from it */
        int32_t o[2 * CTL];
        uint32_t b, loud = 0, after = 0;
        memset(trk, 0, sizeof trk);
        memset(snes_v, 0, sizeof snes_v);
        memset(snes_dsp, 0, sizeof snes_dsp);
        host_tracks_init();
        host_preset(&trk[0], ENGI_SNES, snes_preset("STRINGS"));
        trk[0].p[P_E4] = 0;                         /* (no echo, no sends: the voice alone) */
        trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
        trk_note_on(&trk[0], 60, 100);
        for (b = 0; b < 200u; b++) {                /* (the part's own output: no FX, no master) */
            track_render(&trk[0], o, CTL);
            for (i = 0; i < CTL; i++)
                loud += o[i] > 500 || o[i] < -500;
        }
        snes_bank_drop(1u);                         /* (editor.c ed_smp_erase, before the flash is erased) */
        memset(snes_test_slots + 1u * 0x14000u, 0xFF, 0x14000u);
        for (b = 0; b < 2u; b++) {                  /* (the voice's last two samples, interpolated out) */
            track_render(&trk[0], o, CTL);
        }
        for (b = 0; b < 200u; b++) {
            track_render(&trk[0], o, CTL);
            for (i = 0; i < CTL; i++)
                after += o[i] != 0;
        }
        snprintf(msg, sizeof msg, "a slot of the bank erased under a note: no bank, the voice silent (%u loud before, %u after)",
                 loud, after);
        check(msg, loud > 1000u && !after && !snes_bank.nsel && !voices_on());
        snes_test_load(prefix, 1);
        snes_bank_scan();
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: snes_test BANK REF.bin OUTDIR\n");
        return 2;
    }
    if (ENGINES[ENGI_SNES] != &ENG_SNES) {
        fprintf(stderr, "snes_test: engine %u is not SNES\n", ENGI_SNES);
        return 2;
    }
    bank(argv[1]);
    if (strcmp(argv[2], "-"))                       /* ("-": no reference, snes_spc absent) */
        against_ref(argv[2]);
    counters();
    presets(argv[3]);
    sharing();
    printf("3. a demo\n");
    demo(argv[3]);
    printf(bad ? "snes_test: %d FAILED\n" : "snes_test: all passed\n", bad);
    return bad != 0;
}
