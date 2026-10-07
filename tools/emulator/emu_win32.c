/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP emulator, the Windows side: an FM-1 panel (LCD, 14 buttons, 27 keys with their LEDs,
 * 7 encoders, MASTER), the interrupt model, waveOut audio, MIDI in, the flash image on disk.
 *
 * Interrupts: TIMER5 (10 kHz) and ALNK0 (a half buffer free) run on their own threads holding the
 * CPU lock; the firmware thread (fm1_main) takes the same lock while its interrupts are off
 * (fm1_irq_off / irq_save / fm1__lock). An ISR waiting for the lock goes first, as a pending
 * interrupt fires at sti on the chip. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hal/emu_bridge.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

/* panel labels and encoder roles, as firmware/src/panel.c */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const EN_NAME[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
#define NKEYS 27u
#define NOTE_ID(n) (14u + (n))

/* ------------------------------------------------------------------ CPU / interrupts --- */
static CRITICAL_SECTION cpu;
static __declspec(thread) int in_isr;
static int fw_irq_off;                       /* the firmware thread holds cpu */
static DWORD fw_tid;
static volatile LONG isr_waiting, irq_enabled;

void emu_irq_off(void)
{
    if (in_isr || fw_irq_off)
        return;
    while (isr_waiting)
        SwitchToThread();
    EnterCriticalSection(&cpu);
    fw_irq_off = 1;
}

void emu_irq_on(void)
{
    if (in_isr || !fw_irq_off)
        return;
    fw_irq_off = 0;
    LeaveCriticalSection(&cpu);
}

void emu_irq_enable_all(void) { InterlockedExchange(&irq_enabled, 1); }

static void isr_enter(void)
{
    InterlockedIncrement(&isr_waiting);
    EnterCriticalSection(&cpu);
    InterlockedDecrement(&isr_waiting);
    in_isr = 1;
}

static void isr_leave(void)
{
    in_isr = 0;
    LeaveCriticalSection(&cpu);
}

void emu_idle(void)
{
    if (!in_isr && !fw_irq_off && GetCurrentThreadId() == fw_tid)
        Sleep(1);
}

static LARGE_INTEGER qpf, qp0;
uint32_t emu_ticks24(void)
{
    LARGE_INTEGER c;
    uint64_t d, f = (uint64_t)qpf.QuadPart;
    QueryPerformanceCounter(&c);
    d = (uint64_t)(c.QuadPart - qp0.QuadPart);
    return (uint32_t)((d / f) * 24000000ull + (d % f) * 24000000ull / f);
}

void emu_sleep_ms(uint32_t ms) { Sleep(ms); }

/* ------------------------------------------------------------------ TIMER5 --- */
static void (*timer_isr)(void);

static DWORD WINAPI timer_thread(LPVOID arg)
{
    uint64_t done = 0;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    for (;;) {
        uint64_t due;
        uint32_t n = 0;
        Sleep(1);
        if (!irq_enabled)
            continue;
        due = (uint64_t)emu_ticks24() / 2400u;          /* 100 us periods (wraps with the ticks) */
        if (!done || due < done || due - done > 1000u)
            done = due;                                 /* start, wrap, or a stall: no burst */
        while (done < due && n++ < 200u) {
            isr_enter();
            timer_isr();
            isr_leave();
            done++;
        }
    }
}

void emu_timer5_start(void (*isr)(void))
{
    timer_isr = isr;
    CloseHandle(CreateThread(NULL, 0, timer_thread, NULL, 0, NULL));
}

/* ------------------------------------------------------------------ ALNK0 -> waveOut --- */
#define NBUF 5
volatile uint32_t emu_audio_on;
static const int32_t *abuf_p;
static uint32_t abuf_half_words;
static HWAVEOUT wo;
static WAVEHDR whdr[NBUF];
static int16_t *pcm[NBUF];
static HANDLE audio_ev;
static FILE *volatile wav;                          /* --wav: what the DAC got, 44.1 kHz stereo 16-bit */
static uint32_t wav_bytes;

static void wav_header(FILE *f, uint32_t bytes)
{
    uint32_t v;
    uint16_t a = 1, ch = 2, ba = 4, bits = 16;
    uint32_t sr = 44100, br = 44100 * 4;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
}

static void wav_close(void)
{
    FILE *f = wav;
    if (!f)
        return;
    wav = NULL;
    Sleep(30);                                      /* (the audio thread's last write) */
    wav_header(f, wav_bytes);
    fclose(f);
}

static void audio_fill(int i)
{
    static uint32_t half;
    int16_t *d = pcm[i];
    uint32_t j;
    if (irq_enabled && emu_audio_on) {
        const int32_t *s;
        isr_enter();
        emu_fw_alnk_half(half);
        isr_leave();
        s = abuf_p + half * abuf_half_words;
        for (j = 0; j < abuf_half_words; j++) {     /* 24-bit left-justified -> 16-bit */
            int32_t v = s[j] >> 8;
            d[j] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        half ^= 1u;
    } else {
        memset(d, 0, abuf_half_words * sizeof *d);
    }
    {
        FILE *f = wav;
        if (f) {
            fwrite(d, sizeof *d, abuf_half_words, f);
            wav_bytes += abuf_half_words * (uint32_t)sizeof *d;
        }
    }
    whdr[i].dwFlags &= ~WHDR_DONE;
    waveOutWrite(wo, &whdr[i], sizeof whdr[i]);
}

static DWORD WINAPI audio_thread(LPVOID arg)
{
    WAVEFORMATEX f;
    int i;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = 2;
    f.nSamplesPerSec = 44100;                           /* FS (core.h) */
    f.wBitsPerSample = 16;
    f.nBlockAlign = 4;
    f.nAvgBytesPerSec = 44100 * 4;
    audio_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (waveOutOpen(&wo, WAVE_MAPPER, &f, (DWORD_PTR)audio_ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
        return 0;                                       /* no audio device: run silent */
    for (i = 0; i < NBUF; i++) {
        pcm[i] = calloc(abuf_half_words, sizeof(int16_t));
        whdr[i].lpData = (LPSTR)pcm[i];
        whdr[i].dwBufferLength = abuf_half_words * sizeof(int16_t);
        waveOutPrepareHeader(wo, &whdr[i], sizeof whdr[i]);
        audio_fill(i);
    }
    for (;;) {
        WaitForSingleObject(audio_ev, 50);
        for (i = 0; i < NBUF; i++)
            if (whdr[i].dwFlags & WHDR_DONE)
                audio_fill(i);
    }
}

void emu_audio_start(const int32_t *buf, uint32_t half_words)
{
    abuf_p = buf;
    abuf_half_words = half_words;
    CloseHandle(CreateThread(NULL, 0, audio_thread, NULL, 0, NULL));
}

/* ------------------------------------------------------------------ flash image --- */
uint8_t *emu_flash;
static char flash_path[MAX_PATH];
static volatile LONG flash_dirty;
static volatile DWORD flash_last;

/* SMP_DATA[off] with a uint32 off reaches the user sample slots (eng_sample.c): the image must sit
 * above SMP_DATA and less than 4 GiB from it */
static uint8_t *flash_alloc(void)
{
    uintptr_t a = ((uintptr_t)emu_fw_smp_data() + 0x1000000u) & ~(uintptr_t)0xFFFFu;
    int i;
    for (i = 0; i < 240; i++, a += 0x1000000u) {
        void *p = VirtualAlloc((void *)a, EMU_FLASH_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p)
            return p;
    }
    return VirtualAlloc(NULL, EMU_FLASH_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void flash_load(void)
{
    FILE *f = fopen(flash_path, "rb");
    size_t n = 0;
    if (f) {
        n = fread(emu_flash, 1, EMU_FLASH_SIZE, f);
        fclose(f);
    }
    if (n < EMU_FLASH_SIZE)
        memset(emu_flash + n, 0xFF, EMU_FLASH_SIZE - n);   /* erased NOR */
}

static void flash_save(void)
{
    char tmp[MAX_PATH + 8];
    FILE *f;
    InterlockedExchange(&flash_dirty, 0);
    snprintf(tmp, sizeof tmp, "%s.tmp", flash_path);
    f = fopen(tmp, "wb");
    if (!f)
        return;
    fwrite(emu_flash, 1, EMU_FLASH_SIZE, f);
    fclose(f);
    MoveFileExA(tmp, flash_path, MOVEFILE_REPLACE_EXISTING);
}

void emu_flash_written(void)
{
    flash_last = GetTickCount();
    InterlockedExchange(&flash_dirty, 1);
}

/* ------------------------------------------------------------------ panel state --- */
uint16_t emu_lcd[240 * 240];
static volatile uint64_t keys_mouse, keys_kbd, keys_latch;
static volatile LONG64 keys_tap;                    /* pressed since the last frame: a tap shorter than a
                                                     * scan frame (~1.1 ms) still counts once */
static volatile LONG enc_acc[7];
static int32_t enc_angle[NE];                       /* (drawing only) */
static volatile LONG master = 800;                  /* MASTER pot, 0..1023 */
static char halt_msg[64];
static HWND wnd;

uint64_t emu_keys(void)                             /* (the input frame; drawing reads held_keys) */
{
    return keys_mouse | keys_kbd | keys_latch | (uint64_t)InterlockedExchange64(&keys_tap, 0);
}

static uint64_t held_keys(void) { return keys_mouse | keys_kbd | keys_latch; }

static void press(int id)
{
    InterlockedOr64(&keys_tap, (LONG64)(1ull << id));
}
int32_t emu_enc_take(uint32_t e) { return e < 7u ? InterlockedExchange(&enc_acc[e], 0) : 0; }

int32_t emu_adc(uint32_t ch)
{
    if (ch == 4u)
        return master;
    if (ch == 3u)
        return 620;                                     /* battery: full (ui_draw.c: >= 591) */
    return -1;
}

static void turn(uint32_t role, int32_t steps)
{
    uint32_t e;
    int32_t dir;
    emu_fw_turn(role, &e, &dir);
    InterlockedExchangeAdd(&enc_acc[e % 7u], steps * dir);
    enc_angle[role] += steps;
}

void emu_reboot(void)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    flash_save();
    wav_close();
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (CreateProcessA(NULL, GetCommandLineA(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    ExitProcess(0);
}

void emu_halt(const char *why)
{
    emu_audio_on = 0;
    snprintf(halt_msg, sizeof halt_msg, "%s", why);
    flash_save();
    if (wnd)
        PostMessageA(wnd, WM_APP, 0, 0);
    if (fw_irq_off) {
        fw_irq_off = 0;
        LeaveCriticalSection(&cpu);
    }
    ExitThread(0);
}

/* ------------------------------------------------------------------ MIDI in --- */
static HMIDIIN midi_in;

static void CALLBACK midi_proc(HMIDIIN h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)h; (void)inst; (void)p2;
    if (msg != MIM_DATA || !irq_enabled)
        return;
    isr_enter();
    emu_fw_midi((uint32_t)(p1 & 0xFF), (uint32_t)((p1 >> 8) & 0xFF), (uint32_t)((p1 >> 16) & 0xFF));
    isr_leave();
}

static void midi_open(int dev, char *name, size_t len)
{
    MIDIINCAPSA caps;
    name[0] = 0;
    if (dev < 0 || (UINT)dev >= midiInGetNumDevs())
        return;
    if (midiInOpen(&midi_in, (UINT)dev, (DWORD_PTR)midi_proc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        midi_in = NULL;
        return;
    }
    midiInStart(midi_in);
    if (midiInGetDevCapsA((UINT_PTR)dev, &caps, sizeof caps) == MMSYSERR_NOERROR)
        snprintf(name, len, "%s", caps.szPname);
}

/* ------------------------------------------------------------------ layout --- */
#define LCD_X 16
#define LCD_Y 16
#define LCD_S 2
#define PANEL_X (LCD_X + 240 * LCD_S + 24)
#define WIN_W 1000
#define KEY_Y (LCD_Y + 240 * LCD_S + 20)
#define KEY_H 150
#define WIN_H (KEY_Y + KEY_H + 48)

static RECT btn_rc[NB], key_rc[NKEYS];
static POINT enc_c[NE + 1];                         /* + MASTER */
#define ENC_R 30
static int key_black[NKEYS];

static void layout(void)
{
    static const int black12[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F */
    int i, w = 0, x0 = LCD_X, kw = (WIN_W - 2 * LCD_X) / 16;
    for (i = 0; i < NE; i++) {                          /* SELECT ALGO PRESET / K1..K4 */
        if (i < 3)
            enc_c[i].x = PANEL_X + 50 + i * 100, enc_c[i].y = LCD_Y + 50;
        else
            enc_c[i].x = PANEL_X + 50 + (i - 3) * 100, enc_c[i].y = LCD_Y + 160;
    }
    enc_c[NE].x = PANEL_X + 380;                        /* MASTER */
    enc_c[NE].y = LCD_Y + 50;
    for (i = 0; i < NB; i++) {                          /* two rows of 7 */
        int r = i / 7, c = i % 7;
        btn_rc[i].left = PANEL_X + c * 64;
        btn_rc[i].top = LCD_Y + 250 + r * 90;
        btn_rc[i].right = btn_rc[i].left + 56;
        btn_rc[i].bottom = btn_rc[i].top + 56;
    }
    for (i = 0; i < (int)NKEYS; i++) {
        key_black[i] = black12[i % 12];
        if (!key_black[i]) {
            key_rc[i].left = x0 + w * kw;
            key_rc[i].right = key_rc[i].left + kw - 2;
            key_rc[i].top = KEY_Y;
            key_rc[i].bottom = KEY_Y + KEY_H;
            w++;
        } else {
            int edge = x0 + w * kw;                     /* between the previous white and the next */
            key_rc[i].left = edge - kw / 3;
            key_rc[i].right = edge + kw / 3 - 2;
            key_rc[i].top = KEY_Y;
            key_rc[i].bottom = KEY_Y + KEY_H * 6 / 10;
        }
    }
}

static int hit_key(POINT p)
{
    int i;
    for (i = 0; i < (int)NKEYS; i++)
        if (key_black[i] && PtInRect(&key_rc[i], p))
            return i;
    for (i = 0; i < (int)NKEYS; i++)
        if (!key_black[i] && PtInRect(&key_rc[i], p))
            return i;
    return -1;
}

static int hit_btn(POINT p)
{
    int i;
    for (i = 0; i < NB; i++)
        if (PtInRect(&btn_rc[i], p))
            return i;
    return -1;
}

static int hit_enc(POINT p)                         /* 0..NE-1, NE = MASTER */
{
    int i;
    for (i = 0; i <= NE; i++) {
        int dx = p.x - enc_c[i].x, dy = p.y - enc_c[i].y;
        if (dx * dx + dy * dy <= (ENC_R + 8) * (ENC_R + 8))
            return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ drawing --- */
static uint32_t lcd32[240 * 240];
static HFONT font_s, font_b;

static COLORREF led_col(int led, COLORREF off)
{
    return led == 2 ? RGB(255, 170, 40) : led == 1 ? RGB(130, 85, 30) : led == 3 ? RGB(85, 70, 50) : off;
}

static void fill(HDC dc, const RECT *r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void circle(HDC dc, int x, int y, int r, COLORREF fillc, COLORREF edge)
{
    HBRUSH b = CreateSolidBrush(fillc);
    HPEN p = CreatePen(PS_SOLID, 2, edge);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, x - r, y - r, x + r, y + r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void text(HDC dc, int x, int y, const char *s, COLORREF c, HFONT f)
{
    SIZE sz;
    SelectObject(dc, f);
    SetTextColor(dc, c);
    GetTextExtentPoint32A(dc, s, (int)strlen(s), &sz);
    TextOutA(dc, x - sz.cx / 2, y, s, (int)strlen(s));
}

static void paint(HDC dc)
{
    RECT all = {0, 0, WIN_W, WIN_H};
    BITMAPINFO bi;
    uint64_t held = held_keys();
    int i;
    char s[160];
    fill(dc, &all, RGB(28, 28, 32));
    SetBkMode(dc, TRANSPARENT);

    for (i = 0; i < 240 * 240; i++) {                   /* RGB565 -> BGRA */
        uint32_t p = emu_lcd[i];
        lcd32[i] = ((p >> 11) & 31u) * 255u / 31u << 16 | ((p >> 5) & 63u) * 255u / 63u << 8 | (p & 31u) * 255u / 31u;
    }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = 240;
    bi.bmiHeader.biHeight = -240;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, LCD_X, LCD_Y, 240 * LCD_S, 240 * LCD_S, 0, 0, 240, 240, lcd32, &bi, DIB_RGB_COLORS, SRCCOPY);

    for (i = 0; i <= NE; i++) {                         /* encoders + MASTER */
        int x = enc_c[i].x, y = enc_c[i].y;
        double a = i < NE ? enc_angle[i] * (3.14159265 / 12.0)
                          : (master / 1023.0 - 0.5) * 1.5 * 3.14159265;
        HPEN p = CreatePen(PS_SOLID, 3, RGB(240, 240, 240));
        HGDIOBJ op;
        circle(dc, x, y, ENC_R, RGB(60, 60, 66), i < NE ? RGB(110, 110, 120) : RGB(200, 120, 60));
        op = SelectObject(dc, p);
        MoveToEx(dc, x, y, NULL);
        LineTo(dc, x + (int)(sin(a) * (ENC_R - 6)), y - (int)(cos(a) * (ENC_R - 6)));
        SelectObject(dc, op);
        DeleteObject(p);
        text(dc, x, y + ENC_R + 4, i < NE ? EN_NAME[i] : "MASTER", RGB(200, 200, 200), font_s);
    }

    for (i = 0; i < NB; i++) {                          /* buttons, lit by their LED */
        uint32_t id = emu_fw_btn((uint32_t)i);
        int down = (int)((held >> id) & 1u), latched = (int)((keys_latch >> id) & 1u);
        RECT r = btn_rc[i];
        fill(dc, &r, down ? RGB(150, 150, 160) : RGB(70, 70, 78));
        InflateRect(&r, -4, -4);
        fill(dc, &r, led_col(emu_fw_led(id), down ? RGB(120, 120, 130) : RGB(50, 50, 56)));
        text(dc, (btn_rc[i].left + btn_rc[i].right) / 2, btn_rc[i].bottom + 4, B_NAME[i],
             latched ? RGB(255, 200, 80) : RGB(220, 220, 220), font_b);
    }

    for (i = 0; i < 2 * (int)NKEYS; i++) {              /* keys: whites first, then blacks over them */
        int k = i % (int)NKEYS, black = i >= (int)NKEYS;
        uint32_t id = NOTE_ID((uint32_t)k);
        int down, led;
        RECT r;
        if (key_black[k] != black)
            continue;
        down = (int)((held >> id) & 1u);
        led = emu_fw_led(id);
        r = key_rc[k];
        fill(dc, &r, black ? (down ? RGB(90, 90, 100) : RGB(20, 20, 24)) : (down ? RGB(190, 190, 200) : RGB(235, 235, 235)));
        circle(dc, (r.left + r.right) / 2, r.bottom - 16, 6, led_col(led, black ? RGB(50, 50, 50) : RGB(170, 170, 170)),
               RGB(40, 40, 40));
    }

    snprintf(s, sizeof s, "%s", halt_msg[0] ? halt_msg :
             "Notas Z..  /  Q..O   |   F1-F10 capas   Espacio PLAY   Tab REC   RePag/AvPag OCT   "
             "Flechas SELECT/PRESET   Rueda: encoders   Clic der: enclavar   F12 captura");
    SelectObject(dc, font_s);
    SetTextColor(dc, halt_msg[0] ? RGB(255, 90, 90) : RGB(150, 150, 150));
    TextOutA(dc, LCD_X, KEY_Y + KEY_H + 10, s, (int)strlen(s));
    {
        uint32_t ms, halves, late, cpu, k;
        emu_fw_stats(&ms, &halves, &late, &cpu, &k);
        snprintf(s, sizeof s, "t %u.%us   DSP en la PC %u%%   audio %u medios buffers (tarde %u)   entradas %08X",
                 ms / 1000u, ms / 100u % 10u, cpu * 100u / 256u, halves, late, k);
        TextOutA(dc, LCD_X, KEY_Y + KEY_H + 26, s, (int)strlen(s));
    }
}

static void screenshot(void)
{
    char dir[MAX_PATH], path[MAX_PATH + 64], *sl;
    SYSTEMTIME t;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    FILE *f;
    int y, x;
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    if ((sl = strrchr(dir, '\\')) != NULL)
        *sl = 0;
    strcat(dir, "\\shots");
    CreateDirectoryA(dir, NULL);
    GetLocalTime(&t);
    snprintf(path, sizeof path, "%s\\sloop_%04d%02d%02d_%02d%02d%02d.bmp", dir, t.wYear, t.wMonth, t.wDay,
             t.wHour, t.wMinute, t.wSecond);
    if (!(f = fopen(path, "wb")))
        return;
    memset(&fh, 0, sizeof fh);
    memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + 240 * 240 * 3;
    ih.biSize = sizeof ih;
    ih.biWidth = 240;
    ih.biHeight = -240;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    for (y = 0; y < 240; y++)
        for (x = 0; x < 240; x++) {
            uint32_t p = emu_lcd[y * 240 + x];
            uint8_t px[3] = {(uint8_t)((p & 31u) * 255u / 31u), (uint8_t)(((p >> 5) & 63u) * 255u / 63u),
                             (uint8_t)(((p >> 11) & 31u) * 255u / 31u)};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

/* ------------------------------------------------------------------ input --- */
static int note_of_vk(WPARAM vk)                    /* tracker layout, F3..G5 */
{
    switch (vk) {
    case 'Z': return 0;  case 'S': return 1;  case 'X': return 2;  case 'D': return 3;
    case 'C': return 4;  case 'F': return 5;  case 'V': return 6;  case 'B': return 7;
    case 'H': return 8;  case 'N': return 9;  case 'J': return 10; case 'M': return 11;
    case VK_OEM_COMMA: return 12; case 'L': return 13; case VK_OEM_PERIOD: return 14;
    case VK_OEM_1: return 15; case VK_OEM_2: return 16;
    case 'Q': return 12; case '2': return 13; case 'W': return 14; case '3': return 15;
    case 'E': return 16; case '4': return 17; case 'R': return 18; case 'T': return 19;
    case '6': return 20; case 'Y': return 21; case '7': return 22; case 'U': return 23;
    case 'I': return 24; case '9': return 25; case 'O': return 26;
    }
    return -1;
}

static int label_of_vk(WPARAM vk)
{
    if (vk >= VK_F1 && vk <= VK_F10)
        return B_FX + (int)(vk - VK_F1);
    switch (vk) {
    case VK_SPACE: return B_PLAY;
    case VK_TAB: case VK_RETURN: case VK_BACK: return B_REC;
    case VK_NEXT: case VK_OEM_4: return B_OCTDN;
    case VK_PRIOR: case VK_OEM_6: return B_OCTUP;
    }
    return -1;
}

static int vk_id[256];                              /* matrix id + 1 held by a PC key */
static int mouse_id = -1;

static void key_event(WPARAM vk, int down)
{
    int n, l, id = -1;
    if (vk >= 256)
        return;
    if (!down) {
        if (vk_id[vk]) {
            keys_kbd &= ~(1ull << (vk_id[vk] - 1));
            vk_id[vk] = 0;
        }
        return;
    }
    if (vk == VK_UP || vk == VK_DOWN) {
        turn(EN_SELECT, vk == VK_UP ? 1 : -1);
        return;
    }
    if (vk == VK_LEFT || vk == VK_RIGHT) {
        turn(EN_PRESET, vk == VK_RIGHT ? 1 : -1);
        return;
    }
    if (vk == VK_F12) {
        screenshot();
        return;
    }
    if ((n = note_of_vk(vk)) >= 0)
        id = (int)NOTE_ID((uint32_t)n);
    else if ((l = label_of_vk(vk)) >= 0)
        id = (int)emu_fw_btn((uint32_t)l);
    if (id < 0 || vk_id[vk])
        return;
    vk_id[vk] = id + 1;
    keys_kbd |= 1ull << id;
    press(id);
}

static int id_at(POINT p)
{
    int k = hit_key(p), b;
    if (k >= 0)
        return (int)NOTE_ID((uint32_t)k);
    if ((b = hit_btn(p)) >= 0)
        return (int)emu_fw_btn((uint32_t)b);
    return -1;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps), mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, WIN_W, WIN_H);
        HGDIOBJ old = SelectObject(mem, bm);
        paint(mem);
        BitBlt(dc, 0, 0, WIN_W, WIN_H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bm);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (flash_dirty && GetTickCount() - flash_last > 500u)
            flash_save();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_APP: {
        char t[128];
        snprintf(t, sizeof t, "SLOOP FM-1 Emulator - %s", halt_msg);
        SetWindowTextA(h, t);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!(lp & 0x40000000))                         /* not autorepeat */
            key_event(wp, 1);
        else if (wp == VK_UP || wp == VK_DOWN || wp == VK_LEFT || wp == VK_RIGHT)
            key_event(wp, 1);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        key_event(wp, 0);
        return 0;
    case WM_KILLFOCUS:
        keys_kbd = 0;
        memset(vk_id, 0, sizeof vk_id);
        return 0;
    case WM_LBUTTONDOWN: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        int id = id_at(p);
        SetCapture(h);
        if (id >= 0) {
            mouse_id = id;
            keys_mouse = 1ull << id;
            press(id);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (mouse_id >= 0 && (wp & MK_LBUTTON)) {       /* glide across the keys */
            POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
            int id = id_at(p);
            if (id != mouse_id) {
                mouse_id = id;
                keys_mouse = id >= 0 ? 1ull << id : 0;
                if (id >= 0)
                    press(id);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        mouse_id = -1;
        keys_mouse = 0;
        return 0;
    case WM_RBUTTONDOWN: {                              /* latch: hold a button / key while using the mouse */
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        int id = id_at(p);
        if (id >= 0)
            keys_latch ^= 1ull << id;
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT p = {(short)LOWORD(lp), (short)HIWORD(lp)};
        int steps = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA, e;
        if (!steps)
            steps = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
        ScreenToClient(h, &p);
        e = hit_enc(p);
        if (e == NE) {
            LONG v = master + steps * 32;
            master = v < 0 ? 0 : v > 1023 ? 1023 : v;
        } else {
            turn(e >= 0 ? (uint32_t)e : EN_SELECT, steps);   /* elsewhere: SELECT */
        }
        return 0;
    }
    case WM_CLOSE:
        flash_save();                                   /* the image as written; RAM state is lost, as at power-off */
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

/* ------------------------------------------------------------------ main --- */
static DWORD WINAPI fw_thread(LPVOID arg)
{
    (void)arg;
    emu_fw_main();
    return 0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSEXA wc;
    RECT wr = {0, 0, WIN_W, WIN_H};
    MSG msg;
    char *sl, title[256], midi_name[64];
    int midi_dev = 0, i;
    (void)prev; (void)cmd;

    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&qp0);
    InitializeCriticalSection(&cpu);
    timeBeginPeriod(1);

    GetModuleFileNameA(NULL, flash_path, MAX_PATH);     /* default: next to the exe */
    if ((sl = strrchr(flash_path, '\\')) != NULL)
        sl[1] = 0;
    strcat(flash_path, "sloop_flash.bin");
    for (i = 1; i < __argc; i++) {
        if (!strcmp(__argv[i], "--flash") && i + 1 < __argc)
            snprintf(flash_path, sizeof flash_path, "%s", __argv[++i]);
        else if (!strcmp(__argv[i], "--midi") && i + 1 < __argc)
            midi_dev = atoi(__argv[++i]);
        else if (!strcmp(__argv[i], "--no-midi"))
            midi_dev = -1;
        else if (!strcmp(__argv[i], "--wav") && i + 1 < __argc) {
            FILE *f = fopen(__argv[++i], "wb");
            if (f)
                wav_header(f, 0);
            wav = f;
        }
    }
    emu_flash = flash_alloc();
    if (!emu_flash) {
        MessageBoxA(NULL, "No se pudo reservar la memoria de la flash emulada.", "SLOOP", MB_ICONERROR);
        return 1;
    }
    flash_load();

    font_s = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    font_b = CreateFontA(15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    layout();
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "SLOOP_FM1_EMU";
    RegisterClassExA(&wc);
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    midi_open(midi_dev, midi_name, sizeof midi_name);
    snprintf(title, sizeof title, "SLOOP FM-1 Emulator  |  flash: %s  |  MIDI: %s", flash_path,
             midi_name[0] ? midi_name : "-");
    wnd = CreateWindowExA(0, wc.lpszClassName, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, inst, NULL);
    if (!wnd)
        return 1;
    ShowWindow(wnd, show);
    UpdateWindow(wnd);
    SetTimer(wnd, 1, 16, NULL);

    {
        HANDLE t = CreateThread(NULL, 8u << 20, fw_thread, NULL, CREATE_SUSPENDED, &fw_tid);
        ResumeThread(t);
        CloseHandle(t);
    }

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    wav_close();
    timeEndPeriod(1);
    ExitProcess(0);
}
