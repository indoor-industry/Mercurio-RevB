#pragma once

// =====================================================================
// LoRa-device RevB ("Module") - GPIO pin map
// Verified directly against LoRa-device.net (KiCad netlist, 2026-05-13)
// =====================================================================

// ---------------------------------------------------------------------
// SPI-A (manual SPIClass(HSPI) = SPI3_HOST)
//   Shared by: LoRa SX1262 (RA-01SH-P), XPT2046 resistive touch
// ---------------------------------------------------------------------
#define PIN_SPIA_SCLK   38
#define PIN_SPIA_MOSI   39
#define PIN_SPIA_MISO   41

#define PIN_LORA_CS     2
#define PIN_LORA_RST    17
#define PIN_LORA_BUSY   7
#define PIN_LORA_DIO1   5
#define PIN_LORA_RFEN   16  // drive HIGH before radio.begin()

#define PIN_TOUCH_CS    6
// Touch IRQ is NOT a raw GPIO - it's MCP23017 GPB4 (active LOW when touched)

// ---------------------------------------------------------------------
// SPI-B (TFT_eSPI default SPIClass(VSPI) = SPI2_HOST)
//   Shared by: ILI9341 display, microSD
// ---------------------------------------------------------------------
#define PIN_SPIB_SCLK   14
#define PIN_SPIB_MOSI   13
#define PIN_SPIB_MISO   12

#define PIN_TFT_CS      18
#define PIN_TFT_DC      15
#define PIN_TFT_BL      11  // backlight, active HIGH
// TFT reset is NOT a raw GPIO - it's MCP23017 GPA2 (pulse before tft.init())

#define PIN_SD_CS       42
// SD card-detect is NOT a raw GPIO - it's MCP23017 GPB6

// ---------------------------------------------------------------------
// I2C0 (Wire) - MCP23017 I/O expander @ 0x20
// ---------------------------------------------------------------------
#define PIN_I2C_SDA     47
#define PIN_I2C_SCL     48
#define PIN_MCP_RST     40  // raw GPIO, active LOW
#define PIN_MCP_INTA    10  // raw GPIO, mirrored INTA/INTB from MCP

// ---------------------------------------------------------------------
// I2S0 (full duplex)
//   DOUT -> MAX98357A amplifier, DIN <- ICS-43432 microphone (left ch)
// ---------------------------------------------------------------------
#define PIN_I2S_BCLK    4
#define PIN_I2S_WS      1
#define PIN_I2S_DOUT    21
#define PIN_I2S_DIN     8

// ---------------------------------------------------------------------
// UART (GPS, Serial1 @ 9600) - Quectel L76KB-A58
// ---------------------------------------------------------------------
#define PIN_GPS_TX      43  // ESP32 TX -> GPS RX
#define PIN_GPS_RX      44  // ESP32 RX <- GPS TX
// GPS_RST = MCP GPA0, GPS_WKUP = MCP GPA3, GPS_1PPS = MCP GPA4

// ---------------------------------------------------------------------
// Battery sense
//   ADC1 CH8 = GPIO9, Vbat = 2 * Vadc (47k/47k divider)
// ---------------------------------------------------------------------
#define PIN_BAT_ADC     9
#define BAT_ADC_DIVIDER 2.0f

// ---------------------------------------------------------------------
// BOOT button (usable as 3rd user button after boot)
// ---------------------------------------------------------------------
#define PIN_BOOT_BTN    0

// =====================================================================
// MCP23017 (I2C addr 0x20) bit map
//   GPA = port A bits 0-7, GPB = port B bits 8-15 (combined 16-bit read)
// =====================================================================
#define MCP_I2C_ADDR    0x20

// Port A
#define MCP_BIT_GPS_RST     0  // out
#define MCP_BIT_BAT_STAT1   1  // in, pull-up: HIGH=normal, LOW=fault
#define MCP_BIT_DISP_RST    2  // out: pulse low->high before tft.init()
#define MCP_BIT_GPS_WKUP    3  // out
#define MCP_BIT_GPS_1PPS    4  // in
#define MCP_BIT_AUDIO_EN    5  // out: MAX98357A SD/gain enable
#define MCP_BIT_SW1         6  // in, pull-up: active LOW button
#define MCP_BIT_GPA7        7  // unused (output-only errata)

// Port B (bit index = 8 + GPBn)
#define MCP_BIT_SW2         8   // in, pull-up: active LOW button
#define MCP_BIT_LORA_DIO2   9   // in, diagnostic only
#define MCP_BIT_LORA_DIO3   10  // in, diagnostic only
#define MCP_BIT_GPB3        11  // unused
#define MCP_BIT_TOUCH_IRQ   12  // in, pull-up: active LOW when touched
#define MCP_BIT_BAT_STAT2   13  // in, pull-up: HIGH=charge done, LOW=charging
#define MCP_BIT_SD_DET      14  // in, pull-up: polarity TBD empirically
#define MCP_BIT_GPB7        15  // unused (output-only errata)

// MCP23017 register addresses (BANK=0)
#define MCP_REG_IODIRA  0x00
#define MCP_REG_IODIRB  0x01
#define MCP_REG_GPPUA   0x0C
#define MCP_REG_GPPUB   0x0D
#define MCP_REG_GPIOA   0x12
#define MCP_REG_GPIOB   0x13
#define MCP_REG_OLATA   0x14
#define MCP_REG_OLATB   0x15
#define MCP_REG_IOCON   0x0A

// =====================================================================
// Display geometry (ILI9341, 240x320 portrait)
// =====================================================================
#define SCREEN_W        240
#define SCREEN_H        320
