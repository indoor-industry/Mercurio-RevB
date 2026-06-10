#pragma once

#include "shared_types.h"

/* Bring up WiFi STA mode (once) and run a scan, then briefly enable and
 * disable the BLE controller. Results are reported in g_live. */
void test_wifi_bt(test_entry_t *t);
