#pragma once

/*
 * Shared types and globals used across the test framework and driver
 * modules: the live sensor-value snapshot, GPS fix data, and the
 * test-result bookkeeping types.
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool  fix;
    float lat, lon, alt_m, speed_kmh, hdop;
    int   sats_used, sats_view;
    char  time_str[9];
} gps_data_t;

typedef struct {
    float    bat_v;
    int      bat_raw;
    float    mic_rms, mic_peak;
    uint8_t  lora_status;
    char     lora_tx_detail[32];
    char     sd_type[8];
    uint32_t sd_mb;
    bool     sd_write_ok;
    gps_data_t gps;
    bool     gps_valid;
    uint16_t wifi_ap_count;
    char     wifi_ssid[3][24];
    int8_t   wifi_rssi[3];
    bool     bt_ok;
} live_t;

/* Live sensor snapshot, updated by driver self-tests and live-refresh
 * handlers, and read by the detail-screen drawing code. */
extern live_t g_live;

typedef enum { RESULT_PENDING=0, RESULT_RUNNING, RESULT_PASS, RESULT_WARN, RESULT_FAIL } test_result_t;
typedef struct test_entry test_entry_t;
struct test_entry {
    const char   *name;
    test_result_t result;
    char          detail[48];
    void        (*fn)(test_entry_t *);
};

typedef enum { SCREEN_MAIN, SCREEN_DETAIL } screen_t;
