/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host-side render of the FELUCCA DSP (engines, voices, FX, sequencer) to a WAV,
 * for debugging sound without hardware. Same sources as the firmware.
 *   tests/run_tests.sh builds it into build/host/;
 *   build/host/hostsim ENGINE PRESET MONO OUT.wav [CHORUS]
 * env: SECS=n renders n s (the 2 s note pattern repeats), CHORD[=k] holds k keys
 * (default 4, up to 8), DRUMS=1 adds GM drum hits, BENCH=1 prints the render time,
 * DIST=d, LEVEL=l, SENDS=c,d,r, SWEEP=1, NOTE=k, OCT=o, PSET=id:v,..., VSWEEP=1, PSWEEP=id:a:b,
 * PRESET=1 (the whole preset: sends, ARP, voice mode), STEPS=n,n,... (a pattern, 0 = rest; see below).
 * TRACKS=DIR: the 4-track test (tracks_demo below): a bass / pad / lead / drums pattern
 * with live recording into DIR (mix + solos), checks, and the cost against one track. */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
static void fm1_irq_off(void) {}                 /* (the host: one thread) */
static void fm1_irq_on(void) {}
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/drums.c"
#include "../firmware/src/params.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/slicer.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"                /* TRS MIDI IN: its parser (um_byte) feeds midi_in_q */
#if FELUCCA_ARRANGER
#include "../firmware/src/arranger.c"
#endif
#include "../firmware/src/seq.c"
#define inst (trk[0])                   /* the single-part renders below: part 1 */

static void wav_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    uint16_t a = 1, ch = 2, ba = 4, bits = 16; uint32_t sr = FS, br = FS * 4;
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
}

static void wav_put(FILE *f, int32_t l, int32_t r)
{
    int16_t s[2] = {(int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l),
                    (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r)};
    fwrite(s, 2, 2, f);
}

/* ---------------------------------------------------------------- TRACKS --- */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void host_tracks_init(void)                /* as felucca_init: defaults, empty patterns */
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = 120;                         /* (the tests' tempo; SLOOP powers on at 90) */
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        steps_clear(&trk[k]);
    }
    song.master_q12 = 4096;
}

/* the factory preset as ui.c apply_preset_to sets it (sound, sends, arp; not the pattern, not the mix);
 * the engine as the UI asks for it (eng_req: the audio side switches; host_preset: at once) */
