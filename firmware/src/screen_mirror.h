/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Patrick F. Kellogg
 *
 * FM-1 screen mirror: sends the screen over USB MIDI SysEx to FM-1 Mirror
 * (github.com/pfkellogg/fm1-mirror) or any app that speaks the same messages.
 * Firmware-neutral: it needs only the rows you send to the LCD, a millisecond
 * clock and a way to send one SysEx message. See README.md.
 */
#ifndef SCREEN_MIRROR_H
#define SCREEN_MIRROR_H
#include <stdint.h>

/* Sends one complete SysEx message (F0 ... F7). Return 0 when sent, -2 when
 * the output is busy (try the same message again later), anything else to
 * stop mirroring. */
typedef int (*sm_send_fn)(const uint8_t *message, unsigned length);

/* Call for every incoming SysEx message; returns 1 if it was a mirror hello
 * (F0 7D 'F' 'S' 00/01/02 F7; 02 asks only for the sound's name), which the caller can then ignore. */
int sm_sysex(const uint8_t *m, unsigned n);

/* 1 while an app is asking: only then is sm_rows worth calling. */
int sm_on(void);

/* 1 once when the app wants the whole screen (a new viewer): repaint everything. */
int sm_take_full_request(void);

/* Queue `rows` rows of RGB565 pixels, `width` wide, whose top-left is (left, top).
 * Returns 0 when the buffer has no room yet: call again with the same rows after
 * sm_poll has sent some (or skip, and repaint those rows later). */
int sm_rows(const uint16_t *pixels, unsigned left, unsigned top, unsigned width, unsigned rows);

/* Call often (e.g. every main-loop pass) with the time in milliseconds: sends what's
 * queued, and stops mirroring 3 s after the last hello. */
void sm_poll(uint32_t now_ms, sm_send_fn send);

/* Call often with the sound playing now ("T1 FM6 ICE BELL", ASCII, up to 47 characters): while an app asks (any
 * hello in the last 3 s), sends it as F0 7D 'F' 'S' 02 name... F7 after each hello and whenever it changes. */
void sm_name(const char *name, uint32_t now_ms, sm_send_fn send);

#endif
