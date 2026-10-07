/* SPDX-License-Identifier: GPL-3.0-only */
/* emulator shadow of firmware/hal/fm1_usb.h: a controller with no host on the cable (the SIE
 * never answers, no SOF). MIDI from the PC comes in through emu_fw_midi instead.
 * usb.c includes "../hal/fm1_usb.h"; build_emu.py compiles a copy that includes this one.
 * USB audio (FELUCCA_UAC) builds and stays idle: no host ever selects its alternate setting. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

FM1_INLINE void fm1_usb_reset(void) {}
FM1_INLINE void fm1_usb_attach(void *ep0) { (void)ep0; }
FM1_INLINE void fm1_usb_off(void) {}
FM1_INLINE uint32_t fm1_usb_sie_on(void) { return 0; }
FM1_INLINE void fm1_usb_sie_wr_start(uint32_t r, uint32_t v) { (void)r; (void)v; }
FM1_INLINE void fm1_usb_sie_rd_start(uint32_t r) { (void)r; }
FM1_INLINE uint32_t fm1_usb_sie_done(void) { return 0x8000u; }
FM1_INLINE uint32_t fm1_usb_sie_data(void) { return 0; }
FM1_INLINE void fm1_usb_ep0_buf(void *p) { (void)p; }
FM1_INLINE void fm1_usb_ep_txbuf(uint32_t ep, void *p) { (void)ep; (void)p; }
FM1_INLINE void fm1_usb_ep_rxbuf(uint32_t ep, void *p) { (void)ep; (void)p; }
FM1_INLINE void fm1_usb_ep0_send(void *p, uint32_t n) { (void)p; (void)n; }
FM1_INLINE void fm1_usb_ep_send(uint32_t ep, void *p, uint32_t n) { (void)ep; (void)p; (void)n; }
FM1_INLINE void fm1_usb_ep4_txbuf(void *p) { (void)p; }   /* EP4 IN: the USB audio stream (no host) */
FM1_INLINE void fm1_usb_ep4_send(void *p, uint32_t n) { (void)p; (void)n; }
FM1_INLINE void fm1_usb_rx_sync(void) {}
FM1_INLINE void fm1_usb_ep_enable(uint32_t eps) { (void)eps; }
FM1_INLINE uint32_t fm1_usb_sof_take(void) { return 0; }
