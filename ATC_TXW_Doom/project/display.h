#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include "typesdef.h"

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320

/*
 * RGB565-Framebuffer (PSRAM, 150 KB) — für DOOM-Rendering.
 * Format: 16-bit RGB565, zeilenweise, 320 Pixel pro Zeile.
 *
 * Hilfsmakro: RGB565(r,g,b) → uint16_t  (r,g,b: 0–255)
 */
#define RGB565(r, g, b) \
    ((uint16_t)(((r) & 0xF8) << 8) | (uint16_t)(((g) & 0xFC) << 3) | (uint16_t)(((b) & 0xF8) >> 3))

/* Zeiger auf den RGB565-Framebuffer (für späteres DOOM-Rendering) */
extern uint16_t *display_fb;

/* Muss in hardware_init() nach lcd_module_run() aufgerufen werden */
void display_init(void);

/*
 * Erstellt den Display-Task (ersetzt den originalen gui_thread).
 * Rufe diese Funktion NACH app_network_init() auf, damit alle
 * Streams und Protokolle bereit sind, bevor die Kamera gestartet wird.
 *
 * Der Task:
 *  1. Initialisiert lcd_sema (lcd_sema_init)
 *  2. Startet das Kamerabild (display_show_camera)
 *  3. Läuft als Gameloop (~30 fps) — späterer DOOM-Einstiegspunkt
 */
void display_start(void);

/* Direkt: Kamerabild starten — nur wenn display_start() nicht verwendet wird */
void display_show_camera(void);

#endif /* _DISPLAY_H_ */
