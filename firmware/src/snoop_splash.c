/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (genkigenki/makkuro) */
/* SNOOP37 boot splash and update screens (build flag SNOOP37): a neon doghouse whose roof is a piano
 * keyboard, a Tron-style dog asleep on the ridge, a synthwave sun and a scrolling grid, the rainbow title in
 * a rounded neon-tube font. All procedural, fixed point, no bitmaps: a small 3D renderer (one camera for
 * the floor, the house, the keys and the dog; flat-shaded faces back to front, glowing edges, a reflection
 * in the floor).
 *
 * Everything is a function of the time t (ms): eased (easeOutExpo / easeOutCubic), sprung (a damped spring
 * table) and staggered. The intro (MK_LEN): CRT power-on, the grid rushes in and mountains rise on the
 * horizon (their ridge is the music: snoop_jingle.c mkj_band), the house flies in from far away turning,
 * the 12 roof keys drop in with a bounce (each landing rings a bell and flashes its pad), the dog falls onto
 * the ridge (squash and stretch), the title lights letter by letter (flicker, RGB split, a shine sweep, a
 * starburst), a slow dolly; the sun jumps up behind the mountains and sinks back into place: the ping, and
 * the black keys drop onto the roof together; the house turns on as the picture fades out into SLOOP's first
 * screen fading in. Under it all the jingle glides from a cluster into a chord that blooms as
 * the dog lands. The pads and the button LEDs light along. A key, pad or button press ends it at once (the
 * jingle fades out in 150 ms).
 *
 * A frame is rendered in five strips of 240 x 48 px into the two halves of the canvas pool (cv_px, gfx.c),
 * alternately, so one strip renders while the last one goes out (~12 frames/s at 12 MHz SPI).
 * snoop_splash_bg() draws the first frame at boot, snoop_splash() animates (main.c), snoop_ota() draws
 * one still frame per firmware-update step (felucca.c ota_show). tools/snoop_preview.py renders it all on
 * the host (MK_HOST). */

#define MK_SH 48                   /* strip rows: 5 strips per frame, 2 strip buffers in cv_px */
#define MK_LEN 5200u               /* ms */
#define MK_NK 12                   /* roof keys */
#define Q(v) ((v) * 16)            /* pixels -> 1/16 px */
#define MK_CYAN 0x28DCFFu

static uint16_t *mk_buf;
static int32_t mk_y0, mk_y1;       /* strip rows [y0, y1) */
static int32_t mk_core = 12, mk_glow = 36;    /* neon line: bright core, glow (1/16 px) */
static int32_t mk_hot = 110;       /* how white the core gets (0..255) */
static uint32_t mk_mask = 0xFFFFFFu;          /* colour channels drawn (the RGB split) */
static int32_t mk_shine = -9999;   /* the shine band (x + y / 2, px), -9999 = none */
static uint8_t mk_ota, mk_step;    /* an update screen: its step (1..5, 9 = result) */
static int8_t mk_code;

/* ---- maths */
static uint32_t mk_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* sine of a (1024 = a full turn), -256..256 (Bhaskara) */
static int32_t mk_sin(int32_t a)
{
    int32_t s = 1, p;
    a &= 1023;
    if (a >= 512) {
        a -= 512;
        s = -1;
    }
    p = a * (512 - a);
    return s * (4096 * p / (1310720 - 4 * p));
}

/* progress of t through [t0, t0 + d): 0..256 */
static int32_t mk_prog(uint32_t t, uint32_t t0, uint32_t d)
{
    if (t <= t0)
        return 0;
    if (t >= t0 + d)
        return 256;
    return (int32_t)((t - t0) * 256u / d);
}

static int32_t mk_cubic(int32_t p)          /* easeOutCubic */
{
    int32_t q = 256 - p;
    return 256 - (q * q / 256 * q / 256);
}

static int32_t mk_expo(int32_t p)           /* easeOutExpo: 1 - 2^(-10 p) ~ cubic-bezier(0.16, 1, 0.3, 1) */
{
    int32_t k = p * 10, i = k >> 8, v;
    if (p >= 256)
        return 256;
    v = (65536 >> i) * (512 - (k & 255)) / 512;
    return 256 - (v >> 8);
}

/* a damped spring from 0 to 256 (overshoot 30 %, crosses 256 after 128 ms, settled ~600 ms): 25 ms steps */
static const int16_t MK_SPRING[25] = {0, 19, 67, 130, 195, 252, 295, 321, 332, 329, 316, 298, 279,
                                      261, 247, 238, 234, 234, 237, 242, 248, 253, 258, 261, 262};
#define MK_LAND 128u
static int32_t mk_spring(uint32_t ms)
{
    uint32_t i = ms / 25u, f = ms % 25u;
    int32_t v;
    if (i >= 24u)
        return 256;
    v = MK_SPRING[i] + (MK_SPRING[i + 1] - MK_SPRING[i]) * (int32_t)f / 25;
    return i >= 20u ? v + (256 - v) * (int32_t)(ms - 500u) / 100 : v;    /* (settles at 256) */
}

/* rainbow: h 0..1535 = red, yellow, green, cyan, blue, magenta */
static uint32_t mk_hue(uint32_t h)
{
    uint32_t f = h % 256u, r, g, b;
    switch ((h % 1536u) >> 8) {
    case 0: r = 255; g = f; b = 0; break;
    case 1: r = 255 - f; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = f; break;
    case 3: r = 0; g = 255 - f; b = 255; break;
    case 4: r = f; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - f; break;
    }
    return r << 16 | g << 8 | b;
}

static uint32_t mk_scale(uint32_t c, uint32_t k)       /* colour * k / 256 */
{
    if (k > 256u)
        k = 256u;
    return (((c >> 16) & 255u) * k >> 8) << 16 | (((c >> 8) & 255u) * k >> 8) << 8 | ((c & 255u) * k >> 8);
}

/* ---- pixels: blend colour c at intensity i (0..256), whitened by w, into (x, y): mode 0 = the brightest
 * wins (light), mode 1 = mix toward c by i (paint) */
