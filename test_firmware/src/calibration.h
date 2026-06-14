#pragma once

namespace Calibration {
    // Loads stored calibration from NVS into the Touch module.
    // Returns true if a valid calibration was found and applied.
    bool load();

    // Runs the interactive 2-point calibration UI and persists the
    // result to NVS. Leaves the touch module calibrated on return.
    void run();
}
