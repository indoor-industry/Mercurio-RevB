#pragma once

#include "esp_adc/adc_oneshot.h"
#include "shared_types.h"

/* Lazily-initialized ADC unit handle shared by the battery test and the
 * live battery-detail refresh, so repeated reads don't churn through
 * adc_oneshot_new_unit/del_unit. */
adc_oneshot_unit_handle_t bat_adc_get(void);

void test_battery(test_entry_t *t);
