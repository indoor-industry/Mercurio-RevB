#include "test_lora.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "board.h"
#include "volume.h"
#include <SPI.h>
#include <RadioLib.h>

namespace {
    SX1262 radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, SPI);

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

    void drawResult(int y, const String &msg, uint16_t color) {
        tft.fillRect(0, y, SCREEN_W, 22, COL_BG);
        tft.setTextColor(color, COL_BG);
        tft.drawString(msg, 8, y);
    }
}

void testLora() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("LoRa SX1262");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);

    // RFEN must be HIGH to enable the RF front-end before talking to the radio.
    pinMode(PIN_LORA_RFEN, OUTPUT);
    digitalWrite(PIN_LORA_RFEN, HIGH);
    delay(10);

    int state = radio.begin(LORA_FREQ_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING,
                             LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
                             LORA_PREAMBLE_LEN);
    bool ok = (state == RADIOLIB_ERR_NONE);
    Serial.println("--- LoRa SX1262 ---");
    if (ok) {
        Serial.printf("Init: OK  Freq: %.1f MHz\n", LORA_FREQ_MHZ);
    } else {
        Serial.printf("Init: FAIL (err %d)\n", state);
    }

    int y = HDR_H + 8;
    y = Display::infoLine(y, "Init:", ok ? "OK" : ("FAIL (" + String(state) + ")"), ok ? COL_OK : COL_ERR);
    y = Display::infoLine(y, "Freq:", String(LORA_FREQ_MHZ, 1) + " MHz", COL_FG);

    const int rssiY = y;
    const int dioY = rssiY + 20;
    const int resultY = dioY + 30;

    Btn txBtn = {8, resultY + 30, SCREEN_W - 16, 40, "Send Test Packet"};
    Btn cwBtn = {8, resultY + 30 + 48, SCREEN_W - 16, 40, "TX CW Carrier (5s)"};
    drawButton(txBtn);
    drawButton(cwBtn);

    Menu::drawBackButton();

    if (ok) {
        radio.startReceive();
    }

    unsigned long lastLoraSerialMs = 0;
    while (true) {
        Volume::poll();

        if (ok) {
            float rssi = radio.getRSSI();
            tft.fillRect(0, rssiY, SCREEN_W, 20, COL_BG);
            tft.setTextColor(COL_DIM, COL_BG);
            tft.drawString("RSSI:", 8, rssiY);
            tft.setTextColor(COL_FG, COL_BG);
            tft.drawString(String(rssi, 1) + " dBm", 90, rssiY);
            if (millis() - lastLoraSerialMs >= 2000) {
                lastLoraSerialMs = millis();
                Serial.printf("RSSI: %.1f dBm\n", rssi);
            }
        }

        uint16_t gpio = mcp.readGPIO();
        bool dio2 = MCP23017::gpioBit(gpio, MCP_BIT_LORA_DIO2);
        bool dio3 = MCP23017::gpioBit(gpio, MCP_BIT_LORA_DIO3);
        tft.fillRect(0, dioY, SCREEN_W, 20, COL_BG);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString("DIO2/3:", 8, dioY);
        tft.setTextColor(COL_FG, COL_BG);
        tft.drawString(String(dio2) + " / " + String(dio3), 90, dioY);

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            if (ok && inBtn(txBtn, tx, ty)) {
                drawResult(resultY, "Sending...", COL_WARN);
                int st = radio.transmit("Hello from LoRa-device RevB!");
                if (st == RADIOLIB_ERR_NONE) {
                    drawResult(resultY, "TX OK", COL_OK);
                    Serial.println("TX OK");
                } else {
                    drawResult(resultY, "TX FAIL (" + String(st) + ")", COL_ERR);
                    Serial.printf("TX FAIL (err %d)\n", st);
                }
                radio.startReceive();
            } else if (ok && inBtn(cwBtn, tx, ty)) {
                drawResult(resultY, "CW carrier ON (5s)...", COL_WARN);
                radio.transmitDirect();
                delay(5000);
                radio.standby();
                drawResult(resultY, "CW done", COL_OK);
                radio.startReceive();
            }
        }

        delay(100);
    }

    radio.standby();
}
