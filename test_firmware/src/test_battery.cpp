#include "test_battery.h"
#include "display.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include "board.h"

namespace {
    const int valueX = 90;
    const int lineH = 20;

    // Redraws only the value column for one row, leaving the label and the
    // rest of the screen untouched - a full-area clear/redraw every 250ms
    // was what caused the heavy flicker.
    void drawValue(int y, const String &value, uint16_t color) {
        tft.fillRect(valueX, y, SCREEN_W - valueX, lineH, COL_BG);
        tft.setTextColor(color, COL_BG);
        tft.drawString(value, valueX, y);
    }
}

void testBattery() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("Battery");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    const int yVoltage = HDR_H + 8;
    const int yStatus = yVoltage + lineH;
    const int yStat1 = yStatus + lineH;
    const int yStat2 = yStat1 + lineH;

    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Voltage:", 8, yVoltage);
    tft.drawString("Status:", 8, yStatus);
    tft.drawString("STAT1:", 8, yStat1);
    tft.drawString("STAT2:", 8, yStat2);

    while (!Menu::checkBack()) {
        uint32_t mv = analogReadMilliVolts(PIN_BAT_ADC);
        float vbat = (mv / 1000.0f) * BAT_ADC_DIVIDER;
        drawValue(yVoltage, String(vbat, 2) + " V", COL_FG);

        uint16_t gpio = mcp.readGPIO();
        bool stat1 = MCP23017::gpioBit(gpio, MCP_BIT_BAT_STAT1);
        bool stat2 = MCP23017::gpioBit(gpio, MCP_BIT_BAT_STAT2);

        const char *status;
        uint16_t statColor;
        if (!stat1) {
            status = "Fault";
            statColor = COL_ERR;
        } else if (!stat2) {
            status = "Charging";
            statColor = COL_WARN;
        } else {
            status = "Charged / not charging";
            statColor = COL_OK;
        }
        drawValue(yStatus, status, statColor);
        drawValue(yStat1, stat1 ? "1 (ok)" : "0 (fault)", COL_FG);
        drawValue(yStat2, stat2 ? "1 (done)" : "0 (charging)", COL_FG);

        delay(250);
    }
}
