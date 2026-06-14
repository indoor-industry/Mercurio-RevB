#include "test_sysinfo.h"
#include "display.h"
#include "menu.h"
#include "config.h"
#include <esp_mac.h>
#include <esp_system.h>

namespace {
    const char *flashModeName(FlashMode_t mode) {
        switch (mode) {
            case FM_QIO:  return "QIO";
            case FM_QOUT: return "QOUT";
            case FM_DIO:  return "DIO";
            case FM_DOUT: return "DOUT";
            case FM_FAST_READ: return "Fast Read";
            case FM_SLOW_READ: return "Slow Read";
            default: return "Unknown";
        }
    }

    const char *resetReasonName(esp_reset_reason_t reason) {
        switch (reason) {
            case ESP_RST_POWERON:   return "Power-on";
            case ESP_RST_SW:        return "Software";
            case ESP_RST_PANIC:     return "Panic";
            case ESP_RST_INT_WDT:   return "Interrupt WDT";
            case ESP_RST_TASK_WDT:  return "Task WDT";
            case ESP_RST_WDT:       return "Other WDT";
            case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
            case ESP_RST_BROWNOUT:  return "Brownout";
            case ESP_RST_SDIO:      return "SDIO";
            default: return "Unknown";
        }
    }

}

void testSysInfo() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("System Info");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);

    int y = HDR_H + 8;

    y = Display::infoLine(y, "Chip:", String(ESP.getChipModel()) + " rev" + String(ESP.getChipRevision()), COL_FG);
    y = Display::infoLine(y, "Cores:", String(ESP.getChipCores()) + " @ " + String(ESP.getCpuFreqMHz()) + " MHz", COL_FG);

    uint32_t flashMB = ESP.getFlashChipSize() / (1024 * 1024);
    y = Display::infoLine(y, "Flash:", String(flashMB) + " MB @ " + String(ESP.getFlashChipSpeed() / 1000000) +
                                " MHz (" + flashModeName(ESP.getFlashChipMode()) + ")", COL_FG);

    uint32_t psramMB = ESP.getPsramSize() / (1024 * 1024);
    y = Display::infoLine(y, "PSRAM:", String(psramMB) + " MB (free " + String(ESP.getFreePsram() / 1024) + " KB)", COL_FG);

    y = Display::infoLine(y, "Heap:", String(ESP.getFreeHeap() / 1024) + " / " + String(ESP.getHeapSize() / 1024) +
                               " KB (min " + String(ESP.getMinFreeHeap() / 1024) + " KB)", COL_FG);

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    y = Display::infoLine(y, "MAC:", macStr, COL_FG);

    y = Display::infoLine(y, "IDF ver:", esp_get_idf_version(), COL_FG);

    y = Display::infoLine(y, "Reset:", resetReasonName(esp_reset_reason()), COL_FG);

    unsigned long s = millis() / 1000;
    y = Display::infoLine(y, "Uptime:", String(s / 3600) + "h " + String((s / 60) % 60) + "m " + String(s % 60) + "s", COL_FG);

    Menu::waitForBack();
}
