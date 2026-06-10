idf_component_register(
    SRCS "wifi_bt.c" "sdcard.c" "audio.c" "battery.c" "gps.c" "lora.c" "touch.c" "display.c" "mcp23017.c" "test_firmware.c"
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_spi
        esp_driver_i2c
        esp_driver_uart
        esp_driver_i2s
        esp_driver_sdspi
        esp_adc
        fatfs
        sdmmc
        spi_flash
        nvs_flash
        esp_system
        esp_timer
        log
        heap
        esp_wifi
        esp_event
        esp_netif
        bt
    INCLUDE_DIRS ""
)
