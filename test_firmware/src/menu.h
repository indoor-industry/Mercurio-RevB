#pragma once

#include <Arduino.h>

#define MAX_TESTS 16

typedef void (*TestRunFn)();

namespace Menu {
    // Registers a test. `run` must draw its own full-screen UI and call
    // Menu::waitForBack() before returning to the menu.
    void registerTest(const char *name, TestRunFn run);

    // Draws the initial menu screen. Call once after all tests are
    // registered and the touch panel is calibrated.
    void begin();

    // Polls the touch panel and dispatches to the selected test.
    // Call repeatedly from loop().
    void loop();

    // Draws a "Back" button at the bottom of the screen and blocks
    // until it is tapped. Test screens should call this before returning.
    void waitForBack();

    // Draws the "Back" button without blocking. Pair with checkBack() in
    // a test's own update loop (e.g. for live-updating screens).
    void drawBackButton();

    // Returns true once if the "Back" button has been tapped since the
    // last call. Non-blocking - call repeatedly from a polling loop.
    bool checkBack();

    // Returns true if (x,y) falls within the drawn Back button's bounds.
    // For screens with their own extra touch targets: call
    // Touch::getTouch() once per loop, then test the result against this
    // and your own button rects.
    bool backButtonHit(int16_t x, int16_t y);
}
