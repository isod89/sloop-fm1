/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ANALOG: two band-limited oscillators (saw / square / tri / sine / PWM),
 * noise, drive and a trapezoidal low-pass. UNISON stacks oscillator 1 inside the voice: ANA_UNI
 * copies spread by DETUNE over the width eight voices had, through the one filter, drive and
 * envelope; the voice counts once in the budget (engine_t uni), so a unison chord keeps its notes. */
static const char *const N_ANALOG_WAVE[] = {"SAW", "SQR", "TRI", "SIN", "PWM"};
#define ANA_UNI 7u               /* UNISON: the centre and three a side */
#define ANA_UNI_GAIN 24773u      /* 1 / sqrt(7), Q16: about the level of one at random phases */
/* stack oscillator j: its detune in thirds of voice.c's unison step (outer ones at +-7, as eight voices had);
 * its phase: j 0 is ph[0], then ph[2] and s[3..7] (ANALOG's free state) */
static const int8_t ANA_SPREAD[ANA_UNI] = {0, -7, 7, -14, 14, -21, 21};

static void analog_note_on(track_t *t, voice_t *v)
{
    uint32_t j;
    if (!v->env && !v->env_out) {
        v->ph[0] = 0;                                 /* a fresh note: from phase 0 (an 808 starts the same every time) */
        if (t->p[P_VOICE] == V_UNISON) {              /* the stack at spread phases: in phase they would beat as one */
            v->ph[2] = 0x9E3779B9u * (v->age | 1u);
            for (j = 3; j < 8u; j++)
                v->s[j] = (int32_t)(0x9E3779B9u * (v->age * 8u + j));
        }
    }
    v->ph[1] = v->ph[0] + 0x40000000u;
    v->s[0] = v->s[1] = 0;                            /* filter */
    if (!v->s[2])
        v->s[2] = 0x1234567 + (int32_t)v->age;        /* noise state */
}

/* UNISON: the stack of oscillator 1 over n (<= CTL) samples into a[], about one oscillator's level. One
 * oscillator at a time (the wave chosen once, not per sample); dq8: voice.c's unison step in thirds, Q8.
 * Its own function: the stack's cost is apart from analog_render's (tests/target_budget.txt) */
static __attribute__((noinline)) void analog_stack(voice_t *v, uint32_t wave, uint32_t inc1, uint32_t pw, int32_t dq8,
                                                   int32_t *a, uint32_t n)
{
    uint32_t i, j;
    for (i = 0; i < n; i++)
        a[i] = 0;
    for (j = 0; j < ANA_UNI; j++) {
        uint32_t *php = j == 0u ? &v->ph[0] : j == 1u ? &v->ph[2] : (uint32_t *)&v->s[j + 1u];
        uint32_t ph = *php, inc = inc1 + (uint32_t)((int32_t)(inc1 >> 12) * ((ANA_SPREAD[j] * dq8) >> 8));
        switch (wave) {
        case 1:
            for (i = 0; i < n; i++, ph += inc)
                a[i] += osc_pulse(ph, inc, 0x80000000u);
            break;
        case 2:
            for (i = 0; i < n; i++, ph += inc)
                a[i] += osc_tri(ph);
            break;
        case 3:
            for (i = 0; i < n; i++, ph += inc)
                a[i] += osc_sine(ph);
            break;
        case 4:
            for (i = 0; i < n; i++, ph += inc)
                a[i] += osc_pulse(ph, inc, pw);
            break;
        default:
            for (i = 0; i < n; i++, ph += inc)
                a[i] += osc_saw(ph, inc);
            break;
        }
        *php = ph;
    }
    for (i = 0; i < n; i++)
        a[i] = mulq16(a[i], ANA_UNI_GAIN);
}

