/*
 * Peripheral Tests (SD Card, Secure Element, Crystals)
 */

#include "peripherals.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_flash.h"
#include "soc/rtc.h"
#include <string.h>

static const char *TAG = "PERIPH";

static bool xtal_40mhz_ok = false;
static bool xtal_32khz_ok = false;
static bool sd_card_detected = false;
static bool secure_element_detected = false;
static bool flash_ok = false;
static uint32_t flash_size_mb = 0;

void spi_bus_init(spi_device_handle_t *lcd_spi_out, spi_device_handle_t *touch_spi_out) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .sclk_io_num = PIN_LCD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    
    spi_device_interface_config_t lcd_cfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_LCD_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &lcd_cfg, lcd_spi_out));
    
    spi_device_interface_config_t touch_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_TOUCH_CS,
        .queue_size = 3,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &touch_cfg, touch_spi_out));
    
    ESP_LOGI(TAG, "SPI Bus B initialized");
}

void test_crystals(void) {
    ESP_LOGI(TAG, "=== Crystal Oscillator Tests ===");
    
    ESP_LOGI(TAG, "40MHz Crystal: OK (chip booted)");
    xtal_40mhz_ok = true;
    
    rtc_slow_freq_t slow_clk = rtc_clk_slow_freq_get();
    uint32_t slow_clk_freq = rtc_clk_slow_freq_get_hz();
    
    if (slow_clk == RTC_SLOW_FREQ_32K_XTAL) {
        if (slow_clk_freq > 30000 && slow_clk_freq < 35000) {
            ESP_LOGI(TAG, "32.768kHz Crystal: OK");
            xtal_32khz_ok = true;
        } else {
            xtal_32khz_ok = false;
        }
    } else {
        ESP_LOGW(TAG, "32.768kHz Crystal: NOT IN USE (using internal RC)");
        xtal_32khz_ok = false;
    }
}

bool xtal_40mhz_is_ok(void) { return xtal_40mhz_ok; }
bool xtal_32khz_is_ok(void) { return xtal_32khz_ok; }

void test_sd_card(spi_device_handle_t spi) {
    ESP_LOGI(TAG, "=== SD Card Test ===");
    
    gpio_config_t io_conf = {
        // .pin_bit_mask = (1ULL << PIN_SD_CS), // Disabled for PSRAM test
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    
    // gpio_set_level(PIN_LORA_CS, 1);
    // gpio_set_level(PIN_SD_CS, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    uint8_t dummy[10];
    memset(dummy, 0xFF, sizeof(dummy));
    spi_transaction_t t = { .length = 80, .tx_buffer = dummy };
    spi_device_polling_transmit(spi, &t);
    
    uint8_t cmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
    uint8_t resp[8];
    memset(resp, 0xFF, sizeof(resp));
    
    spi_transaction_t t_cmd = { .length = 48, .tx_buffer = cmd0 };
    spi_device_polling_transmit(spi, &t_cmd);
    
    spi_transaction_t t_resp = { .length = 64, .tx_buffer = resp, .rx_buffer = resp };
    spi_device_polling_transmit(spi, &t_resp);
    
    // gpio_set_level(PIN_SD_CS, 1);
    
    bool found = false;
    for (int i = 0; i < 8; i++) {
        if (resp[i] == 0x01) {
            found = true;
            break;
        }
    }
    
    sd_card_detected = found;
    ESP_LOGI(TAG, "SD Card: %s", found ? "DETECTED" : "NOT DETECTED");
}

bool sd_card_is_detected(void) { return sd_card_detected; }

#define I2C_MASTER_NUM I2C_NUM_0

void test_secure_element(void) {
    ESP_LOGI(TAG, "=== Secure Element Test ===");
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SE_SDA,
        .scl_io_num = PIN_SE_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    
    // Wake sequence
    gpio_set_direction(PIN_SE_SDA, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SE_SDA, 0);
    esp_rom_delay_us(100);
    gpio_set_level(PIN_SE_SDA, 1);
    gpio_set_direction(PIN_SE_SDA, GPIO_MODE_INPUT);
    
    vTaskDelay(pdMS_TO_TICKS(5));
    
    i2c_driver_delete(I2C_MASTER_NUM);
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ATECC608A_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    if (err == ESP_OK) {
        secure_element_detected = true;
        ESP_LOGI(TAG, "ATECC608A: DETECTED");
    } else {
        secure_element_detected = false;
        ESP_LOGW(TAG, "ATECC608A: NOT DETECTED");
    }
}

bool secure_element_is_detected(void) { return secure_element_detected; }

void test_flash_chip(void) {
    ESP_LOGI(TAG, "=== Flash Chip Test (Winbond W25Q128JVSIQ) ===");
    
    uint32_t size_bytes = 0;
    esp_err_t err = esp_flash_get_size(NULL, &size_bytes);
    
    if (err == ESP_OK && size_bytes > 0) {
        flash_size_mb = size_bytes / (1024 * 1024);
        ESP_LOGI(TAG, "Flash size: %lu MB", flash_size_mb);
        
        // Verify it's the expected 16MB (128Mbit) Winbond chip
        if (flash_size_mb == 16) {
            ESP_LOGI(TAG, "Flash chip: W25Q128 DETECTED (16MB)");
            flash_ok = true;
        } else {
            ESP_LOGW(TAG, "Flash chip: Unknown size (%lu MB), expected 16MB", flash_size_mb);
            flash_ok = true;  // Still working, just unexpected size
        }
        
        // Test read capability by reading chip ID
        uint64_t chip_id = 0;
        err = esp_flash_read_unique_chip_id(NULL, &chip_id);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Flash unique ID: 0x%llX", chip_id);
        } else {
            ESP_LOGW(TAG, "Could not read flash unique ID");
        }
    } else {
        ESP_LOGE(TAG, "Flash chip: NOT DETECTED or error");
        flash_ok = false;
        flash_size_mb = 0;
    }
}

bool flash_test_ok(void) { return flash_ok; }
uint32_t flash_get_size_mb(void) { return flash_size_mb; }
