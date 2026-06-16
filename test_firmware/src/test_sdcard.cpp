#include "test_sdcard.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "board.h"
#include "volume.h"
#include <SD.h>
#include <TJpg_Decoder.h>
#include <PNGdec.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

namespace {
    const char *cardTypeName(uint8_t t) {
        switch (t) {
            case CARD_NONE: return "None";
            case CARD_MMC:  return "MMC";
            case CARD_SD:   return "SDSC";
            case CARD_SDHC: return "SDHC";
            default:        return "Unknown";
        }
    }

    struct Btn {
        int x, y, w, h;
        const char *label;
    };

    bool inBtn(const Btn &b, int16_t x, int16_t y) {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    }

    enum class Kind { Text, Image, Audio, Other };

    const int MAX_FILES = 5;
    const int ROW_H = 22;

    struct FileEntry {
        String name;
        uint32_t size;
        Kind kind;
    };

    Kind classify(const String &name) {
        String lower = name;
        lower.toLowerCase();
        if (lower.endsWith(".txt")) return Kind::Text;
        if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png")) return Kind::Image;
        if (lower.endsWith(".mp3") || lower.endsWith(".wav")) return Kind::Audio;
        return Kind::Other;
    }

    // TJpg_Decoder render callback - blits decoded blocks straight to the TFT.
    bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
        tft.pushImage(x, y, w, h, bitmap);
        return true;
    }

    // PNGdec file-access callbacks and render state - PNGdec calls these as
    // free functions, so the open file/buffer/origin are kept as statics.
    PNG png;
    File pngFile;
    uint16_t *pngLineBuf = nullptr;
    int pngDestX = 0, pngDestY = 0;

    void *pngOpen(const char *filename, int32_t *size) {
        pngFile = SD.open(filename, FILE_READ);
        *size = pngFile.size();
        return &pngFile;
    }

    void pngClose(void *handle) {
        pngFile.close();
    }

    int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
        return pngFile.read(buffer, length);
    }

    int32_t pngSeek(PNGFILE *handle, int32_t position) {
        return pngFile.seek(position);
    }

    int pngDraw(PNGDRAW *pDraw) {
        if (!pngLineBuf) return 0;
        png.getLineAsRGB565(pDraw, pngLineBuf, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
        tft.pushImage(pngDestX, pngDestY + pDraw->y, pDraw->iWidth, 1, pngLineBuf);
        return 1;
    }

    // Naive char-wrapping text dump - good enough for short test files.
    void viewText(const FileEntry &fe) {
        tft.fillScreen(COL_BG);
        Display::drawHeader(fe.name.c_str());
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_FG, COL_BG);
        Menu::drawBackButton();

        String content;
        File f = SD.open("/" + fe.name, FILE_READ);
        if (f) {
            content = f.readString();
            f.close();
        }

        const int x = 8;
        const int maxW = SCREEN_W - 2 * x;
        const int lineH = 18;
        const int maxY = SCREEN_H - 2 * lineH - 16;

        int y = HDR_H + 8;
        String line;
        for (size_t i = 0; i < content.length() && y <= maxY; i++) {
            char c = content[i];
            if (c == '\r') continue;
            if (c == '\n') {
                tft.drawString(line, x, y);
                y += lineH;
                line = "";
                continue;
            }
            String test = line + c;
            if (tft.textWidth(test) > maxW) {
                tft.drawString(line, x, y);
                y += lineH;
                line = String(c);
                if (y > maxY) break;
            } else {
                line = test;
            }
        }
        if (y <= maxY) {
            tft.drawString(line, x, y);
        }

        while (!Menu::checkBack()) {
            delay(20);
        }
    }

    void viewImage(const FileEntry &fe) {
        tft.fillScreen(COL_BG);
        Display::drawHeader(fe.name.c_str());
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextDatum(TL_DATUM);
        Menu::drawBackButton();

        String path = "/" + fe.name;
        String lower = fe.name;
        lower.toLowerCase();

        const int bodyW = SCREEN_W;
        const int bodyH = SCREEN_H - HDR_H - 50; // leave room for the back button

        if (lower.endsWith(".png")) {
            // PNGdec has no built-in downscaling - centered, drawn at native
            // size (TFT_eSPI clips anything outside the screen).
            int rc = png.open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
            if (rc == PNG_SUCCESS) {
                int pw = png.getWidth();
                int ph = png.getHeight();
                pngDestX = (bodyW - pw) / 2;
                if (pngDestX < 0) pngDestX = 0;
                pngDestY = HDR_H + (bodyH - ph) / 2;
                if (pngDestY < HDR_H) pngDestY = HDR_H;

                pngLineBuf = (uint16_t *)ps_malloc(pw * sizeof(uint16_t));
                if (pngLineBuf) {
                    png.decode(NULL, 0);
                    free(pngLineBuf);
                    pngLineBuf = nullptr;
                }
                png.close();
            } else {
                tft.setTextColor(COL_ERR, COL_BG);
                tft.drawString("PNG decode failed", 8, HDR_H + 8);
            }
        } else {
            uint16_t jw = 0, jh = 0;
            TJpgDec.getJpgSize(&jw, &jh, path);

            uint8_t scale = 1;
            while (scale < 8 && (jw / scale > (uint16_t)bodyW || jh / scale > (uint16_t)bodyH)) {
                scale *= 2;
            }
            TJpgDec.setJpgScale(scale);

            int dx = (bodyW - (int)(jw / scale)) / 2;
            int dy = HDR_H + (bodyH - (int)(jh / scale)) / 2;
            if (dx < 0) dx = 0;
            if (dy < HDR_H) dy = HDR_H;

            JRESULT res = TJpgDec.drawSdJpg(dx, dy, path);
            if (res != JDR_OK) {
                tft.setTextColor(COL_ERR, COL_BG);
                tft.drawString("JPEG decode failed", 8, HDR_H + 8);
            }
        }

        while (!Menu::checkBack()) {
            delay(20);
        }
    }

    void playAudio(const FileEntry &fe, int statusY) {
        tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
        tft.setTextColor(COL_WARN, COL_BG);
        tft.drawString("Playing " + fe.name + "...", 8, statusY);

        String path = "/" + fe.name;
        String lower = fe.name;
        lower.toLowerCase();

        AudioFileSourceSD source(path.c_str());
        AudioOutputI2S out;
        out.SetPinout(PIN_I2S_BCLK, PIN_I2S_WS, PIN_I2S_DOUT);
        // AudioOutputI2S defaults to I2S_COMM_FORMAT_STAND_I2S (1-BCLK
        // delay), the same format that made the speaker test quiet before
        // it was switched to STAND_MSB. SetLsbJustified(true) selects
        // STAND_MSB here too, for full volume on the MAX98357A.
        out.SetLsbJustified(true);
        out.SetGain(Volume::gain());

        AudioGeneratorMP3 mp3;
        AudioGeneratorWAV wav;
        AudioGenerator *gen = lower.endsWith(".wav") ? (AudioGenerator *)&wav : (AudioGenerator *)&mp3;

        mcp.writePin(MCP_BIT_AUDIO_EN, true);
        delay(5);

        bool started = gen->begin(&source, &out);
        while (started && gen->isRunning()) {
            if (!gen->loop()) {
                gen->stop();
                break;
            }
            Volume::poll();
            out.SetGain(Volume::gain());
            int16_t tx, ty;
            if (Touch::getTouch(tx, ty) && Menu::backButtonHit(tx, ty)) {
                gen->stop();
                break;
            }
        }

        mcp.writePin(MCP_BIT_AUDIO_EN, false);

        tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
        tft.setTextColor(started ? COL_OK : COL_ERR, COL_BG);
        tft.drawString(started ? "Done" : "Audio init failed", 8, statusY);
    }
}

