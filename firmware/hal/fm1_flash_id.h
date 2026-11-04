/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's flash parts: the JEDEC ids (0x9F) the firmware knows. Same size (1 MiB), the same plain SPI NOR commands
 * (0x03/0x0B read, 0x02 program, 0x20 4 KiB erase, 0x05/0x35 status, 0x06 write enable).
 *   0x856014  Puya P25Q80, the FM-1 as first sold
 *   0xB36014  UC25HQ80, FM-1 units sold from late 2026 (mostly AliExpress)
 * Any other id: the flash is left alone (saves stay in RAM, the update loader refuses). */
#pragma once
#ifndef FL_JEDEC_OK
#define FL_JEDEC_OK(id) ((id) == 0x856014u || (id) == 0xB36014u)
#endif