static void host_preset_req(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const preset_t *p = &ENGINES[e]->presets[pi % ENGINES[e]->npresets];
    uint32_t i;
    t->eng_req = (uint8_t)e;
    t->preset = (uint8_t)(pi % ENGINES[e]->npresets);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_ED_FX] = preset_trim(e, pi % ENGINES[e]->npresets);
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++) {
        t->p[P_DIST + i] = (int16_t)(p->fx[i] ? p->fx[i] - 1 : FX_DEF[i]);
        t->p[P_AMODE + i] = (int16_t)(p->arp[i] ? p->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    preset_extras(t->p, p);
    fm6_track_loaded(t);                          /* FM6: the preset's patch (ui.c apply_preset_to) */
}
static void host_preset(track_t *t, uint32_t e, uint32_t pi)
{
    host_preset_req(t, e, pi);
    t->engine = (uint8_t)e;
}

#ifndef HOST_HAS_REAL_UI
void midi_pc(track_t *t, uint32_t prog)
{
    if (!t)
        return;
    if (is_drum(t)) {
        t->p[P_E0] = (int16_t)(prog % DRUM_KITS);
    } else if (t->eng_req == ENGI_FM6) {
        t->p[P_E7] = (int16_t)(prog % FM6_NSLOT);
        fm6_track_loaded(t);
    } else {
        host_preset_req(t, t->eng_req, prog);
    }
}
#endif

/* the sequencer clock as the harness sees it: the step of t at the clock (as the next block plays it),
 * *q100: how far into it, in % of its (swung) length */
static uint32_t at_step(const track_t *t, uint32_t *q100)
{
    uint32_t into, len, abs = trk_grid(t, &into, &len);
    if (q100)
        *q100 = (uint32_t)((uint64_t)into * 100u / len);
    return abs % trk_len(t);
}

static void put_step(track_t *t, uint32_t i, uint32_t n, const uint8_t *notes, uint32_t time, uint32_t flags)
{
    step_t *s = &t->step[i];
    uint32_t k;
    if (is_drum(t)) {                            /* the drum track: GM notes -> its lanes (accent: hard) */
        memset(&t->dstep[i], 0, sizeof t->dstep[i]);
        for (k = 0; k < n && time == ST_NOTE; k++)
            dstep_set(&t->dstep[i], lane_of_note(notes[k]), (flags & SF_ACCENT) ? LV_HARD : LV_NORM, 0);
        return;
    }
    s->n = (uint8_t)n;
    for (k = 0; k < 4u; k++)
        s->note[k] = k < n ? notes[k] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
}

static uint32_t busy_now(void)                   /* sounding voices of the parts (not the ones fading out: given up) */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

/* the demo song: T1 ANALOG ACID (16 steps), T2 DIGITAL PAD (32 steps, tied chords), T3 LOFI PULSE LD
 * (12 steps: 3 against 4), T4 drums (16 steps, up to 3 notes a step); 120 BPM, 8 bars. Bars 5..6:
 * live recording: a clap into the drums just before step 4 (quantised onto it, not triggered twice),
 * MIDI ch 3 into the lead, a two-key chord on the keys into an empty pad step. solo: 0 = the mix,
 * 1..4 = that track only. Writes DIR/NAME; returns the number of failed checks. */
static int tracks_demo(const char *dir, const char *name, uint32_t solo)
{
    static const uint8_t ACID[16] = {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50};
    static const uint8_t ACIDF[16] = {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1};
    static const uint8_t AM[4] = {57, 60, 64, 67}, FM[4] = {53, 57, 60, 64};
    static const uint8_t LEAD[12] = {76, 0, 0, 79, 0, 0, 81, 0, 79, 0, 76, 0};
    const uint32_t frames = 16u * FS, bar = 2u * FS;
    char path[512];
    FILE *w;
    uint32_t f, i, bmax = 0, clips = 0, fail = 0, clap_done = 0, lead_done = 0, keys_t = 0, keys_on = 0;
    uint32_t clap_age = 0, clap_hits = 0xFFFFu, clap_pass = 0, kills0;
    track_t *t1 = &trk[0], *t2 = &trk[1], *t3 = &trk[2], *td = TDRUM;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(w = fopen(path, "wb"))) {
        fprintf(stderr, "tracks: cannot write %s\n", path);
        return 1;
    }
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(t1, 0, 4);
    host_preset(t2, 1, 5);
    host_preset(t3, 3, 0);
    for (i = 0; i < 16u; i++) {
        uint8_t n = ACID[i];
        put_step(t1, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, ACIDF[i]);
    }
    t2->p[P_SLEN] = 32;
    t2->p[P_SGATE] = 120;
    for (i = 0; i < 32u; i++)
        put_step(t2, i, i % 16u == 0u ? 4u : 0u, i < 16u ? AM : FM, i % 16u == 0u ? ST_NOTE : i % 16u < 14u ? ST_TIE : ST_REST, 0);
    t3->p[P_SLEN] = 12;
    for (i = 0; i < 12u; i++) {
        uint8_t n = LEAD[i];
        put_step(t3, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, i == 0u ? SF_ACCENT : 0u);
    }
    for (i = 0; i < 16u; i++) {                    /* kick 4 on the floor, snare 4 / 12, hats on the 8ths */
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        if (i % 2u == 0u)
            n[k++] = i == 14u ? 46 : 42;
        put_step(td, i, k, n, k ? ST_NOTE : ST_REST, i % 4u == 0u ? SF_ACCENT : 0u);
    }
    if (solo) {                                    /* the other tracks silent */
        for (i = 0; i < NPART; i++)
            if (i + 1u != solo)
                trk[i].p[P_LEVEL] = 0;
        if (solo != 4u)
            song.g[G_DRLVL] = 0;
    }
    kills0 = voice_kills;
    transport_req = 1;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t barn = f / bar, period = div_samples(2), q;
        if (barn == 4u && !song.rec)
            song.rec = 0x0Eu;                      /* bars 5..6: tracks 2, 3, 4 armed */
        if (barn == 6u)
            song.rec = 0;
        /* (a) a clap into the drums, late in step 3: lands on step 4, sounds now, step 4 does not repeat it */
        if (song.rec && !clap_done && at_step(td, &q) == 3u && q > 75u) {
            input_on(td, 39, 110);
            clap_done = 1;
            clap_age = drums.age;
            clap_pass = 1;
        } else if (clap_pass == 1u && td->seq_idx == 4u) {
            clap_hits = drums.age - clap_age;      /* what step 4 triggered right after (36, 38, 42; not 39) */
            clap_pass = 2;
        }
        /* (b) MIDI ch 3 into the lead, early in its step 1 */
        if (song.rec && !lead_done && at_step(t3, &q) == 1u && q < 25u) {
            midi_in_q[mi_w++ % MQ] = 0x09u | 0x92u << 8 | 84u << 16 | 100u << 24;
            lead_done = 1;
        } else if (lead_done == 1u && t3->seq_idx == 2u) {
            midi_in_q[mi_w++ % MQ] = 0x08u | 0x82u << 8 | 84u << 16;
            lead_done = 2;
        }
        /* (c) the keys, track 2 selected: two keys at once into the empty pad step 14 */
        if (song.rec && !keys_on && !keys_t && at_step(t2, &q) == 14u && q < 25u) {
            song.sel = 1;
            fm1_in.notes = (1u << 7) | (1u << 11);
            keys_on = 1;
            keys_t = f;
        } else if (keys_on && f - keys_t > period / 2u) {
            fm1_in.notes = 0;
            keys_on = 0;
        }
        mix_block(o, CTL);
        if (busy_now() > bmax)
            bmax = busy_now();
        for (i = 0; i < CTL; i++) {
            int32_t l = o[2 * i];
            if (l >= 32700 || l <= -32700)
                clips++;
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    if (solo)
        return bmax > NVOICE;
    {
        const dstep_t *s4 = &td->dstep[4];
        const step_t *l1 = &t3->step[1], *p14 = &t2->step[14];
        uint32_t lanes = dstep_mask(s4), nl = 0, m;
        int ok_clap;
        for (m = lanes; m; m >>= 1)
            nl += m & 1u;
        ok_clap = nl == 4u && dstep_has(s4, lane_of_note(39)) && clap_hits == 3u;
        int ok_lead = l1->n == 1u && l1->note[0] == 84u && l1->time == ST_NOTE;
        int ok_keys = p14->n == 2u && p14->time == ST_NOTE && p14->note[0] == 60u && p14->note[1] == 64u;
        printf("tracks: recording: drums step 4 = %u lanes (kick snare hat + clap: %s), hits when step 4 played right "
               "after: %u (want 3: the clap is not triggered twice) %s\n", nl, dstep_has(s4, lane_of_note(39)) ? "yes" : "no",
               clap_hits, ok_clap ? "ok" : "FAIL");
        printf("tracks: recording: MIDI ch 3 -> lead step 1 = %u (want 84) %s; keys -> pad step 14 = %u notes %u %u %s\n",
               l1->note[0], ok_lead ? "ok" : "FAIL", p14->n, p14->note[0], p14->note[1], ok_keys ? "ok" : "FAIL");
        printf("tracks: voices sounding at most %u (budget %u) %s; given up to another part %u; %u samples near full "
               "scale\n", bmax, NVOICE, bmax <= NVOICE ? "ok" : "FAIL", voice_kills - kills0, clips);
        fail = !ok_clap + !ok_lead + !ok_keys + (bmax > NVOICE) + (clips > 0);
    }
    return (int)fail;
}

/* ns per 44.1 kHz sample of the whole mix (events, parts, drums on 16ths, buses, master) with notes
 * held: parts[k] = {engine, preset, notes} for part k, 0 notes = idle. 2 s to settle, then the
 * fastest of four 2 s stretches (the host's other load only ever adds). Runs in a child process
 * (a clean state each time); the result comes back through a pipe. */
static double tracks_cost(const uint8_t parts[NPART][3], uint32_t *busy_max)
{
    int fd[2];
    pid_t pid;
    double r[2] = {-1, 0};
    if (pipe(fd))
        return -1;
    fflush(stdout);
    if (!(pid = fork())) {
        static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
        uint64_t ns = 0, best = ~0ull, k, nblk = 8u * FS / CTL, seg = nblk / 4u;   /* the fastest of 4 x 2 s: */
        uint32_t p, i, bm = 0;
        int32_t o[2 * CTL];
        host_tracks_init();
        for (p = 0; p < NPART; p++) {
            host_preset(&trk[p], parts[p][0], parts[p][1]);
            trk[p].p[P_VOICE] = V_POLY;
            trk[p].p[P_SUS] = 127;                  /* held notes keep sounding */
            trk[p].p[P_AMODE] = 0;
        }
        for (p = 0; p < NPART; p++)
            for (i = 0; i < parts[p][2]; i++)
                trk_note_on(&trk[p], NOTES[i] + 12u * p, 100);
        for (k = 0; k < 2u * FS / CTL + nblk; k++) {   /* 2 s to settle (attacks), then measure */
            uint64_t t0;
            if ((k * CTL) % (FS / 8u) < CTL)          /* 16ths at 120 BPM */
                drum_on((k * CTL) % (FS / 2u) < CTL ? 36u : ((k * CTL) / (FS / 8u)) % 4u == 2u ? 38u : 42u, 100u);
            t0 = now_ns();
            mix_block(o, CTL);
            if (k >= 2u * FS / CTL) {                  /* (the host's own noise only ever adds) */
                ns += now_ns() - t0;
                if ((k - 2u * FS / CTL) % seg == seg - 1u) {
                    best = ns < best ? ns : best;
                    ns = 0;
                }
            }
            if (busy_now() > bm)
                bm = busy_now();
        }
        r[0] = (double)best / (double)(seg * CTL);
        r[1] = bm;
        if (write(fd[1], r, sizeof r) != sizeof r)
            _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r)
        r[0] = -1;
    close(fd[0]);
    waitpid(pid, 0, 0);
    if (busy_max)
        *busy_max = (uint32_t)r[1];
    return r[0];
}

/* stealing across parts must not click: three parts of plain sines (smooth: the largest sample
 * step is set by the pitches), 7 + 1 notes fill the budget, then notes on parts 2 and 3 take held
 * voices of part 1 (and part 2). Each taken voice fades over one block. Compares the largest sample
 * step around each take with the largest one in the 0.25 s before it; a hard cut would be several
 * times larger. Writes DIR/steal.wav. */
static int steal_test(const char *dir)
{
    static const uint8_t N1[7] = {48, 52, 55, 59, 62, 65, 69};
    const uint32_t frames = 4u * FS;
    char path[512];
    FILE *w;
    int32_t prev = 0, *L = calloc(frames, sizeof *L);
    uint32_t f, i, p, k, events[6], nev = 0, bad = 0, kills0;
    double worst = 0;
    snprintf(path, sizeof path, "%s/steal.wav", dir);
    if (!L || !(w = fopen(path, "wb")))
        return 1;
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        track_t *t = &trk[p];
        host_preset(t, 0, 5);                      /* ANALOG SINE KEY, as a plain held sine: */
        t->p[P_E4] = 127;                          /* filter open, no resonance, no drive */
        t->p[P_E5] = t->p[P_E6] = 0;
        t->p[P_ED_FLT] = 0;
        t->p[P_ATK] = 40;                          /* slow attack, full sustain, no sends */
        t->p[P_SUS] = 127;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        t->p[P_VOICE] = V_POLY;
        t->p[P_LEVEL] = 90;
    }
    kills0 = voice_kills;
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t ms = f * 1000u / FS, k0 = voice_kills;
        if (f == 0)
            for (i = 0; i < 7u; i++)
                trk_note_on(&trk[0], N1[i], 90);
        if (f == FS / 2u / CTL * CTL)
            trk_note_on(&trk[1], 74, 90);          /* 8: the budget is full */
        if (ms >= 1000u && ms % 500u == 0u && f % (FS / 2u) < CTL && ms < 3500u) {
            p = ms / 500u % 2u ? 1u : 2u;          /* parts 2, 3 in turn: each takes a voice */
            trk_note_on(&trk[p], 76u + ms / 250u, 90);
        }
        mix_block(o, CTL);
        if (voice_kills != k0 && nev < 6u)
            events[nev++] = f;
        for (i = 0; i < CTL; i++) {
            L[f + i] = o[2 * i];
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    for (k = 0; k < nev; k++) {                    /* around the take vs. the 0.25 s before */
        int32_t calm = 0, at = 0;
        uint32_t e = events[k];
        for (i = e - FS / 4u; i < e - 2u * CTL; i++)
            calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
        for (i = e; i < e + 2u * CTL; i++)
            at = abs(L[i] - L[i - 1]) > at ? abs(L[i] - L[i - 1]) : at;
        if ((double)at / calm > worst)
            worst = (double)at / calm;
        if (at > calm * 3 / 2)
            bad++;
    }
    (void)prev;
    printf("tracks: stealing across parts: %u voices taken; largest sample step at a take / before it: at most %.2f x "
           "(a hard cut: several x) %s\n", voice_kills - kills0, worst, nev >= 4u && !bad ? "ok" : "FAIL");
    free(L);
    return nev < 4u || bad;
}

/* a preset change across engines while notes sound (the PRESETS knob, the editor's PRESET) must not click:
 * part 1 as a plain sine pad (ANALOG SINE KEY, filter open, slow release, no sends: smooth, so the largest
 * sample step is set by the pitches), (a) a chord held and released, the switch to DIGITAL in its release
 * tail; (b) twice a chord held, the switch to PHASE / LOFI while it is held (the preset change releases
 * it); (c) a switch to SAMPLE with a note in its second block: that note must sound on the new engine. As ui.c
 * set_engine_of + apply_preset_to: eng_req, the new engine's values and panic_req at once (the main loop
 * writes them with the audio IRQ off). Compares the largest sample step in the 10 ms after (a) and (b)
 * with the largest one in the 0.1 s before (a hard cut of the voices: many times larger).
 * Writes DIR/engine_switch.wav. */
static void xfade_sine(track_t *t)
{
    host_preset(t, 0, 5);                          /* ANALOG, as a plain held sine */
    t->p[P_E0] = 3;                                /* (SIN, whichever preset that is) */
    t->p[P_E1] = t->p[P_E2] = t->p[P_E3] = 0;
    t->p[P_ED_FX] = 0;
    t->p[P_E4] = 127;
    t->p[P_E5] = t->p[P_E6] = 0;
    t->p[P_ED_FLT] = 0;
    t->p[P_ATK] = 40;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 100;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_VOICE] = V_POLY;
    t->p[P_LEVEL] = 100;
}
static int xfade_test(const char *dir)
{
    static const uint8_t CH[3] = {57, 60, 64};
    const uint32_t frames = 4u * FS, B = FS / 10u / CTL * CTL;   /* B: 0.1 s in whole blocks */
    /* switches: (a) in a release tail, (b) twice with a chord held, (c) with a note right after it */
    const uint32_t sw[4] = {11u * B, 23u * B, 29u * B + 7u * CTL, 33u * B};
    const uint32_t on[3] = {B, 15u * B, 25u * B}, sine[3] = {14u * B, 24u * B, 32u * B};
    const uint8_t to[4] = {1, 2, 3, 4};
    char path[512];
    FILE *w;
    int32_t *L = calloc(frames, sizeof *L);
    uint32_t f, i, k, bad = 0, late_ok = 0;
    double worst = 0;
    track_t *t = &trk[0];
    snprintf(path, sizeof path, "%s/engine_switch.wav", dir);
    if (!L || !(w = fopen(path, "wb")))
        return 1;
    host_tracks_init();
    xfade_sine(t);
    wav_hdr(w, frames);
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        for (k = 0; k < 3u; k++) {
            if (f == sine[k])
                xfade_sine(t);                     /* silent since the switch before: the sine again at once */
            if (f == on[k])
                for (i = 0; i < 3u; i++)
                    trk_note_on(t, CH[i] - 2u * k, 100);
        }
        if (f == 9u * B)
            for (i = 0; i < 3u; i++)
                trk_note_off(t, CH[i]);            /* (a): the release tail rings */
        for (k = 0; k < 4u; k++)
            if (f == sw[k]) {
                host_preset_req(t, to[k], 0);
                t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
                panic_req |= 1u;
            } else if (k == 3u && f == sw[k] + CTL) {
                trk_note_on(t, 60, 100);           /* (c): a note during the fade */
            }
        if (f == sw[3] + FS / 50u / CTL * CTL) {
            uint32_t v;
            for (v = 0; v < NVOICE; v++)
                late_ok |= t->v[v].active && t->v[v].note == 60u && t->v[v].gate;
            late_ok = late_ok && t->engine == to[3];
            trk_note_off(t, 60);
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            L[f + i] = o[2 * i];
            wav_put(w, o[2 * i], o[2 * i + 1]);
        }
    }
    fclose(w);
    for (k = 0; k < 3u; k++) {
        int32_t calm = 1, at = 0;
        for (i = sw[k] - FS / 10u; i < sw[k]; i++)
            calm = abs(L[i] - L[i - 1]) > calm ? abs(L[i] - L[i - 1]) : calm;
        for (i = sw[k]; i < sw[k] + FS / 100u; i++)
            at = abs(L[i] - L[i - 1]) > at ? abs(L[i] - L[i - 1]) : at;
        printf("tracks: engine switch %s -> %s (%s): largest sample step %d in the 10 ms after, %d in the 0.1 s "
               "before (%.2f x)\n", ENGINES[0]->name, ENGINES[to[k]]->name, k ? "chord held" : "release tail", at,
               calm, (double)at / calm);
        worst = (double)at / calm > worst ? (double)at / calm : worst;
        bad += at > calm * 3 / 2;
    }
    printf("tracks: engine switch: at most %.2f x the step before (a hard cut: several x) %s\n", worst, bad ? "FAIL" : "ok");
    printf("tracks: a note during the switch sounds on the new engine (%s) %s\n", ENGINES[to[3]]->name,
           late_ok ? "ok" : "FAIL");
    free(L);
    return bad || !late_ok;
}

