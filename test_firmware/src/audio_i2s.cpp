#include "audio_i2s.h"
#include "pins.h"

namespace AudioI2S {
    namespace {
        bool install(i2s_mode_t dir, i2s_bits_per_sample_t bits, uint32_t sampleRate, bool tx) {
            i2s_config_t cfg = {
                .mode = (i2s_mode_t)(I2S_MODE_MASTER | dir),
                .sample_rate = sampleRate,
                .bits_per_sample = bits,
                .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                // TX (MAX98357A) wants STAND_MSB (no 1-BCLK delay, matches
                // the old confirmed-working ESP-IDF audio.c). RX (ICS-43432)
                // is a true I2S-Philips device and needs the 1-BCLK delay of
                // STAND_I2S - this is also what the arduino-esp32 I2SClass
                // always used, which is the basis for the right-discard/
                // left-keep readMicSample() pattern.
                .communication_format = tx ? I2S_COMM_FORMAT_STAND_MSB : I2S_COMM_FORMAT_STAND_I2S,
                .intr_alloc_flags = 0,
                .dma_buf_count = 4,
                .dma_buf_len = 256,
                .use_apll = false,
                .tx_desc_auto_clear = true,
                .fixed_mclk = 0,
            };

            if (i2s_driver_install(PORT, &cfg, 0, NULL) != ESP_OK) {
                return false;
            }

            i2s_pin_config_t pins = {
                .bck_io_num = PIN_I2S_BCLK,
                .ws_io_num = PIN_I2S_WS,
                .data_out_num = tx ? PIN_I2S_DOUT : I2S_PIN_NO_CHANGE,
                .data_in_num = tx ? I2S_PIN_NO_CHANGE : PIN_I2S_DIN,
            };

            if (i2s_set_pin(PORT, &pins) != ESP_OK) {
                i2s_driver_uninstall(PORT);
                return false;
            }
            return true;
        }
    }

    bool beginTx(uint32_t sampleRate) {
        return install(I2S_MODE_TX, I2S_BITS_PER_SAMPLE_16BIT, sampleRate, true);
    }

    bool beginRx(uint32_t sampleRate) {
        return install(I2S_MODE_RX, I2S_BITS_PER_SAMPLE_32BIT, sampleRate, false);
    }

    void end() {
        i2s_driver_uninstall(PORT);
    }
}
