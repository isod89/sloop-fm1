/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_audio.h: ALNK0's double buffer, played through waveOut.
 * The host audio thread raises the "half free" interrupt (emu_fw_alnk_half) and plays the half
 * the ISR rendered. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#include "fm1_time.h"
#include "fm1_irq.h"

#define FM1_AUDIO_HALF 0x80u
static volatile uint8_t emu_alnk_pend;
static volatile uint32_t emu_alnk_free;

FM1_INLINE void fm1_audio_init(int32_t *buf, uint32_t half_words, void (*isr)(void), uint32_t prio)
{
    (void)isr; (void)prio;               /* isr_alnk0 (emu_firmware.c) */
    emu_audio_on = 1;
    emu_audio_start(buf, half_words);
}
FM1_INLINE uint8_t fm1_audio_pending(void) { return emu_alnk_pend; }
FM1_INLINE void fm1_audio_ack_aux(uint8_t p) { (void)p; }
FM1_INLINE uint32_t fm1_audio_free_half(void) { return emu_alnk_free; }
FM1_INLINE void fm1_audio_ack_half(void) { emu_alnk_pend = 0; }
FM1_INLINE void fm1_audio_stop(void) { emu_audio_on = 0; }