static void analog_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)p[P_E0], i;
    int32_t det = p[P_E1], mix = p[P_E2], noise = p[P_E3];
    int32_t cut = (p[P_E4] << 8) + m->cutoff + (p[P_E7] * (v->pitch16 - 60 * 16) >> 4);
    tsvf_t flt;
    int32_t drive = 32768 + p[P_E6] * 768;                       /* 1x .. 4x */
    int32_t dw = p[P_E6] * 258;                                  /* DRV: dry -> driven (a clean range low) */
    uint32_t inc1 = m->inc;
    /* DTN in cents: whole 1/16 semitones from the table, the rest as a fine factor */
    int32_t d16 = det * 16 / 100, rem = det * 16 - d16 * 100;            /* rem: 1/1600 semitone */
    uint32_t inc2 = PITCH_INC[clamp(m->pitch16 + d16, 0, 2047)];
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * (rem * 2367 / 16000 + m->fine));   /* (as inc1: tune, glide, LFO) */
    uint32_t pw = 0x80000000u + (uint32_t)((m->shape - (64 << 8)) << 15);
    int32_t m2 = mix * 258, m1 = 32767 - m2;                    /* osc mix Q15 */
    int32_t nz = noise * 200, drv = p[P_E6];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];                  /* state in locals: out[] may alias v->s[] */
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2];
    uint32_t uni = p[P_VOICE] == V_UNISON;
    int32_t ua[CTL];
    tsvf_coef(&flt, cut, p[P_E5]);
    if (det == 0)
        inc2 = inc1;
    if (uni)                                          /* voice.c's unison step k * DETUNE * 56 / 889, k in thirds (Q8) */
        analog_stack(v, wave, inc1, pw, p[P_DETUNE] * 56 * 256 / (889 * 3), ua, n);
    for (i = 0; i < n; i++) {
        int32_t a, b = 0, s;
        switch (wave) {
        case 1:
            a = osc_pulse(ph0, inc1, 0x80000000u);
            if (m2)
                b = osc_pulse(ph1, inc2, 0x80000000u);
            break;
        case 2:
            a = osc_tri(ph0);
            if (m2)
                b = osc_tri(ph1);
            break;
        case 3:
            a = osc_sine(ph0);
            if (m2)
                b = osc_sine(ph1);
            break;
        case 4:
            a = osc_pulse(ph0, inc1, pw);
            if (m2)
                b = osc_pulse(ph1, inc2, pw);
            break;
        default:
            a = osc_saw(ph0, inc1);
            if (m2)
                b = osc_saw(ph1, inc2);
            break;
        }
        if (uni)
            a = ua[i];                                /* UNISON: the stack for oscillator 1 (2 as it is) */
        ph0 += inc1;
        ph1 += inc2;
        s = m2 ? mulq15(a, m1) + mulq15(b, m2) : a;  /* (MIX 0, the 808s and subs: one oscillator) */
        if (nz)
            s += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, nz);
        if (drv)                                      /* pre-shifts: drive is up to 4x, no overflow */
            s += mulq15(softclip(((s >> 2) * (drive >> 2)) >> 11) - s, dw);
        {   /* filter: linear up to half scale, then a soft knee (only resonance peaks saturate) */
            int32_t y = tsvf_lp(&flt, s >> 1, &ic1, &ic2), a = y < 0 ? -y : y;
            if (a > 16000) {
                a = 16000 + (softclip((a - 16000) * 2) >> 1);
                y = y < 0 ? -a : a;
            }
            s = y << 1;
        }
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS) << 1;
    }
    if (!uni)
        v->ph[0] = ph0;                               /* (UNISON: the stack moved it) */
    v->ph[1] = ph1;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
}

