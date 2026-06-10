#include "wifi_bt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_bt.h"

static bool g_wifi_ready = false;

void test_wifi_bt(test_entry_t *t)
{
    /* ---- WiFi: bring up STA mode (once) and run a scan ---- */
    if(!g_wifi_ready){
        esp_netif_init();
        esp_event_loop_create_default();
        wifi_init_config_t wcfg=WIFI_INIT_CONFIG_DEFAULT();
        if(esp_wifi_init(&wcfg)==ESP_OK &&
           esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK &&
           esp_wifi_start()==ESP_OK){
            g_wifi_ready=true;
        }
    }
    g_live.wifi_ap_count=0;
    for(int i=0;i<3;i++) g_live.wifi_ssid[i][0]='\0';
    esp_err_t werr=ESP_FAIL;
    if(g_wifi_ready){
        wifi_scan_config_t sc={0};
        werr=esp_wifi_scan_start(&sc,true);
        if(werr==ESP_OK){
            uint16_t total=0;
            esp_wifi_scan_get_ap_num(&total);
            g_live.wifi_ap_count=total;
            uint16_t n=3;
            wifi_ap_record_t *aps=calloc(n,sizeof(wifi_ap_record_t));
            if(aps){
                esp_wifi_scan_get_ap_records(&n,aps);
                for(int i=0;i<n && i<3;i++){
                    strncpy(g_live.wifi_ssid[i],(char*)aps[i].ssid,sizeof(g_live.wifi_ssid[i])-1);
                    g_live.wifi_rssi[i]=aps[i].rssi;
                }
                free(aps);
            }
        }
    }

    /* ---- BLE: bring the controller up, then back down ---- */
    bool bt_ok=false;
    esp_bt_controller_config_t btcfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if(esp_bt_controller_init(&btcfg)==ESP_OK){
        if(esp_bt_controller_enable(ESP_BT_MODE_BLE)==ESP_OK){
            bt_ok=true;
            esp_bt_controller_disable();
        }
        esp_bt_controller_deinit();
    }
    g_live.bt_ok=bt_ok;

    snprintf(t->detail,sizeof(t->detail),"WiFi:%u APs  BLE:%s",
             (unsigned)g_live.wifi_ap_count, bt_ok?"OK":"FAIL");
    t->result = (werr==ESP_OK && bt_ok) ? RESULT_PASS :
                (werr==ESP_OK || bt_ok) ? RESULT_WARN : RESULT_FAIL;
}
