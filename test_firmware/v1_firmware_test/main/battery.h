/*
 * Battery ADC Driver
 */

#ifndef BATTERY_H
#define BATTERY_H

void battery_adc_init(void);
int battery_read_mv(void);

#endif // BATTERY_H