/* live recording: held lengths (TIE steps) and swing-aware quantising. Blocks are rendered until track t
 * is at fraction q (1/100) into step idx (its swung length); the keys go in between, as events_block
 * takes them. */
static void rec_run_to(track_t *t, uint32_t idx, uint32_t q)
{
    int32_t o[2 * CTL];
    uint32_t guard = 0, at;
    do {
        mix_block(o, CTL);
    } while (++guard < 100000u && !(at_step(t, &at) == idx && at >= q));
}
static int rec_step_is(const track_t *t, uint32_t i, uint32_t time, uint32_t n, uint32_t note)
{
    const step_t *s = &t->step[i];
    return s->time == time && s->n == n && (!n || s->note[0] == note);
}
static int rec_test(void)
{
    track_t *t1 = &trk[0], *t2 = &trk[1], *t3 = &trk[2];
    uint32_t i;
    int ok_len, ok_short, ok_half, ok_cap, ok_swing, ok_mono, ok_chord, fail;
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(t1, 0, 1);                          /* POLY pad */
    host_preset(t2, 0, 1);
    host_preset(t3, 0, 0);                          /* SAW LEAD */
    t1->p[P_VOICE] = t2->p[P_VOICE] = V_POLY;
    t3->p[P_VOICE] = V_MONO;
    t2->p[P_SLEN] = 4;
    t3->p[P_SSWING] = 50;                           /* odd steps start 0.2 step late */
    put_step(t1, 5, 1, (const uint8_t[]){48}, ST_NOTE, 0);   /* an older take: put back by an early release */
    song.rec = 0x07u;
    transport_req = 1;
    /* (a) held 3.3 steps from step 2: 2 NOTE, 3 4 TIE, 5 (released early in it) as before */
    rec_run_to(t1, 2, 10);
    input_on(t1, 60, 100);
    rec_run_to(t1, 5, 30);
    input_off(t1, 60);
    ok_len = rec_step_is(t1, 2, ST_NOTE, 1, 60) && rec_step_is(t1, 3, ST_TIE, 0, 0) && rec_step_is(t1, 4, ST_TIE, 0, 0) &&
             rec_step_is(t1, 5, ST_NOTE, 1, 48);
    /* (b) released after 0.3 step: one step */
    rec_run_to(t1, 8, 5);
    input_on(t1, 62, 100);
    rec_run_to(t1, 8, 35);
    input_off(t1, 62);
    rec_run_to(t1, 9, 50);
    ok_short = rec_step_is(t1, 8, ST_NOTE, 1, 62) && rec_step_is(t1, 9, ST_REST, 0, 0);
    /* (c) released past the middle of the next step: tied into it */
    rec_run_to(t1, 10, 10);
    input_on(t1, 64, 100);
    rec_run_to(t1, 11, 60);
    input_off(t1, 64);
    rec_run_to(t1, 12, 50);
    ok_half = rec_step_is(t1, 10, ST_NOTE, 1, 64) && rec_step_is(t1, 11, ST_TIE, 0, 0) && rec_step_is(t1, 12, ST_REST, 0, 0);
    /* (d) a two-key chord held 2.6 steps: ties until the last key is up */
    rec_run_to(t1, 13, 5);
    input_on(t1, 67, 100);
    input_on(t1, 71, 100);
    rec_run_to(t1, 14, 20);
    input_off(t1, 67);
    rec_run_to(t1, 15, 60);
    input_off(t1, 71);
    rec_run_to(t1, 0, 10);
    ok_chord = t1->step[13].n == 2u && rec_step_is(t1, 14, ST_TIE, 0, 0) && rec_step_is(t1, 15, ST_TIE, 0, 0) &&
               t1->step[0].time != ST_TIE;
    /* (e) LEN 4, held for 10 steps: the note fills the pattern (3 TIEs), not more */
    rec_run_to(t2, 1, 10);
    input_on(t2, 55, 100);
    for (i = 0; i < 10u; i++)
        rec_run_to(t2, (2u + i) % 4u, 50);
    input_off(t2, 55);
    ok_cap = rec_step_is(t2, 1, ST_NOTE, 1, 55) && rec_step_is(t2, 2, ST_TIE, 0, 0) && rec_step_is(t2, 3, ST_TIE, 0, 0) &&
             rec_step_is(t2, 0, ST_TIE, 0, 0);
    /* (f) SWING 50 %: 0.55 into an even (long) step is before its middle (stays); 0.56 period into an odd
     * (short, 0.8) step is past its middle and the key-to-ear latency REC_LAT (the next one) */
    rec_run_to(t3, 4, 46);                         /* 0.46 x 1.2 = 0.55 period */
    input_on(t3, 72, 100);
    input_off(t3, 72);
    rec_run_to(t3, 7, 70);                         /* 0.70 x 0.8 = 0.56 period */
    input_on(t3, 74, 100);
    input_off(t3, 74);
    ok_swing = rec_step_is(t3, 4, ST_NOTE, 1, 72) && t3->step[5].n == 0u && rec_step_is(t3, 8, ST_NOTE, 1, 74) &&
               t3->step[7].n == 0u;
    /* (g) MONO, legato: A held from step 10, B pressed in step 12 while A is still down, A up, B up late in
     * 13: 10 A, 11 TIE, 12 B (one note), 13 TIE */
    rec_run_to(t3, 10, 10);
    input_on(t3, 60, 100);
    rec_run_to(t3, 12, 10);
    input_on(t3, 62, 100);
    rec_run_to(t3, 12, 40);
    input_off(t3, 60);
    rec_run_to(t3, 13, 70);
    input_off(t3, 62);
    rec_run_to(t3, 14, 50);
    ok_mono = rec_step_is(t3, 10, ST_NOTE, 1, 60) && rec_step_is(t3, 11, ST_TIE, 0, 0) && rec_step_is(t3, 12, ST_NOTE, 1, 62) &&
              rec_step_is(t3, 13, ST_TIE, 0, 0) && t3->step[14].time != ST_TIE;
    printf("tracks: recording lengths: held 3.3 steps -> NOTE TIE TIE (the 4th step put back) %s; 0.3 step -> one "
           "step %s; past the middle of the next -> NOTE TIE %s\n", ok_len ? "ok" : "FAIL", ok_short ? "ok" : "FAIL",
           ok_half ? "ok" : "FAIL");
    printf("tracks: recording lengths: a chord ties until its last key %s; capped at LEN 4 %s; MONO legato A..B -> "
           "A TIE B TIE %s\n", ok_chord ? "ok" : "FAIL", ok_cap ? "ok" : "FAIL", ok_mono ? "ok" : "FAIL");
    printf("tracks: recording with SWING 50 %%: nearest swung step, as heard (0.55 into a long step stays, 0.56 into a "
           "short one moves on) %s\n", ok_swing ? "ok" : "FAIL");
    fail = !ok_len + !ok_short + !ok_half + !ok_chord + !ok_cap + !ok_mono + !ok_swing;
    return fail;
}

