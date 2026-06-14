#include "display.h"
#include "pins.h"
#include "config.h"
#include <Preferences.h>

TFT_eSPI tft = TFT_eSPI();

namespace {
    const uint8_t BL_LEDC_CHANNEL = 7;
    const uint32_t BL_LEDC_FREQ = 5000;
    const uint8_t BL_LEDC_RES = 8;
    uint8_t s_brightness = 100;
}

void Display::begin(MCP23017 &mcp) {
    // Display reset is wired to MCP23017 GPA2, not a raw GPIO.
    mcp.pulseDisplayReset();

    // tft.init() does its own pinMode(TFT_BL, OUTPUT) +
    // digitalWrite(TFT_BL, HIGH) (since TFT_BL/TFT_BACKLIGHT_ON are
    // defined), which would detach the LEDC PWM signal if attached
    // beforehand. Set up PWM *after* tft.init() so it isn't clobbered.
    tft.init();
    tft.setRotation(0); // 240x320 portrait
    tft.fillScreen(COL_BG);

    ledcSetup(BL_LEDC_CHANNEL, BL_LEDC_FREQ, BL_LEDC_RES);
    ledcAttachPin(PIN_TFT_BL, BL_LEDC_CHANNEL);

    Preferences prefs;
    prefs.begin(NVS_NS_DISPLAY, true);
    uint8_t saved = prefs.getUChar(NVS_KEY_BL_PERCENT, 100);
    prefs.end();
    setBacklight(saved);
}

void Display::setBacklight(uint8_t percent) {
    percent = constrain(percent, 5, 100);
    s_brightness = percent;
    ledcWrite(BL_LEDC_CHANNEL, (uint32_t)percent * 255 / 100);
}

uint8_t Display::backlight() {
    return s_brightness;
}

void Display::drawHeader(const char *title) {
    tft.fillRect(0, 0, SCREEN_W, HDR_H, COL_HEADER_BG);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(COL_FG, COL_HEADER_BG);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(title, 8, HDR_H / 2);
    tft.setTextDatum(TL_DATUM);
}

void Display::clearBody() {
    tft.fillRect(0, HDR_H, SCREEN_W, SCREEN_H - HDR_H, COL_BG);
}

int Display::bodyTop() {
    return HDR_H;
}

int Display::bodyHeight() {
    return SCREEN_H - HDR_H;
}

int Display::infoLine(int y, const char *label, const String &value, uint16_t valueColor) {
    const int labelX = 8;
    const int valueX = 90;
    const int lineH = 20;

    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(label, labelX, y);

    tft.setTextColor(valueColor, COL_BG);
    if (tft.textWidth(value) <= SCREEN_W - valueX - 4) {
        tft.drawString(value, valueX, y);
        return y + lineH;
    }

    tft.drawString(value, labelX + 12, y + lineH);
    return y + 2 * lineH;
}
