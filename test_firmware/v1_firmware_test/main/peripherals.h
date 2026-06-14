/*
 * Peripheral Tests (SD Card, Secure Element, Crystals)
 */

#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <stdbool.h>
#include "driver/spi_master.h"

// SPI Bus initialization
void spi_bus_init(spi_device_handle_t *lcd_spi_out, spi_device_handle_t *touch_spi_out);

// Crystal tests
void test_crystals(void);
bool xtal_40mhz_is_ok(void);
bool xtal_32khz_is_ok(void);

// SD Card test
void test_sd_card(spi_device_handle_t spi);
bool sd_card_is_detected(void);

// Secure Element test
void test_secure_element(void);
bool secure_element_is_detected(void);

// Flash chip test (Winbond W25Q128JVSIQ)
void test_flash_chip(void);
bool flash_test_ok(void);
uint32_t flash_get_size_mb(void);

#endif // PERIPHERALS_H
