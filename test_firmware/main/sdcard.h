#pragma once

#include "shared_types.h"

/* Mount the SD card on SPI-B (shares the bus with the display), write a
 * test file, and report capacity/type in g_live. */
void test_sd_card(test_entry_t *t);
