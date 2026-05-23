/*
 * lcd.c — DOOM framebuffer → TXW81x LCD via LCDC OSD layer
 *
 * DOOM renders at 120×160 (portrait, 8bpp palette-indexed, stored as
 * uint16_t with the palette index in both bytes).
 * The LCD is 320×240 (landscape, RGB565).
 *
 * lcd_render_fb() rotates 90° clockwise and scales 2× to fill the
 * display exactly: 120×160 → rotate → 160×120 → ×2 → 320×240.
 */

#include <stdint.h>
#include <string.h>
#include "doomdef.h"        /* SCREENWIDTH, SCREENHEIGHT */
#include "lcd.h"

#include "sys_config.h"
#include "typesdef.h"
#include "dev.h"
#include "devid.h"
#include "hal/lcdc.h"
#include "dev/lcdc/hglcdc.h"     /* OSD_EN_IRQ */
#include "osal/semaphore.h"      /* os_semaphore, os_sema_* */
#include "chip/txw81x/misc.h"   /* sys_dcache_clean_range */
#include "osal/sleep.h"          /* os_sleep_ms */
#if DVP_EN
#include "hal/vpp.h"             /* vpp_open, vpp_set_ifp_en */
#include "lib/lcd/lcd.h"         /* SCALE_WIDTH, SCALE_HIGH, SCALE_CONFIG_W */
#endif

/* display_fb: RGB565 source buffer in PSRAM, defined in display.c */
extern uint16_t *display_fb;
#define LCD_W 240
#define LCD_H 320

/* Pre-converted RGB565 palette */
static uint16_t lcdpal[256];

/* Double-buffered OSD encoder output: while [cur] is being DMA'd, encode into [cur^1] */
static uint8_t enc_buf[2][200*1024]
    __attribute__((section(".psram.data"), aligned(16)));
static int cur_enc = 0;
static int osd_enc_initialized = 0;

/* Semaphore signalled by OSD_EN_IRQ when encoder finishes a frame */
static struct os_semaphore enc_done_sema;

/* OSD_EN_IRQ ISR — fires when the OSD encoder has finished.          */
static void doom_osd_enc_isr(uint32 irq_flag, uint32 irq_data, uint32 param)
{
    os_sema_up(&enc_done_sema);
}

/* ------------------------------------------------------------------ */
/* Camera PiP — bottom-centre overlay                                 */
/* Camera: SCALE_WIDTH×SCALE_HIGH YUV420 planar in video_psram_mem.  */
/* PiP target: 25×50 pixels, bottom-centre of the 240×320 display.   */
/* ------------------------------------------------------------------ */
#if DVP_EN
#define PIP_W  32
#define PIP_H  64
#define PIP_X  ((LCD_W - PIP_W) / 2)       /* 107 */
#define PIP_Y  (LCD_H - PIP_H - 6)          /* 264 */

extern uint8 *video_psram_mem;

/*
 * Camera is 320×240 (landscape).  A 90° CW rotation maps:
 *   pip column (px) ← camera row  axis  (cam_y, reversed)
 *   pip row    (py) ← camera col  axis  (cam_x)
 * So each PiP column samples one camera column, and each PiP row
 * samples one camera row.
 */
