#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "shared_types.h"

/* 3-point affine touch calibration: screen = A * raw + offset */
typedef struct {
    float ax, bx, cx;   /* screen_x = ax*raw_x + bx*raw_y + cx */
    float ay, by, cy;   /* screen_y = ay*raw_x + by*raw_y + cy */
    bool  valid;
} touch_cal_t;

typedef struct { int16_t x, y, z; uint16_t raw_x, raw_y; bool pressed; } touch_pt_t;

/* Active calibration, loaded from NVS at startup (or left at the
 * compiled-in default). Used by touch_read() to map raw ADC counts to
 * screen coordinates. */
extern touch_cal_t g_cal;

/* Add the XPT2046 device to SPI-A (must already be initialized, e.g. by
 * lora_bus_init()). */
esp_err_t touch_dev_init(void);

touch_pt_t touch_read(void);
void touch_wait_release(void);

void cal_load(void);
void cal_save(void);

void test_touch_hw(test_entry_t *t);
