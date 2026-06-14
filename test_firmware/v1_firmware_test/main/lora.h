/*
 * LoRa Driver (SX1276/SX1268)
 */

#ifndef LORA_H
#define LORA_H

#include <stdbool.h>
#include <stdint.h>

#define LORA_TEST_FREQ_HZ 868000000

void lora_init(void);
bool lora_is_detected(void);
uint8_t lora_get_chip_mode(void);
bool lora_is_tx_active(void);
void lora_start_tx_test(void);
void lora_stop_tx_test(void);

#endif // LORA_H