static void cam_pip_blit(uint16_t *fb)
{
    if (!video_psram_mem) return;

    const int stride = SCALE_CONFIG_W;              /* 320 */

    const uint8_t *Y = (const uint8_t *)video_psram_mem;
    const uint8_t *U = Y + stride * SCALE_HIGH;
    const uint8_t *V = U + (stride >> 1) * (SCALE_HIGH >> 1);

    for (int py = 0; py < PIP_H; py++) {
        /* 90° CW: pip row py → camera column cam_x */
        int cam_x = py * SCALE_WIDTH / PIP_H;

        uint16_t *dst = fb + (PIP_Y + py) * LCD_W + PIP_X;

        for (int px = 0; px < PIP_W; px++) {
            /* 90° CW: pip column px → camera row cam_y (high px = low cam_y) */
            int cam_y = (SCALE_HIGH - 1) - px * SCALE_HIGH / PIP_W;

            int y_v = Y[cam_y * stride + cam_x];
            int u_v = U[(cam_y >> 1) * (stride >> 1) + (cam_x >> 1)] - 128;
            int v_v = V[(cam_y >> 1) * (stride >> 1) + (cam_x >> 1)] - 128;

            int r = y_v + v_v + (v_v >> 2) + (v_v >> 3);
            int g = y_v - ((u_v >> 2) + (u_v >> 4))
                        - ((v_v >> 1) + (v_v >> 3) + (v_v >> 4));
            int b = y_v + u_v + (u_v >> 1) + (u_v >> 3);

            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;

            dst[px] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
    }
}
#endif

/* ------------------------------------------------------------------ */
/* RGB565 helper (with same byte-swap as lcdpal)                      */
/* ------------------------------------------------------------------ */
static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* ------------------------------------------------------------------ */
/* 5×7 pixel font — printable ASCII 0x20..0x7E, 1 bit per pixel       */
/* ------------------------------------------------------------------ */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* '\'' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x40,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
};

/* Draw a single char at pixel position (px, py), scale S, fg/bg color */
#define FONT_SCALE 2
static void draw_char(int px, int py, char c, uint16_t fg, uint16_t bg)
{
    if (c < 0x20 || c > 0x7A) c = '?';
    const uint8_t *glyph = font5x7[c - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            uint16_t color = (glyph[col] >> row) & 1 ? fg : bg;
            for (int sy = 0; sy < FONT_SCALE; sy++) {
                for (int sx = 0; sx < FONT_SCALE; sx++) {
                    int x = px + col * FONT_SCALE + sx;
                    int y = py + row * FONT_SCALE + sy;
                    if (x >= 0 && x < LCD_W && y >= 0 && y < LCD_H)
                        display_fb[y * LCD_W + x] = color;
                }
            }
        }
    }
}

static void draw_string(int px, int py, const char *s, uint16_t fg, uint16_t bg)
{
    for (; *s; s++, px += (5 + 1) * FONT_SCALE)
        draw_char(px, py, *s, fg, bg);
}

/* ------------------------------------------------------------------ */
/* lcd_show_bootscreen                                                 */
/* Called from display_init() — shows text before DOOM starts.        */
/* Uses the OSD encoder (same path as lcd_render_fb) so the format    */
/* is correct and the DMA swap is latched at vsync → no tearing.      */
/* ------------------------------------------------------------------ */
void lcd_show_bootscreen(void)
{
    if (!display_fb) return;

    struct lcdc_device *lcd_dev =
        (struct lcdc_device *)dev_get(HG_LCDC_DEVID);
    if (!lcd_dev) return;

    /* Black background */
    memset(display_fb, 0, LCD_W * LCD_H * sizeof(uint16_t));

    uint16_t yellow = rgb(255, 220,   0);
    uint16_t black  = rgb(  0,   0,   0);

    /* "ATC14441" centered: 8 chars × 12 px/char = 96 → x = (240-96)/2 = 72 */
    draw_string( 72, 110, "ATC14441",       yellow, black);
    /* "Loading Doom...": 15 chars × 12 = 180 → x = (240-180)/2 = 30 */
    draw_string( 30, 145, "Loading Doom...", yellow, black);
    /* "Please wait": 11 chars × 12 = 132 → x = (240-132)/2 = 54 */
    draw_string( 54, 180, "Please wait",    yellow, black);

    sys_dcache_clean_range((uint32_t *)display_fb, LCD_W * LCD_H * 2);

    /* Init encoder semaphore and ISR once — replaces SDK's lcd_osd_isr.
     * OSD_EN_IRQ fires when lcdc_osd_enc_start_run() finishes encoding. */
    if (!osd_enc_initialized) {
        osd_enc_initialized = 1;
        os_sema_init(&enc_done_sema, 0);
        lcdc_request_irq(lcd_dev, OSD_EN_IRQ, (lcdc_irq_hdl)doom_osd_enc_isr,
                         (uint32)(uintptr_t)lcd_dev);
    }

    /* Configure OSD and enable so the encoder has an active OSD context */
    lcdc_set_osd_format(lcd_dev, OSD_RGB_565);
    lcdc_set_osd_size(lcd_dev, LCD_W, LCD_H);
    lcdc_set_osd_start_location(lcd_dev, 0, 0);
    lcdc_set_osd_alpha(lcd_dev, 255);
    lcdc_set_osd_en(lcd_dev, 1);

    /* Encode display_fb → enc_buf[next], then latch DMA ptr at next vsync */
    int next = cur_enc ^ 1;
    os_sema_eat(&enc_done_sema);
    lcdc_set_osd_enc_src_addr(lcd_dev, (uint32_t)(uintptr_t)display_fb);
    lcdc_set_osd_enc_dst_addr(lcd_dev, (uint32_t)(uintptr_t)enc_buf[next]);
    lcdc_set_osd_enc_src_len(lcd_dev, LCD_W * LCD_H * 2);
    lcdc_osd_enc_start_run(lcd_dev, 1);
    os_sema_down(&enc_done_sema, -1);

    lcdc_set_osd_dma_addr(lcd_dev, (uint32_t)(uintptr_t)enc_buf[next]);
    lcdc_set_osd_dma_len(lcd_dev, (uint32_t)lcdc_get_osd_enc_dst_len(lcd_dev));
    cur_enc = next;
}

