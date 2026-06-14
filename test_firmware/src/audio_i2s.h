#pragma once

#include <driver/i2s.h>

// Shared I2S0 setup for the speaker (MAX98357A) and mic (ICS-43432). The two
// directions need different communication formats:
// - TX: I2S_COMM_FORMAT_STAND_MSB (left-justified, no 1-BCLK delay), matching
//   the old ESP-IDF firmware's working audio.c. This is what fixed the
//   speaker's low volume.
// - RX: I2S_COMM_FORMAT_STAND_I2S (Philips, 1-BCLK delay), which is what the
//   ICS-43432 actually expects and what the arduino-esp32 I2SClass always
//   used (it forces STAND_I2S regardless of mode). Using STAND_MSB for RX
//   misaligned every mic sample by a bit and produced noise/silence.
namespace AudioI2S {
    const i2s_port_t PORT = I2S_NUM_0;

    // TX-only, 16-bit samples, mono (ONLY_LEFT - one sample per
    // i2s_write call). Used for the speaker tone and recorded playback.
    bool beginTx(uint32_t sampleRate);

    // RX-only, 32-bit samples, mono (ONLY_LEFT - one sample per i2s_read
    // call). The ICS-43432 (L/R tied low) drives the left slot of each WS
    // frame. With STAND_I2S's 1-BCLK delay, the 24-bit sample lands in bits
    // 30:7 of the 32-bit word (bit 31 always 0, bit 30 = sign). Shift right
    // by 15 (not 16!) so bit 30 becomes the int16 sign bit - shifting by 16
    // puts the always-0 bit 31 there instead, making every sample
    // non-negative and wrapping at each zero-crossing.
    bool beginRx(uint32_t sampleRate);

    void end();
}