void testSdCard() {
    Serial.println("--- microSD ---");
    // SD_DET polarity is unconfirmed - shown raw for now (see project plan).
    uint16_t gpio = mcp.readGPIO();
    bool det = MCP23017::gpioBit(gpio, MCP_BIT_SD_DET);
    Serial.printf("SD_DET: %d\n", det);

    bool mounted = SD.begin(PIN_SD_CS, tft.getSPIinstance(), SPI_FREQ_SD);
    Serial.printf("Mount: %s\n", mounted ? "OK" : "FAIL");

    if (!mounted) {
        tft.fillScreen(COL_BG);
        Display::drawHeader("microSD");
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextDatum(TL_DATUM);
        Menu::drawBackButton();

        int y = HDR_H + 8;
        y = Display::infoLine(y, "Detect:", det ? "1" : "0", COL_FG);
        Display::infoLine(y, "Mount:", "FAIL", COL_ERR);

        Menu::waitForBack();
        return;
    }

    String typeName = cardTypeName(SD.cardType());
    uint32_t sizeMB = (uint32_t)(SD.cardSize() / (1024 * 1024));
    Serial.printf("Card: %s  Size: %d MB\n", typeName.c_str(), sizeMB);

    bool rwOk;
    {
        const char *path = "/test_firmware.txt";
        const char *content = "LoRa-device RevB test";

        bool wrote = false;
        File wf = SD.open(path, FILE_WRITE);
        if (wf) {
            wrote = wf.print(content) == (int)strlen(content);
            wf.close();
        }

        String readBack;
        if (wrote) {
            File rf = SD.open(path, FILE_READ);
            if (rf) {
                readBack = rf.readString();
                rf.close();
            }
        }

        rwOk = wrote && readBack == content;
        SD.remove(path);
    }
    Serial.printf("R/W test: %s\n", rwOk ? "OK" : "FAIL");

    // List files in the root directory and offer to view/play known types
    // (.txt, .jpg/.jpeg, .mp3) - lets the user verify their own test media
    // placed on the card is actually readable.
    FileEntry files[MAX_FILES];
    int fileCount = 0;
    File root = SD.open("/");
    if (root) {
        File entry = root.openNextFile();
        while (entry && fileCount < MAX_FILES) {
            if (!entry.isDirectory()) {
                files[fileCount].name = String(entry.name());
                files[fileCount].size = entry.size();
                files[fileCount].kind = classify(files[fileCount].name);
                fileCount++;
            }
            entry.close();
            entry = root.openNextFile();
        }
        root.close();
    }
    Serial.printf("Files found: %d\n", fileCount);
    for (int i = 0; i < fileCount; i++) {
        Serial.printf("  [%d] %s  %d bytes\n", i, files[i].name.c_str(), files[i].size);
    }

    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpgOutput);

    Btn rowBtns[MAX_FILES];
    int statusY = 0;

    // Draws the file browser (header, info lines, file list). Re-run after
    // returning from a full-screen sub-view (text/image) so "Back" lands
    // here instead of falling through to the main menu.
    auto drawBrowser = [&]() {
        tft.fillScreen(COL_BG);
        Display::drawHeader("microSD");
        tft.setFreeFont(&FreeSans9pt7b);
        tft.setTextDatum(TL_DATUM);
        Menu::drawBackButton();

        int y = HDR_H + 8;
        y = Display::infoLine(y, "Detect:", det ? "1" : "0", COL_FG);
        y = Display::infoLine(y, "Mount:", "OK", COL_OK);
        y = Display::infoLine(y, "Type:", typeName, COL_FG);
        y = Display::infoLine(y, "Size:", String(sizeMB) + " MB", COL_FG);
        y = Display::infoLine(y, "R/W test:", rwOk ? "OK" : "FAIL", rwOk ? COL_OK : COL_ERR);

        y += 4;
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString("Files:", 8, y);
        y += ROW_H;

        const int listTop = y;
        for (int i = 0; i < fileCount; i++) {
            int rowY = listTop + i * ROW_H;
            rowBtns[i] = {0, rowY - 2, SCREEN_W, ROW_H, ""};

            tft.setTextColor(COL_FG, COL_BG);
            String label = files[i].name;
            if (label.length() > 13) label = label.substring(0, 10) + "...";
            tft.drawString(label, 8, rowY);

            tft.setTextColor(COL_DIM, COL_BG);
            tft.drawString(String(files[i].size) + "B", 92, rowY);

            // Files we know how to open get a ">" hint - the whole row is
            // the tap target and auto-dispatches based on file type.
            if (files[i].kind != Kind::Other) {
                tft.setTextColor(COL_ACCENT, COL_BG);
                tft.drawString(">", SCREEN_W - 20, rowY);
            }
        }

        statusY = listTop + fileCount * ROW_H + 8;
    };

    drawBrowser();

    // Menu::checkBack() also consumes touch events internally, so it can't
    // be used alongside our own getTouch() call below (it would eat taps on
    // file rows before we ever see them) - replicate it manually instead.
    while (true) {
        Volume::poll();

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            for (int i = 0; i < fileCount; i++) {
                if (files[i].kind == Kind::Other || !inBtn(rowBtns[i], tx, ty)) {
                    continue;
                }
                switch (files[i].kind) {
                    case Kind::Text:
                        viewText(files[i]);
                        drawBrowser();
                        break;
                    case Kind::Image:
                        viewImage(files[i]);
                        drawBrowser();
                        break;
                    case Kind::Audio: playAudio(files[i], statusY); break;
                    default: break;
                }
                break;
            }
        }
        delay(20);
    }

    SD.end();
}