/* ------------------------------------------------------------------ */
/* lcd_update_progress                                                 */
/* Draws a progress bar + "XX%" below the boot screen text.           */
/* Uses the OSD encoder for the frame update — same double-buffer     */
/* swap as lcd_render_fb() → vsync-latched, no tearing.              */
/* ------------------------------------------------------------------ */
void lcd_update_progress(int percent)
{
    if (!display_fb) return;
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;

    uint16_t black  = rgb(  0,   0,   0);
    uint16_t green  = rgb( 80, 200,  80);
    uint16_t border = rgb(100, 100, 100);
    uint16_t white  = rgb(255, 255, 255);

    /* --- progress bar: x=20..219, y=205..216 (12 px tall) --- */
    int bar_x = 20, bar_y = 205, bar_w = 200, bar_h = 12;
    int fill_w = (bar_w - 2) * percent / 100;
    for (int y = bar_y; y < bar_y + bar_h; y++) {
        int by = y - bar_y;
        for (int x = bar_x; x < bar_x + bar_w; x++) {
            int bx = x - bar_x;
            uint16_t c;
            if (by == 0 || by == bar_h - 1 || bx == 0 || bx == bar_w - 1)
                c = border;
            else if (bx - 1 < fill_w)
                c = green;
            else
                c = black;
            display_fb[y * LCD_W + x] = c;
        }
    }

    /* --- percentage text at y=225 (14 px high with FONT_SCALE 2) --- */
    int txt_y = 225;
    for (int y = txt_y; y < txt_y + 14; y++)
        for (int x = 0; x < LCD_W; x++)
            display_fb[y * LCD_W + x] = black;

    char buf[5];
    int  n = 0;
    if (percent >= 100) { buf[n++]='1'; buf[n++]='0'; buf[n++]='0'; }
    else if (percent >= 10) { buf[n++]='0'+percent/10; buf[n++]='0'+percent%10; }
    else                    { buf[n++]='0'+percent; }
    buf[n++] = '%';
    buf[n]   = '\0';

    int tx = (LCD_W - n * (5 + 1) * FONT_SCALE) / 2;
    draw_string(tx, txt_y, buf, white, black);

    /* Flush entire display_fb — encoder reads the whole buffer */
    sys_dcache_clean_range((uint32_t *)display_fb, LCD_W * LCD_H * 2);

    struct lcdc_device *lcd_dev =
        (struct lcdc_device *)dev_get(HG_LCDC_DEVID);
    if (!lcd_dev) return;

    /* Encode and swap — identical to lcd_render_fb() without palette step */
    int next = cur_enc ^ 1;
    os_sema_eat(&enc_done_sema);
    lcdc_set_osd_enc_src_addr(lcd_dev, (uint32_t)(uintptr_t)display_fb);
    lcdc_set_osd_enc_dst_addr(lcd_dev, (uint32_t)(uintptr_t)enc_buf[next]);
    lcdc_set_osd_enc_src_len(lcd_dev, LCD_W * LCD_H * 2);
    lcdc_osd_enc_start_run(lcd_dev, 1);
    os_sema_down(&enc_done_sema, -1);

    lcdc_set_osd_dma_addr(lcd_dev, (uint32_t)(uintptr_t)enc_buf[next]);
    lcdc_set_osd_dma_len(lcd_dev, (uint32_t)lcdc_get_osd_enc_dst_len(lcd_dev));
    cur_enc = next;
}