static void mk_put(int32_t x, int32_t y, uint32_t c, int32_t i, int32_t w, int mode)
{
    uint16_t *p;
    int32_t o, r, g, b, cr, cg, cb;
    if ((uint32_t)x >= 240u || y < mk_y0 || y >= mk_y1)
        return;
    p = mk_buf + (y - mk_y0) * 240 + x;
    o = *p;
    cr = (o >> 11) << 3;
    cg = ((o >> 5) & 63) << 2;
    cb = (o & 31) << 3;
    r = (int32_t)(c >> 16) & 255;
    g = (int32_t)(c >> 8) & 255;
    b = (int32_t)c & 255;
    if (mode) {
        r = cr + ((r - cr) * i >> 8);
        g = cg + ((g - cg) * i >> 8);
        b = cb + ((b - cb) * i >> 8);
    } else {
        c = mk_mask;
        r = (r * i >> 8) + (c & 0xFF0000u ? w : 0);
        g = (g * i >> 8) + (c & 0xFF00u ? w : 0);
        b = (b * i >> 8) + (c & 0xFFu ? w : 0);
        if (r < cr) r = cr;
        if (g < cg) g = cg;
        if (b < cb) b = cb;
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    *p = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

/* neon shading at distance e (1/16 px) from the tube's centre line, level lvl (0..256) */
static void mk_shade(int32_t x, int32_t y, int32_t e, uint32_t c, int32_t lvl, int mode)
{
    int32_t i, w, g = mk_core + mk_glow;
    if (e >= g)
        return;
    w = (mk_core + 8 - e) * 16;                     /* the core, its edge smoothed over a pixel */
    w = w < 0 ? 0 : w > 256 ? 256 : w;
    i = (g - e) * 256 / g;                          /* the glow */
    i = (i * i >> 8) * 200 >> 8;
    if (w > i)
        i = w;
    w = w * mk_hot >> 8;
    if (mk_shine > -9000) {                         /* the shine: a white band sweeping across */
        int32_t s = x + y / 2 - mk_shine;
        s = s < 0 ? -s : s;
        if (s < 10)
            w += (10 - s) * 22 * i >> 8;
    }
    mk_put(x, y, c & mk_mask, i * lvl >> 8, w * lvl >> 8, mode);
}

typedef struct {
    int32_t ax, ay, dx, dy, len2, inv;
} mk_seg_t;

static void mk_seg(mk_seg_t *s, int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    uint32_t len;
    s->ax = ax;
    s->ay = ay;
    s->dx = bx - ax;
    s->dy = by - ay;
    s->len2 = s->dx * s->dx + s->dy * s->dy;
    len = mk_isqrt((uint32_t)s->len2);
    s->inv = len ? (int32_t)((1u << 20) / len) : 0;
}

static int32_t mk_dist(const mk_seg_t *s, int32_t px, int32_t py)     /* 1/16 px */
{
    int32_t qx = px - s->ax, qy = py - s->ay, dot = qx * s->dx + qy * s->dy, c;
    if (dot > 0 && dot < s->len2) {
        c = qx * s->dy - qy * s->dx;
        if (c < 0)
            c = -c;
        return (int32_t)(((uint32_t)c * (uint32_t)s->inv) >> 20);
    }
    if (dot > 0) {
        qx -= s->dx;
        qy -= s->dy;
    }
    return (int32_t)mk_isqrt((uint32_t)(qx * qx + qy * qy));
}

static int32_t mk_lo(int32_t v, int32_t lo) { return v < lo ? lo : v; }
static int32_t mk_hi(int32_t v, int32_t hi) { return v > hi ? hi : v; }

/* a neon line (1/16 px end points); only the pixels near it are visited */
static void mk_line(int32_t ax, int32_t ay, int32_t bx, int32_t by, uint32_t c, int32_t lvl, int mode)
{
    mk_seg_t s;
    int32_t pad = mk_core + mk_glow, y, ylo, yhi, xlo, xhi, m;
    ylo = mk_lo(((ay < by ? ay : by) - pad) >> 4, mk_y0);
    yhi = mk_hi(((ay > by ? ay : by) + pad) >> 4, mk_y1 - 1);
    if (ylo > yhi || lvl <= 0)
        return;
    xlo = mk_lo(((ax < bx ? ax : bx) - pad) >> 4, 0);
    xhi = mk_hi(((ax > bx ? ax : bx) + pad) >> 4, 239);
    mk_seg(&s, ax, ay, bx, by);
    m = (pad + 24) * (int32_t)mk_isqrt((uint32_t)s.len2);
    for (y = ylo; y <= yhi; y++) {
        int32_t py = Q(y) + 8, x0 = xlo, x1 = xhi, x;
        if (s.dy) {               /* the row's pixels within pad of the line */
            int32_t base = (py - ay) * s.dx, u = ax + (base - m) / s.dy, v = ax + (base + m) / s.dy;
            if (u > v) {
                x = u;
                u = v;
                v = x;
            }
            x0 = mk_lo(u >> 4, xlo);
            x1 = mk_hi((v >> 4) + 1, xhi);
        }
        for (x = x0; x <= x1; x++)
            mk_shade(x, y, mk_dist(&s, Q(x) + 8, py), c, lvl, mode);
    }
}

/* a filled polygon (1/16 px, even-odd), colour c mixed in by a (256 = solid) */
static void mk_poly(const int32_t *p, int n, uint32_t c, int32_t a)
{
    int32_t ylo = 1 << 20, yhi = -(1 << 20), y, k;
    uint16_t col = (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u));
    if (a <= 0)
        return;
    for (k = 0; k < n; k++) {
        if (p[2 * k + 1] < ylo)
            ylo = p[2 * k + 1];
        if (p[2 * k + 1] > yhi)
            yhi = p[2 * k + 1];
    }
    ylo = mk_lo(ylo >> 4, mk_y0);
    yhi = mk_hi(yhi >> 4, mk_y1 - 1);
    for (y = ylo; y <= yhi; y++) {
        int32_t py = Q(y) + 8, xs[12], m = 0, i, x;
        for (k = 0; k < n && m < 12; k++) {
            int32_t ax = p[2 * k], ay = p[2 * k + 1], j = (k + 1) % n, bx = p[2 * j], by = p[2 * j + 1];
            if ((ay <= py) != (by <= py)) {
                x = ax + (py - ay) * (bx - ax) / (by - ay);
                for (i = m++; i > 0 && xs[i - 1] > x; i--)
                    xs[i] = xs[i - 1];
                xs[i] = x;
            }
        }
        for (i = 0; i + 1 < m; i += 2) {
            int32_t xl = mk_lo((xs[i] + 7) >> 4, 0), xr = mk_hi((xs[i + 1] - 8) >> 4, 239);
            for (x = xl; x <= xr; x++) {
                if (a >= 256)
                    mk_buf[(y - mk_y0) * 240 + x] = col;
                else
                    mk_put(x, y, c, a, 0, 1);
            }
        }
    }
}

/* ---- the font: geometric neon tubes (Tron style), strokes on a 20 x 28 grid, every inner corner rounded.
 * A glyph: the character, its width, the points; MK_UP lifts the pen, MK_END ends it. */
#define MK_UP 100
#define MK_END 101
static const int8_t MK_FONT[] = {
    'A', 20, 0, 28, 0, 10, 10, 0, 20, 10, 20, 28, MK_UP, 0, 18, 20, 18, MK_END,
    'C', 20, 20, 0, 0, 0, 0, 28, 20, 28, MK_END,
    'D', 20, 0, 14, 0, 0, 13, 0, 20, 7, 20, 21, 13, 28, 0, 28, 0, 14, MK_END,
    'E', 19, 19, 0, 0, 0, 0, 28, 19, 28, MK_UP, 0, 14, 13, 14, MK_END,
    'F', 19, 19, 0, 0, 0, 0, 28, MK_UP, 0, 14, 13, 14, MK_END,
    'G', 20, 20, 0, 0, 0, 0, 28, 20, 28, 20, 14, 11, 14, MK_END,
    'H', 20, 0, 0, 0, 28, MK_UP, 20, 0, 20, 28, MK_UP, 0, 14, 20, 14, MK_END,
    'I', 0, 0, 0, 0, 28, MK_END,
    'K', 19, 0, 0, 0, 28, MK_UP, 19, 0, 19, 6, 11, 14, 0, 14, MK_UP, 11, 14, 19, 22, 19, 28, MK_END,
    'L', 18, 0, 0, 0, 28, 18, 28, MK_END,
    'M', 24, 0, 28, 0, 0, 12, 13, 24, 0, 24, 28, MK_END,
    'N', 20, 0, 28, 0, 0, 20, 28, 20, 0, MK_END,
    'O', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, MK_END,
    'P', 20, 0, 28, 0, 0, 20, 0, 20, 15, 0, 15, MK_END,
    'R', 20, 0, 28, 0, 0, 20, 0, 20, 14, 0, 14, MK_UP, 12, 14, 20, 21, 20, 28, MK_END,
    'S', 20, 20, 0, 0, 0, 0, 14, 20, 14, 20, 28, 0, 28, MK_END,
    'T', 20, 0, 0, 20, 0, MK_UP, 10, 0, 10, 28, MK_END,
    'U', 20, 0, 0, 0, 28, 20, 28, 20, 0, MK_END,
    'W', 24, 0, 0, 0, 28, 24, 28, 24, 0, MK_UP, 12, 28, 12, 8, MK_END,
    'Y', 20, 0, 0, 0, 13, 20, 13, MK_UP, 20, 0, 20, 28, 0, 28, MK_END,
    '0', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, MK_UP, 6, 20, 14, 8, MK_END,
    '1', 12, 0, 5, 7, 0, 7, 28, MK_END,
    '2', 20, 0, 0, 20, 0, 20, 14, 0, 14, 0, 28, 20, 28, MK_END,
    '3', 20, 0, 0, 20, 0, 20, 28, 0, 28, MK_UP, 6, 14, 20, 14, MK_END,
    '4', 20, 0, 0, 0, 17, 20, 17, MK_UP, 15, 7, 15, 28, MK_END,
    '5', 20, 20, 0, 0, 0, 0, 13, 20, 13, 20, 28, 0, 28, MK_END,
    '6', 20, 20, 0, 0, 0, 0, 28, 20, 28, 20, 14, 0, 14, MK_END,
    '7', 20, 0, 0, 20, 0, 20, 7, 9, 28, MK_END,
    '8', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, 20, 14, MK_END,
    '9', 20, 20, 14, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, MK_END,
    '-', 14, 0, 15, 14, 15, MK_END,
    '.', 0, 0, 27, 0, 28, MK_END,
    '\'', 0, 0, 0, 0, 7, MK_END,
    '!', 0, 0, 0, 0, 19, MK_UP, 0, 27, 0, 28, MK_END,
};
#define MK_GAP 9                   /* between letters, grid units */
#define MK_SPACE 9

static const int8_t *mk_glyph_of(char ch)
{
    const int8_t *g = MK_FONT;
    while (g < MK_FONT + sizeof MK_FONT) {
        if (g[0] == ch)
            return g;
        while (*g != MK_END)
            g++;
        g++;
    }
    return 0;
}

/* text width, 1/16 px, at k (1/256 px per grid unit) */
static int32_t mk_text_w(const char *s, int32_t k)
{
    int32_t w = 0;
    for (; *s; s++) {
        const int8_t *g = mk_glyph_of(*s);
        w += ((g ? g[1] : MK_SPACE) + (s[1] ? MK_GAP : 0)) * k / 16;
    }
    return w;
}

/* one glyph at (x, y) (1/16 px, top left), k = 1/256 px per grid unit; corners rounded by quadratic
 * curves (radius ~5 units); returns the advance */
static int32_t mk_glyph(char ch, int32_t x, int32_t y, int32_t k, uint32_t c, int32_t lvl)
{
    const int8_t *g = mk_glyph_of(ch);
    int32_t pt[24], n = 0, i;
    if (!g)
        return MK_SPACE * k / 16;
    if (y - Q(4) > Q(mk_y1) || y + 28 * k / 16 + Q(4) < Q(mk_y0))
        return g[1] * k / 16;
    for (i = 2;; i += 2) {
        if (g[i] == MK_UP || g[i] == MK_END) {   /* draw the stroke */
            int32_t cx = pt[0], cy = pt[1], j;
            for (j = 1; j < n; j++) {
                int32_t bx = pt[2 * j], by = pt[2 * j + 1];
                if (j < n - 1) {                      /* a corner: line to its start, then the curve */
                    int32_t ax = pt[2 * j - 2] - bx, ay = pt[2 * j - 1] - by, dx = pt[2 * j + 2] - bx,
                            dy = pt[2 * j + 3] - by, la = (int32_t)mk_isqrt((uint32_t)(ax * ax + ay * ay)),
                            lc = (int32_t)mk_isqrt((uint32_t)(dx * dx + dy * dy)), r = 5 * k / 16, t1x, t1y, t2x,
                            t2y, q, px = 0, py = 0;
                    if (r > la / 2)
                        r = la / 2;
                    if (r > lc / 2)
                        r = lc / 2;
                    t1x = bx + ax * r / la;
                    t1y = by + ay * r / la;
                    t2x = bx + dx * r / lc;
                    t2y = by + dy * r / lc;
                    mk_line(cx, cy, t1x, t1y, c, lvl, 0);
                    cx = t1x;
                    cy = t1y;
                    for (q = 1; q <= 3; q++) {        /* (1-t)^2 T1 + 2t(1-t) B + t^2 T2, t = q / 3 */
                        px = ((3 - q) * (3 - q) * t1x + 2 * q * (3 - q) * bx + q * q * t2x) / 9;
                        py = ((3 - q) * (3 - q) * t1y + 2 * q * (3 - q) * by + q * q * t2y) / 9;
                        mk_line(cx, cy, px, py, c, lvl, 0);
                        cx = px;
                        cy = py;
                    }
                } else {
                    mk_line(cx, cy, bx, by, c, lvl, 0);
                    cx = bx;
                    cy = by;
                }
            }
            n = 0;
            if (g[i] == MK_END)
                break;
            i--;                                       /* (MK_UP is one byte) */
            continue;
        }
        if (n < 12) {
            pt[2 * n] = x + g[i] * k / 16;
            pt[2 * n + 1] = y + g[i + 1] * k / 16;
            n++;
        }
    }
    return g[1] * k / 16;
}

/* a line of text centred on cx (px) at y (px), k, colour c (0: a rainbow along it) */
static void mk_text(const char *s, int32_t cx, int32_t y, int32_t k, uint32_t c, int32_t lvl)
{
    int32_t x = Q(cx) - mk_text_w(s, k) / 2, n = 0, len = 0;
    while (s[len])
        len++;
    for (; *s; s++, n++)
        x += mk_glyph(*s, x, Q(y), k, c ? c : mk_hue(1350u - (uint32_t)n * 1350u / (uint32_t)(len > 1 ? len - 1 : 1)),
                      lvl) + MK_GAP * k / 16;
}

/* ---- the title: lights letter by letter (flicker + an RGB split converging), a shine sweeps across it,
 * a starburst at the end */
#define MK_TK 224                  /* 0.875 px per grid unit */
#define MK_TY 7
#define MK_T0 3000u                /* the title's start */
static void mk_title(uint32_t t)
{
    static const char T[] = "SNOOP37";
    int32_t x, li;
    if (mk_y0 > MK_TY + 34 || t < MK_T0)
        return;
    mk_core = 24;
    mk_glow = 48;
    mk_hot = 220;
    x = Q(120) - mk_text_w(T, MK_TK) / 2;
    if (t >= MK_T0 + 950u && t < MK_T0 + 1450u)
        mk_shine = -30 + mk_cubic(mk_prog(t, MK_T0 + 950u, 500u)) * 330 / 256;
    for (li = 0; li < (int32_t)sizeof T - 1; li++) {
        uint32_t on = MK_T0 + (uint32_t)li * 70u, f, c = mk_hue(1350u - (uint32_t)li * 1350u / (sizeof T - 2u));
        int32_t lvl, split, adv = 0;
        if (t < on) {
            x += (mk_glyph_of(T[li])[1] + MK_GAP) * MK_TK / 16;
            continue;
        }
        f = (t - on) / 35u;
        lvl = f == 1u || f == 3u ? 60 : 236 + (mk_sin((int32_t)(t / 2u) - li * 90) * 20 >> 8);
        split = 6 * 16 * (256 - mk_cubic(mk_prog(t, on, 320u))) / 256;
        if (split > 4) {                           /* red and blue copies, converging */
            mk_mask = 0xFF0000u;
            mk_glyph(T[li], x + split, Q(MK_TY), MK_TK, c | 0xA00000u, lvl);
            mk_mask = 0x0000FFu;
            mk_glyph(T[li], x - split, Q(MK_TY), MK_TK, c | 0x0000A0u, lvl);
            mk_mask = 0x00FF00u;
            adv = mk_glyph(T[li], x, Q(MK_TY), MK_TK, c, lvl);
            mk_mask = 0xFFFFFFu;
        } else
            adv = mk_glyph(T[li], x, Q(MK_TY), MK_TK, c, lvl);
        x += adv + MK_GAP * MK_TK / 16;
    }
    mk_shine = -9999;
    mk_hot = 110;
    if (t >= MK_T0 + 1250u && t < MK_T0 + 1700u) {  /* starburst at the 7 */
        int32_t p = mk_prog(t, MK_T0 + 1250u, 450u), r = Q(4) + Q(14) * mk_expo(p) / 256,
                lvl = 256 - p, a;
        int32_t cx = Q(120) + mk_text_w(T, MK_TK) / 2 + Q(4), cy = Q(MK_TY + 1);
        mk_core = 10;
        mk_glow = 26;
        for (a = 0; a < 1024; a += 128) {
            int32_t rr = a % 256 ? r / 2 : r, sx = mk_sin(a + 256) * rr / 256, sy = mk_sin(a) * rr / 256;
            mk_line(cx + sx / 3, cy + sy / 3, cx + sx, cy + sy, 0xFFF8D0, lvl, 0);
        }
    }
}

/* ---- one camera for everything: position (1/16 unit), pitch, the focal length; the screen's centre at
 * (120, MK_Y0). World: x right, y up, z away from the camera; the floor is y = 0. */
#define MK_F 330                   /* px */
#define MK_Y0 150
static int32_t mk_cx, mk_cy, mk_cz, mk_sb, mk_cb;  /* (pitch: sin, cos Q15, looking down) */
static int32_t mk_yh;              /* the horizon row (1/16 px) */
static int32_t mk_hs, mk_hc, mk_hx, mk_hz;         /* the house: yaw sin, cos (Q15), position (1/16 unit) */
static uint32_t mk_t;

static void mk_camera(uint32_t t)
{
    int32_t p = mk_cubic(mk_prog(t, 3300, MK_LEN - 3300u)), a;
    mk_cx = -Q(10) * p / 256;                                    /* the end: a slow dolly in, drifting left */
    mk_cy = Q(55) - Q(5) * p / 256;
    mk_cz = mk_ota ? -Q(300) : -Q(245) + Q(24) * p / 256;          /* (the update screens: further back) */
    a = 1024 * 7 / 360;                                          /* pitch 7 degrees (1024 = a turn) */
    mk_sb = mkj_sin((uint32_t)a << 22);
    mk_cb = mkj_sin((uint32_t)(a + 256) << 22);
    mk_yh = Q(MK_Y0) - MK_F * 16 * mk_sb / mk_cb;
}

/* ---- the sky: a synthwave sun rising over the horizon at the end, twinkling stars */
#define MK_SUN 3900u               /* the sun jumps up (a spring: over, then back down) */
#define MK_PING (MK_SUN + 330u)    /* it settles into place: the ping (snoop_jingle.c MKJ_PING), the black keys */
static void mk_sky(uint32_t t)
{
    static const uint8_t STARS[] = {34, 52, 70, 88, 200, 44, 186, 98, 50, 140, 226, 128, 140, 58,
                                    10, 104, 164, 72, 112, 62, 232, 104, 60, 100, 20, 80, 214, 70};
    int32_t y, x, k, yh = mk_yh >> 4, r = 40,
            cy = mk_ota ? yh + 4 : t < MK_SUN ? yh + r + 1 : yh + r - (r - 4) * mk_spring(t - MK_SUN) / 256;
    for (y = mk_lo(cy - r, mk_y0); r > 0 && y < mk_y1 && y < yh; y++) {
        int32_t d = y - cy, hw, rel = (d + r) * 256 / (2 * r);
        uint32_t c;
        if (d >= r)
            break;
        hw = (int32_t)mk_isqrt((uint32_t)(r * r - d * d));
        if (rel > 120 && (y - (cy - r)) % 8 < (rel - 120) * 5 / 136)
            continue;                                            /* the stripes, wider toward the bottom */
        c = rel < 128 ? 0xFF0000u | (uint32_t)(224 - rel * 120 / 128) << 8 | (uint32_t)(60 - rel * 30 / 128)
                      : 0xFF0000u | (uint32_t)(104 - (rel - 128) * 80 / 128) << 8 | (uint32_t)(30 + (rel - 128) * 140 / 128);
        for (x = mk_lo(176 - hw, 0); x <= mk_hi(176 + hw, 239); x++)
            mk_put(x, y, c, 180, 0, 0);
    }
    for (k = 0; k < (int)sizeof STARS; k += 2) {
        int32_t i = 128 + (mk_sin((int32_t)t * (2 + k % 3) / 3 + k * 97) >> 1);
        if (STARS[k + 1] >= yh)
            continue;
        mk_put(STARS[k], STARS[k + 1], 0xC8C8FF, i, 0, 0);
        mk_put(STARS[k] + 1, STARS[k + 1], 0xC8C8FF, i / 4, 0, 0);
        mk_put(STARS[k] - 1, STARS[k + 1], 0xC8C8FF, i / 4, 0, 0);
        mk_put(STARS[k], STARS[k + 1] + 1, 0xC8C8FF, i / 4, 0, 0);
        mk_put(STARS[k], STARS[k + 1] - 1, 0xC8C8FF, i / 4, 0, 0);
    }
}

/* ---- the floor: a magenta grid (24 units) seen by the camera, rushing in from the horizon, then scrolling
 * toward the viewer. Per row: the depth hit and the x step; anti-aliased lines by their screen distance. */
#define MK_G Q(30)
static void mk_floor(uint32_t t)
{
    int32_t y, x, rush = mk_expo(mk_prog(t, 300, 1000)), yh = mk_yh >> 4;
    int32_t ext = yh + 1 + (240 - yh) * rush / 256, phase = (int32_t)(t * (uint32_t)MK_G / 600u) % MK_G;
    for (y = mk_lo(yh - 14, mk_y0); y < mk_y1; y++) {
        uint16_t *row = mk_buf + (y - mk_y0) * 240;
        int32_t v, d, dn, z, zn, dz, r, fz, pz, lvl, sx, xw;
        if (y < yh) {                                            /* haze over the horizon */
            uint32_t k = (uint32_t)(14 - (yh - y)) * 18u * (uint32_t)rush / 256u;
            uint32_t c = mk_scale(0x5A0A78, k * k >> 8);
            for (x = 0; x < 240; x++)
                mk_put(x, y, c, 256, 0, 0);
            continue;
        }
        if (y > ext)
            break;
        v = Q(MK_Y0) - (Q(y) + 8);
        d = (MK_F * 16 * mk_sb - v * mk_cb) >> 12;               /* -(the ray's y), the next row's */
        dn = (MK_F * 16 * mk_sb - (v - 16) * mk_cb) >> 12;
        if (d < 24)
            continue;
        z = mk_cz + mk_cy * ((v * mk_sb + MK_F * 16 * mk_cb) >> 12) / d;
        zn = mk_cz + mk_cy * (((v - 16) * mk_sb + MK_F * 16 * mk_cb) >> 12) / dn;
        dz = z - zn;                                             /* depth per pixel row */
        if (dz < 1)
            dz = 1;
        r = (mk_cy << 11) / d;                                   /* x: (x - 120) * r / 256, 1/16 unit per 1/16 px */
        fz = ((z + phase) % MK_G + MK_G) % MK_G;
        fz = fz < MK_G / 2 ? fz : MK_G - fz;
        pz = fz * 16 / dz;                                       /* 1/16 px to the nearest cross line */
        lvl = (y - yh) * 256 / (240 - yh);
        lvl = 24 + (lvl * (512 - lvl) >> 9) * 210 / 128;
        if (lvl > 256)
            lvl = 256;
        sx = r >> 4;                                             /* 1/16 unit per pixel */
        if (sx < 1)
            sx = 1;
        xw = mk_cx + (((Q(0) + 8 - Q(120)) * r) >> 8);
        for (x = 0; x < 240; x++, xw += sx) {
            int32_t fx = (xw % MK_G + MK_G) % MK_G, pd, i;
            fx = fx < MK_G / 2 ? fx : MK_G - fx;
            pd = fx * 16 / sx;
            if (pz < pd)
                pd = pz;
            if (pd >= 40)
                continue;
            i = pd < 8 ? 256 : (40 - pd) * 256 / 32;
            i = pd < 8 ? 256 - pd * 8 : (i * i >> 8) * 180 >> 8;
            row[x] = 0;
            mk_put(x, y, 0xFF32C8, i * lvl >> 8, pd < 8 ? (8 - pd) * 8 * lvl >> 8 : 0, 0);
        }
    }
}

/* ---- mountains on the horizon (far behind the house, seen by the camera) whose ridge is the music: the
 * jingle's 12 pitch bands (snoop_jingle.c mkj_band), high on the left, low on the right (under the sun) */
static uint8_t mk_bandz[12];       /* smoothed: up fast, down slowly (per frame) */
static void mk_band_tick(void)
{
    int k;
    for (k = 0; k < 12; k++) {
        int32_t v = mkj_band[k], z = mk_bandz[k];
        mk_bandz[k] = (uint8_t)(v > z ? z + (v - z) * 3 / 4 : z - (z - v) / 4);
    }
}

static void mk_pw(int32_t wx, int32_t wy, int32_t wz, int32_t *p)    /* a world point (1/16 unit) to the screen */
{
    int32_t yc, zc;
    wx -= mk_cx;
    wy -= mk_cy;
    wz -= mk_cz;
    yc = (wy * mk_cb + wz * mk_sb) >> 15;
    zc = (wz * mk_cb - wy * mk_sb) >> 15;
    if (zc < Q(8))
        zc = Q(8);
    p[0] = mk_lo(mk_hi(Q(120) + MK_F * 16 * wx / zc, 8000), -8000);
    p[1] = mk_lo(mk_hi(Q(MK_Y0) - MK_F * 16 * yc / zc, 8000), -8000);
}

#define MK_HN 25                   /* ridge points */
static void mk_hills(uint32_t t)
{
    static const uint8_t JAG[5] = {16, 30, 12, 36, 22};
    int32_t i, a[2], b[2], pa[2] = {0, 0}, pb[2] = {0, 0}, rise = mk_ota ? 256 : mk_cubic(mk_prog(t, 500, 900));
    if (mk_y0 > (mk_yh >> 4) + 2 || mk_y1 < (mk_yh >> 4) - 50 || !rise)
        return;
    mk_core = 10;
    mk_glow = 26;
    for (i = 0; i < MK_HN; i++) {
        int32_t f = (MK_HN - 1 - i) * 11 * 256 / (MK_HN - 1), k = f >> 8, w = f & 255, lv, h, x = Q(-1150) + i * Q(2300) / (MK_HN - 1);
        lv = (mk_bandz[k] * (256 - w) + mk_bandz[k < 11 ? k + 1 : 11] * w) >> 8;
        h = (Q(JAG[i % 5]) * (i & 1 ? 2 : 3) / 3 + Q(230) * lv / 255) * rise / 256;
        mk_pw(x, h, Q(1100), a);
        mk_pw(x, 0, Q(1100), b);
        if (i) {
            int32_t q[8];
            q[0] = pa[0]; q[1] = pa[1]; q[2] = a[0]; q[3] = a[1]; q[4] = b[0]; q[5] = b[1] + 16; q[6] = pb[0]; q[7] = pb[1] + 16;
            mk_poly(q, 4, 0x0C0418, 256);
            mk_line(pa[0], pa[1], a[0], a[1], mk_hue(1280u - (uint32_t)i * 640u / MK_HN), 150 + lv * 106 / 255, 0);
        }
        pa[0] = a[0]; pa[1] = a[1]; pb[0] = b[0]; pb[1] = b[1];
    }
}

/* ---- the scene's mesh, built each frame in house-local coordinates (1/16 unit), then seen by the camera
 * twice: mirrored in the floor (pass 0: the reflection) and as is (pass 1). Faces are convex polygons, wound
 * outward when built (from a point inside), culled by their screen winding, flat shaded, drawn back to front
 * within groups (walls, roof, keys, dog) with glowing edges. Memory: past the strip buffers in the pool. */
#define MK_NV 340
#define MK_NFW 2100                /* face words */
#define MK_NL 230                  /* visible faces per pass */
enum { G_WALL, G_ROOF, G_KEY, G_DOG };
#define MK_VIS(pass, a) ((pass) ? (a) > 0 : (a) < 0)      /* a face's screen winding: it faces the camera */
enum { S_WALL, S_ROOF, S_KEY, S_BLACK, S_DOG, S_EAR, S_DOOR, S_GABLE };
static const uint32_t MK_SFILL[] = {0x1A2050, 0x2C0C3C, 0, 0x0A0814, 0x0E1428, 0x160C24, 0x000000, 0x1A2050};
static const uint32_t MK_SEDGE[] = {MK_CYAN, 0xFF34C8, 0, 0x8058C8, MK_CYAN, 0x70F4FF, MK_CYAN, MK_CYAN};
static const uint8_t MK_SCORE[] = {13, 12, 8, 6, 9, 9, 13, 13};   /* edge core, 1/16 px */
static int16_t *mk_lv;             /* local vertices x y z */
static int16_t *mk_sv[2];          /* screen x y (1/16 px), per pass */
static int16_t *mk_sz;             /* the camera depth of the pass being built */
static uint16_t *mk_fd;            /* faces: hdr (n | style << 4 | key << 8 | group << 12), edge mask (2 bits an
                                    * edge: 1 dim, 2 bright), normal x y (bytes), z, the vertices */
static uint16_t *mk_lo_[2], *mk_lc[2];             /* visible faces back to front: word offset, fill (565) */
static uint8_t *mk_ll[2];                          /* their edge level */
static int16_t *mk_lz;                             /* (sorting) */
static uint16_t mk_nv, mk_nfw, mk_nl[2];
static uint16_t mk_vtail[3], mk_veye[3], mk_vnose, mk_vhead, mk_froof, mk_fneck;
static int32_t mk_kdy[MK_NK + 1];                   /* the keys' drop, 1/16 unit (MK_NK: the black keys) */
static uint16_t mk_kflash[MK_NK], mk_bflash;         /* (the black keys' flash) */
static uint16_t mk_vblack[8], mk_nblack;            /* a top corner of each black key (the sparkles) */

static void mk_mem(uint16_t *m)    /* 6720 words */
{
    mk_lv = (int16_t *)m;
    m += 3 * MK_NV;
    mk_sv[0] = (int16_t *)m;
    m += 2 * MK_NV;
    mk_sv[1] = (int16_t *)m;
    m += 2 * MK_NV;
    mk_sz = (int16_t *)m;
    m += MK_NV;
    mk_fd = m;
    m += MK_NFW;
    mk_lo_[0] = m;
    m += MK_NL;
    mk_lo_[1] = m;
    m += MK_NL;
    mk_lc[0] = m;
    m += MK_NL;
    mk_lc[1] = m;
    m += MK_NL;
    mk_lz = (int16_t *)m;
    m += MK_NL;
    mk_ll[0] = (uint8_t *)m;
    mk_ll[1] = (uint8_t *)m + MK_NL;
}

static int mk_v(int32_t x, int32_t y, int32_t z)
{
    if (mk_nv >= MK_NV)
        return MK_NV - 1;
    mk_lv[3 * mk_nv] = (int16_t)x;
    mk_lv[3 * mk_nv + 1] = (int16_t)y;
    mk_lv[3 * mk_nv + 2] = (int16_t)z;
    return mk_nv++;
}

/* a face of n vertices; ra, rb: two vertices whose midpoint is inside the solid (it sets the winding) */
static uint32_t mk_face(const uint16_t *vi, int n, uint32_t hdr, uint32_t emask, int ra, int rb)
{
    int32_t nx = 0, ny = 0, nz = 0, cx = 0, cy = 0, cz = 0, k, len;
    uint32_t off = mk_nfw;
    if (mk_nfw + 4u + (uint32_t)n > MK_NFW)
        return 0;
    for (k = 0; k < n; k++) {
        const int16_t *a = mk_lv + 3 * vi[k], *b = mk_lv + 3 * vi[(k + 1) % n];
        nx += (a[1] - b[1]) * (a[2] + b[2]) >> 4;
        ny += (a[2] - b[2]) * (a[0] + b[0]) >> 4;
        nz += (a[0] - b[0]) * (a[1] + b[1]) >> 4;
        cx += a[0];
        cy += a[1];
        cz += a[2];
    }
    while (nx > 20000 || nx < -20000 || ny > 20000 || ny < -20000 || nz > 20000 || nz < -20000) {
        nx /= 2;
        ny /= 2;
        nz /= 2;
    }
    cx = cx / n - (mk_lv[3 * ra] + mk_lv[3 * rb]) / 2;
    cy = cy / n - (mk_lv[3 * ra + 1] + mk_lv[3 * rb + 1]) / 2;
    cz = cz / n - (mk_lv[3 * ra + 2] + mk_lv[3 * rb + 2]) / 2;
    len = (int32_t)mk_isqrt((uint32_t)(nx * nx + ny * ny + nz * nz));
    if (!len)
        return 0;
    if (nx * (cx >> 4) + ny * (cy >> 4) + nz * (cz >> 4) < 0) {   /* inward: reverse it */
        uint32_t m = emask & (3u << (2 * (n - 1))), j;
        for (j = 0; j + 1 < (uint32_t)n; j++)
            m |= ((emask >> (2 * (n - 2 - j))) & 3u) << (2 * j);
        if (n <= 8)
            emask = m;
        nx = -nx;
        ny = -ny;
        nz = -nz;
        for (k = 0; k < n; k++)
            mk_fd[off + 4 + k] = vi[n - 1 - k];
    } else
        for (k = 0; k < n; k++)
            mk_fd[off + 4 + k] = vi[k];
    mk_fd[off] = (uint16_t)(hdr | (uint32_t)n);
    mk_fd[off + 1] = (uint16_t)emask;
    mk_fd[off + 2] = (uint16_t)((uint8_t)(nx * 127 / len) | (uint32_t)(uint8_t)(ny * 127 / len) << 8);
    mk_fd[off + 3] = (uint16_t)(uint8_t)(nz * 127 / len);
    mk_nfw = (uint16_t)(off + 4u + (uint32_t)n);
    return off;
}
#define MK_HDR(style, key, group) ((uint32_t)(style) << 4 | (uint32_t)(key) << 8 | (uint32_t)(group) << 12)

static uint32_t mk_quadf(int a, int b, int c, int d, uint32_t hdr, uint32_t emask, int ra, int rb)
{
    uint16_t q[4];
    q[0] = (uint16_t)a; q[1] = (uint16_t)b; q[2] = (uint16_t)c; q[3] = (uint16_t)d;
    return mk_face(q, 4, hdr, emask, ra, rb);
}

/* a box from 8 corners (c[0..3] the bottom ring, c[4..7] the top ring), all but the bottom */
static void mk_box(const int *c, uint32_t hdr, uint32_t emask)
{
    int k;
    mk_quadf(c[4], c[5], c[6], c[7], hdr, emask, c[0], c[6]);
    for (k = 0; k < 4; k++)
        mk_quadf(c[k], c[(k + 1) & 3], c[4 + ((k + 1) & 3)], c[4 + k], hdr, emask, c[0], c[6]);
}

/* the doghouse: walls 56 x 72 x 36 in four plank strips, gables to 60, an arched door, a roof of two
 * 3-unit slabs from the ridge (61) over the eaves (32, 5 out) */
static void mk_house_mesh(void)
{
    static const int8_t COL[4][2] = {{-28, -36}, {28, -36}, {28, 36}, {-28, 36}};
    static const int8_t DOOR[18] = {-10, 0, 10, 0, 10, 12, 9, 17, 5, 21, 0, 22, -5, 21, -9, 17, -10, 12};
    int w[4][5], c, j, ctr = mk_v(0, Q(30), 0), af, ab;
    uint16_t d[9];
    for (c = 0; c < 4; c++)
        for (j = 0; j < 5; j++)
            w[c][j] = mk_v(Q(COL[c][0]), Q(9 * j), Q(COL[c][1]));
    for (c = 0; c < 4; c++)
        for (j = 0; j < 4; j++)
            mk_quadf(w[c][j], w[(c + 1) & 3][j], w[(c + 1) & 3][j + 1], w[c][j + 1], MK_HDR(S_WALL, 0, G_WALL),
                     (j ? 1u : 2u) | 2u << 2 | (j == 3 ? 2u : 0u) << 4 | 2u << 6, ctr, ctr);
    af = mk_v(0, Q(60), Q(-36));
    ab = mk_v(0, Q(60), Q(36));
    {
        uint16_t g[3];
        g[0] = (uint16_t)w[0][4]; g[1] = (uint16_t)w[1][4]; g[2] = (uint16_t)af;
        mk_face(g, 3, MK_HDR(S_GABLE, 0, G_WALL), 1u << 2 | 1u << 4, ctr, ctr);
        g[0] = (uint16_t)w[2][4]; g[1] = (uint16_t)w[3][4]; g[2] = (uint16_t)ab;
        mk_face(g, 3, MK_HDR(S_GABLE, 0, G_WALL), 1u << 2 | 1u << 4, ctr, ctr);
    }
    for (j = 0; j < 9; j++)
        d[j] = (uint16_t)mk_v(Q(DOOR[2 * j]), Q(DOOR[2 * j + 1]), Q(-36) - 3);
    mk_face(d, 9, MK_HDR(S_DOOR, 0, G_WALL), 0xFFFFu, ctr, ctr);
    for (c = -1; c <= 1; c += 2) {                   /* the roof slabs */
        int s[8], k;
        for (k = 0; k < 8; k++) {
            int32_t sl = k & 1 ? Q(44) : 0, h = k & 4 ? 0 : -Q(3), z = k & 2 ? Q(41) : -Q(41);
            s[k] = mk_v(c * (sl * 192 + h * 169) / 256, Q(61) + (-sl * 169 + h * 192) / 256, z);
        }
        /* (k: bit 0 eave, bit 1 back, bit 2 top) */
        j = (int)mk_quadf(s[4], s[5], s[7], s[6], MK_HDR(S_ROOF, 0, G_ROOF), 0xAAu, s[0], s[7]);
        if (c > 0)
            mk_froof = (uint16_t)j;
        mk_quadf(s[5], s[1], s[3], s[7], MK_HDR(S_ROOF, 0, G_ROOF), 0xAAu, s[0], s[7]);
        mk_quadf(s[4], s[0], s[1], s[5], MK_HDR(S_ROOF, 0, G_ROOF), 0xAAu, s[0], s[7]);
        mk_quadf(s[6], s[7], s[3], s[2], MK_HDR(S_ROOF, 0, G_ROOF), 0xAAu, s[0], s[7]);
    }
}

/* a point on the right roof slope: s along it from the ridge, h above it, z (1/16 unit) */
static int mk_slope(int32_t s, int32_t h, int32_t z, int32_t dy)
{
    return mk_v((s * 192 + h * 169) / 256, Q(61) + (-s * 169 + h * 192) / 256 + dy, z);
}

/* the 12 keys on the right slope (boxes 3 high), the black ones raised between them */
#define MK_KW (Q(82) / MK_NK)
static void mk_keys_mesh(void)
{
    int k, c[8], j;
    for (k = 0; k < MK_NK; k++) {
        int32_t z0 = -Q(41) + k * MK_KW + 5, z1 = z0 + MK_KW - 10, dy = mk_kdy[k];
        if (dy >= Q(200))
            continue;
        for (j = 0; j < 8; j++)
            c[j] = mk_slope(j == 1 || j == 2 || j == 5 || j == 6 ? Q(43) : Q(3), j & 4 ? Q(3) : 0,
                            j == 2 || j == 3 || j == 6 || j == 7 ? z1 : z0, dy);
        mk_box(c, MK_HDR(S_KEY, k, G_KEY), 0xAAu);
    }
    mk_nblack = 0;
    if (mk_kdy[MK_NK] >= Q(200))
        return;
    for (k = 0; k + 1 < MK_NK; k++) {
        int32_t zm = -Q(41) + (k + 1) * MK_KW;
        if (k % 7 == 2 || k % 7 == 6)
            continue;
        for (j = 0; j < 8; j++)
            c[j] = mk_slope(j == 1 || j == 2 || j == 5 || j == 6 ? Q(25) : Q(3), j & 4 ? Q(7) : Q(3),
                            zm + (j == 2 || j == 3 || j == 6 || j == 7 ? 1 : -1) * MK_KW * 3 / 10, mk_kdy[MK_NK]);
        mk_box(c, MK_HDR(S_BLACK, 0, G_KEY), 0xAAu);
        if (mk_nblack < 8u)
            mk_vblack[mk_nblack++] = (uint16_t)c[5];
    }
}

/* ---- the dog: an original low-poly beagle lying on the ridge, nose to the front: hexagonal rings lofted
 * into a body and a head, two paws, floppy ears; Tron style (dark faces, glowing edges, brighter where
 * they turn away). Units; y from the ridge top, squashed and dropped as a whole. */
static const int8_t MK_BODY[] = {   /* z, centre y, half width, half height */
    -6, 10, 6, 6,   0, 10, 9, 8,   8, 10, 9, 8,   16, 9, 8, 7,   25, 10, 10, 9,   33, 10, 8, 7,   37, 9, 3, 3,
};
static const int8_t MK_HEAD[] = {
    -8, 14, 7, 7,  -13, 15, 8, 8,  -18, 14, 7, 7,  -21, 12, 5, 5,  -28, 10, 5, 4,  -32, 10, 3, 3,
};
static const int8_t MK_PAW[] = {-2, 3, 3, 3,  -29, 3, 3, 2};
static int32_t mk_dsu = 256, mk_dsv = 256, mk_ddy;  /* squash (x z, y; 1/256), drop (1/16 unit) */

static int mk_dv(int32_t x, int32_t y, int32_t z)  /* dog units (1/16) -> a house vertex */
{
    int32_t ymin = -(x < 0 ? -x : x) * 7 / 8;        /* (the belly drapes over the ridge) */
    if (y < ymin)
        y = ymin;
    return mk_v(x * mk_dsu / 213, Q(62) + y * mk_dsv / 213 + mk_ddy, Q(-6) + z * mk_dsu / 213);   /* (x 1.2) */
}

static void mk_tube(const int8_t *r, int nr, int32_t ox, int sty)
{
    int i, k, base = mk_nv;
    for (i = 0; i < nr; i++) {
        int32_t z = Q(r[4 * i]), yc = Q(r[4 * i + 1]), hw = Q(r[4 * i + 2]), hh = Q(r[4 * i + 3]);
        mk_dv(ox, yc + hh, z);
        mk_dv(ox + hw * 7 / 8, yc + hh / 2, z);
        mk_dv(ox + hw * 7 / 8, yc - hh / 2, z);
        mk_dv(ox, yc - hh, z);
        mk_dv(ox - hw * 7 / 8, yc - hh / 2, z);
        mk_dv(ox - hw * 7 / 8, yc + hh / 2, z);
    }
    for (i = 0; i + 1 < nr; i++)
        for (k = 0; k < 6; k++) {
            uint32_t f = mk_quadf(base + 6 * i + k, base + 6 * i + (k + 1) % 6, base + 6 * (i + 1) + (k + 1) % 6,
                                  base + 6 * (i + 1) + k, MK_HDR(sty, 0, G_DOG), 0x55u, base + 6 * i,
                                  base + 6 * (i + 1) + 3);
            if (!i && sty == S_DOG && !ox && mk_fneck == 0xFFFFu)
                mk_fneck = (uint16_t)f;
        }
    for (i = 0; i < 2; i++) {                        /* the end caps */
        uint16_t c[6];
        int ring = i ? nr - 1 : 0, in = i ? nr - 2 : 1;
        for (k = 0; k < 6; k++)
            c[k] = (uint16_t)(base + 6 * ring + k);
        mk_face(c, 6, MK_HDR(sty, 0, G_DOG), 0x555u, base + 6 * ring, base + 6 * in + 3);
    }
}

/* mood: 0 asleep, 1 awake (ears up), 2 sad; ear: its swing (1024 = a turn) */
static void mk_dog_mesh(int mood, int32_t ear, int32_t wag)
{
    int s, h0 = mk_nv;
    mk_fneck = 0xFFFFu;
    mk_tube(MK_BODY, 7, 0, S_DOG);
    h0 = mk_nv;
    mk_tube(MK_HEAD, 6, 0, S_DOG);
    mk_vhead = (uint16_t)(h0 + 6);                     /* the skull's top */
    mk_tube(MK_PAW, 2, Q(9) / 2, S_DOG);
    mk_tube(MK_PAW, 2, -Q(9) / 2, S_DOG);
    for (s = -1; s <= 1; s += 2) {                    /* the ears: hinged at the skull's side */
        int32_t a = (mood == 1 ? 1024 * 70 / 360 : mood == 2 ? 0 : 1024 * 12 / 360) + ear;
        int32_t sn = mkj_sin((uint32_t)a << 22) >> 7, cs = mkj_sin((uint32_t)(a + 256) << 22) >> 7, L = mood == 1 ? 13 : 17;
        int e0 = mk_dv(s * Q(7), Q(20), Q(-10)), e1 = mk_dv(s * Q(7), Q(20), Q(-17));
        int e2 = mk_dv(s * (Q(7) + Q(L) * sn / 256), Q(20) - Q(L) * cs / 256, Q(-21)),
            e3 = mk_dv(s * (Q(7) + Q(L) * sn / 256), Q(20) - Q(L) * cs / 256, Q(-7));
        mk_quadf(e0, e1, e2, e3, MK_HDR(S_EAR, 0, G_DOG), 0xAAu, h0 + 6, h0 + 9);
    }
    s = mk_hs >= 0 ? 1 : -1;                          /* the eye on the side the camera sees */
    if (mood == 1) {
        mk_veye[0] = (uint16_t)mk_dv(s * Q(7), Q(16), Q(-18));
        mk_veye[1] = mk_veye[2] = (uint16_t)mk_dv(s * Q(6), Q(15), Q(-22));
    } else {
        mk_veye[0] = (uint16_t)mk_dv(s * Q(7), Q(16), Q(-17));
        mk_veye[1] = (uint16_t)mk_dv(s * Q(7), Q(mood == 2 ? 17 : 15), Q(-19));
        mk_veye[2] = (uint16_t)mk_dv(s * Q(6), Q(16), Q(-21));
    }
    mk_vnose = (uint16_t)mk_dv(0, Q(11), Q(-33));
    mk_vtail[0] = (uint16_t)mk_dv(0, Q(15), Q(35));
    mk_vtail[1] = (uint16_t)mk_dv(wag / 2, Q(mood == 2 ? 13 : 22), Q(41));
    mk_vtail[2] = (uint16_t)mk_dv(wag, Q(mood == 2 ? 9 : 28), Q(42));
}

/* camera: the vertices of a pass (0 mirrored in the floor) to the screen */
static void mk_xform(int pass)
{
    int32_t i;
    for (i = 0; i < mk_nv; i++) {
        int32_t lx = mk_lv[3 * i], ly = mk_lv[3 * i + 1], lz = mk_lv[3 * i + 2];
        int32_t wx = ((lx * mk_hc + lz * mk_hs) >> 15) + mk_hx - mk_cx, wy = (pass ? ly : -ly) - mk_cy;
        int32_t wz = ((lz * mk_hc - lx * mk_hs) >> 15) + mk_hz - mk_cz;
        int32_t yc = (wy * mk_cb + wz * mk_sb) >> 15, zc = (wz * mk_cb - wy * mk_sb) >> 15;
        if (zc < Q(8))
            zc = Q(8);
        mk_sv[pass][2 * i] = (int16_t)mk_lo(mk_hi(Q(120) + MK_F * 16 * wx / zc, 8000), -8000);
        mk_sv[pass][2 * i + 1] = (int16_t)mk_lo(mk_hi(Q(MK_Y0) - MK_F * 16 * yc / zc, 8000), -8000);
        mk_sz[i] = (int16_t)mk_hi(zc, 32767);
    }
}

static uint32_t mk_rgb(uint16_t c)
{
    return (uint32_t)(c >> 11) << 19 | (uint32_t)((c >> 5) & 63u) << 10 | (uint32_t)(c & 31u) << 3;
}

static uint32_t mk_key_col(int32_t k)
{
    if (mk_ota && mk_step == 9u && mk_code != 1)
        return 0xFF2020;                                         /* failed: red */
    if (mk_ota && k >= (mk_step >= 5u ? MK_NK : mk_step * MK_NK / 5))
        return 0x383850;                                         /* the progress: keys not lit yet */
    return mk_hue(1350u + 1536u * 4u - ((uint32_t)k * 1350u / (MK_NK - 1) + (mk_t > 3000u ? (mk_t - 3000u) / 3u : 0u)) % 1536u);
}

/* the visible faces of a pass, shaded, back to front */
static const uint8_t MK_RANK[2][4] = {{1, 0, 2, 3}, {0, 1, 2, 3}};
static void mk_faces(int pass)
{
    uint32_t off = 0, n;
    int32_t m = 0, i;
    const int16_t *sv = mk_sv[pass];
    for (; off < mk_nfw; off += 4u + n) {
        uint32_t h = mk_fd[off], g = h >> 12, sty = (h >> 4) & 15u;
        const uint16_t *v = mk_fd + off + 4;
        int32_t a, z = 0, nx, ny, nz, l, rim;
        uint32_t c;
        n = h & 15u;
        if (pass == 0 && g >= G_KEY)
            continue;
        a = (sv[2 * v[1]] - sv[2 * v[0]]) * (sv[2 * v[2] + 1] - sv[2 * v[0] + 1]) -
            (sv[2 * v[1] + 1] - sv[2 * v[0] + 1]) * (sv[2 * v[2]] - sv[2 * v[0]]);
        if (!MK_VIS(pass, a))
            continue;
        if (m >= MK_NL)
            break;
        for (i = 0; i < (int32_t)n; i++)
            z += mk_sz[v[i]];
        z /= (int32_t)n;
        if (sty == S_DOOR)
            z -= Q(4);                                   /* (a decal on the wall) */
        nx = (int8_t)(mk_fd[off + 2] & 255u);
        ny = (int8_t)(mk_fd[off + 2] >> 8);
        nz = (int8_t)(mk_fd[off + 3] & 255u);
        l = (nx * mk_hc + nz * mk_hs) >> 15;             /* the normal, turned with the house */
        nz = (nz * mk_hc - nx * mk_hs) >> 15;
        nx = l;
        if (!pass)
            ny = -ny;
        l = (-nx * 58 + ny * 102 - nz * 51) >> 7;         /* light from the upper left front, 0..127 */
        if (l < 0)
            l = 0;
        rim = (ny * (-mk_sb >> 8) + nz * (mk_cb >> 8)) >> 7;   /* toward the camera, 0..127 */
        rim = 127 - (rim < 0 ? -rim : rim);
        c = sty == S_KEY ? mk_scale(mk_key_col((int32_t)(h >> 8) & 15), 70u + (uint32_t)mk_kflash[(h >> 8) & 15u] / 2u)
                         : MK_SFILL[sty];
        c = mk_scale(c, 96u + (uint32_t)l * 160u / 127u);
        l = g == G_DOG ? 40 + (rim * rim >> 7) * 215 / 127 : 255;  /* the dog: brighter where it turns away */
        z = (int32_t)MK_RANK[pass][g] << 13 | (8191 - mk_hi(mk_lo(z >> 2, 0), 8191));   /* rank, then far first */
        for (i = m; i > 0 && mk_lz[i - 1] > z; i--) {             /* (insertion: few faces, mostly in order) */
            mk_lz[i] = mk_lz[i - 1];
            mk_lo_[pass][i] = mk_lo_[pass][i - 1];
            mk_lc[pass][i] = mk_lc[pass][i - 1];
            mk_ll[pass][i] = mk_ll[pass][i - 1];
        }
        mk_lz[i] = (int16_t)z;
        mk_lo_[pass][i] = (uint16_t)off;
        mk_lc[pass][i] = (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u));
        mk_ll[pass][i] = (uint8_t)(pass ? l : l / 3);
        m++;
    }
    mk_nl[pass] = (uint16_t)m;
}

