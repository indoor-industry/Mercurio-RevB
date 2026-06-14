#include "menu.h"
#include "display.h"
#include "touch.h"
#include "pins.h"
#include "config.h"
#include "volume.h"

namespace {
    struct TestEntry {
        const char *name;
        TestRunFn run;
    };

    TestEntry s_tests[MAX_TESTS];
    uint8_t s_testCount = 0;

    const int LIST_TOP = HDR_H;
    const int LIST_HEIGHT = SCREEN_H - HDR_H - STATUS_BAR_H;
    const int SCROLLBAR_W = 4;
    const int DRAG_THRESHOLD = 8; // px of movement before a tap becomes a scroll
    const unsigned long RELEASE_MS = 40; // debounce before treating a touch as released

    int s_scrollPx = 0;

    // Drag-scroll tracking, carried across Menu::loop() calls: a tap only
    // runs a test on release if the finger never moved past the threshold,
    // otherwise the movement scrolls the list. Driven entirely by
    // Touch::getPosition() (current state) rather than Touch::getTouch()
    // (edge-triggered) - the resistive panel can briefly report "untouched"
    // mid-press, and getTouch()'s edge would misread that blip as a release
    // followed by a brand new press, resetting the drag before it starts.
    bool s_tracking = false;
    bool s_dragging = false;
    int16_t s_startY = 0;
    int s_startScrollPx = 0;
    int s_pressRow = -1;
    bool s_releasePending = false;
    unsigned long s_releaseStartMs = 0;

    int s_backBtnX, s_backBtnY, s_backBtnW, s_backBtnH;

    int totalEntries() {
        return s_testCount;
    }

    int contentHeight() {
        return totalEntries() * MENU_ROW_H;
    }

    int maxScrollPx() {
        int m = contentHeight() - LIST_HEIGHT;
        return m > 0 ? m : 0;
    }

    bool needsScroll() {
        return maxScrollPx() > 0;
    }

    const char *entryName(int idx) {
        return s_tests[idx].name;
    }

    void clampScroll() {
        if (s_scrollPx < 0) s_scrollPx = 0;
        if (s_scrollPx > maxScrollPx()) s_scrollPx = maxScrollPx();
    }

    void drawScrollbar() {
        if (!needsScroll()) return;

        int trackX = SCREEN_W - SCROLLBAR_W;
        tft.fillRect(trackX, LIST_TOP, SCROLLBAR_W, LIST_HEIGHT, COL_HEADER_BG);

        int thumbH = LIST_HEIGHT * LIST_HEIGHT / contentHeight();
        if (thumbH < 12) thumbH = 12;
        int thumbY = LIST_TOP + (LIST_HEIGHT - thumbH) * s_scrollPx / maxScrollPx();
        tft.fillRect(trackX, thumbY, SCROLLBAR_W, thumbH, COL_ACCENT);
    }