static const preset_t ANALOG_PRESETS[] = {
    /* SLOOP hip-hop / drum & bass bank. {WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR}, {A D S R}, fenv, mono */
    /* 808s: a sine that decays (SUS 0) with a little pitch drop (ENV -> PITCH) and slides (GLIDE, legato) */
    {"808 BOOM", {3, 0, 0, 0, 127, 0, 48, 0}, {0, 112, 0, 52}, 0, 1, FX(0, 0, 0, 0), XP(P_GLIDE + 1, 59, P_GLMODE + 1, 1, P_ED_PIT + 1, 18, P_TRANS + 1, -24)},
    {"808 DIRTY", {3, 0, 0, 0, 96, 12, 118, 30}, {0, 108, 0, 46}, 0, 1, FX(24, 0, 0, 0), XP(P_GLIDE + 1, 56, P_GLMODE + 1, 1, P_ED_PIT + 1, 24, P_TRANS + 1, -24)},
    {"SUB BASS", {3, 0, 0, 0, 127, 0, 10, 0}, {0, 60, 118, 24}, 0, 1, FX(0, 0, 0, 0), XP(P_GLIDE + 1, 40, P_TRANS + 1, -24)},
    {"808 SLIDE", {3, 0, 0, 0, 127, 0, 72, 0}, {0, 118, 0, 60}, 0, 1, FX(30, 0, 0, 0), XP(P_GLIDE + 1, 82, P_GLMODE + 1, 1, P_ED_PIT + 1, 12, P_TRANS + 1, -24)},
    /* acid: one saw, high resonance, the envelope on the filter, slides where notes overlap */
    {"ACID 303", {0, 0, 0, 0, 36, 112, 44, 64}, {0, 56, 24, 20}, 52, 1, FX(28, 0, 22, 10), XP(P_GLIDE + 1, 34, P_GLMODE + 1, 1, P_TRANS + 1, -12)},
    /* plugg / soft trap: a round triangle bass */
    {"PLUGG BASS", {2, 0, 0, 0, 92, 0, 30, 0}, {0, 96, 92, 34}, 0, 1, FX(0, 0, 0, 6), XP(P_GLIDE + 1, 52, P_GLMODE + 1, 1, P_TRANS + 1, -24)},
    /* drum & bass: two detuned saws, slowly moving filter */
    {"REESE", {0, 22, 64, 0, 56, 22, 36, 40}, {0, 70, 118, 28}, 0, 1, FX(0, 22, 0, 6), XP(P_LRATE + 1, 22, P_LD_FLT + 1, 10, P_TRANS + 1, -24)},
    {"WOBBLE", {0, 9, 64, 0, 38, 72, 62, 30}, {0, 64, 127, 22}, 0, 1, FX(10, 0, 0, 4), XP(P_LRATE + 1, 74, P_LD_FLT + 1, 40, P_TRANS + 1, -24)},
    /* west coast: a square bass that slides, the high sine-ish lead with a delayed vibrato */
    {"FUNK BASS", {1, 6, 50, 0, 50, 30, 22, 64}, {0, 55, 62, 20}, 30, 1, FX(0, 0, 0, 4), XP(P_GLIDE + 1, 50, P_GLMODE + 1, 1, P_TRANS + 1, -24)},
    {"G-FUNK LD", {2, 0, 0, 0, 92, 0, 10, 64}, {6, 70, 112, 46}, 0, 1, FX(0, 0, 28, 30),
     XP(P_GLIDE + 1, 74, P_LD_PIT + 1, 2, P_LRATE + 1, 89, P_LFADE + 1, 50)},
    {"TRAP PLUCK", {0, 8, 64, 0, 52, 35, 20, 64}, {0, 80, 24, 45}, 48, 0, FX(0, 15, 34, 26)},
    {"SYN BRASS", {0, 10, 64, 0, 45, 20, 16, 64}, {12, 70, 96, 35}, 34, 0, FX(0, 15, 12, 24)},
    /* trance / EDM: seven detuned saws on one note (UNISON inside the voice) */
    {"SUPERSAW", {0, 14, 64, 0, 96, 12, 10, 64}, {4, 70, 112, 46}, 10, 1, FX(0, 20, 30, 36), XP(P_VOICE + 1, 3, P_DETUNE + 1, 64, P_GLIDE + 1, 30)},
    {"WARM PAD", {4, 12, 64, 6, 52, 8, 0, 32}, {75, 90, 115, 90}, 0, 0, FX(0, 55, 20, 55), XP(P_LRATE + 1, 30, P_LD_SHP + 1, 20)},
    {"DARK STR", {0, 18, 64, 0, 48, 6, 0, 32}, {60, 90, 118, 85}, 0, 0, FX(0, 50, 18, 60)},
    {"ATMOS PAD", {4, 25, 64, 20, 60, 15, 0, 32}, {90, 100, 120, 100}, 0, 0, FX(0, 60, 30, 75), XP(P_LRATE + 1, 18, P_LD_PIT + 1, 1)},
};

static const engine_t ENG_ANALOG = {
    "ANALOG", {"OSC", "FLT"},
    {
        {"WAVE", F_ENUM, 0, 4, 0, N_ANALOG_WAVE, 0},
        {"DTN", F_INT, 0, 127, 10, 0, "ct"},
        {"MIX", F_PCT, 0, 127, 64, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 30, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"KTR", F_PCT, 0, 127, 64, 0, 0},
    },
    ANALOG_PRESETS, sizeof(ANALOG_PRESETS) / sizeof(ANALOG_PRESETS[0]), 1, analog_note_on, analog_render,
    0xF986, {P_E4, P_E5, P_ATK, P_REL},
    0, 0, 0, 0, ANA_UNI,                              /* (poly, amp, desc, block), uni: UNISON in the voice */
};