/* draw a pass's faces into the strip */
static void mk_draw(int pass)
{
    int32_t k;
    const int16_t *sv = mk_sv[pass];
    for (k = 0; k < mk_nl[pass]; k++) {
        uint32_t off = mk_lo_[pass][k], h = mk_fd[off], n = h & 15u, sty = (h >> 4) & 15u, em = mk_fd[off + 1], e;
        const uint16_t *v = mk_fd + off + 4;
        int32_t p[18], ylo = 99999, yhi = -99999, i, lvl = mk_ll[pass][k];
        uint32_t ec = MK_SEDGE[sty];
        for (i = 0; i < (int32_t)n; i++) {
            p[2 * i] = sv[2 * v[i]];
            p[2 * i + 1] = sv[2 * v[i] + 1];
            ylo = p[2 * i + 1] < ylo ? p[2 * i + 1] : ylo;
            yhi = p[2 * i + 1] > yhi ? p[2 * i + 1] : yhi;
        }
        if ((yhi >> 4) + 5 < mk_y0 || (ylo >> 4) - 5 >= mk_y1)
            continue;
        mk_poly(p, (int)n, mk_rgb(mk_lc[pass][k]), pass ? 256 : 170);
        if (sty == S_KEY) {
            uint32_t kk = (h >> 8) & 15u;
            ec = mk_key_col((int32_t)kk);
            if (mk_kflash[kk])
                ec = mk_scale(ec, 256u - mk_kflash[kk] / 2u) + mk_scale(0xFFFFFF, mk_kflash[kk] / 2u);
        } else if (sty == S_BLACK && mk_bflash) {
            ec = mk_scale(ec, 256u - mk_bflash * 3u / 4u) + mk_scale(0xFFE8FF, mk_bflash * 3u / 4u);
            lvl += (255 - lvl) * mk_bflash / 256;
        }
        mk_core = pass ? MK_SCORE[sty] : 6;
        mk_glow = pass ? MK_SCORE[sty] * 5 / 2 : 40;
        for (i = 0; i < (int32_t)n; i++) {
            e = n > 8u ? 2u : (em >> (2 * i)) & 3u;
            if (!e)
                continue;
            mk_line(p[2 * i], p[2 * i + 1], p[(2 * i + 2) % (2 * (int32_t)n)], p[(2 * i + 3) % (2 * (int32_t)n)], ec,
                    (e == 2u || (h >> 12) == G_DOG ? lvl : lvl * 2 / 5), 0);
        }
    }
}