/* TRS MIDI IN (midi_uart.c, untested on hardware): bytes through its parser into the same queue as
 * USB, routed by channel: 1..3 -> parts 1..3, 10 -> drums, others -> the selected track; running
 * status, note-on velocity 0 = note-off; a note-off on a "selected track" channel reaches the track
 * its note-on went to after another track was selected; recording into an armed track. */
static int trs_held(const track_t *t, uint32_t note)
{
    uint32_t v;
    for (v = 0; v < NVOICE; v++)
        if (t->v[v].active && t->v[v].gate && t->v[v].note == note)
            return 1;
    return 0;
}
static void trs_bytes(const uint8_t *b, uint32_t n)
{
    int32_t o[2 * CTL];
    while (n--)
        um_byte(*b++);
    mix_block(o, CTL);
}
static int trs_test(void)
{
    uint32_t d0, i;
    int ok_parts, ok_drum, ok_sel, ok_off, ok_hang, ok_rec;
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 1);
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_SUS] = 127;
    }
    song.sel = 1;
    d0 = drums.age;
    trs_bytes((const uint8_t[]){0x90, 60, 100, 65, 0xF8, 100, 0x91, 62, 100, 0x92, 64, 100, 0x99, 36, 110, 0x94, 67, 90}, 18);
    ok_parts = trs_held(&trk[0], 60) && trs_held(&trk[0], 65) && trs_held(&trk[1], 62) && trs_held(&trk[2], 64) && !trs_held(&trk[0], 62);
    ok_drum = drums.age == d0 + 1u;
    ok_sel = trs_held(&trk[1], 67) && !trs_held(&trk[0], 67);
    trs_bytes((const uint8_t[]){0x90, 60, 0, 65, 0, 0x81, 62, 0, 0x82, 64, 64}, 11);   /* vel 0 = off, 0x8n */
    ok_off = !trs_held(&trk[0], 60) && !trs_held(&trk[0], 65) && !trs_held(&trk[1], 62) && !trs_held(&trk[2], 64);
    song.sel = 2;                                   /* another track selected while ch 5's note is down */
    trs_bytes((const uint8_t[]){0x84, 67, 0}, 3);
    ok_hang = !trs_held(&trk[1], 67);
    song.rec = 1u << 2;                             /* track 3 armed, transport on: ch 3 records */
    transport_req = 1;
    trs_bytes((const uint8_t[]){0xF8}, 1);
    trs_bytes((const uint8_t[]){0x92, 72, 100}, 3);
    trs_bytes((const uint8_t[]){0x92, 72, 0}, 3);
    ok_rec = trk[2].step[0].n == 1u && trk[2].step[0].note[0] == 72u && trk[2].step[0].time == ST_NOTE && trk[1].step[0].n == 0u;
    printf("tracks: TRS MIDI IN: ch 1..3 -> parts %s, ch 10 -> drums %s, ch 5 -> the selected track %s; note-offs "
           "(running status, vel 0) %s\n", ok_parts ? "ok" : "FAIL", ok_drum ? "ok" : "FAIL", ok_sel ? "ok" : "FAIL",
           ok_off ? "ok" : "FAIL");
    printf("tracks: TRS MIDI IN: note-off after another track was selected reaches the note's track %s; ch 3 records "
           "into armed track 3 %s\n", ok_hang ? "ok" : "FAIL", ok_rec ? "ok" : "FAIL");
    return !ok_parts + !ok_drum + !ok_sel + !ok_off + !ok_hang + !ok_rec;
}

