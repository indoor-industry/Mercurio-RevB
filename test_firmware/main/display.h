#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Bring up SPI-B (display + SD share this bus) and add the ILI9341 device. */
esp_err_t display_bus_init(void);

/* Reset and configure the ILI9341 panel controller. */
void ili9341_init(void);

/* Solid-fill a rectangle. */
void disp_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c);

/* Render a text string (8x8 font, scaled by `sc`) as one DMA transfer. */
void disp_text(uint16_t x, uint16_t y, const char *s,
                uint16_t fg, uint16_t bg, uint8_t sc);

/* =========================================================================
 * Detail-view layout helpers, shared by every per-test detail screen.
 * ========================================================================= */

/* Printf-style row draw in the detail content area (row index 0..N). */
void drow(int row, uint16_t fg, uint16_t bg, const char *fmt, ...);

/* Horizontal progress bar (fraction 0.0-1.0). */
void draw_bar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               float frac, uint16_t fill, uint16_t empty);

/* Full-width touch button spanning `rows_h` detail rows. */
void detail_btn(int row, int rows_h, const char *label, uint16_t bg);

/* Detail view header with back arrow. */
void detail_header(const char *title);
