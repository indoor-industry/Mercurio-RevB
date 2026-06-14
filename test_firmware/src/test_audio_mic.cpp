#include "test_audio_mic.h"
#include "display.h"
#include "menu.h"
#include "touch.h"
#include "config.h"
#include "pins.h"
#include "board.h"
#include "audio_i2s.h"
#include "volume.h"
#include <SD.h>

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

    const uint32_t SAMPLE_RATE = 16000;
    const uint32_t RECORD_SECONDS = 5;
    const uint32_t RECORD_SAMPLES = SAMPLE_RATE * RECORD_SECONDS;
    const char *WAV_PATH = "/recording.wav";

    struct __attribute__((packed)) WavHeader {
        char riffId[4] = {'R', 'I', 'F', 'F'};
        uint32_t riffSize;
        char waveId[4] = {'W', 'A', 'V', 'E'};
        char fmtId[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 1;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char dataId[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize;
    };

    // Writes the recorded buffer to the SD card as a 16-bit PCM mono WAV
    // file so it can be pulled off and checked on a PC.
    bool saveWav(const int16_t *buf, uint32_t numSamples) {
        File f = SD.open(WAV_PATH, FILE_WRITE);
        if (!f) {
            return false;
        }

        uint32_t dataSize = numSamples * sizeof(int16_t);
        WavHeader hdr;
        hdr.riffSize = 36 + dataSize;
        hdr.sampleRate = SAMPLE_RATE;
        hdr.blockAlign = hdr.numChannels * hdr.bitsPerSample / 8;
        hdr.byteRate = hdr.sampleRate * hdr.blockAlign;
        hdr.dataSize = dataSize;

        size_t written = f.write((const uint8_t *)&hdr, sizeof(hdr));
        written += f.write((const uint8_t *)buf, dataSize);
        f.close();

        return written == sizeof(hdr) + dataSize;
    }

    // Reads `n` mic samples via the I2S DMA ring buffer in batches. See
    // audio_i2s.h for the bit-alignment rationale behind the >>15 shift.
    //
    // Previously each sample was read with its own i2s_read() call (80000
    // calls for a 5s recording). The per-call driver overhead (mutex +
    // ring-buffer bookkeeping) is enough that the read loop can't keep up
    // with the 16kHz DMA fill rate, so the ring buffer occasionally
    // overflows and samples get dropped - audible as clicks/crackle on top
    // of the recorded audio, independent of the filtering/gain chain below.
    // Reading many samples per call amortizes that overhead.
    void readMicSamples(int16_t *out, size_t n) {
        int32_t raw[256];
        size_t i = 0;
        while (i < n) {
            size_t chunk = n - i;
            if (chunk > 256) chunk = 256;
            size_t br;
            i2s_read(AudioI2S::PORT, raw, chunk * sizeof(int32_t), &br, portMAX_DELAY);
            size_t got = br / sizeof(int32_t);
            for (size_t j = 0; j < got; j++) {
                out[i + j] = (int16_t)(raw[j] >> 15);
            }
            i += got;
        }
    }

    // Records RECORD_SAMPLES of left-channel mic audio, saves it to the SD
    // card, then plays it back through the amp.
    void recordAndPlay(int16_t *buf, int statusY, bool sdOk) {
        tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
        tft.setTextColor(COL_WARN, COL_BG);
        tft.drawString("Recording...", 8, statusY);

        readMicSamples(buf, RECORD_SAMPLES);

        // The ICS-43432 has a small DC bias and the captured level is far
        // below the amp's full-scale drive. Remove the DC offset and
        // remember the peak so playback can be normalized to the
        // recording's own dynamic range instead of using a fixed gain
        // that either clips loud sounds or barely moves quiet ones.
        int64_t sum = 0;
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            sum += buf[i];
        }
        int16_t dcOffset = (int16_t)(sum / (int64_t)RECORD_SAMPLES);
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            buf[i] = (int16_t)((int32_t)buf[i] - dcOffset);
        }

        // The fixed dcOffset above only corrects a single average bias - any
        // slow drift over the recording (low-frequency rumble, handling
        // noise, residual mic self-bias wander) leaks through as a "boomy"/
        // muddy quality. A one-pole high-pass (~100Hz cutoff, below any
        // voice fundamental) removes that dynamically.
        const float HPF_ALPHA = 0.962f;
        float hpfPrevIn = 0.0f, hpfPrevOut = 0.0f;
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            float in = (float)buf[i];
            float out = HPF_ALPHA * (hpfPrevOut + in - hpfPrevIn);
            hpfPrevIn = in;
            hpfPrevOut = out;
            buf[i] = (int16_t)out;
        }

        // The mic's high-frequency self-noise gets amplified right along
        // with the voice by the peak-based gain below, which is what made
        // playback sound "crispy"/hissy. A one-pole low-pass (~3.5kHz
        // cutoff at 16kHz) knocks that down while leaving speech intact.
        // Together with the high-pass above, this forms a ~100Hz-3.5kHz
        // voice band.
        const float LPF_ALPHA = 0.75f;
        float lpf = 0.0f;
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            lpf += LPF_ALPHA * ((float)buf[i] - lpf);
            buf[i] = (int16_t)lpf;
        }

        int32_t peak = 1;
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            int32_t a = abs((int32_t)buf[i]);
            if (a > peak) peak = a;
        }

        // Normalize to ~90% of full scale based on the recording's own
        // peak (capped) so the recording is as loud as possible without the
        // hard-clipping distortion a fixed gain caused on louder sounds.
        // Applied to buf in place *before* saving so the WAV file on the SD
        // card is at the same level as the direct playback below - it was
        // previously saved pre-gain, which is why files played back from
        // the SD browser sounded much quieter than the live playback.
        const float maxGain = 32.0f;
        float autoGain = (32767.0f * 0.9f) / (float)peak;
        if (autoGain > maxGain) autoGain = maxGain;
        if (autoGain < 1.0f) autoGain = 1.0f;
        for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
            // Triangular-PDF dither (+-1 LSB) decorrelates the rounding
            // error from the signal - without it, the final truncation to
            // int16 adds a faint but consistent "grain" that gets more
            // audible the harder autoGain pushes a quiet recording.
            float dither = ((float)random(1000) - (float)random(1000)) / 1000.0f;
            int32_t s = (int32_t)((float)buf[i] * autoGain + dither);
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            buf[i] = (int16_t)s;
        }

        if (sdOk) {
            tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
            tft.setTextColor(COL_WARN, COL_BG);
            tft.drawString("Saving to SD...", 8, statusY);

            bool saved = saveWav(buf, RECORD_SAMPLES);
            tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
            tft.setTextColor(saved ? COL_OK : COL_ERR, COL_BG);
            tft.drawString(saved ? "Saved " + String(WAV_PATH) : "SD save FAILED", 8, statusY);
            delay(600);
        }

        tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
        tft.setTextColor(COL_WARN, COL_BG);
        tft.drawString("Playing...", 8, statusY);

        AudioI2S::end();
        bool txOk = AudioI2S::beginTx(SAMPLE_RATE);
        if (txOk) {
            mcp.writePin(MCP_BIT_AUDIO_EN, true);
            delay(5);

            float vol = Volume::gain();
            for (uint32_t i = 0; i < RECORD_SAMPLES; i++) {
                int16_t sample = (int16_t)((float)buf[i] * vol);
                size_t written;
                i2s_write(AudioI2S::PORT, &sample, sizeof(sample), &written, portMAX_DELAY);
            }

            mcp.writePin(MCP_BIT_AUDIO_EN, false);
            AudioI2S::end();
        }

        // Switch back to RX so the live meter keeps working.
        AudioI2S::beginRx(SAMPLE_RATE);

        tft.fillRect(0, statusY, SCREEN_W, 22, COL_BG);
        tft.setTextColor(COL_OK, COL_BG);
        tft.drawString("Done", 8, statusY);
    }
}