/* ------------------------------------------------------------------ */
/* lcd_set_pal                                                         */
/* Convert DOOM's 768-byte RGB palette (r,g,b per entry) to RGB565.   */
/* ------------------------------------------------------------------ */
void lcd_set_pal(uint8_t *pal)
{
    for (int i = 0; i < 256; i++) {
        uint8_t r = *pal++;
        uint8_t g = *pal++;
        uint8_t b = *pal++;
        uint16_t pp = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        /* Byte-swap for LCDC big-endian DMA */
        lcdpal[i] = pp;
    }
}

/* ------------------------------------------------------------------ */
/* lcd_render_fb                                                       */
/* Convert 120×160 palette-indexed DOOM fb → 320×240 RGB565 display.  */
/* Rotation: 90° clockwise.  Scale: 2×.                               */
/* ------------------------------------------------------------------ */
void lcd_render_fb(uint8_t *fb)
{
    if (!display_fb || !fb) return;

    /*
     * Iterate over every DOOM pixel (x_src, y_src).
     * After 90° CW rotation + 2× scale, each DOOM pixel maps to a
     * 2×2 block in display_fb:
     *
     *   x_dst = (SCREENHEIGHT - 1) - y_src   → 0..159
     *   y_dst = x_src                          → 0..119
     *   LCD pixels: (x_dst*2, y_dst*2) .. (x_dst*2+1, y_dst*2+1)
     */
    /*
     * fb is stored as uint16_t (SCREENPITCH shorts per row) with the
     * palette index in both bytes.  Read via uint16_t* to get the
     * correct stride; take the low byte as the palette index.
     */
    /*
     * DOOM 120×160 (portrait) → 2× scale → 240×320 (portrait LCD).
     * No rotation needed: SCREENWIDTH=120 → LCD_W=240, SCREENHEIGHT=160 → LCD_H=320.
     */
    const uint16_t *fb16 = (const uint16_t *)fb;

    for (int y_src = 0; y_src < SCREENHEIGHT; y_src++) {
        for (int x_src = 0; x_src < SCREENWIDTH; x_src++) {
            uint8_t  idx   = (uint8_t)(fb16[y_src * SCREENPITCH + x_src]);
            uint16_t color = lcdpal[idx];

            int base = (y_src * 2) * LCD_W + (x_src * 2);
            display_fb[base]             = color;
            display_fb[base + 1]         = color;
            display_fb[base + LCD_W]     = color;
            display_fb[base + LCD_W + 1] = color;
        }
    }

#if DVP_EN
    cam_pip_blit(display_fb);
#endif

    uint16_t black = rgb(0, 0, 0);
    uint16_t yellow = rgb(255, 255, 0);
    draw_string(0, 0,        "by ATC1441",  yellow,  black);
    /* Flush CPU cache → PSRAM so the encoder DMA sees the new pixels */
    sys_dcache_clean_range((uint32_t *)display_fb, LCD_W * LCD_H * 2);

    struct lcdc_device *lcd_dev =
        (struct lcdc_device *)dev_get(HG_LCDC_DEVID);
    if (!lcd_dev) return;

    /* Encode into the inactive buffer (cur_enc^1 is not being DMA'd) */
    int next = cur_enc ^ 1;
    uint8_t *dst = enc_buf[next];

    os_sema_eat(&enc_done_sema);           /* drain any stale signal */
    lcdc_set_osd_enc_src_addr(lcd_dev, (uint32_t)(uintptr_t)display_fb);
    lcdc_set_osd_enc_dst_addr(lcd_dev, (uint32_t)(uintptr_t)dst);
    lcdc_set_osd_enc_src_len(lcd_dev, LCD_W * LCD_H * 2);
    lcdc_osd_enc_start_run(lcd_dev, 1);

    os_sema_down(&enc_done_sema, -1);      /* wait for OSD_EN_IRQ — encoder is done */

    /* Update OSD DMA pointer — hardware latches the new address at the
     * next frame start (VSYNC), so no OSD_EN disable/enable is needed
     * and there is no visible blank between frames. */
    lcdc_set_osd_dma_addr(lcd_dev, (uint32_t)(uintptr_t)dst);
    lcdc_set_osd_dma_len(lcd_dev, (uint32_t)lcdc_get_osd_enc_dst_len(lcd_dev));
    cur_enc = next;
}


