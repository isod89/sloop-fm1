#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
"""Host preview of the SNOOP37 boot splash (firmware/src/snoop_splash.c).

Compiles the splash's drawing code with the host C compiler (MK_HOST: no LCD, no timers), renders
whole frames at the given times and writes them as PNG files (Pillow, 1x and --scale), a contact sheet,
the update screens (ota_<step>.png), an animated GIF of the intro (intro.gif, 25 frames/s) and the boot
jingle (firmware/src/snoop_jingle.c) as jingle.wav (44.1 kHz, at the master volume's middle, through a
simple stand-in for SLOOP's reverb) plus jingle_skip.wav (a key pressed at 2 s).

    python3 tools/snoop_preview.py OUTDIR [--ms 0,400,...] [--scale 2] [--no-gif] [--no-wav]
"""
import argparse
import math
import subprocess
import wave
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware" / "src" / "snoop_splash.c"

HARNESS = r"""
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MK_HOST 1
static int16_t SINE[1025];
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void str_cpy(char *d, const char *s, unsigned n)
{
    while (n > 1 && *s) {
        *d++ = *s++;
        n--;
    }
    *d = 0;
}
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
static const uint16_t TE_COL[4] = {RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26)};
#include "snoop_jingle.c"
#include "snoop_splash.c"
static uint16_t strip[240 * MK_SH], pool[6720];
int main(int argc, char **argv)
{
    int a, y;
    for (a = 0; a < 1025; a++)
        SINE[a] = (int16_t)lrint(32767.0 * sin(2 * M_PI * a / 1024));
    mk_mem(pool);
    for (a = 1; a < argc; a++) {
        uint32_t t = (uint32_t)strtoul(argv[a] + (argv[a][0] == 'o'), 0, 10);
        mk_ota = argv[a][0] == 'o';
        if (mk_ota) {                    /* o<step>,<code> */
            mk_step = (uint8_t)t;
            mk_code = (int8_t)strtol(strchr(argv[a], ',') + 1, 0, 10);
            t = 4000;
        }
        if (!mk_ota) {        /* the jingle runs along: the mountains follow its bands */
            static uint32_t last = 0xFFFFFFFFu;
            int32_t l[32], r[32], s[32];
            if (t < last || last == 0xFFFFFFFFu)
                mkj_start();
            last = t;
            while (mkj.on && mkj.s * 10u / 441u < t)
                mkj_render(l, r, s, 32);
            mk_band_tick();
        }
        mk_prep(t);
        for (y = 0; y < 240; y += MK_SH) {
            mk_render(strip, y, t);
            fwrite(strip, 2, 240 * MK_SH, stdout);   /* big-endian RGB565, as the panel takes it */
        }
    }
    return 0;
}
"""

JINGLE = r"""
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static int16_t SINE[1025];
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
#include "snoop_jingle.c"
/* a Schroeder reverb as a stand-in for SLOOP's (fx.c): 4 damped combs, 2 all-passes */
static float cb[4][2000], ap[2][600], cl[4];
static const int CL[4] = {1557, 1617, 1491, 1422}, AL[2] = {556, 225};
static int ci[4], ai[2];
static float reverb(float x)
{
    float y = 0, o;
    int k;
    for (k = 0; k < 4; k++) {
        o = cb[k][ci[k]];
        cl[k] += (o - cl[k]) * 0.4f;
        cb[k][ci[k]] = x + cl[k] * 0.86f;
        ci[k] = (ci[k] + 1) % CL[k];
        y += o;
    }
    for (k = 0; k < 2; k++) {
        o = ap[k][ai[k]];
        ap[k][ai[k]] = y + o * 0.5f;
        y = o - y * 0.5f;
        ai[k] = (ai[k] + 1) % AL[k];
    }
    return y * 0.25f;
}
int main(int argc, char **argv)
{
    int skip = atoi(argv[1]), i, b;
    for (i = 0; i < 1025; i++)
        SINE[i] = (int16_t)lrint(32767.0 * sin(2 * M_PI * i / 1024));
    mkj_start();
    for (b = 0; b < 44100 * 6 / 32; b++) {
        int32_t l[32] = {0}, r[32] = {0}, s[32] = {0};
        if (skip && b * 32 >= skip * 441 / 10)
            mkj_stop();
        mkj_render(l, r, s, 32);
        for (i = 0; i < 32; i++) {
            float w = reverb(s[i] * (13000.0f / 32768.0f)) * 0.7f;
            float ol = (l[i] + w) * 2048.0f / 4096.0f, orr = (r[i] + w) * 2048.0f / 4096.0f;   /* master 2048 */
            int16_t o[2] = {(int16_t)fmaxf(-32768, fminf(32767, ol / 2)), (int16_t)fmaxf(-32768, fminf(32767, orr / 2))};
            fwrite(o, 2, 2, stdout);                       /* Q15 full scale = -6 dBFS (audio.c OUT_SHIFT) */
        }
    }
    return 0;
}
"""


