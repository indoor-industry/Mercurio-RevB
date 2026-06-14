#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------
// SPI bus frequencies
// ---------------------------------------------------------------------
#define SPI_FREQ_SD     25000000
#define SPI_FREQ_LORA   2000000
#define SPI_FREQ_TOUCH  2000000

// ---------------------------------------------------------------------
// I2C
// ---------------------------------------------------------------------
#define I2C_FREQ        400000

// ---------------------------------------------------------------------
// LoRa radio defaults (RA-01SH-P / SX1262, 868MHz EU ISM)
// ---------------------------------------------------------------------
#define LORA_FREQ_MHZ       868.0f
#define LORA_BANDWIDTH_KHZ  125.0f
#define LORA_SPREADING      9
#define LORA_CODING_RATE    7
#define LORA_SYNC_WORD      0x12
#define LORA_TX_POWER_DBM   17
#define LORA_PREAMBLE_LEN   8

// ---------------------------------------------------------------------
// GPS UART
// ---------------------------------------------------------------------
#define GPS_BAUD        9600

// ---------------------------------------------------------------------
// Preferences (NVS) namespaces / keys
// ---------------------------------------------------------------------
// 3-point affine calibration: screenX = xa*rawX + xb*rawY + xc
//                              screenY = ya*rawX + yb*rawY + yc
// (handles axis swap/inversion, not just scale/offset)
// Namespace bumped to "touchcal2" because the calibration model changed
// from 2-point scale/offset to a 3-point affine fit (different NVS keys).
#define NVS_NS_TOUCHCAL     "touchcal2"
#define NVS_KEY_CAL_VALID   "valid"
#define NVS_KEY_CAL_XA      "xa"
#define NVS_KEY_CAL_XB      "xb"
#define NVS_KEY_CAL_XC      "xc"
#define NVS_KEY_CAL_YA      "ya"
#define NVS_KEY_CAL_YB      "yb"
#define NVS_KEY_CAL_YC      "yc"

// Backlight brightness (5-100%), persisted across reboots.
#define NVS_NS_DISPLAY      "display"
#define NVS_KEY_BL_PERCENT  "bl"

// Volume level (0-10), persisted across reboots.
#define NVS_NS_VOLUME       "volume"
#define NVS_KEY_VOL_LEVEL   "level"

// ---------------------------------------------------------------------
// UI colors (RGB565)
// ---------------------------------------------------------------------
#define COL_BG          0x0000  // black
#define COL_FG          0xFFFF  // white
#define COL_HEADER_BG   0x10A2  // dark blue
#define COL_ACCENT      0x07FF  // cyan
#define COL_OK          0x07E0  // green
#define COL_WARN        0xFFE0  // yellow
#define COL_ERR         0xF800  // red
#define COL_DIM         0x7BEF  // grey
#define COL_HILITE      0x39C7  // selection highlight (dark grey-blue)

// ---------------------------------------------------------------------
// UI layout
// ---------------------------------------------------------------------
#define HDR_H           28      // header bar height
#define MENU_ROW_H      26      // menu row height
#define STATUS_BAR_H    20      // bottom status bar height
