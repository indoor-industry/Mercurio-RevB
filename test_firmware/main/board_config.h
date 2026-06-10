#pragma once

/*
 * LoRa Device RevB — board pin map, register/bitmask defines, and shared
 * UI constants. Included by every driver module and the test framework.
 *
 * SPI-A (SPI2) SCLK=38 MOSI=39 MISO=41 → LoRa SX1262 CS=2, Touch XPT2046 CS=6
 * SPI-B (SPI3) SCLK=14 MOSI=13 MISO=12 → ILI9341 CS=18 DC=15 LED=11, SD CS=42
 * I2C0 SDA=47 SCL=48 → MCP23017 0x20 (RST=GPIO40)
 *   GPA: 0=GPS_RST 1=STAT1(in) 2=DISP_RST 3=GPS_WKUP 4=GPS_1PPS(in) 5=AUD_EN 6=SW1(in)
 *   GPB: 0=SW2(in) 1=LORA_IO2(in) 2=LORA_IO3(in) 4=TOUCH_IRQ(in) 5=STAT2(in) 6=SD_DET(in)
 * I2S0: BCLK=4 LRCLK=1 DOUT=21(MAX98357A) DIN=8(ICS-43432)
 * UART1: GPS L76KB TX=43 RX=44 9600baud
 * ADC1ch8 GPIO9: battery (Vpin=Vbat/2, R=47k/47k)
 */

#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/uart.h"

/* =========================================================================
 * Hardware pins
 * ========================================================================= */
#define SPI_A_HOST   SPI2_HOST
#define PIN_LORA_SCLK 38
#define PIN_LORA_MOSI 39
#define PIN_LORA_MISO 41
#define PIN_LORA_CS   2
#define PIN_LORA_RST  17
#define PIN_LORA_BUSY 7
#define PIN_LORA_DIO1 5
#define PIN_LORA_RFEN 16
#define PIN_TOUCH_CS  6

#define SPI_B_HOST    SPI3_HOST
#define PIN_DISP_SCLK 14
#define PIN_DISP_MOSI 13
#define PIN_DISP_MISO 12
#define PIN_DISP_CS   18
#define PIN_DISP_DC   15
#define PIN_DISP_LED  11
#define PIN_SD_CS     42

#define PIN_I2C_SDA   47
#define PIN_I2C_SCL   48
#define MCP_ADDR      0x20
#define PIN_EXP_RST   40
#define PIN_EXP_INTA  10

#define PIN_I2S_BCLK  4
#define PIN_I2S_LRCLK 1
#define PIN_I2S_DOUT  21
#define PIN_I2S_DIN   8

#define GPS_UART      UART_NUM_1
#define PIN_GPS_TX    43
#define PIN_GPS_RX    44

#define BAT_ADC_UNIT  ADC_UNIT_1
#define BAT_ADC_CHAN  ADC_CHANNEL_8

#define PIN_BOOT_BTN  0

/* MCP23017 register addresses */
#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUA  0x0C
#define MCP_GPPUB  0x0D
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13
#define MCP_OLATA  0x14
#define MCP_OLATB  0x15

/* MCP23017 bit masks */
#define GPA_GPS_RST    (1u<<0)
#define GPA_STAT1      (1u<<1)
#define GPA_DISP_RST   (1u<<2)
#define GPA_GPS_WKUP   (1u<<3)
#define GPA_GPS_1PPS   (1u<<4)
#define GPA_AUDIO_EN   (1u<<5)
#define GPA_SW1        (1u<<6)

#define GPB_SW2        (1u<<0)
#define GPB_LORA_IO2   (1u<<1)
#define GPB_LORA_IO3   (1u<<2)
#define GPB_TOUCH_IRQ  (1u<<4)
#define GPB_STAT2      (1u<<5)
#define GPB_SD_DET     (1u<<6)

/* Screen */
#define SCR_W 240
#define SCR_H 320

#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))
#define C_BLACK  RGB565(  0,  0,  0)
#define C_WHITE  RGB565(255,255,255)
#define C_RED    RGB565(220, 40, 40)
#define C_GREEN  RGB565( 40,200, 40)
#define C_BLUE   RGB565( 30, 80,200)
#define C_ORANGE RGB565(255,140,  0)
#define C_YELLOW RGB565(255,220,  0)
#define C_GRAY   RGB565( 80, 80, 80)
#define C_DGRAY  RGB565( 25, 25, 25)
#define C_TEAL   RGB565(  0,155,155)
#define C_NAVY   RGB565(  0, 40,100)
#define C_VIOLET RGB565(120,  0,180)

/* UI layout */
#define HDR_H     28   /* main/detail header height */
#define DROW_H    14   /* detail content row height (scale-1 text + margin) */
#define DPAD       4   /* left text margin */
#define DCONTENT_Y HDR_H  /* content area starts here */

/* Test indices (must match g_tests order) */
#define IDX_SYSTEM  0
#define IDX_BATTERY 1
#define IDX_MCP     2
#define IDX_LORA    3
#define IDX_SD      4
#define IDX_GPS     5
#define IDX_MIC     6
#define IDX_SPEAKER 7
#define IDX_TOUCH   8
#define IDX_WIFI    9

/* Main-menu layout */
#define MAIN_ROW_H  26
#define BADGE_W     64
#define RUNALL_Y    (HDR_H + 10 * MAIN_ROW_H)
#define RUNALL_H    (SCR_H - RUNALL_Y)
