/*
 * LoRa Driver (SX1276/SX1268)
 */

#include "lora.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LORA";

static spi_device_handle_t lora_spi = NULL;
static bool lora_detected = false;
static uint8_t lora_chip_mode = 0xFF;
static bool lora_tx_active = false;

// SX1276 Registers
#define SX1276_REG_OPMODE       0x01
#define SX1276_REG_FRF_MSB      0x06
#define SX1276_REG_FRF_MID      0x07
#define SX1276_REG_FRF_LSB      0x08
#define SX1276_REG_PA_CONFIG    0x09
#define SX1276_REG_OCP          0x0B
#define SX1276_REG_PA_DAC       0x4D
#define SX1276_REG_FIFO         0x00
#define SX1276_REG_PAYLOAD_LEN  0x22

// SX1276 Modes/Commands
#define SX1276_MODE_SLEEP       0x00
#define SX1276_MODE_STANDBY     0x01
#define SX1276_MODE_TX          0x03
#define SX1276_LONG_RANGE_MODE  0x80

// SX1268 Commands
#define SX1268_CMD_GET_STATUS   0xC0
#define SX1268_CMD_SET_STANDBY  0x80
#define SX1268_STANDBY_RC       0x00

static TaskHandle_t lora_tx_task_handle = NULL;

static void lora_write_reg_sx1276(uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {reg | 0x80, value};
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    spi_device_polling_transmit(lora_spi, &t);
}

static uint8_t lora_read_reg_sx127x(uint8_t reg) {
    uint8_t tx[2] = {reg & 0x7F, 0x00};
    uint8_t rx[2] = {0, 0};
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(lora_spi, &t);
    return rx[1];
}

