#include "lib/lcd/lcd.h"

#if LCD_nv3031a_MCU565_EN

#define CMD(x)    {LCD_CMD,x}
#define DAT(x)    {LCD_DAT,x}
#define DLY(x)    {DELAY_MS,x}
#define END		{LCD_TAB_END,LCD_TAB_END}

uint8_t nv3031a_register_init_tab[][2] = {
    // Software Reset — zwingt Controller in definierten Zustand (klärt Half-Reset durch entladene Kondensatoren)
    CMD(0x01),
    DLY(120),
    // ─── Vendor-spezifische Init (NV3031A-Register) ───
    CMD(0xFD), DAT(0x06), DAT(0x08), // Password/Enable
    CMD(0x61), DAT(0x07), DAT(0x07),
    CMD(0x73), DAT(0x70),          // Power
    CMD(0x73), DAT(0x00),
    CMD(0x61), DAT(0x07), DAT(0x07),
    CMD(0x62), DAT(0x00), DAT(0x44), DAT(0x40),
    CMD(0x65), DAT(0x08), DAT(0x10), DAT(0x21),
    CMD(0x66), DAT(0x08), DAT(0x10), DAT(0x21),
    CMD(0x67), DAT(0x21), DAT(0x40),
    CMD(0x68), DAT(0x70), DAT(0x37), DAT(0x2A), DAT(0x16),

    // ─── Timing / Interface ───
    CMD(0xB1), DAT(0x0F), DAT(0x02), DAT(0x00), // RGBCTRL
    CMD(0xB4), DAT(0x01),
    CMD(0xB5), DAT(0x02), DAT(0x02), DAT(0x0A), DAT(0x14), // Porch
    CMD(0xB6), DAT(0x44), DAT(0x01), DAT(0x9F), DAT(0x00), DAT(0x02), // Display Func

    // ─── Gamma ───
    CMD(0xE6), DAT(0x00), DAT(0xFF),
    CMD(0xE7), DAT(0x01), DAT(0x04), DAT(0x03), DAT(0x03), DAT(0x00), DAT(0x12),
    CMD(0xE8), DAT(0x00), DAT(0x70), DAT(0x00), // Gate timing
    CMD(0xEC), DAT(0x52),
    CMD(0xDF), DAT(0x11),
    CMD(0xE0), DAT(0x07), DAT(0x07), DAT(0x0A), DAT(0x13), DAT(0x0E), DAT(0x11), DAT(0x11), DAT(0x19), // PVGAMCTRL
    CMD(0xE3), DAT(0x12), DAT(0x0F), DAT(0x11), DAT(0x0E), DAT(0x10), DAT(0x09), DAT(0x05), DAT(0x06),
    CMD(0xE1), DAT(0x2A), DAT(0x71), // NVGAMCTRL
    CMD(0xE4), DAT(0x6B), DAT(0x2A),
    CMD(0xE2), DAT(0x00), DAT(0x13), DAT(0x17), DAT(0x31), DAT(0x36), DAT(0x3F),
    CMD(0xE5), DAT(0x3F), DAT(0x36), DAT(0x32), DAT(0x17), DAT(0x13), DAT(0x00),

    // ─── Panel / Interface Config ───
    CMD(0xF6), DAT(0x01), DAT(0x30), DAT(0x00), DAT(0x00),
    CMD(0xF1), DAT(0x01), DAT(0x21), DAT(0x22),
    CMD(0xFD), DAT(0xFA), DAT(0xFC), // Relock

    // ─── Standard MIPI ───
    CMD(0x3A), DAT(0x55), // COLMOD: RGB565
    CMD(0x35), DAT(0x00), // TE on
    CMD(0x36), DAT(0x00), // MADCTL
    CMD(0x11),            // SLPOUT
    DLY(15),
    CMD(0x29),            // DISPON
    DLY(5),
    CMD(0x2C),            // RAMWR
    END
};

lcddev_t  lcdstruct = {
    .name = "NV3031A",
    .lcd_bus_type = LCD_BUS_I80,
    .bus_width = LCD_BUS_WIDTH_8,
    .color_mode = LCD_MODE_565,
	.osd_scan_mode = LCD_ROTATE_90,
    .scan_mode = LCD_ROTATE_90,//rotate 90
    .te_mode = 0xff,//te mode, 0xff:disable
    .colrarray = 0,//0:_RGB_ 1:_RBG_,2:_GBR_,3:_GRB_,4:_BRG_,5:_BGR_
    .pclk = 10000000,
    .even_order = 0,
    .odd_order = 0,
    .screen_w = 240,
    .screen_h = 320,
    .video_x  = 0,
    .video_y  = 0,
    .video_w  = 240,
    .video_h  = 320,
	.osd_x = 0,
	.osd_y = 0,
	.osd_w = 240, // 0 : value will set to video_w  , use for 4:3 LCD +16:9 sensor show UPDOWN BLACK
	.osd_h = 320, // 0 : value will set to video_h  , use for 4:3 LCD +16:9 sensor show UPDOWN BLACK
	.init_table = nv3031a_register_init_tab,

    .pclk_inv = 0,
    
    .vlw = 0,// 0,
    .vbp = 0,// 12,
    .vfp = 0,// 12,
    .hlw = 0,// 1,
    .hbp = 0,//14,
    .hfp = 0,//13,

	.de_en  = 0,
	.vs_en	= 0,
	.hs_en	= 0,
    .de_inv = 0,
    .hs_inv = 0,
    .vs_inv = 0,



	.signal_config.mcu.resev = 0,
	.signal_config.mcu.wr_inv = 1,
	.signal_config.mcu.rd_inv = 0,
	.signal_config.mcu.cs_inv = 0,
	.signal_config.mcu.dc_inv = 1,

};
#endif