/* ---- the scene each frame: the house flies in turning, the keys drop, the dog drops */
#define MK_K0 1900u                /* the first key's drop */
#define MK_KD 70u                  /* between keys */
#define MK_DF 2750u                /* the dog's fall */
#define MK_DL 2950u                /* it lands */
static uint32_t mk_land(int k) { return MK_K0 + (uint32_t)k * MK_KD + MK_LAND; }

static void mk_prep(uint32_t t)
{
    int32_t k, p, yaw, ear = 0, wag, mood = 0;
    mk_t = t;
    mk_vnose = mk_vhead = mk_fneck = 0xFFFFu;
    mk_camera(t);
    p = mk_expo(mk_prog(t, 700, 1300));                          /* the fly-in */
    yaw = 1024 * 35 / 360 - 1024 * 3 / 4 * (256 - p) / 256 + (mk_ota ? 0 : 1024 * 9 / 360 * mk_prog(t, 3300, MK_LEN - 3300u) / 256);
                                                                  /* (it keeps turning, slowly, to the end) */
    if (t > 1700u && t < 2600u && !mk_ota)                       /* a little settle swing */
        yaw += mk_sin((int32_t)((t - 1700u) * 1024u / 450u)) * 10 * (int32_t)(2600u - t) / 900 / 256;
    mk_hs = mkj_sin((uint32_t)yaw << 22);
    mk_hc = mkj_sin((uint32_t)(yaw + 256) << 22);
    mk_hx = 0;
    mk_hz = mk_ota ? 0 : Q(900) * (256 - p) / 256;
    mk_nv = 0;
    mk_nfw = 0;
    if (!mk_ota && t < 700u)
        return;
    mk_house_mesh();
    for (k = 0; k <= MK_NK; k++) {                                /* the keys: drop, bounce, flash */
        uint32_t t0 = k < MK_NK ? MK_K0 + (uint32_t)k * MK_KD : MK_PING - MK_LAND;   /* (the black keys land
                                                                                           * on the ping) */
        int32_t s;
        if (mk_ota) {
            int lit = mk_step >= 5u ? MK_NK : mk_step * MK_NK / 5;
            mk_kdy[k] = 0;
            (void)lit;
            if (k < MK_NK)
                mk_kflash[k] = 0;
            mk_bflash = 0;
            continue;
        }
        if (t < t0) {
            mk_kdy[k] = Q(999);
            continue;
        }
        s = 256 - mk_spring(t - t0);
        mk_kdy[k] = Q(60) * (s < 0 ? -s : s) / 256;
        if (k < MK_NK)
            mk_kflash[k] = (uint16_t)(t - t0 >= MK_LAND ? 256 - mk_prog(t, t0 + MK_LAND, 300u) : 0);
        else
            mk_bflash = (uint16_t)(t - t0 >= MK_LAND ? 256 - mk_prog(t, t0 + MK_LAND, 350u) : 0);
    }
    mk_keys_mesh();
    if (mk_ota) {
        mood = mk_step == 9u && mk_code != 1 ? 2 : mk_step == 5u ? 0 : 1;
        wag = mood == 1 ? Q(3) : 0;
        mk_dsu = mk_dsv = 256;
        mk_ddy = 0;
    } else {
        if (t < MK_DF)
            goto xf;
        if (t < MK_DL) {                                          /* falling, stretched */
            p = mk_prog(t, MK_DF, MK_DL - MK_DF);
            mk_ddy = Q(120) * (65536 - p * p) / 65536;
            mk_dsu = 236;
            mk_dsv = 290;
            ear = -60;
            mood = 1;
        } else {                                                  /* landed: squash, spring back */
            uint32_t s = t - MK_DL;
            int32_t sq = s < 600u ? mk_spring(s + MK_LAND) - 256 : 0;
            mk_ddy = 0;
            mk_dsv = 256 - sq * 9 / 10;
            mk_dsu = 256 + sq * 4 / 10;
            ear = s < 900u ? mk_sin((int32_t)(s * 1024u / 330u)) * (int32_t)(900u - s) / 900 / 4 : 0;
            mood = s < 450u ? 1 : 0;
        }
        wag = mk_sin((int32_t)(t * 1024u / 600u)) * Q(3) / 256;
    }
    mk_dog_mesh(mood, ear, wag);
    mk_dsu = mk_dsv = 256;
    mk_ddy = 0;
xf:
    mk_nl[0] = 0;
    mk_xform(0);
    mk_faces(0);
    mk_xform(1);
    mk_faces(1);
}

