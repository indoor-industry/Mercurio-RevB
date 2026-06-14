/*
 * ESP32-S3 Custom Board Peripheral Test - Main
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "board_config.h"
#include "lcd.h"
#include "touch.h"
#include "led.h"
#include "buzzer.h"
#include "battery.h"
#include "gps.h"
#include "lora.h"
#include "wifi_test.h"
#include "bluetooth_test.h"
#include "peripherals.h"
#include "lcd.h"

static const char *TAG = "MAIN";

static spi_device_handle_t lcd_spi = NULL;
static spi_device_handle_t touch_spi = NULL;

static void update_status(const char *label, int y, const char *status, uint16_t color) {
    (void)label;
    lcd_fill_rect(100, y, 110, 12, COLOR_BACKGROUND);  // Reduced width to not overlap buttons
    lcd_draw_string(100, y, status, color, COLOR_BACKGROUND);
}

static void display_test_screen(void) {
    char buf[48];
    
    lcd_fill_screen(COLOR_BACKGROUND);
    
    lcd_fill_rect(0, 0, LCD_WIDTH, 18, COLOR_BLUE);
    lcd_draw_string(60, 4, "ESP32-S3 PERIPHERAL TEST", COLOR_WHITE, COLOR_BLUE);
    
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t flash_size_mb = flash_get_size_mb();
    
    snprintf(buf, sizeof(buf), "RAM:%dK FLASH:%dMB", (int)(internal_free/1024), (int)flash_size_mb);
    lcd_draw_string(10, 22, buf, COLOR_CYAN, COLOR_BACKGROUND);
    
    int y = 36;
    lcd_draw_string(10, y, "LED:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+14, "BUZZER:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+28, "BATTERY:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+42, "GPS:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+56, "LORA:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+70, "TOUCH:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+84, "SD CARD:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+98, "ATECC608:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+112, "WIFI:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+126, "BLUETOOTH:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+140, "40MHZ:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+154, "32KHZ:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+168, "FLASH:", COLOR_WHITE, COLOR_BACKGROUND);
    lcd_draw_string(10, y+182, "UPTIME:", COLOR_WHITE, COLOR_BACKGROUND);
    
    update_status("40MHZ", y+140, xtal_40mhz_is_ok() ? "OK" : "FAIL", xtal_40mhz_is_ok() ? COLOR_GREEN : COLOR_RED);
    update_status("32KHZ", y+154, xtal_32khz_is_ok() ? "OK" : "NOT CONFIGURED", xtal_32khz_is_ok() ? COLOR_GREEN : COLOR_YELLOW);
    update_status("SD", y+84, sd_card_is_detected() ? "DETECTED" : "NO CARD", sd_card_is_detected() ? COLOR_GREEN : COLOR_YELLOW);
    update_status("SE", y+98, secure_element_is_detected() ? "DETECTED" : "NOT FOUND", secure_element_is_detected() ? COLOR_GREEN : COLOR_RED);
    update_status("TOUCH", y+70, "READY", COLOR_YELLOW);
    update_status("BUZZER", y+14, "IDLE", COLOR_WHITE);
    
    if (flash_test_ok()) {
        snprintf(buf, sizeof(buf), "W25Q128 %dMB OK", (int)flash_get_size_mb());
        update_status("FLASH", y+168, buf, COLOR_GREEN);
    } else {
        update_status("FLASH", y+168, "FAILED", COLOR_RED);
    }
    
    if (wifi_test_ok()) {
        snprintf(buf, sizeof(buf), "%d NETWORKS", wifi_get_networks_found());
        update_status("WIFI", y+112, buf, COLOR_GREEN);
    } else {
        update_status("WIFI", y+112, "FAILED", COLOR_RED);
    }
    
    if (bluetooth_test_ok()) {
        snprintf(buf, sizeof(buf), "%d DEVICES", bluetooth_get_devices_found());
        update_status("BT", y+126, buf, COLOR_GREEN);
    } else {
        update_status("BT", y+126, "FAILED", COLOR_RED);
    }
    
    // Draw test buttons on the right side
    #define BTN_X       220
    #define BTN_W       90
    #define BTN_H       28
    #define BTN_SPACING 8
    
    int btn_y = 36;
    lcd_fill_rect(BTN_X, btn_y, BTN_W, BTN_H, COLOR_BLUE);
    lcd_draw_string(BTN_X + 15, btn_y + 8, "LED TEST", COLOR_WHITE, COLOR_BLUE);
    
    btn_y += BTN_H + BTN_SPACING;
    lcd_fill_rect(BTN_X, btn_y, BTN_W, BTN_H, COLOR_BLUE);
    lcd_draw_string(BTN_X + 5, btn_y + 8, "BUZZER TEST", COLOR_WHITE, COLOR_BLUE);
    
    btn_y += BTN_H + BTN_SPACING;
    lcd_fill_rect(BTN_X, btn_y, BTN_W, BTN_H, COLOR_BLUE);
    lcd_draw_string(BTN_X + 10, btn_y + 8, "WIFI SCAN", COLOR_WHITE, COLOR_BLUE);
    
    btn_y += BTN_H + BTN_SPACING;
    lcd_fill_rect(BTN_X, btn_y, BTN_W, BTN_H, COLOR_BLUE);
    lcd_draw_string(BTN_X + 15, btn_y + 8, "BT SCAN", COLOR_WHITE, COLOR_BLUE);
    
    btn_y += BTN_H + BTN_SPACING;
    lcd_fill_rect(BTN_X, btn_y, BTN_W, BTN_H, COLOR_RED);
    lcd_draw_string(BTN_X + 22, btn_y + 8, "REBOOT", COLOR_WHITE, COLOR_RED);

    // --- Brightness slider at bottom ---
    #define SLIDER_X 20
    #define SLIDER_Y (LCD_HEIGHT - 30)
    #define SLIDER_W 200
    #define SLIDER_H 18
    #define SLIDER_BAR_W (SLIDER_W - 20)
    lcd_draw_string(SLIDER_X, SLIDER_Y - 14, "BRIGHTNESS", COLOR_YELLOW, COLOR_BACKGROUND);
    lcd_fill_rect(SLIDER_X, SLIDER_Y, SLIDER_W, SLIDER_H, COLOR_WHITE);
    lcd_fill_rect(SLIDER_X + 10, SLIDER_Y + 4, SLIDER_BAR_W, SLIDER_H - 8, COLOR_CYAN);
    // Draw initial knob (full brightness)
    int knob_x = SLIDER_X + 10 + SLIDER_BAR_W - 6;
    lcd_fill_rect(knob_x, SLIDER_Y, 12, SLIDER_H, COLOR_BLUE);
    snprintf(buf, sizeof(buf), "%d", 255);
    lcd_draw_string(SLIDER_X + SLIDER_W + 8, SLIDER_Y + 2, buf, COLOR_YELLOW, COLOR_BACKGROUND);
}

static void peripheral_test_task(void *pvParameters) {
    char buf[64];
    char gps_buf[GPS_BUF_SIZE];
    int blink_state = 0;
    const int y = 36;
    
    // Button coordinates
    #define BTN_X       220
    #define BTN_W       90
    #define BTN_H       28
    #define BTN_SPACING 8
    const int btn_led_y = 36;
    const int btn_buzzer_y = btn_led_y + BTN_H + BTN_SPACING;
    const int btn_wifi_y = btn_buzzer_y + BTN_H + BTN_SPACING;
    const int btn_bt_y = btn_wifi_y + BTN_H + BTN_SPACING;
    const int btn_reboot_y = btn_bt_y + BTN_H + BTN_SPACING;
    
    while (1) {
        blink_state = !blink_state;
        led_set(blink_state);
        update_status("LED", y, blink_state ? "ON" : "OFF", blink_state ? COLOR_GREEN : COLOR_RED);
        
        int battery_mv = battery_read_mv();
        if (battery_mv < 3000) {
            snprintf(buf, sizeof(buf), "%d MV LOW", battery_mv);
            update_status("BATTERY", y+28, buf, COLOR_RED);
        } else if (battery_mv < 3500) {
            snprintf(buf, sizeof(buf), "%d MV", battery_mv);
            update_status("BATTERY", y+28, buf, COLOR_YELLOW);
        } else if (battery_mv > 4300) {
            snprintf(buf, sizeof(buf), "%d MV CHARGING?", battery_mv);
            update_status("BATTERY", y+28, buf, COLOR_CYAN);
        } else {
            snprintf(buf, sizeof(buf), "%d MV OK", battery_mv);
            update_status("BATTERY", y+28, buf, COLOR_GREEN);
        }
        
        int gps_len = gps_read(gps_buf, sizeof(gps_buf) - 1, 500);
        if (gps_len > 0) {
            gps_buf[gps_len] = '\0';
            if (strstr(gps_buf, "$GP") || strstr(gps_buf, "$GN")) {
                gps_process_buffer(gps_buf);
                const gps_position_t *pos = gps_get_position();
                if (pos->valid) {
                    snprintf(buf, sizeof(buf), "%.4f,%.4f", pos->latitude, pos->longitude);
                    update_status("GPS", y+42, buf, COLOR_GREEN);
                } else {
                    snprintf(buf, sizeof(buf), "SEARCH %dSAT", pos->satellites);
                    update_status("GPS", y+42, buf, COLOR_YELLOW);
                }
            } else {
                update_status("GPS", y+42, "DATA (NO NMEA)", COLOR_YELLOW);
            }
        } else {
            update_status("GPS", y+42, "NO DATA", COLOR_RED);
        }
        
        if (lora_is_detected()) {
            if (lora_is_tx_active()) {
                snprintf(buf, sizeof(buf), "TX @ %dMHz!", LORA_TEST_FREQ_HZ / 1000000);
                update_status("LORA", y+56, buf, COLOR_CYAN);
            } else {
                update_status("LORA", y+56, "SX1276 TOUCH=TX", COLOR_GREEN);
            }
        } else {
            update_status("LORA", y+56, "NOT FOUND", COLOR_RED);
        }
        
        int64_t uptime_us = esp_timer_get_time();
        int64_t uptime_sec = uptime_us / 1000000;
        int hours = (uptime_sec / 3600) % 24;
        int mins = (uptime_sec / 60) % 60;
        int secs = uptime_sec % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, secs);
        update_status("UPTIME", y+182, buf, COLOR_GREEN);
        
        // Fast loop for responsive touch, slow updates for other sensors
        uint16_t tx, ty;
        static bool touch_was_pressed = false;
        static uint8_t brightness = 255;
        for (int i = 0; i < 10; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Check touch during wait
            if (touch_read(&tx, &ty)) {
                if (!touch_was_pressed) {
                    touch_was_pressed = true;
                    snprintf(buf, sizeof(buf), "X=%3d Y=%3d", tx, ty);
                    update_status("TOUCH", y+70, buf, COLOR_GREEN);
                    
                    // --- Brightness slider touch ---
                    if (tx >= SLIDER_X + 10 && tx <= SLIDER_X + 10 + SLIDER_BAR_W && ty >= SLIDER_Y && ty <= SLIDER_Y + SLIDER_H) {
                        int rel = tx - (SLIDER_X + 10);
                        brightness = (rel * 255) / (SLIDER_BAR_W - 1);
                        lcd_set_backlight(brightness);
                        // Redraw knob
                        lcd_fill_rect(SLIDER_X + 10, SLIDER_Y, SLIDER_BAR_W, SLIDER_H, COLOR_CYAN);
                        int knob_x = SLIDER_X + 10 + rel - 6;
                        if (knob_x < SLIDER_X + 10) knob_x = SLIDER_X + 10;
                        if (knob_x > SLIDER_X + 10 + SLIDER_BAR_W - 12) knob_x = SLIDER_X + 10 + SLIDER_BAR_W - 12;
                        lcd_fill_rect(knob_x, SLIDER_Y, 12, SLIDER_H, COLOR_BLUE);
                        snprintf(buf, sizeof(buf), "%d", brightness);
                        lcd_fill_rect(SLIDER_X + SLIDER_W + 8, SLIDER_Y + 2, 40, 14, COLOR_BACKGROUND);
                        lcd_draw_string(SLIDER_X + SLIDER_W + 8, SLIDER_Y + 2, buf, COLOR_YELLOW, COLOR_BACKGROUND);
                        buzzer_tone(1000, 30);
                    } else
                    // Check button presses
                    if (tx >= BTN_X && tx <= BTN_X + BTN_W) {
                        if (ty >= btn_led_y && ty <= btn_led_y + BTN_H) {
                            for (int j = 0; j < 6; j++) {
                                led_set(j % 2);
                                vTaskDelay(pdMS_TO_TICKS(100));
                            }
                            buzzer_tone(1000, 50);
                        } else if (ty >= btn_buzzer_y && ty <= btn_buzzer_y + BTN_H) {
                            buzzer_tone(500, 100);
                            vTaskDelay(pdMS_TO_TICKS(150));
                            buzzer_tone(1000, 100);
                            vTaskDelay(pdMS_TO_TICKS(150));
                            buzzer_tone(1500, 100);
                            vTaskDelay(pdMS_TO_TICKS(150));
                            buzzer_tone(2000, 200);
                        } else if (ty >= btn_wifi_y && ty <= btn_wifi_y + BTN_H) {
                            update_status("WIFI", y+112, "SCANNING...", COLOR_YELLOW);
                            test_wifi();
                            if (wifi_test_ok()) {
                                snprintf(buf, sizeof(buf), "%d NETWORKS", wifi_get_networks_found());
                                update_status("WIFI", y+112, buf, COLOR_GREEN);
                            } else {
                                update_status("WIFI", y+112, "FAILED", COLOR_RED);
                            }
                            buzzer_tone(1000, 50);
                        } else if (ty >= btn_bt_y && ty <= btn_bt_y + BTN_H) {
                            update_status("BT", y+126, "SCANNING...", COLOR_YELLOW);
                            test_bluetooth();
                            if (bluetooth_test_ok()) {
                                snprintf(buf, sizeof(buf), "%d DEVICES", bluetooth_get_devices_found());
                                update_status("BT", y+126, buf, COLOR_GREEN);
                            } else {
                                update_status("BT", y+126, "FAILED", COLOR_RED);
                            }
                            buzzer_tone(1000, 50);
                        } else if (ty >= btn_reboot_y && ty <= btn_reboot_y + BTN_H) {
                            buzzer_tone(2000, 500);
                            vTaskDelay(pdMS_TO_TICKS(600));
                            esp_restart();
                        }
                    } else if (lora_is_detected()) {
                        if (lora_is_tx_active()) {
                            lora_stop_tx_test();
                            buzzer_tone(500, 100);
                        } else {
                            lora_start_tx_test();
                            buzzer_tone(2000, 100);
                        }
                    } else {
                        buzzer_tone(1000, 50);
                    }
                    update_status("BUZZER", y+14, lora_is_tx_active() ? "TX ON!" : "BEEP!", COLOR_GREEN);
                }
            } else {
                if (touch_was_pressed) {
                    // Touch just released
                    update_status("TOUCH", y+70, "READY", COLOR_YELLOW);
                    update_status("BUZZER", y+14, "IDLE", COLOR_WHITE);
                }
                touch_was_pressed = false;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 Custom Board Peripheral Test ===");
    
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: %s with %d cores", CONFIG_IDF_TARGET, chip_info.cores);
    
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 "MB", flash_size / (1024 * 1024));
    }
    
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM size: %d bytes (%d MB)", (int)psram_size, (int)(psram_size / (1024 * 1024)));
    
    // Initialize peripherals
    led_init();
    buzzer_init();
    battery_adc_init();
    gps_init();
    lora_init();
    spi_bus_init(&lcd_spi, &touch_spi);
    lcd_init(lcd_spi);
    touch_init(touch_spi);
    
    test_crystals();
    test_sd_card(lcd_spi);
    test_secure_element();
    test_flash_chip();
    test_wifi();
    test_bluetooth();
    
    // Startup beep
    buzzer_tone(1000, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(1500, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(2000, 200);
    
    display_test_screen();
    
    xTaskCreate(peripheral_test_task, "periph_test", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Initialization complete!");
}

