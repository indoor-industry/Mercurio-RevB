/*
 * LED Driver
 */

#include "led.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LED";

void led_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "LED initialized on GPIO%d", PIN_LED);
}

void led_set(bool on) {
    gpio_set_level(PIN_LED, on ? 1 : 0);
}
