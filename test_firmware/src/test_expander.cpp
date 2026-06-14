#include "test_expander.h"
#include "display.h"
#include "menu.h"
#include "config.h"
#include "pins.h"
#include "board.h"

namespace {
    struct PinInfo {
        const char *name;
        uint8_t bit;
    };

    // Port A (bits 0-7) and Port B (bits 8-15), per the pins.h MCP_BIT_* map.
    const PinInfo PORT_A[8] = {
        {"GPS_RST",   MCP_BIT_GPS_RST},
        {"BAT_STAT1", MCP_BIT_BAT_STAT1},
        {"DISP_RST",  MCP_BIT_DISP_RST},
        {"GPS_WKUP",  MCP_BIT_GPS_WKUP},
        {"GPS_1PPS",  MCP_BIT_GPS_1PPS},
        {"AUDIO_EN",  MCP_BIT_AUDIO_EN},
        {"SW1",       MCP_BIT_SW1},
        {"GPA7",      MCP_BIT_GPA7},
    };

    const PinInfo PORT_B[8] = {
        {"SW2",       MCP_BIT_SW2},
        {"DIO2",      MCP_BIT_LORA_DIO2},
        {"DIO3",      MCP_BIT_LORA_DIO3},
        {"GPB3",      MCP_BIT_GPB3},
        {"TOUCH_IRQ", MCP_BIT_TOUCH_IRQ},
        {"BAT_STAT2", MCP_BIT_BAT_STAT2},
        {"SD_DET",    MCP_BIT_SD_DET},
        {"GPB7",      MCP_BIT_GPB7},
    };

    void drawRow(int x, int y, int w, const PinInfo &p, uint16_t gpio) {
        bool val = MCP23017::gpioBit(gpio, p.bit);
        tft.fillRect(x, y, w, 22, COL_BG);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString(p.name, x + 4, y + 2);
        tft.setTextColor(val ? COL_OK : COL_DIM, COL_BG);
        tft.drawString(val ? "1" : "0", x + w - 18, y + 2);
    }
}

void testExpander() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("I/O Expander");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    const int colW = SCREEN_W / 2;
    const int rowH = 24;
    const int top = HDR_H + 6;

    while (!Menu::checkBack()) {
        uint16_t gpio = mcp.readGPIO();
        for (int i = 0; i < 8; i++) {
            drawRow(0, top + i * rowH, colW, PORT_A[i], gpio);
            drawRow(colW, top + i * rowH, colW, PORT_B[i], gpio);
        }
        delay(150);
    }
}
