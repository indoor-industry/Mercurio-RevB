#pragma once

#include "shared_types.h"

/* Power up the L76KB module via the MCP23017, listen for NMEA sentences
 * on UART1 for a few seconds, and report fix status in g_live.gps. */
void test_gps(test_entry_t *t);