static int tracks_test(const char *dir)
{
    static const char *const SOLO[5] = {"tracks_demo.wav", "t1_bass.wav", "t2_pad.wav", "t3_lead.wav", "t4_drums.wav"};
    uint32_t e, pi, s, worst_e = 0, worst_p = 0, heavy[NENGINES] = {0}, bm = 0;
    double worst = 0, best_e[NENGINES] = {0}, four, four_full;
    int fail = 0;
    for (s = 0; s < 5u; s++) {
        pid_t pid;
        fflush(stdout);
        pid = fork();
        int st = 0;
        if (!pid)
            { int rc = tracks_demo(dir, SOLO[s], s); fflush(stdout); _exit(rc); }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
    }
    {
        pid_t pid;
        int st = 0;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int rc = steal_test(dir);
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int rc = trs_test();
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int rc = rec_test();
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            int rc = xfade_test(dir);
            fflush(stdout);
            _exit(rc);
        }
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st))
            fail++;
    }
    printf("tracks: WAVs in %s: %s (mix), %s, %s, %s, %s, steal.wav, engine_switch.wav\n", dir, SOLO[0], SOLO[1], SOLO[2], SOLO[3], SOLO[4]);
    /* one part: every preset with 8 held notes (the engine's cap: VOICE 4) + drums; the worst one */
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ENGINES[e]->npresets; pi++) {
            uint8_t parts[NPART][3] = {{(uint8_t)e, (uint8_t)pi, 8}, {0, 0, 0}, {0, 0, 0}};
            double c = tracks_cost(parts, 0);
            if (c > best_e[e]) {
                best_e[e] = c;
                heavy[e] = pi;
            }
            if (c > worst) {
                worst = c;
                worst_e = e;
                worst_p = pi;
            }
        }
    printf("tracks: one part, 8 notes held + drums (host -O2, ns per sample, heaviest preset per engine):");
    for (e = 0; e < NENGINES; e++)
        printf(" %s %s %.1f%s", ENGINES[e]->name, ENGINES[e]->presets[heavy[e]].name, best_e[e], e + 1u < NENGINES ? "," : "\n");
    {   /* DIGITAL, PHASE, VOICE (their heaviest presets) at once: 3 + 3 + 2 notes = the budget of 8 */
        uint8_t parts[NPART][3] = {{1, (uint8_t)heavy[1], 3}, {2, (uint8_t)heavy[2], 3}, {5, (uint8_t)heavy[5], 2}};
        uint8_t full[NPART][3] = {{1, (uint8_t)heavy[1], 8}, {2, (uint8_t)heavy[2], 8}, {5, (uint8_t)heavy[5], 4}};
        uint8_t vv[NPART][3] = {{5, (uint8_t)heavy[5], 4}, {5, (uint8_t)heavy[5], 4}, {0, 0, 0}};
        uint8_t idle[NPART][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        uint32_t bm2 = 0, bm3 = 0;
        double two_voice, none;
        four = tracks_cost(parts, &bm);
        four_full = tracks_cost(full, &bm2);
        two_voice = tracks_cost(vv, &bm3);
        none = tracks_cost(idle, 0);
        printf("tracks: worst single part: %s %s %.1f ns\n", ENGINES[worst_e]->name, ENGINES[worst_e]->presets[worst_p].name, worst);
        printf("tracks: DIGITAL + PHASE + VOICE + drums, 3 + 3 + 2 notes: %.1f ns (%.2f x the worst single part), "
               "%u voices\n", four, four / worst, bm);
        printf("tracks: the same, 8 + 8 + 4 notes asked for (budget keeps %u): %.1f ns (%.2f x)\n", bm2, four_full,
               four_full / worst);
        printf("tracks: two VOICE parts, 4 + 4 voices (the heaviest 8 the budget allows): %.1f ns (%.2f x); "
               "no notes (drums, buses, 3 idle parts): %.1f ns\n", two_voice, two_voice / worst, none);
        if (bm > NVOICE || bm2 > NVOICE || bm3 > NVOICE)
            fail++;
    }
    printf("tracks: %s\n", fail ? "FAILED" : "all checks ok");
    return fail;
}

