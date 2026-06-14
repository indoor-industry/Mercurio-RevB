#include "touch.h"
#include "pins.h"
#include <XPT2046_Touchscreen.h>

static XPT2046_Touchscreen ts(PIN_TOUCH_CS);

static bool s_calibrated = false;
static float s_xa = 1, s_xb = 0, s_xc = 0;
static float s_ya = 0, s_yb = 1, s_yc = 0;
static bool s_prevTouched = false;

void Touch::begin() {
    ts.begin();
}

bool Touch::readRaw(int16_t &x, int16_t &y, int16_t &z) {
    // No dedicated IRQ GPIO is wired (touch IRQ lives on the MCP23017),
    // so poll the controller's pressure reading directly.
    if (!ts.touched()) {
        return false;
    }
    TS_Point p = ts.getPoint();
    x = p.x;
    y = p.y;
    z = p.z;
    return true;
}

void Touch::setCalibration(float xa, float xb, float xc, float ya, float yb, float yc) {
    s_xa = xa;
    s_xb = xb;
    s_xc = xc;
    s_ya = ya;
    s_yb = yb;
    s_yc = yc;
    s_calibrated = true;
}

bool Touch::isCalibrated() {
    return s_calibrated;
}

bool Touch::getTouch(int16_t &screenX, int16_t &screenY) {
    int16_t rx, ry, rz;
    bool t = readRaw(rx, ry, rz);

    bool isNewTouch = (t && !s_prevTouched && s_calibrated);
    s_prevTouched = t;

    if (!isNewTouch) {
        return false;
    }

    int sx = (int)(s_xa * rx + s_xb * ry + s_xc);
    int sy = (int)(s_ya * rx + s_yb * ry + s_yc);
    screenX = (int16_t)constrain(sx, 0, SCREEN_W - 1);
    screenY = (int16_t)constrain(sy, 0, SCREEN_H - 1);
    return true;
}

bool Touch::getPosition(int16_t &screenX, int16_t &screenY) {
    int16_t rx, ry, rz;
    if (!readRaw(rx, ry, rz) || !s_calibrated) {
        return false;
    }

    int sx = (int)(s_xa * rx + s_xb * ry + s_xc);
    int sy = (int)(s_ya * rx + s_yb * ry + s_yc);
    screenX = (int16_t)constrain(sx, 0, SCREEN_W - 1);
    screenY = (int16_t)constrain(sy, 0, SCREEN_H - 1);
    return true;
}