    void drawStatusBar() {
        tft.fillRect(0, SCREEN_H - STATUS_BAR_H, SCREEN_W, STATUS_BAR_H, COL_HEADER_BG);
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextColor(COL_DIM, COL_HEADER_BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString("Tap a test to run it", 6, SCREEN_H - STATUS_BAR_H / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Redraws just the scrollable list + scrollbar (rows tile the whole
    // viewport with no gaps, so each row's own fillRect is enough - no
    // separate clear needed). Used for drag-scroll updates so the header
    // and status bar aren't repainted on every frame, which was causing the
    // black flicker during scrolling.
    void drawList() {
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextDatum(ML_DATUM);

        // Clip rows to the list area (without shifting the coordinate
        // origin) so partially-scrolled rows at the top/bottom edges don't
        // paint over the header or status bar.
        tft.setViewport(0, LIST_TOP, SCREEN_W, LIST_HEIGHT, false);
        for (int idx = 0; idx < totalEntries(); idx++) {
            int y = LIST_TOP + idx * MENU_ROW_H - s_scrollPx;
            if (y + MENU_ROW_H <= LIST_TOP || y >= LIST_TOP + LIST_HEIGHT) continue;

            uint16_t bg = (idx % 2 == 0) ? COL_BG : COL_HEADER_BG;
            tft.fillRect(0, y, SCREEN_W, MENU_ROW_H, bg);
            tft.drawRect(0, y, SCREEN_W, MENU_ROW_H, COL_DIM);
            tft.setTextColor(COL_FG, bg);
            tft.drawString(entryName(idx), 8, y + MENU_ROW_H / 2);
        }
        tft.resetViewport();

        drawScrollbar();
        tft.setTextDatum(TL_DATUM);
    }

    void drawMenu() {
        tft.fillScreen(COL_BG);
        Display::drawHeader("Main Menu");
        drawList();
        drawStatusBar();
    }
}

void Menu::registerTest(const char *name, TestRunFn run) {
    if (s_testCount < MAX_TESTS) {
        s_tests[s_testCount].name = name;
        s_tests[s_testCount].run = run;
        s_testCount++;
    }
}

void Menu::begin() {
    s_scrollPx = 0;
    s_tracking = false;
    s_dragging = false;
    s_releasePending = false;
    drawMenu();
}

void Menu::loop() {
    Volume::poll();

    int16_t x, y;
    bool touched = Touch::getPosition(x, y);

    if (touched) {
        s_releasePending = false;

        if (!s_tracking) {
            // New press. Record where it started but don't act yet - a tap
            // only fires on release if the finger stayed put.
            if (y >= LIST_TOP && y < LIST_TOP + LIST_HEIGHT) {
                s_tracking = true;
                s_dragging = false;
                s_startY = y;
                s_startScrollPx = s_scrollPx;
                s_pressRow = (y + s_scrollPx - LIST_TOP) / MENU_ROW_H;
            }
            return;
        }

        int delta = s_startY - y;
        if (!s_dragging && abs(delta) > DRAG_THRESHOLD) {
            s_dragging = true;
        }
        if (s_dragging) {
            s_scrollPx = s_startScrollPx + delta;
            clampScroll();
            drawList();
        }
        return;
    }

    if (!s_tracking) {
        return;
    }

    // Not touched - debounce before treating this as a real release, since
    // the panel can briefly read "untouched" for a frame mid-press.
    if (!s_releasePending) {
        s_releasePending = true;
        s_releaseStartMs = millis();
        return;
    }
    if (millis() - s_releaseStartMs < RELEASE_MS) {
        return;
    }

    // Confirmed released.
    s_tracking = false;
    s_releasePending = false;
    if (!s_dragging && s_pressRow >= 0 && s_pressRow < totalEntries()) {
        s_tests[s_pressRow].run();
        drawMenu();
    }
    s_pressRow = -1;
}

void Menu::drawBackButton() {
    const int btnW = 110, btnH = 34;
    s_backBtnX = (SCREEN_W - btnW) / 2;
    s_backBtnY = SCREEN_H - btnH - 8;
    s_backBtnW = btnW;
    s_backBtnH = btnH;

    tft.fillRoundRect(s_backBtnX, s_backBtnY, btnW, btnH, 6, COL_ACCENT);
    tft.drawRoundRect(s_backBtnX, s_backBtnY, btnW, btnH, 6, COL_FG);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextColor(COL_BG, COL_ACCENT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Back", s_backBtnX + btnW / 2, s_backBtnY + btnH / 2);
    tft.setTextDatum(TL_DATUM);
}

bool Menu::backButtonHit(int16_t x, int16_t y) {
    // The resistive panel's calibration is a single screen-wide affine fit,
    // so small per-region errors are expected - pad the hit area a bit
    // beyond the drawn button so near-miss taps (especially just above it)
    // still register.
    const int pad = 10;
    return x >= s_backBtnX - pad && x <= s_backBtnX + s_backBtnW + pad &&
           y >= s_backBtnY - pad && y <= s_backBtnY + s_backBtnH + pad;
}

bool Menu::checkBack() {
    Volume::poll();

    int16_t x, y;
    if (Touch::getTouch(x, y)) {
        return backButtonHit(x, y);
    }
    return false;
}

void Menu::waitForBack() {
    drawBackButton();
    while (!checkBack()) {
        delay(20);
    }
}
