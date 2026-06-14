/*
 * ILI9341 LCD Display Driver
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

// Initialize LCD SPI device (call after spi_bus_init)
void lcd_init(spi_device_handle_t spi_handle);

// Drawing functions
void lcd_fill_screen(uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg);
void lcd_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

// Set LCD backlight brightness (0-255)
void lcd_set_backlight(uint8_t brightness);

#endif // LCD_H
