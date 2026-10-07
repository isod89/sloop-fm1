/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP emulator: the seam between the firmware unit (emu_firmware.c: firmware/src/felucca.c over
 * the shadow HAL in this directory) and the Windows side (emu_win32.c). Plain C, no windows.h. */
#pragma once
#include <stdint.h>

/* ---- provided by emu_win32.c ---- */
/* "interrupts": the timer and audio ISRs run on their own threads with the CPU lock held; the
 * firmware thread takes it while its interrupts are off */
void emu_irq_off(void);
void emu_irq_on(void);
void emu_irq_enable_all(void);
void emu_idle(void);                        /* main loop polling with nothing new: give the CPU back */
uint32_t emu_ticks24(void);                 /* TIMER4: 24 MHz, wrapping */
void emu_sleep_ms(uint32_t ms);
void emu_timer5_start(void (*isr)(void));
void emu_audio_start(const int32_t *buf, uint32_t half_words);
extern volatile uint32_t emu_audio_on;      /* ALNK0 DMA running (fm1_audio_stop clears it) */
extern uint16_t emu_lcd[240 * 240];         /* ST7789 GRAM, RGB565 */
extern uint8_t *emu_flash;                  /* the 1 MiB SPI NOR image */
#define EMU_FLASH_SIZE 0x100000u
void emu_flash_written(void);               /* schedule the image to disk */
uint64_t emu_keys(void);                    /* matrix ids held: 0..13 buttons, 14..40 note keys */
int32_t emu_enc_take(uint32_t e);           /* steps turned on matrix encoder e */
int32_t emu_adc(uint32_t ch);
void emu_reboot(void);                      /* never returns */
void emu_halt(const char *why);             /* never returns: the firmware thread stops */

/* ---- provided by emu_firmware.c ---- */
void emu_fw_main(void);
void emu_fw_alnk_half(uint32_t half);       /* the ALNK0 interrupt: half `half` is free */
void emu_fw_timer5(void);                   /* the TIMER5 interrupt */
int emu_fw_led(uint32_t id);                /* LED of matrix id: 0 off, 1 glow, 2 lit, 3 backlight */
uint32_t emu_fw_btn(uint32_t label);        /* matrix id of a panel label (panel.c B_*) */
void emu_fw_turn(uint32_t role, uint32_t *enc, int32_t *dir);   /* role (EN_*) -> matrix encoder */
void emu_fw_midi(uint32_t status, uint32_t d1, uint32_t d2);    /* a channel message, as USB-MIDI */
const void *emu_fw_smp_data(void);          /* SMP_DATA: user samples are addressed from it (uint32) */
void emu_fw_stats(uint32_t *ms, uint32_t *halves, uint32_t *late, uint32_t *cpu_q8, uint32_t *keys);