/* the dog's glowing details over its faces: the tail, the collar, the eye, the nose */
static void mk_dog_lines(void)
{
    const int16_t *sv = mk_sv[1];
    int k;
    if (mk_vnose >= mk_nv || !mk_nv)
        return;
    mk_core = 11;
    mk_glow = 28;
    mk_line(sv[2 * mk_vtail[0]], sv[2 * mk_vtail[0] + 1], sv[2 * mk_vtail[1]], sv[2 * mk_vtail[1] + 1], MK_CYAN, 256, 0);
    mk_line(sv[2 * mk_vtail[1]], sv[2 * mk_vtail[1] + 1], sv[2 * mk_vtail[2]], sv[2 * mk_vtail[2] + 1], MK_CYAN, 256, 0);
    mk_core = 20;
    mk_line(sv[2 * mk_vtail[2]], sv[2 * mk_vtail[2] + 1], sv[2 * mk_vtail[2]], sv[2 * mk_vtail[2] + 1], 0xE0FFFF, 256, 0);
    if (mk_fneck != 0xFFFFu) {                                    /* the collar: the neck ring's visible part */
        mk_core = 12;
        mk_glow = 26;
        for (k = 0; k < 6; k++) {
            uint32_t off = mk_fneck + 8u * (uint32_t)k;
            const uint16_t *v = mk_fd + off + 4;
            int32_t a = (sv[2 * v[1]] - sv[2 * v[0]]) * (sv[2 * v[2] + 1] - sv[2 * v[0] + 1]) -
                        (sv[2 * v[1] + 1] - sv[2 * v[0] + 1]) * (sv[2 * v[2]] - sv[2 * v[0]]);
            if (MK_VIS(1, a))
                mk_line(sv[2 * v[0]], sv[2 * v[0] + 1], sv[2 * v[1]], sv[2 * v[1] + 1], 0xFF30C8, 256, 0);
        }
    }
    mk_core = 13;
    mk_glow = 28;
    mk_hot = 200;
    mk_line(sv[2 * mk_veye[0]], sv[2 * mk_veye[0] + 1], sv[2 * mk_veye[1]], sv[2 * mk_veye[1] + 1], 0xB0FAFF, 256, 0);
    mk_line(sv[2 * mk_veye[1]], sv[2 * mk_veye[1] + 1], sv[2 * mk_veye[2]], sv[2 * mk_veye[2] + 1], 0xB0FAFF, 256, 0);
    mk_core = 22;
    mk_line(sv[2 * mk_vnose], sv[2 * mk_vnose + 1], sv[2 * mk_vnose], sv[2 * mk_vnose + 1], 0xFF50D0, 256, 0);
    mk_hot = 110;
}

