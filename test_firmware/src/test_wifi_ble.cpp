#include "test_wifi_ble.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "volume.h"
#include <WiFi.h>
#include <NimBLEDevice.h>

namespace {
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

    String truncated(const String &s, int maxLen) {
        return (s.length() > (size_t)maxLen) ? s.substring(0, maxLen) : s;
    }
}

void testWifiBle() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("WiFi / BLE Scan");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    Btn scanBtn = {8, HDR_H + 8, SCREEN_W - 16, 40, "Scan"};
    drawButton(scanBtn);

    const int listTop = HDR_H + 56;
    const int listBottom = SCREEN_H - 50;

    while (true) {
        Volume::poll();

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            if (inBtn(scanBtn, tx, ty)) {
                Serial.println("--- WiFi / BLE Scan ---");
                tft.fillRect(0, listTop, SCREEN_W, listBottom - listTop, COL_BG);
                int y = listTop;

                tft.setTextColor(COL_DIM, COL_BG);
                tft.drawString("Scanning WiFi...", 8, y);
                WiFi.mode(WIFI_STA);
                int n = WiFi.scanNetworks();
                tft.fillRect(0, y, SCREEN_W, 20, COL_BG);
                y = Display::infoLine(y, "WiFi APs:", String(n), COL_FG);
                Serial.printf("WiFi APs found: %d\n", n);

                int wifiShown = (n > 0 && n < 4) ? n : (n >= 4 ? 4 : 0);
                for (int i = 0; i < wifiShown; i++) {
                    String line = truncated(WiFi.SSID(i), 20) + " (" + String(WiFi.RSSI(i)) + ")";
                    tft.setTextColor(COL_FG, COL_BG);
                    tft.drawString(line, 16, y);
                    y += 18;
                    Serial.printf("  [%d] %s  RSSI: %d dBm\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
                }
                WiFi.scanDelete();
                WiFi.mode(WIFI_OFF);

                y += 8;
                tft.setTextColor(COL_DIM, COL_BG);
                tft.drawString("Scanning BLE...", 8, y);

                NimBLEDevice::init("");
                NimBLEScan *scan = NimBLEDevice::getScan();
                scan->start(3, false);
                NimBLEScanResults results = scan->getResults();

                tft.fillRect(0, y, SCREEN_W, 20, COL_BG);
                int bn = results.getCount();
                y = Display::infoLine(y, "BLE devices:", String(bn), COL_FG);
                Serial.printf("BLE devices found: %d\n", bn);

                int bleShown = (bn > 0 && bn < 4) ? bn : (bn >= 4 ? 4 : 0);
                for (int i = 0; i < bleShown; i++) {
                    const NimBLEAdvertisedDevice *dev = results.getDevice(i);
                    String name = dev->getName().c_str();
                    if (name.length() == 0) {
                        name = dev->getAddress().toString().c_str();
                    }
                    String line = truncated(name, 20) + " (" + String(dev->getRSSI()) + ")";
                    tft.setTextColor(COL_FG, COL_BG);
                    tft.drawString(line, 16, y);
                    y += 18;
                    Serial.printf("  [%d] %s  RSSI: %d dBm\n", i, name.c_str(), dev->getRSSI());
                }
                scan->clearResults();
                NimBLEDevice::deinit(true);
            }
        }
        delay(20);
    }
}
