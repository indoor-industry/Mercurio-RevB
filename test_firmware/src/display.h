#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "mcp23017.h"

extern TFT_eSPI tft;

namespace Display {
    // Pulses MCP23017 GPA2 (display reset), enables backlight, and
    // initializes TFT_eSPI. Must be called after mcp.begin().
    void begin(MCP23017 &mcp);

    // Fills the header bar (y=0..HDR_H) with a title.
    void drawHeader(const char *title);

    // Clears the area below the header.
    void clearBody();

    int bodyTop();
    int bodyHeight();

    // Sets the backlight brightness via PWM (5-100%, clamped).
    void setBacklight(uint8_t percent);

    // Returns the brightness last applied via setBacklight() (or the
    // value loaded from NVS at startup).
    uint8_t backlight();

    // Draws a dimmed "label" followed by "value" on the same line if it
    // fits, or wraps the value to an indented line below if it doesn't.
    // Caller must set the desired font before calling. Returns the y
    // position for the next line.
    int infoLine(int y, const char *label, const String &value, uint16_t valueColor);
}