static void sx1276_set_standby(void) {
    lora_write_reg_sx1276(SX1276_REG_OPMODE, SX1276_LONG_RANGE_MODE | SX1276_MODE_STANDBY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void sx1276_set_frequency(uint32_t freq_hz) {
    uint32_t frf = (uint32_t)((uint64_t)freq_hz * 524288ULL / 32000000ULL);
    lora_write_reg_sx1276(SX1276_REG_FRF_MSB, (frf >> 16) & 0xFF);
    lora_write_reg_sx1276(SX1276_REG_FRF_MID, (frf >> 8) & 0xFF);
    lora_write_reg_sx1276(SX1276_REG_FRF_LSB, frf & 0xFF);
}

static void sx1276_set_tx_power(int8_t power_dbm) {
    uint8_t pa_config;
    if (power_dbm > 17) {
        lora_write_reg_sx1276(SX1276_REG_PA_DAC, 0x87);
        pa_config = 0x8F;
    } else {
        lora_write_reg_sx1276(SX1276_REG_PA_DAC, 0x84);
        int8_t output_power = power_dbm - 2;
        if (output_power < 0) output_power = 0;
        if (output_power > 15) output_power = 15;
        pa_config = 0x80 | output_power;
    }
    lora_write_reg_sx1276(SX1276_REG_PA_CONFIG, pa_config);
    lora_write_reg_sx1276(SX1276_REG_OCP, 0x2B);
}

static void sx1276_set_tx_mode(void) {
    lora_write_reg_sx1276(SX1276_REG_OPMODE, SX1276_LONG_RANGE_MODE | SX1276_MODE_TX);
}

static uint8_t sx1268_get_status(void) {
    uint8_t tx[2] = {SX1268_CMD_GET_STATUS, 0x00};
    uint8_t rx[2] = {0, 0};
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(lora_spi, &t);
    return rx[1];
}

static void sx1268_set_standby(uint8_t mode) {
    uint8_t tx[2] = {SX1268_CMD_SET_STANDBY, mode};
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    spi_device_polling_transmit(lora_spi, &t);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lora_spi_bus_init(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_LORA_MOSI,
        .miso_io_num = PIN_LORA_MISO,
        .sclk_io_num = PIN_LORA_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    
    spi_device_interface_config_t lora_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_LORA_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &lora_cfg, &lora_spi));
}

void lora_init(void) {
    // Deselect SD card
    // gpio_config_t sd_cs_conf = { .pin_bit_mask = (1ULL << PIN_SD_CS), .mode = GPIO_MODE_OUTPUT };
    // gpio_config(&sd_cs_conf);
    // gpio_set_level(PIN_SD_CS, 1);
    
    // // Configure reset pin
    // gpio_config_t io_conf = { .pin_bit_mask = (1ULL << PIN_LORA_RST), .mode = GPIO_MODE_OUTPUT };
    // gpio_config(&io_conf);
    // 
    // // Configure DIO pins
    // io_conf.pin_bit_mask = (1ULL << PIN_LORA_DIO0) | (1ULL << PIN_LORA_DIO1);
    // io_conf.mode = GPIO_MODE_INPUT;
    // io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    // gpio_config(&io_conf);
    
    // Reset sequence
    ESP_LOGI(TAG, "Resetting LoRa module...");
    // gpio_set_level(PIN_LORA_RST, 0);
    // vTaskDelay(pdMS_TO_TICKS(10));
    // gpio_set_level(PIN_LORA_RST, 1);
    // vTaskDelay(pdMS_TO_TICKS(100));
    
    lora_spi_bus_init();
    
    // Try SX1276 first
    ESP_LOGI(TAG, "Trying SX1276 protocol...");
    uint8_t ver = lora_read_reg_sx127x(0x42);
    vTaskDelay(pdMS_TO_TICKS(5));
    uint8_t ver2 = lora_read_reg_sx127x(0x42);
    
    if (ver == 0x12 && ver2 == 0x12) {
        lora_detected = true;
        lora_chip_mode = 0x12;
        ESP_LOGI(TAG, "SX1276 DETECTED!");
        sx1276_set_standby();
    } else {
        // Try SX1268
        ESP_LOGI(TAG, "Trying SX1268 protocol...");
        sx1268_set_standby(SX1268_STANDBY_RC);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        uint8_t status = sx1268_get_status();
        uint8_t chip_mode = (status >> 4) & 0x07;
        
        if (chip_mode >= 2 && chip_mode <= 6) {
            lora_detected = true;
            lora_chip_mode = chip_mode;
            ESP_LOGI(TAG, "SX1262/68 DETECTED!");
        } else {
            lora_detected = false;
            ESP_LOGE(TAG, "NO LORA MODULE DETECTED!");
        }
    }
}

bool lora_is_detected(void) { return lora_detected; }
uint8_t lora_get_chip_mode(void) { return lora_chip_mode; }
bool lora_is_tx_active(void) { return lora_tx_active; }

static void lora_tx_task(void *pv);

void lora_start_tx_test(void) {
    if (!lora_detected) return;
    ESP_LOGI(TAG, "Starting TX test @ %lu MHz", LORA_TEST_FREQ_HZ / 1000000);
    sx1276_set_standby();
    lora_write_reg_sx1276(SX1276_REG_OPMODE, SX1276_LONG_RANGE_MODE | SX1276_MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    sx1276_set_frequency(LORA_TEST_FREQ_HZ);
    sx1276_set_standby();
    sx1276_set_tx_power(17);
    lora_tx_active = true;
    if (lora_tx_task_handle == NULL) {
        xTaskCreate(lora_tx_task, "lora_tx_task", 2048, NULL, 5, &lora_tx_task_handle);
    }
}

void lora_stop_tx_test(void) {
    if (!lora_detected) return;
    sx1276_set_standby();
    lora_tx_active = false;
    if (lora_tx_task_handle) {
        vTaskDelete(lora_tx_task_handle);
        lora_tx_task_handle = NULL;
    }
    ESP_LOGI(TAG, "LoRa TX stopped");
}

static void lora_write_fifo(const uint8_t *data, uint8_t len) {
    uint8_t tx[1 + 255];
    tx[0] = SX1276_REG_FIFO | 0x80;
    memcpy(&tx[1], data, len);
    spi_transaction_t t = { .length = (1 + len) * 8, .tx_buffer = tx };
    spi_device_polling_transmit(lora_spi, &t);
}

static void lora_send_packet(const uint8_t *data, uint8_t len) {
    sx1276_set_standby();
    lora_write_reg_sx1276(SX1276_REG_FIFO, 0x00); // FIFO pointer to base
    lora_write_fifo(data, len);
    lora_write_reg_sx1276(SX1276_REG_PAYLOAD_LEN, len);
    sx1276_set_tx_mode();
}

static void lora_tx_task(void *pv) {
    const char msg[] = "test";
    while (lora_tx_active) {
        lora_send_packet((const uint8_t*)msg, sizeof(msg)-1);
        vTaskDelay(pdMS_TO_TICKS(200)); // Send every 200ms
    }
    vTaskDelete(NULL);
}