void testAudioMic() {
    tft.fillScreen(COL_BG);
    Display::drawHeader("Microphone");
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    Menu::drawBackButton();

    // ICS-43432 is wired to the I2S left channel (L/R tied low).
    bool ok = AudioI2S::beginRx(SAMPLE_RATE);

    // SD shares the display's SPI bus - mounted here so a recording can be
    // saved to /recording.wav for off-board verification.
    bool sdOk = SD.begin(PIN_SD_CS, tft.getSPIinstance(), SPI_FREQ_SD);

    int y = HDR_H + 8;
    y = Display::infoLine(y, "I2S init:", ok ? "OK" : "FAIL", ok ? COL_OK : COL_ERR);
    y = Display::infoLine(y, "SD card:", sdOk ? "OK" : "FAIL", sdOk ? COL_OK : COL_ERR);

    const int meterX = 8;
    const int meterY = y + 16;
    const int meterW = SCREEN_W - 16;
    const int meterH = 24;
    tft.drawRect(meterX, meterY, meterW, meterH, COL_DIM);

    const int peakY = meterY + meterH + 16;

    Btn recordBtn = {8, peakY + 28, SCREEN_W - 16, 44, "Record & Play (5s)"};
    if (ok) {
        drawButton(recordBtn);
    }
    const int statusY = recordBtn.y + recordBtn.h + 16;

    int16_t *recBuf = nullptr;
    if (ok) {
        recBuf = (int16_t *)ps_malloc(RECORD_SAMPLES * sizeof(int16_t));
        if (!recBuf) {
            tft.setTextColor(COL_ERR, COL_BG);
            tft.drawString("Record buffer alloc failed", 8, statusY);
        }
    }

    // Peak-hold with slow decay so the bar is readable instead of
    // jumping every ~100ms.
    int32_t heldPeak = 0;
    const int32_t decayPerUpdate = 600;

    while (true) {
        Volume::poll();

        int16_t tx, ty;
        if (Touch::getTouch(tx, ty)) {
            if (Menu::backButtonHit(tx, ty)) {
                break;
            }
            if (ok && recBuf && inBtn(recordBtn, tx, ty)) {
                recordAndPlay(recBuf, statusY, sdOk);
            }
        }

        if (ok) {
            // 1600 samples @ 16kHz = 100ms per update.
            static int16_t meterBuf[1600];
            readMicSamples(meterBuf, 1600);
            int32_t peak = 0;
            for (int i = 0; i < 1600; i++) {
                int32_t a = abs((int32_t)meterBuf[i]);
                if (a > peak) peak = a;
            }

            if (peak > heldPeak) {
                heldPeak = peak;
            } else if (heldPeak > decayPerUpdate) {
                heldPeak -= decayPerUpdate;
            } else {
                heldPeak = 0;
            }

            int barW = map(heldPeak, 0, 32767, 0, meterW - 2);
            tft.fillRect(meterX + 1, meterY + 1, meterW - 2, meterH - 2, COL_BG);
            tft.fillRect(meterX + 1, meterY + 1, barW, meterH - 2, COL_OK);

            tft.fillRect(0, peakY, SCREEN_W, 20, COL_BG);
            tft.setTextColor(COL_DIM, COL_BG);
            tft.drawString("Peak:", 8, peakY);
            tft.setTextColor(COL_FG, COL_BG);
            tft.drawString(String(heldPeak), 90, peakY);
        } else {
            delay(20);
        }
    }

    if (recBuf) {
        free(recBuf);
    }

    if (ok) {
        AudioI2S::end();
    }

    if (sdOk) {
        SD.end();
    }
}
