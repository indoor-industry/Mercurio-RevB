/*
 * GPS Driver (L70-R via UART)
 */

#include "gps.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "GPS";

#define GPS_UART_NUM UART_NUM_1

static gps_position_t gps_position = {0};

static double parse_nmea_coord(const char *str, char dir) {
    if (!str || strlen(str) < 4) return 0.0;
    
    double raw = atof(str);
    int degrees = (int)(raw / 100);
    double minutes = raw - (degrees * 100);
    double result = degrees + (minutes / 60.0);
    
    if (dir == 'S' || dir == 'W') result = -result;
    return result;
}

static bool parse_gga(const char *sentence) {
    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *fields[15] = {0};
    int field_count = 0;
    char *p = buf;
    fields[field_count++] = p;
    
    while (*p && field_count < 15) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            p++;
            if (field_count < 15) fields[field_count++] = p;
        } else {
            p++;
        }
    }
    
    if (field_count < 8) return false;
    
    char lat_dir = 'N', lon_dir = 'E';
    
    if (fields[1] && strlen(fields[1]) > 0)
        strncpy(gps_position.time_str, fields[1], sizeof(gps_position.time_str) - 1);
    
    if (fields[3] && fields[3][0]) lat_dir = fields[3][0];
    if (fields[5] && fields[5][0]) lon_dir = fields[5][0];
    if (fields[6] && fields[6][0]) gps_position.fix_quality = fields[6][0];
    if (fields[7]) gps_position.satellites = atoi(fields[7]);
    
    if (gps_position.fix_quality > '0' && fields[2] && strlen(fields[2]) > 0 
        && fields[4] && strlen(fields[4]) > 0) {
        gps_position.latitude = parse_nmea_coord(fields[2], lat_dir);
        gps_position.longitude = parse_nmea_coord(fields[4], lon_dir);
        gps_position.valid = true;
        return true;
    }
    
    gps_position.valid = false;
    return false;
}

void gps_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_GPS_STANDBY) | (1ULL << PIN_GPS_RESET),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Resetting GPS module...");
    gpio_set_level(PIN_GPS_STANDBY, 1);
    gpio_set_level(PIN_GPS_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_GPS_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, PIN_GPS_TX, PIN_GPS_RX, -1, -1));
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, GPS_BUF_SIZE * 2, 0, 0, NULL, 0));
    
    ESP_LOGI(TAG, "GPS UART initialized");
}

int gps_read(char *buf, size_t max_len, uint32_t timeout_ms) {
    return uart_read_bytes(GPS_UART_NUM, buf, max_len - 1, pdMS_TO_TICKS(timeout_ms));
}

void gps_process_buffer(const char *buf) {
    const char *gga = strstr(buf, "$GPGGA");
    if (!gga) gga = strstr(buf, "$GNGGA");
    
    if (gga) {
        char sentence[128];
        int i = 0;
        while (gga[i] && gga[i] != '\r' && gga[i] != '\n' && i < 127) {
            sentence[i] = gga[i];
            i++;
        }
        sentence[i] = '\0';
        
        if (parse_gga(sentence)) {
            ESP_LOGI(TAG, "GPS FIX: %.6f, %.6f (%d sats)", 
                     gps_position.latitude, gps_position.longitude, gps_position.satellites);
        }
    }
}

const gps_position_t* gps_get_position(void) {
    return &gps_position;
}
