#pragma once

#include <Arduino.h>

// Phone-style volume control using the two pushbuttons (MCP23017 SW1/SW2):
// SW1 = up, SW2 = down, 0-10 steps persisted in NVS. poll() should be
// called periodically from any screen's main loop - it edge-detects the
// buttons and briefly shows a tick-mark overlay in the header when the
// level changes.
namespace Volume {
    void begin();
    void poll();

    // Output gain, 0.0 (muted) - 1.0 (full / unchanged from the previous
    // fixed-gain behavior). Multiply into sample amplitudes or pass to
    // AudioOutputI2S::SetGain() so the level applies uniformly across mic
    // playback, SD audio playback, and test tones.
    float gain();
}