/* ------------------------------------------------------------------ */
/* lcd_init                                                            */
/* Set up LCDC OSD layer to display display_fb (320×240 RGB565).      */
/* Called once from I_PreInitGraphics().                               */
/* ------------------------------------------------------------------ */
void lcd_init(uint8_t *fb)
{
    struct lcdc_device *lcd_dev =
        (struct lcdc_device *)dev_get(HG_LCDC_DEVID);
    if (!lcd_dev) {
        printf("lcd_init: no LCDC device\r\n");
        return;
    }

    /* Stop camera/video paths — DOOM owns the display */
    lcdc_set_video_en(lcd_dev, 0);
    lcdc_set_p0p1_enable(lcd_dev, 0, 0);

    /* Configure OSD layer */
    lcdc_set_osd_format(lcd_dev, OSD_RGB_565);
    lcdc_set_osd_size(lcd_dev, LCD_W, LCD_H);
    lcdc_set_osd_start_location(lcd_dev, 0, 0);
    lcdc_set_osd_dma_addr(lcd_dev, (uint32_t)(uintptr_t)display_fb);
    lcdc_set_osd_dma_len(lcd_dev, LCD_W * LCD_H * 2);
    lcdc_set_osd_alpha(lcd_dev, 255);

    /* Enable OSD */
    lcdc_set_osd_en(lcd_dev, 1);

    printf("lcd_init: OSD %dx%d RGB565 at %p\r\n", LCD_W, LCD_H, display_fb);

    /* Initialise semaphore and register the encoder-done ISR (if not already
     * done by lcd_show_bootscreen during early boot). */
    if (!osd_enc_initialized) {
        osd_enc_initialized = 1;
        os_sema_init(&enc_done_sema, 0);
        lcdc_request_irq(lcd_dev, OSD_EN_IRQ, (lcdc_irq_hdl)doom_osd_enc_isr,
                         (uint32)(uintptr_t)lcd_dev);
    }

    /* Pre-fill DOOM's fb with the boot image so the first lcd_render_fb()
     * call sends it to the display without any extra initialisation. */
    if (fb)
        memcpy(fb, imageBoot, sizeof(imageBoot));

#if DVP_EN
    /* Start DVP→VPP pipeline so video_psram_mem fills with camera frames.
     * The LCDC video layer stays disabled — we blit the data in software. */
    {
        struct vpp_device *vpp_dev =
            (struct vpp_device *)dev_get(HG_VPP_DEVID);
        if (vpp_dev) {
            vpp_set_ifp_en(vpp_dev, 0);
            vpp_open(vpp_dev);
        }
    }
#endif
}

void lcd_render_finish(void)
{
    /* Nothing needed — LCDC DMA reads display_fb automatically */
}