/* Zz rising from the head */
static void mk_zz(uint32_t t, uint32_t t0)
{
    int k;
    int32_t hx, hy;
    if (t < t0 || mk_vhead >= mk_nv || !mk_nv)
        return;
    hx = mk_sv[1][2 * mk_vhead];
    hy = mk_sv[1][2 * mk_vhead + 1];
    if ((hy >> 4) - 50 > mk_y1 || (hy >> 4) < mk_y0)
        return;
    mk_core = 12;
    mk_glow = 26;
    for (k = 0; k < 2; k++) {
        int32_t p = (int32_t)((t - t0 + (uint32_t)k * 900u) % 1800u) * 256 / 1800, x, y, s, lvl;
        if (t - t0 + (uint32_t)k * 900u < 1800u && k)
            continue;
        x = hx - Q(30) - p * 7 / 4;                   /* (drifting up and left, under the title) */
        y = hy + Q(4) - p * 3 / 4;
        s = 56 + p / 4;                                  /* half size, 1/16 px */
        lvl = mk_sin(p * 2);
        if (lvl < 0)
            lvl = 0;
        mk_line(x - s, y - s, x + s, y - s, 0xB4E6FF, lvl, 0);
        mk_line(x + s, y - s, x - s, y + s, 0xB4E6FF, lvl, 0);
        mk_line(x - s, y + s, x + s, y + s, 0xB4E6FF, lvl, 0);
    }
}

