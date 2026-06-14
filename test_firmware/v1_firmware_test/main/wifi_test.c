/*
 * WiFi Test
 */

#include "wifi_test.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "WIFI_TEST";
static bool wifi_ok = false;
static int wifi_networks_found = 0;
static bool wifi_initialized = false;

void test_wifi(void) {
    ESP_LOGI(TAG, "=== WiFi Test ===");
    
    esp_err_t ret;
    
    if (!wifi_initialized) {
        ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        wifi_initialized = true;
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Scanning for WiFi networks...");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };
    
    ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(ret));
        wifi_ok = false;
        esp_wifi_stop();
        return;
    }
    
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    wifi_networks_found = ap_count;
    wifi_ok = true;
    
    ESP_LOGI(TAG, "WiFi: FOUND %d NETWORKS", ap_count);
    
    if (ap_count > 0) {
        wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * (ap_count > 10 ? 10 : ap_count));
        if (ap_list) {
            uint16_t num = ap_count > 10 ? 10 : ap_count;
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num, ap_list));
            for (int i = 0; i < num; i++) {
                ESP_LOGI(TAG, "  WiFi %d: \"%s\" (RSSI: %d)", i + 1, ap_list[i].ssid, ap_list[i].rssi);
            }
            free(ap_list);
        }
    }
    
    esp_wifi_stop();
}

bool wifi_test_ok(void) { return wifi_ok; }
int wifi_get_networks_found(void) { return wifi_networks_found; }
