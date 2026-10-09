// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 ugotworms
// The reference for tests/snes_test.c: the scenarios of snes_cases.h on snes_spc's accurate S-DSP
// (SPC_DSP.cpp, LGPL-2.1-or-later, built from a checkout: -I<snes_spc>/snes_spc; run_tests.sh: SNES_SPC), the instruments
// (built in, felucca_brr.h, or the bank's: tools/gen_brr.py --bank) in its 64 KiB RAM.
//   snes_ref BANK_PREFIX OUT.bin      writes, per case: int32 n, then n int16 samples (the left output)
// Voice 0 alone, its volumes and the master's at -128: the DSP's output is then the voice's own
// output exactly ((x * -128) >> 7 twice), so the port is compared before any volume.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int16_t *ref_out;
static int ref_n;
#define SPC_DSP_OUT_HOOK(l, r) (ref_out[ref_n++] = (int16_t)(l))
#include "SPC_DSP.cpp"

#include "felucca_brr.h"
#include "snes_cases.h"
static unsigned char snes_test_slots[4 * 0x14000];
#include "snes_bankfile.h"

static uint8_t ram[0x10000];

int main(int argc, char **argv)
{
    FILE *f;
    if (argc != 3 || !snes_test_load(argv[1], 0) || !(f = fopen(argv[2], "wb"))) {
        fprintf(stderr, "usage: snes_ref BANK_PREFIX OUT.bin (the bank: tools/gen_brr.py --bank BANK_PREFIX)\n");
        return 2;
    }
    for (unsigned k = 0; k < SNES_NCASES; k++) {
        const snes_case_t *c = &SNES_CASES[k];
        static SPC_DSP dsp;
        static int16_t buf[1 << 17];
        snes_test_smp_t sm;
        uint32_t s = 0x1000, l;
        if (!snes_test_find(c->inst, 0, &sm)) {
            fprintf(stderr, "snes_ref: no instrument %s\n", c->inst);
            return 2;
        }
        if (s + sm.len > 0x8000) {
            fprintf(stderr, "snes_ref: %s does not fit below the echo line\n", c->inst);
            return 2;
        }
        memset(ram, 0, sizeof ram);                                // as a game would: the one sample it plays,
        memcpy(ram + s, sm.data + sm.start, sm.len);               // directory entry 0 at 0x200
        l = s + (sm.loop - sm.start);
        ram[0x200] = (uint8_t)s;
        ram[0x201] = (uint8_t)(s >> 8);
        ram[0x202] = (uint8_t)l;
        ram[0x203] = (uint8_t)(l >> 8);
        dsp.init(ram);
        for (int v = 0; v < 8; v++) {
            dsp.write(v * 0x10 + SPC_DSP::v_voll, v ? 0 : 0x80);
            dsp.write(v * 0x10 + SPC_DSP::v_volr, v ? 0 : 0x80);
            dsp.write(v * 0x10 + SPC_DSP::v_adsr0, 0);
            dsp.write(v * 0x10 + SPC_DSP::v_gain, 0);
        }
        dsp.write(SPC_DSP::v_pitchl, c->pitch & 0xFF);
        dsp.write(SPC_DSP::v_pitchh, c->pitch >> 8);
        dsp.write(SPC_DSP::v_srcn, 0);
        dsp.write(SPC_DSP::v_adsr0, c->adsr0);
        dsp.write(SPC_DSP::v_adsr1, c->adsr1);
        dsp.write(SPC_DSP::v_gain, c->gain);
        dsp.write(SPC_DSP::r_mvoll, 0x80);
        dsp.write(SPC_DSP::r_mvolr, 0x80);
        dsp.write(SPC_DSP::r_evoll, c->echo ? (uint8_t)c->evol : 0);
        dsp.write(SPC_DSP::r_evolr, 0);
        dsp.write(SPC_DSP::r_efb, c->echo ? (uint8_t)c->efb : 0);
        dsp.write(SPC_DSP::r_pmon, 0);
        dsp.write(SPC_DSP::r_non, c->non ? 1 : 0);
        dsp.write(SPC_DSP::r_eon, c->echo ? 1 : 0);
        dsp.write(SPC_DSP::r_dir, 0x02);
        dsp.write(SPC_DSP::r_esa, 0x80);                         // the echo line at 0x8000 (the sample below it)
        dsp.write(SPC_DSP::r_edl, c->echo ? c->edl : 0);
        for (int i = 0; i < 8; i++)
            dsp.write(SPC_DSP::r_fir + i * 0x10, c->echo ? (uint8_t)c->fir[i] : 0);
        dsp.write(SPC_DSP::v_volr, 0);                           // (the right channel: silent)
        dsp.write(SPC_DSP::r_flg, (c->echo ? 0 : 0x20) | c->nrate);   // echo writes on with the echo; not muted
        dsp.write(SPC_DSP::r_koff, 0);
        dsp.write(SPC_DSP::r_kon, 1);
        ref_out = buf;
        ref_n = 0;
        for (int s = 0; s < c->n; s++) {
            if (s == c->koff_at) {
                if (c->rel_gain) {
                    dsp.write(SPC_DSP::v_adsr0, c->adsr0 & 0x7F);
                    dsp.write(SPC_DSP::v_gain, c->rel_gain);
                } else {
                    dsp.write(SPC_DSP::r_koff, 1);
                }
            }
            dsp.run(32);
        }
        fwrite(&ref_n, 4, 1, f);
        fwrite(buf, 2, (size_t)ref_n, f);
    }
    fclose(f);
    printf("snes_ref: %u cases -> %s\n", (unsigned)SNES_NCASES, argv[2]);
    return 0;
}
