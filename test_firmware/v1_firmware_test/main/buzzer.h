/*
 * Buzzer Driver (PWM)
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

#endif // BUZZER_H
