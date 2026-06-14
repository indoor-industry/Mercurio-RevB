#include "test_calibration.h"
#include "calibration.h"
#include "menu.h"

void testRecalibrate() {
    Calibration::run();
    Menu::waitForBack();
}
