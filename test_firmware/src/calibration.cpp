#include "calibration.h"
#include "touch.h"
#include "display.h"
#include "pins.h"
#include "config.h"
#include <Preferences.h>

namespace {
    const int TARGET_MARGIN = 24;

    void drawTarget(int x, int y, uint16_t color) {
        tft.drawFastHLine(x - 10, y, 21, color);
        tft.drawFastVLine(x, y - 10, 21, color);
        tft.drawCircle(x, y, 6, color);
    }

    // Blocks until the panel is pressed, then released. Returns the raw
    // coordinates at the moment of the press.
    void waitForRawTouch(int16_t &rx, int16_t &ry) {
        int16_t x, y, z;
        while (!Touch::readRaw(x, y, z)) {
            delay(10);
        }
        rx = x;
        ry = y;
        while (Touch::readRaw(x, y, z)) {
            delay(10);
        }
        delay(150); // debounce
    }

    // Solves s[i] = p*rx[i] + q*ry[i] + r for i=0..2 via Cramer's rule.
    void solveAffine(const float rx[3], const float ry[3], const float s[3],
                      float &p, float &q, float &r) {
        float det = rx[0] * (ry[1] - ry[2]) - ry[0] * (rx[1] - rx[2]) + (rx[1] * ry[2] - ry[1] * rx[2]);
        float detP = s[0] * (ry[1] - ry[2]) - ry[0] * (s[1] - s[2]) + (s[1] * ry[2] - ry[1] * s[2]);
        float detQ = rx[0] * (s[1] - s[2]) - s[0] * (rx[1] - rx[2]) + (rx[1] * s[2] - s[1] * rx[2]);
        float detR = rx[0] * (ry[1] * s[2] - s[1] * ry[2]) - ry[0] * (rx[1] * s[2] - s[1] * rx[2]) + s[0] * (rx[1] * ry[2] - ry[1] * rx[2]);
        p = detP / det;
        q = detQ / det;
        r = detR / det;
    }
}

bool Calibration::load() {
    Preferences prefs;
    prefs.begin(NVS_NS_TOUCHCAL, true);
    bool valid = prefs.getBool(NVS_KEY_CAL_VALID, false);
    if (valid) {
        float xa = prefs.getFloat(NVS_KEY_CAL_XA, 1.0f);
        float xb = prefs.getFloat(NVS_KEY_CAL_XB, 0.0f);
        float xc = prefs.getFloat(NVS_KEY_CAL_XC, 0.0f);
        float ya = prefs.getFloat(NVS_KEY_CAL_YA, 0.0f);
        float yb = prefs.getFloat(NVS_KEY_CAL_YB, 1.0f);
        float yc = prefs.getFloat(NVS_KEY_CAL_YC, 0.0f);
        Touch::setCalibration(xa, xb, xc, ya, yb, yc);
    }
    prefs.end();
    return valid;
}

void Calibration::run() {
    const int m = TARGET_MARGIN;
    // Non-collinear "L" of targets: top-left, top-right, bottom-left.
    // Three points let the affine fit correct for axis swap/inversion,
    // not just scale and offset.
    int sx[3] = { m, SCREEN_W - 1 - m, m };
    int sy[3] = { m, m, SCREEN_H - 1 - m };
    float rx[3], ry[3];

    tft.fillScreen(COL_BG);
    Display::drawHeader("Touch Calibration");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(COL_FG, COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Tap each target precisely", SCREEN_W / 2, HDR_H + 10);
    tft.setTextDatum(TL_DATUM);

    for (int i = 0; i < 3; i++) {
        drawTarget(sx[i], sy[i], COL_ACCENT);
        int16_t rrx, rry;
        waitForRawTouch(rrx, rry);
        rx[i] = rrx;
        ry[i] = rry;
        drawTarget(sx[i], sy[i], COL_DIM);
    }

    float fsx[3] = { (float)sx[0], (float)sx[1], (float)sx[2] };
    float fsy[3] = { (float)sy[0], (float)sy[1], (float)sy[2] };

    float xa, xb, xc, ya, yb, yc;
    solveAffine(rx, ry, fsx, xa, xb, xc);
    solveAffine(rx, ry, fsy, ya, yb, yc);

    Touch::setCalibration(xa, xb, xc, ya, yb, yc);

    Preferences prefs;
    prefs.begin(NVS_NS_TOUCHCAL, false);
    prefs.putFloat(NVS_KEY_CAL_XA, xa);
    prefs.putFloat(NVS_KEY_CAL_XB, xb);
    prefs.putFloat(NVS_KEY_CAL_XC, xc);
    prefs.putFloat(NVS_KEY_CAL_YA, ya);
    prefs.putFloat(NVS_KEY_CAL_YB, yb);
    prefs.putFloat(NVS_KEY_CAL_YC, yc);
    prefs.putBool(NVS_KEY_CAL_VALID, true);
    prefs.end();

    tft.fillScreen(COL_BG);
    Display::drawHeader("Touch Calibration");
    tft.setTextColor(COL_OK, COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Calibration saved", SCREEN_W / 2, SCREEN_H / 2);
    tft.setTextDatum(TL_DATUM);
    delay(800);
}