BRAND = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240 && y + j < 240)
                fb[(y + j) * 240 + x + i] = px[j * w + i];
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240 && y + j < 240)
                fb[(y + j) * 240 + x + i] = (uint16_t)(c >> 8 | c << 8);
}
#include "gfx.c"
#include "snoop_theme.h"
static const uint16_t TE_COL[4] = {RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26)};
#define H_HEAD 20
int main(void)
{
    uint32_t pass;
    palette_set(1);                                 /* AMBER */
    /* the about screen (ui_menu.c draw_menu, ui.menu == 2) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);
    cv_text(4, 1, &FONT_S, "ABOUT", C_HI);
    cv_blit(0, 0);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, pass ? 95u : 124u, C_BLACK);
        cv_oy = pass ? -124 : 0;
        cv_text(4, 4, &FONT_L, BRAND_NAME, C_WHITE);
        cv_rect(BRAND_SAIL_X, 10, 8, 4, TE_COL[0]), cv_rect(BRAND_SAIL_X, 16, 12, 4, TE_COL[1]);
        cv_rect(BRAND_SAIL_X, 22, 16, 4, TE_COL[2]), cv_rect(BRAND_SAIL_X, 28, 20, 4, TE_COL[3]);
        cv_text(4, 36, &FONT_S, BRAND_BASED, C_AMB);
        cv_text(4, 54, &FONT_S, "SLOOP 2.5", C_HI);
        cv_text(cv_text(4, 72, &FONT_S, "LEO KUROSHITA", C_HI) + 8, 72, &FONT_S, "@KUROGEDELIC", C_AMB);
        cv_text(4, 88, &FONT_S, "H\xDCGELTON INSTRUMENTS", C_HI);
        cv_text(4, 104, &FONT_S, "HUGELTON.COM", C_AMB);
        cv_text(4, 119, &FONT_S, "GPL-3.0, NO WARRANTY", C_HI);
        cv_text(4, 132, &FONT_S, "GITHUB.COM/ISOD89/SLOOP-FM1", C_AMB);
        cv_oy = 0;
        cv_blit(0, H_HEAD + 1 + pass * 124u);
    }
    fwrite(fb, 2, 240 * 240, stdout);
    /* the crash screen (main.c fm1_fault) and the USB rescue screen (recovery.c) */
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, BRAND_CRASH, C_WHITE, 1);
    draw_text_box(10, 40, 220, &FONT_S, "VEC 00000010", C_WHITE, 0);
    fwrite(fb, 2, 240 * 240, stdout);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 64, 240, &FONT_S, BRAND_RESCUE, C_WHITE, 1);
    draw_text_box(0, 96, 240, &FONT_S, "CONNECT USB", C_WHITE, 1);
    draw_text_box(0, 124, 240, &FONT_S, "OPEN THE INSTALLER", C_WHITE, 1);
    draw_text_box(0, 164, 240, &FONT_S, "AUDIO OFF", C_WHITE, 1);
    fwrite(fb, 2, 240 * 240, stdout);
    return 0;
}
"""


def brand(outdir, rgb):
    gen = SRC.parent.parent.parent / "build" / "gen"
    if not (gen / "felucca_font.h").exists():
        print("(no build/gen/felucca_font.h: build once for the branded screens)")
        return
    with tempfile.TemporaryDirectory() as td:
        c, exe = Path(td) / "b.c", Path(td) / "b"
        c.write_text(BRAND)
        subprocess.run(["cc", "-O1", "-Wno-unused-function", "-I", str(SRC.parent), "-I", str(gen), str(c),
                        "-o", str(exe)], check=True)
        raw = subprocess.run([str(exe)], check=True, capture_output=True).stdout
    for n, name in enumerate(("about", "crash", "rescue")):
        img = rgb(raw[n * 115200:(n + 1) * 115200])
        img.save(outdir / f"brand_{name}_1x.png")
        img.resize((480, 480), Image.NEAREST).save(outdir / f"brand_{name}.png")
        print(outdir / f"brand_{name}_1x.png")


def jingle(outdir):
    with tempfile.TemporaryDirectory() as td:
        c, exe = Path(td) / "j.c", Path(td) / "j"
        c.write_text(JINGLE)
        subprocess.run(["cc", "-O2", "-Wall", "-Wno-unused-function", "-I", str(SRC.parent), str(c), "-lm",
                        "-o", str(exe)], check=True)
        for name, skip in (("jingle.wav", 0), ("jingle_skip.wav", 2000)):
            raw = subprocess.run([str(exe), str(skip)], check=True, capture_output=True).stdout
            with wave.open(str(outdir / name), "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(44100)
                w.writeframes(raw)
            v = [int.from_bytes(raw[i:i + 2], "little", signed=True) for i in range(0, len(raw), 2)]
            peak = max(abs(x) for x in v) or 1
            rms = math.sqrt(sum(x * x for x in v) / len(v)) or 1
            tail = max(abs(x) for x in v[2 * 44100 * 5:]) if len(v) > 2 * 44100 * 5 else 0
            print(f"{outdir / name}: peak {20 * math.log10(peak / 32768):.1f} dBFS, "
                  f"RMS {20 * math.log10(rms / 32768):.1f} dBFS, after 5 s peak {tail}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--ms", default="60,300,700,1100,1500,1900,2300,2900,3020,3200,3600,4100,4400,4700,4900")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--no-gif", action="store_true")
    ap.add_argument("--no-wav", action="store_true")
    a = ap.parse_args()
    times = [int(v) for v in a.ms.split(",")]
    otas = ["o1,0", "o2,0", "o3,0", "o4,0", "o5,0", "o9,-14", "o9,1"]
    gif = [] if a.no_gif else list(range(0, 5220, 40))   # (MK_LEN)
    a.outdir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        c, exe = Path(td) / "h.c", Path(td) / "h"
        c.write_text(HARNESS)
        subprocess.run(["cc", "-O2", "-Wall", "-Wno-unused-function", "-I", str(SRC.parent), str(c), "-lm",
                        "-o", str(exe)], check=True)
        raw = subprocess.run([str(exe), *map(str, times + otas + gif)], check=True,
                             capture_output=True).stdout

    def rgb(px):
        img = Image.new("RGB", (240, 240))
        img.putdata([((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                     for v in (int.from_bytes(px[i:i + 2], "big") for i in range(0, len(px), 2))])
        return img

    def frame(n):
        return rgb(raw[n * 240 * 240 * 2:(n + 1) * 240 * 240 * 2])

    def save(img, stem):
        img.save(a.outdir / f"{stem}_1x.png")
        big = img.resize((240 * a.scale, 240 * a.scale), Image.NEAREST) if a.scale > 1 else img
        big.save(a.outdir / f"{stem}.png")
        print(a.outdir / f"{stem}.png")
        return big

    frames = [save(frame(n), f"frame_{n:02d}") for n in range(len(times))]
    for k, o in enumerate(otas):
        save(frame(len(times) + k), "ota_" + o[1:].replace(",", "_").replace("-", "m"))
    base = len(times) + len(otas)
    if gif:
        g = [frame(base + k) for k in range(len(gif))]
        g[0].save(a.outdir / "intro.gif", save_all=True, append_images=g[1:], duration=40, loop=0)
        print(a.outdir / "intro.gif")
    brand(a.outdir, rgb)
    if not a.no_wav:
        jingle(a.outdir)
    cols = min(3, len(frames))
    w, h = frames[0].size
    sheet = Image.new("RGB", (cols * w, (len(frames) + cols - 1) // cols * h))
    for n, img in enumerate(frames):
        sheet.paste(img, (n % cols * w, n // cols * h))
    sheet.save(a.outdir / "sheet.png")


if __name__ == "__main__":
    main()