/* ---- CRT power on at the start, a fade to black at the end (on the finished strip) */
#define MK_FADE (MK_PING + 600u)   /* the whole scene held, the house turning on, then all fades out */
static void mk_crt(uint32_t t)
{
    int32_t hh, hw, wh, y, x;                        /* half height, half width (1/16 px), whitening */
    if (t < 500u) {
        if (t < 140u) {
            hh = 24;
            hw = Q(120) * mk_expo(mk_prog(t, 0, 140)) / 256;
            wh = 256;
        } else {
            int32_t p = mk_expo(mk_prog(t, 140, 360));
            hh = 24 + (Q(120) - 24) * p / 256;
            hw = Q(121);
            wh = 220 - p * 220 / 256;
        }
    } else if (t >= MK_FADE) {                     /* the end: a fade to black */
        int32_t k = 256 - mk_prog(t, MK_FADE, MK_LEN - MK_FADE), n = 240 * (mk_y1 - mk_y0);
        for (y = 0; y < n; y++) {
            uint32_t c = mk_buf[y];
            mk_buf[y] = (uint16_t)((((c >> 11) * (uint32_t)k >> 8) << 11) | ((((c >> 5) & 63u) * (uint32_t)k >> 8) << 5) |
                                   ((c & 31u) * (uint32_t)k >> 8));
        }
        return;
    } else
        return;
    for (y = mk_y0; y < mk_y1; y++) {
        uint16_t *r = mk_buf + (y - mk_y0) * 240;
        int32_t d = Q(y) + 8 - Q(120), w;
        d = d < 0 ? -d : d;
        if (d > hh) {
            for (x = 0; x < 240; x++)
                r[x] = 0;
            continue;
        }
        w = wh + (hh - d < 32 ? (32 - (hh - d)) * 6 : 0);       /* the edges glow */
        if (w > 256)
            w = 256;
        for (x = 0; x < 240; x++) {
            int32_t e = Q(x) + 8 - Q(120);
            if ((e < 0 ? -e : e) > hw)
                r[x] = 0;
            else if (w > 0)
                mk_put(x, y, 0xFFFFFF, w, 0, 1);
        }
    }
}

