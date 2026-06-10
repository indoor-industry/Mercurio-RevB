#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "shared_types.h"

/* LoRa test-TX frequency. Adjust to a frequency legal in your region —
 * this is only used for a brief on-air self-test transmission. */
#define LORA_TEST_FREQ_HZ 915000000.0

/* Bring up SPI-A (LoRa + touch share this bus) and add the SX1262 device. */
esp_err_t lora_bus_init(void);

/* Reset the radio, read its status register, and put it back to sleep. */
void test_lora(test_entry_t *t);

/* Configure the radio for LoRa and transmit a short test packet.
 * Returns true if the chip reports TxDone within the timeout. */
bool lora_send_test_packet(char *out, size_t out_sz);
