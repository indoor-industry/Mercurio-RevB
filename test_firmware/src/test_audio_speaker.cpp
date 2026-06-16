#include "test_audio_speaker.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "board.h"
#include "audio_i2s.h"
#include "volume.h"
#include <math.h>

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

    struct ToneSpec {
        float freqHz; // < 0 = sweep from SWEEP_LOW to SWEEP_HIGH
        int durationMs;
        const char *label;
    };

    const float SWEEP_LOW = 200.0f;
    const float SWEEP_HIGH = 4000.0f;

    const ToneSpec TONES[] = {
        {440.0f,  1000, "440 Hz Tone"},
        {1000.0f, 1000, "1 kHz Tone"},
        {100.0f,  1000, "100 Hz (Bass)"},
        {2000.0f, 1000, "2 kHz Tone"},
        {-1.0f,   2000, "Sweep 200Hz-4kHz"},
    };
    const int NUM_TONES = sizeof(TONES) / sizeof(TONES[0]);

    // 28000 (~85% full scale) sounded harsh/clipped on the amp - back off
    // to a level with more headroom.
    const float AMPLITUDE = 20000.0f;

    void playTone(const ToneSpec &tone, uint32_t sampleRate) {
        mcp.writePin(MCP_BIT_AUDIO_EN, true);
        delay(5);

        uint32_t totalSamples = sampleRate * tone.durationMs / 1000;

        // Linear fade in/out (~10ms) so each tone starts/ends at zero
        // crossing instead of jumping straight to full amplitude, which
        // caused an audible click/pop at the start and end of every tone.
        uint32_t rampSamples = sampleRate / 100;
        if (rampSamples > totalSamples / 2) {
            rampSamples = totalSamples / 2;
        }

        auto envelope = [&](uint32_t i) -> float {
            if (i < rampSamples) {
                return (float)i / (float)rampSamples;
            }
            if (i >= totalSamples - rampSamples) {
                return (float)(totalSamples - 1 - i) / (float)rampSamples;
            }
            return 1.0f;
        };

        float vol = Volume::gain();

        if (tone.freqHz < 0) {
            float phase = 0;
            for (uint32_t i = 0; i < totalSamples; i++) {
                float f = SWEEP_LOW + (SWEEP_HIGH - SWEEP_LOW) * (float)i / totalSamples;
                phase += 2.0f * PI * f / sampleRate;
                int16_t sample = (int16_t)(sinf(phase) * AMPLITUDE * vol * envelope(i));
                size_t written;
                i2s_write(AudioI2S::PORT, &sample, sizeof(sample), &written, portMAX_DELAY);
            }
        } else {
            for (uint32_t i = 0; i < totalSamples; i++) {
                float t = (float)i / sampleRate;
                int16_t sample = (int16_t)(sinf(2.0f * PI * tone.freqHz * t) * AMPLITUDE * vol * envelope(i));
                size_t written;
                i2s_write(AudioI2S::PORT, &sample, sizeof(sample), &written, portMAX_DELAY);
            }
        }

        mcp.writePin(MCP_BIT_AUDIO_EN, false);
    }
}

void testAudioSpeaker() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("Speaker");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    Serial.println("--- Speaker ---");
    const uint32_t sampleRate = 16000;
    bool ok = AudioI2S::beginTx(sampleRate);
    Serial.printf("I2S init: %s\n", ok ? "OK" : "FAIL");

    int y = HDR_H + 8;
    y = Display::infoLine(y, "I2S init:", ok ? "OK" : "FAIL", ok ? COL_OK : COL_ERR);

    const int btnH = 32, btnGap = 4;
    Btn toneBtns[NUM_TONES];
    int by = y + 8;
    for (int i = 0; i < NUM_TONES; i++) {
        toneBtns[i] = {8, by, SCREEN_W - 16, btnH, TONES[i].label};
        if (ok) {
            drawButton(toneBtns[i]);
        }
        by += btnH + btnGap;
    }

    const int statusY = by + 10;

    while (true) {
        Volume::poll();

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            if (ok) {
                for (int i = 0; i < NUM_TONES; i++) {
                    if (!inBtn(toneBtns[i], tx, ty)) {
                        continue;
                    }

                    Serial.printf("Playing: %s\n", TONES[i].label);
                    tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
                    tft.setTextColor(COL_WARN, COL_BG);
                    tft.drawString("Playing...", 8, statusY);

                    playTone(TONES[i], sampleRate);

                    tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
                    tft.setTextColor(COL_OK, COL_BG);
                    tft.drawString("Done", 8, statusY);
                    Serial.println("Done");
                    break;
                }
            }
        }
        delay(20);
    }

    if (ok) {
        AudioI2S::end();
    }
}
