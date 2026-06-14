#include "test_backlight.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include <Preferences.h>

namespace {
    const int barX = 16;
    const int barY = HDR_H + 60;
    const int barW = SCREEN_W - 2 * barX;
    const int barH = 28;

    void drawSlider(uint8_t percent) {
        int fillW = (barW - 2) * percent / 100;
        tft.drawRect(barX, barY, barW, barH, COL_FG);
        tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, COL_BG);
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, COL_ACCENT);

        tft.fillRect(0, barY + barH + 14, SCREEN_W, 24, COL_BG);
        tft.setTextColor(COL_FG, COL_BG);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(String(percent) + "%", SCREEN_W / 2, barY + barH + 14);
        tft.setTextDatum(TL_DATUM);
    }
}

void testBacklight() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("Backlight");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Drag the bar to adjust brightness", 8, HDR_H + 16);

    uint8_t percent = Display::backlight();
    drawSlider(percent);

    while (!Menu::checkBack()) {
        int16_t x, y;
        if (Touch::getPosition(x, y) && y >= barY - 16 && y <= barY + barH + 16) {
            int p = constrain((int)((x - barX) * 100L / (barW - 1)), 5, 100);
            if (p != percent) {
                percent = (uint8_t)p;
                Display::setBacklight(percent);
                drawSlider(percent);
            }
        }
        delay(15);
    }

    Preferences prefs;
    prefs.begin(NVS_NS_DISPLAY, false);
    prefs.putUChar(NVS_KEY_BL_PERCENT, percent);
    prefs.end();
}