int main(int argc, char **argv)
{
    int eng = argc > 1 ? atoi(argv[1]) : 0, preset = argc > 2 ? atoi(argv[2]) : 4;
    int mono = argc > 3 ? atoi(argv[3]) : 1;
    const char *out = argc > 4 ? argv[4] : "out.wav";
    uint32_t i, frames = FS * (getenv("SWEEP") ? 4 : 2), f, nk = 0;
    clock_t c0;
    FILE *w = fopen(out, "wb");
    if (getenv("SECS"))
        frames = FS * (uint32_t)atoi(getenv("SECS"));
    if (getenv("CHORD"))
        nk = atoi(getenv("CHORD")) > 4 ? (uint32_t)atoi(getenv("CHORD")) : 4u;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (i = 0; i < P_E0; i++) inst.p[i] = TP[i].def;
    inst.eng_req = inst.engine = (uint8_t)eng;
    for (i = 0; i < 8; i++) inst.p[P_E0 + i] = ENGINES[eng]->edit[i].def;
    if (ENGINES[eng]->npresets) {
        const preset_t *p = &ENGINES[eng]->presets[preset % ENGINES[eng]->npresets];
        for (i = 0; i < 8; i++) inst.p[P_E0 + i] = p->e[i];
        inst.p[P_ATK] = p->env[0]; inst.p[P_DEC] = p->env[1];
        inst.p[P_SUS] = p->env[2]; inst.p[P_REL] = p->env[3]; inst.p[P_ED_FLT] = p->fenv;
        preset_extras(inst.p, p);
    }
    fm6_track_loaded(&inst);                            /* FM6: the preset's patch */
    inst.p[P_VOICE] = (int16_t)mono;
    inst.p[P_CHOR] = argc > 5 ? atoi(argv[5]) : 24;      /* as felucca_init */
    inst.p[P_DLY] = argc > 5 ? atoi(argv[5]) : 28;
    inst.p[P_REV] = argc > 5 ? atoi(argv[5]) : 36;
    if (getenv("PRESET"))                               /* PRESET=1: sends, ARP, voice mode too (then SENDS, PSET) */
        host_preset(&inst, (uint32_t)eng, (uint32_t)preset);
    if (getenv("SENDS"))                                 /* SENDS=chorus,delay,reverb */
        sscanf(getenv("SENDS"), "%hd,%hd,%hd", &inst.p[P_CHOR], &inst.p[P_DLY], &inst.p[P_REV]);
    if (getenv("OCT"))                                   /* OCT=o: keyboard octave shift (key 7 = C4 + 12 o) */
        song.octave = (int8_t)atoi(getenv("OCT"));
    if (getenv("PSET")) {                               /* PSET=id:value,... (track parameter ids, P_E0 = 49) */
        const char *s = getenv("PSET");
        int id, val, k;
        while (sscanf(s, "%d:%d%n", &id, &val, &k) == 2) {
            if (id >= 0 && id < P_COUNT)
                inst.p[id] = (int16_t)val;
            s += k;
            if (*s == ',')
                s++;
        }
    }
    if (getenv("STEPS")) {                              /* STEPS=n,n,...: a pattern (0 = rest) played by the sequencer */
        const char *s = getenv("STEPS");
        int nt, k, c = 0;
        while (c < NSTEP && sscanf(s, "%d%n", &nt, &k) == 1) {
            uint8_t u = (uint8_t)nt;
            put_step(&inst, (uint32_t)c++, nt ? 1u : 0u, &u, nt ? ST_NOTE : ST_REST, 0);
            s += k;
            if (*s == ',')
                s++;
        }
        inst.p[P_SLEN] = (int16_t)c;
        transport_req = 1;
    }
    song.master_q12 = 4096;
    if (getenv("TRACKS")) {
        fclose(w);
        remove(out);
        return tracks_test(getenv("TRACKS"));
    }
    wav_hdr(w, frames);
    c0 = clock();
    for (f = 0; f < frames; f += CTL) {
        int32_t o[2 * CTL];
        uint32_t fp = f % (2u * FS);    /* position in the repeating 2 s pattern */
        {   /* C4 0.1-0.5 s, E4 0.45-0.8 s (overlap: legato/steal), G4 0.9-1.2 s */
            uint32_t n = 0, f = fp;
            if (f > FS / 10 && f < FS / 2) n |= 1u << 7;
            if (f > FS * 45 / 100 && f < FS * 8 / 10) n |= 1u << 11;
            if (f > FS * 9 / 10 && f < FS * 12 / 10) n |= 1u << 14;
            fm1_in.notes = getenv("STEPS") ? 0u : n;   /* STEPS: only the sequencer plays */
        }
        if (nk) {                       /* keys held 0.1-1.5 s (C E G B, then D F A C) */
            static const uint8_t K[8] = {7, 11, 14, 18, 9, 12, 16, 19};
            uint32_t n = 0, k;
            for (k = 0; k < nk && k < 8u; k++)
                n |= 1u << K[k];
            fm1_in.notes = (fp > FS / 10 && fp < FS * 3 / 2) ? n : 0;
        }
        if (getenv("DRUMS") && fp % (FS / 4u) < CTL)   /* kick / closed hat / snare on 8ths */
            drum_on(fp % (FS / 2u) < CTL ? 36u : (fp / (FS / 4u)) % 4u == 3u ? 38u : 42u, 100u);
        if (getenv("DIST"))
            inst.p[P_DIST] = (int16_t)atoi(getenv("DIST"));
        if (getenv("LEVEL"))
            inst.p[P_LEVEL] = (int16_t)atoi(getenv("LEVEL"));
        if (getenv("SWEEP")) {          /* cutoff and resonance sweeps while a note is held */
            uint32_t t = f * 1000 / FS;  /* ms */
            inst.p[P_E4] = (int16_t)((t / 4) % 256 < 128 ? (t / 4) % 128 : 127 - (t / 4) % 128);
            inst.p[P_E5] = (int16_t)((t / 7) % 256 < 128 ? (t / 7) % 128 : 127 - (t / 7) % 128);
            fm1_in.notes = (f > FS / 20) ? (1u << 7) : 0;
        }
        if (getenv("NOTE"))             /* NOTE=k: one key (7 = C4, see OCT) held to 0.5 s before the end */
            fm1_in.notes = (f > FS / 20 && f + FS / 2 < frames) ? 1u << atoi(getenv("NOTE")) : 0;
        if (getenv("VSWEEP"))           /* VSWEEP=1: EDIT 1 (P_E0) from 0 to 127 over the render */
            inst.p[P_E0] = (int16_t)((uint64_t)f * 128u / frames);
        if (getenv("PSWEEP")) {         /* PSWEEP=id:from:to: a track parameter from .. to over the render */
            int id, a, b;
            if (sscanf(getenv("PSWEEP"), "%d:%d:%d", &id, &a, &b) == 3 && id >= 0 && id < P_COUNT)
                inst.p[id] = (int16_t)(a + (int64_t)(b - a) * f / frames);
        }
        mix_block(o, CTL);
        {   /* VOICES=1: report the most voices active at once (engine voice caps) */
            static uint32_t vmax, hi;
            uint32_t k, a = 0;
            for (k = 0; k < NVOICE; k++)
                if (inst.v[k].active) {
                    a++;
                    hi |= 1u << k;
                }
            if (a > vmax)
                vmax = a;
            if (getenv("VOICES") && f + CTL >= frames)
                fprintf(stderr, "voices: at most %u active, slots used 0x%02X\n", vmax, hi);
        }
        for (i = 0; i < 2 * CTL; i++) {
            int16_t s = (int16_t)(o[i] > 32767 ? 32767 : o[i] < -32768 ? -32768 : o[i]);
            fwrite(&s, 2, 1, w);
        }
    }
    if (getenv("BENCH"))
        fprintf(stderr, "%.1f s audio in %.1f ms (%.2f %% of real time)\n", (double)frames / FS,
                (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC,
                (double)(clock() - c0) * 100.0 / CLOCKS_PER_SEC / ((double)frames / FS));
    fclose(w);
    return 0;
}