/* ---- update screens: big words, the scene, the keys as the progress */
static void mk_ota_text(void)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    char b[12];
    mk_core = 22;
    mk_glow = 44;
    mk_hot = 190;
    if (mk_step == 5u) {
        mk_text("UPDATING...", 120, 3, 230, 0, 250);
        mk_core = 20;
        mk_glow = 40;
        mk_text("DON'T", 120, 192, 184, 0xFFB020, 256);
        mk_text("SWITCH OFF", 120, 217, 184, 0xFFB020, 256);
    } else if (mk_step == 9u) {
        if (mk_code == 1) {
            mk_text("UPDATE", 120, 6, 300, 0, 250);
            mk_text("DRY RUN OK", 120, 206, 200, 0x40FF70, 256);
        } else {
            uint32_t v = (uint32_t)(mk_code < 0 ? -mk_code : mk_code), k = 0;
            mk_text("UPDATE", 120, 6, 300, 0xFF2828, 256);
            str_cpy(b, "FAILED -", sizeof b);
            while (b[k])
                k++;
            if (v >= 10u)
                b[k++] = (char)('0' + v / 10u % 10u);
            b[k++] = (char)('0' + v % 10u);
            b[k] = 0;
            mk_text(b, 120, 206, 200, 0xFF3030, 256);
        }
    } else {
        mk_text("UPDATE", 120, 6, 300, 0, 250);
        mk_core = 18;
        mk_glow = 38;
        mk_text(STEP[mk_step < 6u ? mk_step : 0], 120, 208, 196, 0x60F0FF, 256);
    }
    mk_hot = 110;
}

/* one strip, rows y0 .. y0 + MK_SH - 1, into buf (byte-swapped for the panel); mk_prep(t) first */
static void mk_render(uint16_t *buf, int32_t y0, uint32_t t)
{
    uint32_t i;
    int32_t sy = mk_ota ? 4 : 0;                                  /* (the update screens lift the scene) */
    mk_buf = buf;
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = 0;
    mk_y0 = y0 + sy;
    mk_y1 = y0 + MK_SH + sy;
    mk_sky(t);
    mk_floor(t);
    mk_hills(t);
    if (mk_nv) {
        mk_draw(0);
        mk_draw(1);
        mk_dog_lines();
        if (!mk_ota && t >= MK_PING && t < MK_PING + 400u) {        /* the black keys land: a sparkle on each */
            int32_t p = mk_prog(t, MK_PING, 400u), sz = Q(2) + Q(5) * mk_expo(p) / 256, lv = 256 - p, x, y;
            uint32_t k;
            mk_core = 8;
            mk_glow = 22;
            for (k = 0; k < mk_nblack; k++) {
                x = mk_sv[1][2 * mk_vblack[k]] + (int32_t)(k & 1u) * Q(3);
                y = mk_sv[1][2 * mk_vblack[k] + 1] - Q(3);
                mk_line(x - sz, y, x + sz, y, 0xFFF0FF, lv, 0);
                mk_line(x, y - sz, x, y + sz, 0xFFF0FF, lv, 0);
                mk_line(x - sz / 2, y - sz / 2, x + sz / 2, y + sz / 2, 0xE0A0FF, lv / 2, 0);
                mk_line(x - sz / 2, y + sz / 2, x + sz / 2, y - sz / 2, 0xE0A0FF, lv / 2, 0);
            }
        }
    }
    if (mk_ota) {
        if (mk_step == 5u)
            mk_zz(t, 0);
        mk_y0 = y0;
        mk_y1 = y0 + MK_SH;
        for (i = (uint32_t)mk_lo(186 - y0, 0) * 240u; i < 240u * MK_SH; i++)
            buf[i] = (uint16_t)((buf[i] >> 2) & 0x39E7u);           /* a darker band under the words */
        mk_ota_text();
    } else {
        mk_title(t);
        mk_crt(t);
    }
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = (uint16_t)(buf[i] >> 8 | buf[i] << 8);
}

/* ---- the lights: the pads (reading order: 0..7 the top row, 8..15 the bottom row; <= 60 a channel) and
 * the button LEDs, with the music (snoop_jingle.c) and the pictures */
static uint32_t mk_pad(uint32_t r, uint32_t t)
{
    uint32_t c = 0, k, pos;
    if (t < 160u)
        return mk_scale(0x282828, 256u - t * 256u / 160u);         /* the CRT flash */
    if (t >= 600u && t < 2000u) {                                  /* a rainbow comet runs round the pads */
        uint32_t ring = r < 8u ? r : 15u - (r - 8u), head = (t - 600u) * 32u / 1400u % 16u, d = (head + 16u - ring) % 16u;
        if (d < 6u)
            c = mk_scale(mk_hue(ring * 96u + t), (6u - d) * 60u * 256u / 6u / 256u);
    }
    if (r < MK_NK && t >= mk_land((int)r)) {                       /* its key landed: a flash, then a glow */
        k = t - mk_land((int)r);
        k = k < 400u ? 240u - k * 160u / 400u : 80u;
        c = mk_scale(mk_key_col((int32_t)r), k * 60u / 256u);
    }
    if (t >= MK_DL) {                                              /* the chord blooms: all join a rainbow sweep */
        k = t < MK_DL + 300u ? (t - MK_DL) * 256u / 300u : 256u;
        if (t > MK_FADE)
            k = t < MK_LEN ? k * (MK_LEN - t) / (MK_LEN - MK_FADE) : 0u;
        pos = (r & 7u) * 150u + (r >> 3) * 60u;
        c = mk_scale(mk_hue(t * 3u / 2u + 1536u * 4u - pos), k * 50u / 256u);
        if (t < MK_DL + 450u)                                       /* a white swell */
            c |= mk_scale(0x3C3C3C, 256u - (t - MK_DL) * 256u / 450u);
    }
    if (t >= MK_PING && t < MK_PING + 200u && (r * 5u + t / 40u) % 3u == 0u)   /* the ping: a sparkle */
        c = 0x3C3C3C;
    return c;
}

/* button LED i (0..18) at t: a running light while the house flies in, a pulse on each key's landing, all
 * on at the bloom, a slow chase, off at the end */
static int mk_led(uint32_t i, uint32_t t)
{
    if (t < 600u || t >= MK_FADE)
        return 0;
    if (t < 1900u)
        return (t - 600u) / 45u % 19u == i;
    if (t < MK_DL)
        return (t - MK_K0) / MK_KD % 19u == i || (t - MK_K0) / MK_KD % 19u == (i + 9u) % 19u;
    if (t < MK_DL + 300u || (t >= MK_PING && t < MK_PING + 100u))
        return 1;                                                  /* (the bloom, the ping) */
    return (t / 120u + i) % 6u == 0u;
}

#ifndef MK_HOST
static uint16_t *mk_last;

static void mk_frame(uint32_t t)
{
    uint32_t s;
    lcd_sync();
    mk_mem(cv_px + 2u * 240u * MK_SH);
    mk_prep(t);
    for (s = 0; s < 240u / MK_SH; s++) {
        uint16_t *b = cv_px + (s & 1u) * (240u * MK_SH);
        if (b == mk_last)
            lcd_sync();                     /* its last strip may still be going out */
        mk_render(b, (int32_t)(s * MK_SH), t);
        fm1_wdt_feed();
        lcd_blit(0, s * MK_SH, 240, MK_SH, b);
        mk_last = b;
    }
}

static void mk_lights(uint32_t t)
{
#if BOARD_SMK37
    uint32_t r, c;
    for (r = 0; r < 16u; r++) {
        c = mk_pad(r, t);
        padled_set(SMK_PAD_OF_WHITE(r), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
    }
    padled_flush();
    for (r = 0; r < SMK_NLED; r++)
        fm1_led_key(r, mk_led(r, t));
#else
    (void)t;
#endif
}

/* the first frame, at boot (before the input and the timers run): black (the CRT is off) */
static void snoop_splash_bg(void)
{
    lcd_fill(0, 0, 240, 240, 0);
}

/* the animation, MK_LEN ms; a new key, pad or button press ends it */
static void snoop_splash(void)
{
    uint32_t t0 = fm1_ms, t = 0, held = fm1_in.notes | fm1_in.buttons;
    mk_ota = 0;
    mkj_start();
    for (;;) {
        uint32_t now = fm1_in.notes | fm1_in.buttons;
        mk_band_tick();
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        t = fm1_ms - t0;
        if (t >= MK_LEN || (now & ~held))
            break;
        held &= now;                        /* (held at boot: a new press of it counts) */
        mk_frame(t);
        mk_lights(t);
    }
    mkj_stop();                             /* (a skip: fades out in 150 ms; else it has ended) */
    mk_lights(MK_LEN);
    lcd_sync();
    lcd_fill(0, 0, 240, 240, 0);
    if (t >= MK_LEN) {                      /* SLOOP's first screen fades in (drawn four times, brighter) */
        uint32_t k;
        for (k = 1; k <= 4u; k++) {
            uint32_t t1 = fm1_ms;
            lcd_dim = k * 64u;
            ui.force = 1;
            ui_draw();
            lcd_sync();
            while (fm1_ms - t1 < 70u && !((fm1_in.notes | fm1_in.buttons) & ~held)) {
                fm1_wdt_feed();
                usb_retry(fm1_ms);
            }
        }
        lcd_dim = 256u;
    }
    ui.force = 1;                           /* (the main loop draws it all again, at full brightness) */
}

/* a firmware-update screen (felucca.c ota_show): one still frame; results stay 1.5 s */
static void snoop_ota(uint32_t step, int32_t code)
{
    uint32_t r, c;
    mk_ota = 1;
    mk_step = (uint8_t)step;
    mk_code = (int8_t)(code < -99 ? -99 : code > 99 ? 99 : code);
    mk_frame(4000u);
    mk_ota = 0;
#if BOARD_SMK37
    for (r = 0; r < 16u; r++) {
        if (step == 5u)                                   /* calm: a dim rainbow */
            c = mk_scale(mk_hue(r * 96u), 40u);
        else if (step == 9u)
            c = code == 1 ? 0x00C040u : 0xC00000u;
        else
            c = r < step * 16u / 5u ? mk_scale(mk_hue(1350u - r * 90u), 200u) : 0u;
        padled_set(SMK_PAD_OF_WHITE(r), (uint8_t)(c >> 16) * 60u / 255u, (uint8_t)(c >> 8) * 60u / 255u,
                   (uint8_t)c * 60u / 255u);
    }
    padled_flush();
#else
    (void)r;
    (void)c;
#endif
    lcd_sync();
    if (step >= 9u)
        fm1_delay_ms(1500);
}
#endif
