/*
 * Bluetooth Test
 */

#include "bluetooth_test.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BT_TEST";
static bool bluetooth_ok = false;
static int ble_devices_found = 0;
static volatile bool ble_scan_done = false;
static int ble_device_count = 0;
static volatile bool nimble_synced = false;
static bool bt_initialized = false;

static int nimble_gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            ble_device_count++;
            if (ble_device_count <= 10) {
                char name[32] = {0};
                struct ble_hs_adv_fields fields;
                if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0) {
                    if (fields.name != NULL && fields.name_len > 0) {
                        int len = fields.name_len < sizeof(name) - 1 ? fields.name_len : sizeof(name) - 1;
                        memcpy(name, fields.name, len);
                    }
                }
                ESP_LOGI(TAG, "  BLE %d: %02X:%02X:%02X:%02X:%02X:%02X %s",
                         ble_device_count,
                         event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
                         event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0],
                         name[0] ? name : "");
            }
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ble_scan_done = true;
            break;
        default:
            break;
    }
    return 0;
}

static void nimble_host_task(void *param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_on_sync(void) {
    nimble_synced = true;
}

void test_bluetooth(void) {
    ESP_LOGI(TAG, "=== Bluetooth Test ===");
    
    esp_err_t ret;
    
    if (!bt_initialized) {
        ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
        
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BT controller init failed");
            bluetooth_ok = false;
            return;
        }
        
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BT controller enable failed");
            bluetooth_ok = false;
            return;
        }
        
        ret = nimble_port_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NimBLE init failed");
            bluetooth_ok = false;
            return;
        }
        
        ble_hs_cfg.sync_cb = ble_on_sync;
        nimble_port_freertos_init(nimble_host_task);
        
        int timeout = 30;
        while (!nimble_synced && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            timeout--;
        }
        
        if (!nimble_synced) {
            ESP_LOGE(TAG, "NimBLE sync timeout");
            bluetooth_ok = false;
            return;
        }
        
        bt_initialized = true;
    }
    
    ESP_LOGI(TAG, "Scanning for BLE devices...");
    ble_device_count = 0;
    ble_scan_done = false;
    
    struct ble_gap_disc_params scan_params = {
        .itvl = 0x50,
        .window = 0x30,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 0,
    };
    
    ret = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 3000, &scan_params, nimble_gap_event, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Start scanning failed");
        bluetooth_ok = false;
        return;
    }
    
    int timeout = 40;
    while (!ble_scan_done && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout--;
    }
    
    ble_devices_found = ble_device_count;
    bluetooth_ok = true;
    
    ESP_LOGI(TAG, "Bluetooth: FOUND %d BLE DEVICES", ble_devices_found);
}

bool bluetooth_test_ok(void) { return bluetooth_ok; }
int bluetooth_get_devices_found(void) { return ble_devices_found; }
