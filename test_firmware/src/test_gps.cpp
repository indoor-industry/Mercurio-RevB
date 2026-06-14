#include "test_gps.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "board.h"
#include "volume.h"
#include <TinyGPSPlus.h>

namespace {
    const int valueX = 90;
    const int lineH = 20;

    // A row reserved just below the header for the Raw/Text toggle button.
    // It used to live in the header bar's top-right corner, which is also
    // where Volume::poll() draws its level overlay - same accent colour,
    // same spot, so the two sat on top of each other. Giving the button its
    // own row keeps the header corner free for the overlay.
    const int BTN_ROW_H = 30;

    // Content area between the button row and the back button.
    const int CONTENT_TOP = HDR_H + BTN_ROW_H;
    const int CONTENT_H = SCREEN_H - CONTENT_TOP - 44;

    struct Btn {
        int x, y, w, h;
        const char *label;
    };

    bool inBtn(const Btn &b, int16_t x, int16_t y) {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    }

    void drawButton(const Btn &b) {
        tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, COL_ACCENT);
        tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, COL_FG);
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextColor(COL_BG, COL_ACCENT);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
        tft.setTextDatum(TL_DATUM);
        tft.setFreeFont(&FreeSans9pt7b);
    }

    void drawLabel(int y, const char *label) {
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString(label, 8, y);
    }

    // Redraws only the value column for one row, avoiding a full-screen
    // clear/redraw (which caused heavy flicker every update).
    void drawValue(int y, const String &value, uint16_t color) {
        tft.fillRect(valueX, y, SCREEN_W - valueX, lineH, COL_BG);
        tft.setTextColor(color, COL_BG);
        tft.drawString(value, valueX, y);
    }

    void clearContent() {
        tft.fillRect(0, CONTENT_TOP, SCREEN_W, CONTENT_H, COL_BG);
    }

    // Raw NMEA dump - lets you confirm bytes are actually arriving over
    // Serial1 (and what they look like) without TinyGPSPlus in the way,
    // for diagnosing "no fix" vs "no data at all".
    const int RAW_LINE_LEN = 60;
    const int RAW_LINE_H = 18;
    const int RAW_LINES = CONTENT_H / RAW_LINE_H - 1; // last line is the live partial sentence

    String rawBuf[32]; // sized for the largest plausible RAW_LINES
    String rawCur;

    void feedRawChar(char c) {
        if (c == '\r') {
            return;
        }
        if (c == '\n') {
            for (int i = 0; i < RAW_LINES - 1; i++) {
                rawBuf[i] = rawBuf[i + 1];
            }
            rawBuf[RAW_LINES - 1] = rawCur;
            rawCur = "";
            return;
        }
        if (rawCur.length() < RAW_LINE_LEN) {
            rawCur += c;
        }
    }

    void drawRawLine(int y, const String &s, uint16_t color) {
        tft.fillRect(0, y, SCREEN_W, RAW_LINE_H, COL_BG);
        tft.setTextColor(color, COL_BG);
        tft.drawString(s, 4, y);
    }

    void drawRawView() {
        int y = CONTENT_TOP + 2;
        for (int i = 0; i < RAW_LINES; i++) {
            drawRawLine(y, rawBuf[i], COL_DIM);
            y += RAW_LINE_H;
        }
        drawRawLine(y, rawCur, COL_OK);
    }
}

