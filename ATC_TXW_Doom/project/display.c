#include "sys_config.h"
#include "typesdef.h"
#include "dev.h"
#include "devid.h"
#include "buttons.h"
#include "hal/lcdc.h"
#include "lib/lcd/lcd.h"
#include "osal/string.h"
#include "osal/sleep.h"
#include "osal/task.h"
#include "display.h"

#include "doom/doomdef.h"
#include "doom/d_main.h"
#include "doom/m_fixed.h"
#include "doom/i_system.h"
#include "doom/i_video.h"
#include "doom/z_zone.h"
#include "doom/lprintf.h"
#include "doom/m_random.h"
#include "doom/doomstat.h"
#include "doom/g_game.h"
#include "doom/m_misc.h"
#include "doom/i_sound.h"
#include "doom/i_main.h"
#include "doom/lprintf.h"
#include "doom/global_data.h"
#include "doom/input.h"
#if DVP_EN
#include "dev/vpp/hgvpp.h"
#endif

#include "custom_mem/custom_mem.h"

#include "doom/doom_iwad_gz.h"
#include "zlib/uzlib.h"

/* lcd_info ist in sdk/lib/lcd/lcd.c definiert, in sdk/include/lib/lcd/lcd.h nicht als extern deklariert */
extern lcd_msg lcd_info;

/*
 * lcd_sema_init/lcd_sema_up werden von interface_mgnt.c (gui_thread/lvgl_run) bereitgestellt.
 * Ohne gui_thread müssen wir sie selbst aufrufen, damit der OSD-Interrupt-Handler
 * (lcd_osd_isr → lcd_sema_up) keine uninitialisierte Semaphore beschreibt.
 */
extern void lcd_sema_init(void);

/* 320×240 RGB565-Puffer im PSRAM (150 KB) — für späteres DOOM-Rendering */
static uint16_t fb[DISPLAY_WIDTH * DISPLAY_HEIGHT]
    __attribute__((aligned(16), section(".psram.src")));

uint16_t *display_fb = fb;

extern void lcd_show_bootscreen(void);
extern void lcd_update_progress(int percent);

void display_init(void)
{
    /*
     * lcd_module_run() ends with lcdc_set_osd_en(1), immediately triggering
     * lcd_osd_isr → lcd_sema_up().  That ISR fires long before display_task
     * starts, so the semaphore MUST be initialised here — not in the task.
     */
    lcd_sema_init();
    lcd_show_bootscreen();
}

void display_show_camera(void)
{
#if DVP_EN
    struct lcdc_device *lcd_dev = (struct lcdc_device *)dev_get(HG_LCDC_DEVID);
    struct vpp_device  *vpp_dev = (struct vpp_device  *)dev_get(HG_VPP_DEVID);

    lcdc_set_video_en(lcd_dev, 0);   /* Video aus                                  */
    lcdc_set_osd_en(lcd_dev, 0);     /* OSD aus — löscht alten LVGL-Frame im PSRAM */
    vpp_set_ifp_en(vpp_dev, 0);      /* IFP (Bildfilter) deaktivieren              */
    vpp_open(vpp_dev);               /* DVP → VPP → Scale → P0-Puffer (YUV420)    */

    /* P0 direkt setzen + ISR-Zustand synchronisieren */
    lcd_info.lcd_p0p1_state = 1;
    lcd_info.lcd_p0p1_cur   = 1;
    lcdc_set_p0p1_enable(lcd_dev, 1, 0);  /* P0=an, P1=aus                         */

    os_sleep_ms(200);                /* VPP Zeit geben, P0-Puffer zu füllen        */
    lcdc_set_video_en(lcd_dev, 1);   /* Video einschalten → Kamerabild sichtbar    */
#endif
}

/* Decompressed WAD size (smaller_doom.bin uncompressed) */
#define DOOM_WAD_SIZE  1338700u

/* PSRAM buffers — both placed in .psram.data so they don't consume flash */
static uint8_t doom_wad_buf[DOOM_WAD_SIZE]
    __attribute__((aligned(4), section(".psram.data")));

/* uzlib needs a 32 KB sliding-window dict; too big for the task stack */
static uint8_t uzlib_dict[32768]
    __attribute__((aligned(4), section(".psram.data")));

const unsigned char  *doom_iwad     = NULL;
const unsigned int    doom_iwad_len = DOOM_WAD_SIZE;

static int decompress_wad(void)
{
    TINF_DATA d;
    int res, step;

    os_printf("DOOM: decompressing WAD (%u bytes gz) ...\r\n", doom_iwad_gz_len);

    uzlib_uncompress_init(&d, uzlib_dict, sizeof(uzlib_dict));
    d.source       = doom_iwad_gz;
    d.source_limit = doom_iwad_gz + doom_iwad_gz_len;

    res = uzlib_gzip_parse_header(&d);
    if (res != TINF_OK) {
        os_printf("DOOM: gzip header error %d\r\n", res);
        return -1;
    }

    d.dest_start = doom_wad_buf;
    d.dest       = doom_wad_buf;

    /* Decompress in 20 chunks so we can update the progress bar.
     * uzlib returns TINF_OK when dest_limit is hit → advance limit and continue.
     * TINF_DONE means the stream finished (possibly before the last chunk). */
#define DECOMP_STEPS 20
    for (step = 1; step <= DECOMP_STEPS; step++) {
        d.dest_limit = doom_wad_buf + (unsigned)DOOM_WAD_SIZE * step / DECOMP_STEPS;
        res = uzlib_uncompress_chksum(&d);
        lcd_update_progress((int)((unsigned)(d.dest - d.dest_start) * 100u / DOOM_WAD_SIZE));
        os_printf("DOOM: %d%%\r\n", (int)((unsigned)(d.dest - d.dest_start) * 100u / DOOM_WAD_SIZE));
        if (res != TINF_OK)
            break;
    }
    /* Edge case: buffer filled exactly → end-of-block still pending */
    if (res == TINF_OK) {
        res = uzlib_uncompress_chksum(&d);
        lcd_update_progress(100);
    }
    if (res != TINF_DONE) {
        os_printf("DOOM: decompress error %d\r\n", res);
        return -1;
    }

    if (doom_wad_buf[0] != 'I' || doom_wad_buf[1] != 'W' ||
        doom_wad_buf[2] != 'A' || doom_wad_buf[3] != 'D') {
        os_printf("DOOM: bad WAD magic %02x%02x%02x%02x\r\n",
                  doom_wad_buf[0], doom_wad_buf[1], doom_wad_buf[2], doom_wad_buf[3]);
        return -1;
    }

    doom_iwad = doom_wad_buf;
    os_printf("DOOM: WAD ready (%u bytes)\r\n", (unsigned)(d.dest - doom_wad_buf));
    return 0;
}

extern void initTheCustomIO();
static void display_task(void *arg)
{
	initTheCustomIO();
    if (1 && (decompress_wad() == 0)) {
        while (1) {
            Z_Init();
            InitGlobals();
            I_PreInitGraphics();
            D_DoomMain();
        }
    }
	os_printf("DOOM: cannot start without WAD\r\n");
	display_show_camera();

    while (1) {		
		os_printf("ADC Button : %d\r\n", buttons_raw());
        os_sleep_ms(333);
    }
}

void display_start(void)
{
    os_task_create("display_thread", display_task, NULL,
                   OS_TASK_PRIORITY_NORMAL, 0, NULL, 8192);
}
