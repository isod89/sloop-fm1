/* SPDX-License-Identifier: GPL-3.0-only */
/* The actual HAL compositor without touching registers, as encoder_test exercises its decoder. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_input.h"

int main(void)
{
    const unsigned scale_frames[] = {0, 2, 3, 4}, root_frames[] = {0, 4, 5, 6};
    unsigned level, frame, col;
    for (col = 0; col < FM1_NCOL; col++) {
        fm1_led[col] = 2;
        fm1_led_scale[col] = 4;
        fm1_led_root[col] = 8;
    }
    for (level = 0; level <= 3; level++) {
        unsigned counts[3] = {0}, off_run = 0, max_off = 0;
        fm1_led_scale_level = (uint8_t)level;
        for (frame = 0; frame < 16; frame++) {
            uint32_t lit;
            fm1__led_phase = (uint8_t)(frame & 7u);
            lit = fm1__led_lit(0);
            assert((lit & 2u) && !(lit & ~14u));
            for (col = 1; col < FM1_NCOL; col++)
                assert(fm1__led_lit(col) == lit);      /* all columns use one frame's phase */
            if (frame < 8) {
                counts[0] += !!(lit & 2u);
                counts[1] += !!(lit & 4u);
                counts[2] += !!(lit & 8u);
            }
            off_run = lit & 4u ? 0 : off_run + 1;
            if (off_run > max_off) max_off = off_run;
        }
        assert(counts[0] == 8 && counts[1] == scale_frames[level] && counts[2] == root_frames[level]);
        if (level) {
            assert(counts[1] >= 2 && counts[2] > counts[1] && counts[2] < counts[0]);
            assert(max_off <= 3);                    /* distributed pulses, not six dark frames together */
        }
    }
    fm1_led_scale_level = 255;                       /* malformed level cannot force lights on */
    assert(fm1__led_lit(0) == fm1_led[0]);
    fm1_led_scale_level = 1;
    fm1_led[0] = fm1_led_scale[0] = fm1_led_root[0] = 30;
    for (frame = 0; frame < 8; frame++) {
        fm1__led_phase = (uint8_t)frame;
        assert(fm1__led_lit(0) == 30);                /* pressed beats every hint, even with overlapping masks */
    }
    puts("LED_PWM_OK: scale 25/37.5/50%, roots 50/62.5/75%, played 100%; all columns, off, overlap and bounded dark gaps");
    return 0;
}
