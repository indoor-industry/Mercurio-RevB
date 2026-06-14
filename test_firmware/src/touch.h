#pragma once

#include <Arduino.h>

namespace Touch {
    // Initializes the XPT2046 on the global SPI bus (SPI.begin() must
    // already have been called with the correct pins).
    void begin();

    // Raw 12-bit ADC reading from the touch controller.
    // Returns false if the panel is not currently pressed.
    bool readRaw(int16_t &x, int16_t &y, int16_t &z);

    // Stores a 3-point affine calibration:
    //   screenX = xa*rawX + xb*rawY + xc
    //   screenY = ya*rawX + yb*rawY + yc
    // This (unlike a simple 2-point scale/offset) also corrects for
    // axis swap/inversion between the touch controller and the display.
    void setCalibration(float xa, float xb, float xc, float ya, float yb, float yc);
    bool isCalibrated();

    // Returns true (with screen coordinates) on a new touch-down edge.
    // Debounced internally - only fires once per press.
    bool getTouch(int16_t &screenX, int16_t &screenY);

    // Returns the current screen coordinates while the panel is pressed,
    // with no debounce - suitable for continuous tracking (e.g. drawing).
    bool getPosition(int16_t &screenX, int16_t &screenY);
}
