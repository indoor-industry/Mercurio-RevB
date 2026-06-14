/*
 * Touch Controller Driver (XPT2046)
 */

#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

void touch_init(spi_device_handle_t spi_handle);
bool touch_read(uint16_t *x, uint16_t *y);

#endif // TOUCH_H
