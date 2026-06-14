/*
 * Touch Controller Driver (XPT2046)
 */

#include "touch.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TOUCH";
static spi_device_handle_t touch_spi = NULL;

// Touch calibration values (from XPT2046 raw ADC readings)
// Note: X and Y channels are swapped relative to screen orientation
// Raw X channel (0xD0) controls screen Y
// Raw Y channel (0x90) controls screen X
#define TOUCH_RAW_X_MIN     280     // Top of screen
#define TOUCH_RAW_X_MAX     3840    // Bottom of screen
#define TOUCH_RAW_Y_MIN     300     // Left of screen
#define TOUCH_RAW_Y_MAX     3800    // Right of screen

// Map function similar to Arduino
static int32_t touch_map(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static uint16_t touch_read_channel(uint8_t cmd) {
    uint8_t tx[3] = {cmd, 0, 0};
    uint8_t rx[3] = {0};
    
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(touch_spi, &t);
    
    return ((rx[1] << 8) | rx[2]) >> 3;
}

bool touch_read(uint16_t *x, uint16_t *y) {
    if (gpio_get_level(PIN_TOUCH_IRQ) == 1) {
        return false;
    }
    
    uint16_t raw_x = touch_read_channel(0xD0);  // Controls screen Y
    uint16_t raw_y = touch_read_channel(0x90);  // Controls screen X
    
    // Map raw values to screen coordinates (channels are swapped)
    int32_t screen_x = touch_map(raw_y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, LCD_WIDTH);
    int32_t screen_y = touch_map(raw_x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, LCD_HEIGHT);
    
    // Clamp to screen bounds
    if (screen_x < 0) screen_x = 0;
    if (screen_x >= LCD_WIDTH) screen_x = LCD_WIDTH - 1;
    if (screen_y < 0) screen_y = 0;
    if (screen_y >= LCD_HEIGHT) screen_y = LCD_HEIGHT - 1;
    
    *x = (uint16_t)screen_x;
    *y = (uint16_t)screen_y;
    
    return true;
}

void touch_init(spi_device_handle_t spi_handle) {
    touch_spi = spi_handle;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_TOUCH_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Touch controller initialized");
}
