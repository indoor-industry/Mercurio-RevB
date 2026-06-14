/*
 * Board Configuration - Pin Definitions and Constants
 * ESP32-S3 Custom Board
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// ============== PIN DEFINITIONS ==============

// LED
#define PIN_LED             GPIO_NUM_1

// Buzzer
#define PIN_BUZZER          GPIO_NUM_21

// ILI9341 Display (SPI Bus B)
#define PIN_LCD_CS          GPIO_NUM_10
#define PIN_LCD_RST         GPIO_NUM_17
#define PIN_LCD_DC          GPIO_NUM_5
#define PIN_LCD_MOSI        GPIO_NUM_13
#define PIN_LCD_MISO        GPIO_NUM_12
#define PIN_LCD_SCK         GPIO_NUM_14
#define PIN_LCD_BL          GPIO_NUM_9

// Touch Controller (SPI Bus B - shared)
#define PIN_TOUCH_CS        GPIO_NUM_18
#define PIN_TOUCH_IRQ       GPIO_NUM_6

// Battery ADC
#define PIN_BATTERY         GPIO_NUM_11
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0

// GPS Module (L70-R) - UART1
#define PIN_GPS_RX          GPIO_NUM_42
#define PIN_GPS_TX          GPIO_NUM_40
#define PIN_GPS_STANDBY     GPIO_NUM_8
#define PIN_GPS_RESET       GPIO_NUM_4

// LoRa Module - SX1268/SX1276 - SPI Bus A
#define PIN_LORA_MISO       GPIO_NUM_41
#define PIN_LORA_MOSI       GPIO_NUM_39
#define PIN_LORA_SCK        GPIO_NUM_38
#define PIN_LORA_CS         GPIO_NUM_34
#define PIN_LORA_RST        GPIO_NUM_37
#define PIN_LORA_DIO0       GPIO_NUM_2
#define PIN_LORA_DIO1       GPIO_NUM_36

// SD Card (SPI Bus A - shared)
#define PIN_SD_CS           GPIO_NUM_35

// Secure Element (ATECC608A) - I2C
#define PIN_SE_SDA          GPIO_NUM_47
#define PIN_SE_SCL          GPIO_NUM_48
#define ATECC608A_I2C_ADDR  0x60

// ============== DISPLAY CONSTANTS ==============
#define LCD_WIDTH           320
#define LCD_HEIGHT          240

// Colors (RGB565)
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_BACKGROUND    0x000A

#endif // BOARD_CONFIG_H
