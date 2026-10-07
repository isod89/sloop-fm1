/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP emulator, the firmware unit: firmware/src/felucca.c exactly as it is flashed (main.c,
 * project / storage / autosave, panel, UI, sequencer, DSP), built for the host over the shadow HAL
 * in tools/emulator/hal. No windows.h here: the Windows side is emu_win32.c, the seam emu_bridge.h.
 *
 * build_emu.py compiles copies of felucca.c, usb.c and midi_uart.c (build/emu/fwsrc): those two
 * include "../hal/fm1_usb.h" / "../hal/fm1_uart.h" by a relative path, which would bypass the
 * shadow; their copies include ours.
 * _Static_assert stays on: a layout that would not build for the FM-1 does not build here either
 * (build_emu.py --no-static-assert turns them off, to look at such a tree anyway). */
#include <stdint.h>
#include <stddef.h>
#pragma warning(disable: 4005 4018 4090 4100 4101 4102 4133 4146 4189 4244 4267 4305 4312 4334 4996)
/* GCC-isms of the target build (as tests/hostsim.c) */
#define __attribute__(x)
#define __asm__
#define volatile(...)
#ifdef EMU_NO_STATIC_ASSERT
#define _Static_assert(x, y)
#endif
#define __builtin_offsetof(t, m) ((size_t) & (((t *)0)->m))
/* the firmware's own libc.c (its memset / memcpy / memcmp), clear of the C runtime's */
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "emu_bridge.h"
/* user sample slots: read through the flash image (eng_sample.c's host hook) */
#define SMP_USER_XIP(k) ((const uint8_t *)emu_flash + SMP_USER_BASE + (k) * SMP_USER_SIZE)

#include "felucca.c"

/* the linker script's symbols (fm1_cstart, never called here: the emulator enters at fm1_main) */
uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];

/* the asm interrupt wrappers (hal/fm1_isr.S) */
void isr_timer5(void) { fm1_timer5_irq(); }
void isr_alnk0(void) { fm1_alnk0_irq(); }

void emu_fw_main(void) { fm1_main(); }

void emu_fw_timer5(void) { isr_timer5(); }

void emu_fw_alnk_half(uint32_t half)
{
    emu_alnk_free = half;
    emu_alnk_pend = FM1_AUDIO_HALF;
    isr_alnk0();
}

int emu_fw_led(uint32_t id)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if ((fm1_led[p] >> r) & 1u)
                    return 2;
                if ((fm1_led_dim[p] >> r) & 1u)
                    return 1;
                return fm1_led_bg_ns && ((fm1_led_bg[p] >> r) & 1u) ? 3 : 0;
            }
    return 0;
}

uint32_t emu_fw_btn(uint32_t label) { return label < NB ? panel.btn[label] : 0u; }

void emu_fw_turn(uint32_t role, uint32_t *enc, int32_t *dir)
{
    *enc = role < NE ? panel.enc[role] : 0u;
    *dir = role < NE ? panel.dir[role] : 1;
}

void emu_fw_midi(uint32_t status, uint32_t d1, uint32_t d2)   /* as usb.c's ep1_rx: cable 0 */
{
    uint32_t pkt;
    if (status < 0x80u || status >= 0xF0u)
        return;
    if ((status & 0xE0u) == 0xC0u)
        d2 = 0;                                      /* Cx, Dx: one data byte */
    pkt = (status >> 4) | status << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
    if (mi_w - mi_r < MQ) {
        midi_in_q[mi_w % MQ] = pkt;
        RING_PUBLISH();
        mi_w++;
    }
}

const void *emu_fw_smp_data(void) { return SMP_DATA; }

void emu_fw_stats(uint32_t *ms, uint32_t *halves, uint32_t *late, uint32_t *cpu_q8, uint32_t *keys)
{
    *ms = fm1_ms;
    *halves = felucca_dbg.halves;
    *late = felucca_dbg.late;
    *cpu_q8 = song.cpu_q8;
    *keys = fm1_in.buttons | fm1_in.notes << 14;
}
