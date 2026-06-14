idf_component_register(SRCS "ui.c" "main.c"
                            "lcd.c"
                            "touch.c"
                            "led.c"
                            "buzzer.c"
                            "battery.c"
                            "gps.c"
                            "lora.c"
                            "wifi_test.c"
                            "bluetooth_test.c"
                            "peripherals.c"
                       PRIV_REQUIRES spi_flash driver esp_adc log esp_timer nvs_flash esp_wifi esp_event bt esp_phy
                       INCLUDE_DIRS ".")
