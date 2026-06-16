#include "test_touch.h"
#include "display.h"
#include "touch.h"
#include "menu.h"
#include "config.h"
#include "pins.h"

void testTouch() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("Touch Test");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    const int infoY = HDR_H + 6;
    const int canvasTop = infoY + 24;
    const int canvasBottom = SCREEN_H - 70;

    tft.drawFastHLine(0, canvasTop - 2, SCREEN_W, COL_DIM);
    tft.drawFastHLine(0, canvasBottom + 2, SCREEN_W, COL_DIM);

    // Escape hatch reminder: if the stored calibration is bad, touch may
    // not be able to reach "Recalibrate Touch" in the menu - holding SW1
    // while powering on forces Calibration::run() at boot (see main.cpp).
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Hold SW1 at power-on", SCREEN_W / 2, canvasBottom + 8);
    tft.drawString("to force recalibration", SCREEN_W / 2, canvasBottom + 24);
    tft.setTextDatum(TL_DATUM);

    Serial.println("--- Touch Test ---");
    int16_t lastX = -1, lastY = -1;

    while (!Menu::checkBack()) {
        int16_t x, y;
        if (Touch::getPosition(x, y)) {
            if (y >= canvasTop && y <= canvasBottom) {
                tft.fillCircle(x, y, 2, COL_ACCENT);
            }
            if (x != lastX || y != lastY) {
                tft.fillRect(0, infoY, SCREEN_W, 18, COL_BG);
                tft.setTextColor(COL_FG, COL_BG);
                tft.drawString("X: " + String(x) + "   Y: " + String(y), 8, infoY);
                Serial.printf("Touch: X=%d  Y=%d\n", x, y);
                lastX = x;
                lastY = y;
            }
        }
        delay(15);
    }
}
