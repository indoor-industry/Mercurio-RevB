#include "volume.h"
#include "display.h"
#include "board.h"
#include "config.h"
#include "pins.h"
#include <Preferences.h>

namespace {
    const int MAX_LEVEL = 10;
    const unsigned long OVERLAY_MS = 1200;
    const unsigned long POLL_MS = 50; // rate-limit MCP I2C reads + debounce

    const int SEG_W = 5, SEG_GAP = 2;
    const int OVERLAY_W = MAX_LEVEL * (SEG_W + SEG_GAP) - SEG_GAP;
    const int OVERLAY_X = SCREEN_W - OVERLAY_W - 4;
    const int OVERLAY_H = 10;
    const int OVERLAY_Y = (HDR_H - OVERLAY_H) / 2;

    int s_level = MAX_LEVEL;
    bool s_prevSw1 = false;
    bool s_prevSw2 = false;
    bool s_overlayShown = false;
    unsigned long s_overlayUntil = 0;
    unsigned long s_lastPoll = 0;

    void save() {
        Preferences prefs;
        prefs.begin(NVS_NS_VOLUME, false);
        prefs.putUChar(NVS_KEY_VOL_LEVEL, (uint8_t)s_level);
        prefs.end();
    }

    void drawOverlay() {
        for (int i = 0; i < MAX_LEVEL; i++) {
            int x = OVERLAY_X + i * (SEG_W + SEG_GAP);
            uint16_t c = (i < s_level) ? COL_ACCENT : COL_DIM;
            tft.fillRect(x, OVERLAY_Y, SEG_W, OVERLAY_H, c);
        }
    }

    void clearOverlay() {
        tft.fillRect(OVERLAY_X, OVERLAY_Y, OVERLAY_W, OVERLAY_H, COL_HEADER_BG);
    }
}

void Volume::begin() {
    Preferences prefs;
    prefs.begin(NVS_NS_VOLUME, true);
    s_level = constrain((int)prefs.getUChar(NVS_KEY_VOL_LEVEL, MAX_LEVEL), 0, MAX_LEVEL);
    prefs.end();
}

void Volume::poll() {
    unsigned long now = millis();
    if (now - s_lastPoll < POLL_MS) {
        if (s_overlayShown && now >= s_overlayUntil) {
            clearOverlay();
            s_overlayShown = false;
        }
        return;
    }
    s_lastPoll = now;

    uint16_t gpio = mcp.readGPIO();
    bool sw1 = !MCP23017::gpioBit(gpio, MCP_BIT_SW1); // active-low
    bool sw2 = !MCP23017::gpioBit(gpio, MCP_BIT_SW2);

    bool changed = false;
    if (sw1 && !s_prevSw1 && s_level < MAX_LEVEL) {
        s_level++;
        changed = true;
    }
    if (sw2 && !s_prevSw2 && s_level > 0) {
        s_level--;
        changed = true;
    }
    s_prevSw1 = sw1;
    s_prevSw2 = sw2;

    if (changed) {
        save();
        drawOverlay();
        s_overlayShown = true;
        s_overlayUntil = now + OVERLAY_MS;
    } else if (s_overlayShown && now >= s_overlayUntil) {
        clearOverlay();
        s_overlayShown = false;
    }
}

float Volume::gain() {
    return (float)s_level / (float)MAX_LEVEL;
}
