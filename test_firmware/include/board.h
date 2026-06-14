#pragma once

#include "mcp23017.h"

// Shared MCP23017 instance, defined in main.cpp.
extern MCP23017 mcp;

// millis() timestamp at which the GPS module was brought out of reset
// (set once in main.cpp's setup()). Used by the GPS test screen to show a
// time-to-first-fix figure - a quick way to tell a warm/hot start (a few
// seconds, backup data retained) from a cold start (30s+, backup data lost).
extern unsigned long g_gpsBootMs;