void testGps() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("GPS");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    // GPS_WKUP/GPS_RST and Serial1 are set up once at boot (main.cpp) and
    // left running continuously - the module is V_BCKP-backed for hot
    // starts, so it must never be reset just to view this screen.
    TinyGPSPlus gps;

    enum class Mode { Parsed, Raw };
    Mode mode = Mode::Parsed;

    Btn modeBtn = {SCREEN_W - 62, HDR_H + 2, 56, BTN_ROW_H - 4, "Raw"};
    drawButton(modeBtn);

    // Fixed row layout - all rows are always drawn so the screen layout
    // never shifts (which previously made the whole screen appear to
    // flicker whenever the fix status changed).
    const int yFix = CONTENT_TOP + 8;
    const int ySats = yFix + lineH;
    const int yLat = ySats + lineH;
    const int yLon = yLat + lineH;
    const int yAlt = yLon + lineH;
    const int ySpeed = yAlt + lineH;
    const int yHdop = ySpeed + lineH;
    const int yChars = yHdop + lineH;
    const int yPps = yChars + lineH;
    const int yTtff = yPps + lineH;

    auto drawParsedLabels = [&]() {
        drawLabel(yFix, "Fix:");
        drawLabel(ySats, "Sats:");
        drawLabel(yLat, "Lat:");
        drawLabel(yLon, "Lon:");
        drawLabel(yAlt, "Alt:");
        drawLabel(ySpeed, "Speed:");
        drawLabel(yHdop, "HDOP:");
        drawLabel(yChars, "Chars:");
        drawLabel(yPps, "1PPS:");
        drawLabel(yTtff, "TTFF:");
    };

    drawParsedLabels();

    // Time-to-first-fix since GPS_RST was released at boot (main.cpp). A
    // few seconds means the module did a warm/hot start (V_BCKP-backed
    // ephemeris/RTC was retained); 30s+ means it did a cold start, i.e. the
    // backup data was lost while the board was off.
    bool ttffLatched = false;
    unsigned long ttffMs = 0;

    unsigned long lastUpdate = 0;

    while (true) {
        Volume::poll();

        while (Serial1.available()) {
            char c = (char)Serial1.read();
            gps.encode(c);
            feedRawChar(c);
        }

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            if (inBtn(modeBtn, tx, ty)) {
                mode = (mode == Mode::Parsed) ? Mode::Raw : Mode::Parsed;
                modeBtn.label = (mode == Mode::Parsed) ? "Raw" : "Text";
                drawButton(modeBtn);
                clearContent();
                if (mode == Mode::Parsed) {
                    drawParsedLabels();
                }
                lastUpdate = 0;
            }
        }

        if (millis() - lastUpdate >= 250) {
            lastUpdate = millis();

            if (mode == Mode::Raw) {
                drawRawView();
            } else {
                bool fix = gps.location.isValid();
                drawValue(yFix, fix ? "Yes" : "No", fix ? COL_OK : COL_WARN);
                drawValue(ySats, String(gps.satellites.value()), COL_FG);

                if (fix) {
                    drawValue(yLat, String(gps.location.lat(), 6), COL_FG);
                    drawValue(yLon, String(gps.location.lng(), 6), COL_FG);

                    // The GGA sentence (which carries altitude) doesn't
                    // always arrive on the same cadence as the fix itself,
                    // and a stale/never-set altitude reads as a leftover or
                    // default value - only show it when TinyGPS++ has a
                    // recent altitude field to go with the current fix.
                    if (gps.altitude.isValid() && gps.altitude.age() < 2000) {
                        drawValue(yAlt, String(gps.altitude.meters(), 1) + " m", COL_FG);
                    } else {
                        drawValue(yAlt, "-", COL_DIM);
                    }

                    drawValue(ySpeed, String(gps.speed.kmph(), 1) + " km/h", COL_FG);

                    if (!ttffLatched) {
                        ttffMs = millis() - g_gpsBootMs;
                        ttffLatched = true;
                    }
                } else {
                    drawValue(yLat, "-", COL_DIM);
                    drawValue(yLon, "-", COL_DIM);
                    drawValue(yAlt, "-", COL_DIM);
                    drawValue(ySpeed, "-", COL_DIM);
                }

                drawValue(yHdop, String(gps.hdop.hdop(), 1), COL_FG);
                drawValue(yChars, String(gps.charsProcessed()), COL_FG);

                uint16_t gpio = mcp.readGPIO();
                bool pps = MCP23017::gpioBit(gpio, MCP_BIT_GPS_1PPS);
                drawValue(yPps, pps ? "1" : "0", COL_FG);

                if (ttffLatched) {
                    drawValue(yTtff, String(ttffMs / 1000.0f, 1) + " s", COL_FG);
                } else {
                    drawValue(yTtff, "-", COL_DIM);
                }
            }
        }

        delay(10);
    }
}
