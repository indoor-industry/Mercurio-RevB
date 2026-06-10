/*
 * LoRa Device RevB — Board Test Firmware v2
 *
 * This file holds the test framework, the main-menu UI, and the per-test
 * detail screens. Each peripheral's low-level driver and self-test lives
 * in its own module:
 *   board_config.h  - pin map, register/bitmask defines, UI constants
 *   shared_types.h  - live sensor snapshot + test bookkeeping types
 *   display.h       - ILI9341 driver + detail-screen drawing helpers
 *   touch.h         - XPT2046 driver + calibration storage
 *   mcp23017.h      - I/O expander driver
 *   lora.h          - SX1262 driver
 *   gps.h           - L76KB / NMEA
 *   battery.h       - battery ADC
 *   audio.h         - microphone + speaker (I2S)
 *   sdcard.h        - SD card (SPI)
 *   wifi_bt.h       - WiFi scan + BLE controller smoke test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "board_config.h"
#include "shared_types.h"
#include "display.h"
#include "touch.h"
#include "mcp23017.h"
#include "lora.h"
#include "gps.h"
#include "battery.h"
#include "audio.h"
#include "sdcard.h"
#include "wifi_bt.h"

static const char *TAG = "test_fw";

/* =========================================================================
 * Globals
 * ========================================================================= */
live_t g_live = {0};
static screen_t g_screen = SCREEN_MAIN;
static int      g_detail_idx = 0;
static int16_t  g_dot_x = -1, g_dot_y = -1;   /* touch-detail live dot */
static uint32_t g_last_refresh_ms = 0;

/* =========================================================================
 * Test table
 * ========================================================================= */
static void test_system(test_entry_t *t);

static test_entry_t g_tests[] = {
    {"System",      RESULT_PENDING,"",test_system},
    {"Battery",     RESULT_PENDING,"",test_battery},
    {"MCP23017",    RESULT_PENDING,"",test_mcp23017},
    {"LoRa SX1262", RESULT_PENDING,"",test_lora},
    {"SD Card",     RESULT_PENDING,"",test_sd_card},
    {"GPS",         RESULT_PENDING,"",test_gps},
    {"Microphone",  RESULT_PENDING,"",test_microphone},
    {"Speaker",     RESULT_PENDING,"",test_speaker},
    {"Touch",       RESULT_PENDING,"",test_touch_hw},
    {"WiFi & BLE",  RESULT_PENDING,"",test_wifi_bt},
};
#define N_TESTS 10

/* =========================================================================
 * Board initialisation
 * ========================================================================= */
static void init_gpio(void)
{
    gpio_config_t out={
        .pin_bit_mask=(1ULL<<PIN_EXP_RST)|(1ULL<<PIN_DISP_DC)|
                      (1ULL<<PIN_DISP_LED)|(1ULL<<PIN_LORA_RFEN)|(1ULL<<PIN_LORA_RST),
        .mode=GPIO_MODE_OUTPUT,.pull_up_en=0,.pull_down_en=0,.intr_type=GPIO_INTR_DISABLE};
    gpio_config(&out);
    gpio_config_t in={
        .pin_bit_mask=(1ULL<<PIN_BOOT_BTN)|(1ULL<<PIN_LORA_BUSY)|
                      (1ULL<<PIN_LORA_DIO1)|(1ULL<<PIN_EXP_INTA),
        .mode=GPIO_MODE_INPUT,.pull_up_en=1,.pull_down_en=0,.intr_type=GPIO_INTR_DISABLE};
    gpio_config(&in);
    gpio_set_level(PIN_EXP_RST,0);
    gpio_set_level(PIN_DISP_LED,0);
    gpio_set_level(PIN_LORA_RFEN,0);
    gpio_set_level(PIN_LORA_RST,1);
}

/* =========================================================================
 * Touch calibration – 3-point affine (UI)
 * ========================================================================= */
static void draw_crosshair(int16_t x, int16_t y, uint16_t col)
{
    disp_fill(x-20, y-1, 18, 3, col);
    disp_fill(x+3,  y-1, 18, 3, col);
    disp_fill(x-1, y-20, 3, 18, col);
    disp_fill(x-1, y+3,  3, 18, col);
    disp_fill(x-3, y-3,  7,  7, col);
    disp_fill(x-1, y-1,  3,  3, C_BLACK);
}

/* Run interactive 3-point calibration. Blocks until done. */
static void run_calibration(void)
{
    /* Three target screen positions */
    static const int16_t tx[3]={20,220,120}, ty[3]={20,20,280};
    uint16_t raw_x[3], raw_y[3];

    disp_fill(0, 0, SCR_W, SCR_H, C_BLACK);
    disp_text(8, 20, "TOUCH CALIBRATION", C_WHITE, C_BLACK, 2);
    disp_text(8, 50, "Touch each cross", C_YELLOW, C_BLACK, 2);
    disp_text(8, 70, "and hold 1 second", C_YELLOW, C_BLACK, 2);
    vTaskDelay(pdMS_TO_TICKS(1200));

    for (int i=0; i<3; i++) {
        disp_fill(0, 0, SCR_W, SCR_H, C_BLACK);
        char step[20]; snprintf(step,sizeof(step),"Step %d / 3",i+1);
        disp_text(8, 8, step, C_GRAY, C_BLACK, 2);
        draw_crosshair(tx[i], ty[i], C_TEAL);
        disp_text(8, SCR_H-44, "Touch the target", C_WHITE, C_BLACK, 2);

        /* Accumulate ~10 consecutive pressed samples. Any release before
         * that just restarts the count, and a live raw readout is shown
         * so it's obvious whether touches are being picked up at all. */
        uint32_t sumx=0, sumy=0; int n=0; bool got=false;
        int64_t dl = esp_timer_get_time() + 20000000LL; /* 20s per point */
        while (esp_timer_get_time() < dl) {
            touch_pt_t p = touch_read();
            char dbg[28];
            snprintf(dbg,sizeof(dbg),"raw: %4u,%4u z=%4d",p.raw_x,p.raw_y,p.z);
            disp_text(8, SCR_H-22, dbg, p.pressed?C_GREEN:C_GRAY, C_BLACK, 1);
            if (p.pressed) {
                sumx += p.raw_x; sumy += p.raw_y; n++;
                if (n >= 10) { got = true; break; }
            } else if (n > 0) {
                sumx = 0; sumy = 0; n = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        if (!got) {
            disp_fill(0,0,SCR_W,SCR_H,C_BLACK);
            disp_text(8,80,"Cal TIMEOUT",C_RED,C_BLACK,2);
            disp_text(8,104,"No touch detected",C_WHITE,C_BLACK,2);
            vTaskDelay(pdMS_TO_TICKS(2000));
            return;
        }
        raw_x[i]=(uint16_t)(sumx/(uint32_t)n);
        raw_y[i]=(uint16_t)(sumy/(uint32_t)n);
        draw_crosshair(tx[i], ty[i], C_GREEN);
        ESP_LOGI(TAG,"Cal P%d: screen(%d,%d) raw(%u,%u)",
                 i+1,tx[i],ty[i],raw_x[i],raw_y[i]);
        touch_wait_release();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    /* Solve 3x3 affine system via Cramer's rule
     * M * [a b c]^T = [sx1 sx2 sx3]^T
     * M = | tx1 ty1 1 |
     *     | tx2 ty2 1 |
     *     | tx3 ty3 1 | */
    float t1=raw_x[0], t2=raw_x[1], t3=raw_x[2];
    float u1=raw_y[0], u2=raw_y[1], u3=raw_y[2];

    float detM = t1*(u2-u3) - u1*(t2-t3) + (t2*u3-t3*u2);
    if (fabsf(detM) < 1.0f) {
        disp_fill(0,0,SCR_W,SCR_H,C_BLACK);
        disp_text(8,80,"Cal FAILED",C_RED,C_BLACK,2);
        disp_text(8,104,"Points too close",C_WHITE,C_BLACK,2);
        vTaskDelay(pdMS_TO_TICKS(2000)); return;
    }

    float sx1=tx[0],sx2=tx[1],sx3=tx[2];
    float sy1=ty[0],sy2=ty[1],sy3=ty[2];

    g_cal.ax = (sx1*(u2-u3)-u1*(sx2-sx3)+(sx2*u3-sx3*u2))/detM;
    g_cal.bx = (t1*(sx2-sx3)-sx1*(t2-t3)+(t2*sx3-t3*sx2))/detM;
    g_cal.cx = (t1*(u2*sx3-u3*sx2)-u1*(t2*sx3-t3*sx2)+sx1*(t2*u3-t3*u2))/detM;

    g_cal.ay = (sy1*(u2-u3)-u1*(sy2-sy3)+(sy2*u3-sy3*u2))/detM;
    g_cal.by = (t1*(sy2-sy3)-sy1*(t2-t3)+(t2*sy3-t3*sy2))/detM;
    g_cal.cy = (t1*(u2*sy3-u3*sy2)-u1*(t2*sy3-t3*sy2)+sy1*(t2*u3-t3*u2))/detM;
    g_cal.valid = true;

    cal_save();
    disp_fill(0,0,SCR_W,SCR_H,C_BLACK);
    disp_text(8,120,"Calibration saved",C_GREEN,C_BLACK,2);
    vTaskDelay(pdMS_TO_TICKS(1200));
}

/* =========================================================================
 * SYSTEM self-test
 * ========================================================================= */
static void test_system(test_entry_t *t)
{
    esp_chip_info_t ci; esp_chip_info(&ci);
    uint32_t fl=0; esp_flash_get_size(NULL,&fl);
    size_t ps=heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    snprintf(t->detail,sizeof(t->detail),"%s r%d %"PRIu32"MB fl %zuMB PS",
             CONFIG_IDF_TARGET,ci.revision,fl>>20,ps>>20);
    t->result=RESULT_PASS;
}

/* =========================================================================
 * Detail pages
 * ========================================================================= */

/* --- SYSTEM --- */
static void detail_draw_system(void)
{
    detail_header("System");
    esp_chip_info_t ci; esp_chip_info(&ci);
    uint32_t fl=0; esp_flash_get_size(NULL,&fl);
    size_t tot_heap=heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t tot_ps  =heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t fr_heap =heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t fr_ps   =heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t min_heap=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    drow(0,C_WHITE,C_BLACK,"Chip: %s r%d",CONFIG_IDF_TARGET,ci.revision);
    drow(1,C_WHITE,C_BLACK,"CPU:  %d cores",ci.cores);
    drow(2,C_WHITE,C_BLACK,"Flash: %"PRIu32" MB",fl>>20);
    drow(3,C_WHITE,C_BLACK,"PSRAM: %zu MB",tot_ps>>20);
    drow(4,C_DGRAY,C_BLACK,"");
    drow(5,C_GREEN,C_BLACK,"Heap free: %zuK",fr_heap>>10);
    drow(6,C_YELLOW,C_BLACK,"Heap min:  %zuK",min_heap>>10);
    drow(7,C_TEAL,C_BLACK,"Heap total:%zuK",tot_heap>>10);
    drow(8,C_GREEN,C_BLACK,"PS free:  %zu MB",fr_ps>>20);
    drow(9,C_DGRAY,C_BLACK,"");
    drow(10,C_WHITE,C_BLACK,"IDF: %s",esp_get_idf_version());
    drow(11,C_WHITE,C_BLACK,"App: %s",g_tests[IDX_SYSTEM].detail);
}
static void detail_refresh_system(void)
{
    size_t fr=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t mn=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    uint32_t up=(uint32_t)(esp_timer_get_time()/1000000);
    drow(5,C_GREEN,C_BLACK,"Heap free: %zuK",fr>>10);
    drow(6,C_YELLOW,C_BLACK,"Heap min:  %zuK",mn>>10);
    drow(12,C_WHITE,C_BLACK,"Uptime:  %"PRIu32"s",up);
}

/* --- BATTERY --- */
static const char *charger_state(uint8_t gpa, uint8_t gpb)
{
    int s1=(gpa&GPA_STAT1)?1:0, s2=(gpb&GPB_STAT2)?1:0;
    if(!s1&&!s2) return "Charging";
    if( s1&&!s2) return "Complete";
    if(!s1&& s2) return "Fault";
    return "Standby";
}
static void detail_draw_battery(void)
{
    detail_header("Battery");
    drow(0,C_WHITE,C_BLACK,"Voltage: ----");
    drow(1,C_DGRAY,C_BLACK,"");          /* bar row */
    drow(2,C_WHITE,C_BLACK,"ADC raw: ----");
    drow(3,C_DGRAY,C_BLACK,"");
    drow(4,C_WHITE,C_BLACK,"Charger: ----");
    drow(5,C_WHITE,C_BLACK,"STAT1:  --");
    drow(6,C_WHITE,C_BLACK,"STAT2:  --");
}
static void detail_refresh_battery(void)
{
    adc_oneshot_unit_handle_t h=bat_adc_get();
    if(!h) return;
    int sum=0;
    for(int i=0;i<8;i++){int r=0;adc_oneshot_read(h,BAT_ADC_CHAN,&r);sum+=r;}
    float vpin=(float)(sum/8)*3.1f/4095.0f;
    float vbat=vpin*2.0f;
    float pct=(vbat-3.0f)/(4.2f-3.0f)*100.0f;
    if(pct<0) pct=0;
    if(pct>100) pct=100;
    g_live.bat_v=vbat; g_live.bat_raw=sum/8;

    uint16_t vcol=vbat>3.6f?C_GREEN:vbat>3.2f?C_YELLOW:C_RED;
    drow(0,vcol,C_BLACK,"Voltage: %.2fV (%.0f%%)",vbat,pct);
    draw_bar(DPAD, DCONTENT_Y+1*DROW_H+2, SCR_W-2*DPAD, 10,
             pct/100.0f, vcol, C_GRAY);
    drow(2,C_WHITE,C_BLACK,"ADC raw: %d",sum/8);
    uint8_t gpa=0,gpb=0; mcp_read2(&gpa,&gpb);
    drow(4,C_WHITE,C_BLACK,"Charger: %s",charger_state(gpa,gpb));
    drow(5,C_WHITE,C_BLACK,"STAT1: %s",(gpa&GPA_STAT1)?"HIGH":"LOW");
    drow(6,C_WHITE,C_BLACK,"STAT2: %s",(gpb&GPB_STAT2)?"HIGH":"LOW");
}

/* --- MCP23017 --- */
static void detail_draw_mcp(void)
{
    detail_header("MCP23017");
    /* labels (static) */
    drow(0,C_WHITE,C_BLACK,"SW1 left:  ----");
    drow(1,C_WHITE,C_BLACK,"SW2 right: ----");
    drow(2,C_DGRAY,C_BLACK,"");
    drow(3,C_WHITE,C_BLACK,"SD card:   ----");
    drow(4,C_WHITE,C_BLACK,"Touch IRQ: ----");
    drow(5,C_WHITE,C_BLACK,"GPS 1PPS:  ----");
    drow(6,C_DGRAY,C_BLACK,"");
    drow(7,C_WHITE,C_BLACK,"STAT1: --  STAT2: --");
    drow(8,C_DGRAY,C_BLACK,"");
    drow(9,C_WHITE,C_BLACK,"GPA=0x--  GPB=0x--");
}
static void detail_refresh_mcp(void)
{
    uint8_t gpa=0,gpb=0;
    if(mcp_read2(&gpa,&gpb)!=ESP_OK){
        drow(0,C_RED,C_BLACK,"I2C error"); return;}

    uint16_t sw1c=(gpa&GPA_SW1)?C_GRAY:C_GREEN;
    uint16_t sw2c=(gpb&GPB_SW2)?C_GRAY:C_GREEN;
    drow(0,sw1c,C_BLACK,"SW1 left:  %s",(gpa&GPA_SW1)?"open":"PRESSED");
    drow(1,sw2c,C_BLACK,"SW2 right: %s",(gpb&GPB_SW2)?"open":"PRESSED");
    drow(3,C_WHITE,C_BLACK,"SD card:   %s",(gpb&GPB_SD_DET)?"empty":"INSERTED");
    drow(4,C_WHITE,C_BLACK,"Touch IRQ: %s",(gpb&GPB_TOUCH_IRQ)?"idle":"ACTIVE");
    drow(5,C_WHITE,C_BLACK,"GPS 1PPS:  %s",(gpa&GPA_GPS_1PPS)?"HIGH":"low");
    drow(7,C_WHITE,C_BLACK,"STAT1: %s  STAT2: %s",
         (gpa&GPA_STAT1)?"H":"L",(gpb&GPB_STAT2)?"H":"L");
    drow(9,C_TEAL,C_BLACK,"GPA=0x%02X  GPB=0x%02X",gpa,gpb);
}

/* --- LORA --- */
static void detail_draw_lora(void)
{
    detail_header("LoRa SX1262");
    uint8_t s=g_live.lora_status;
    drow(0,C_WHITE,C_BLACK,"Status: 0x%02X",s);
    static const char *modes[]={"--","--","STBY_RC","STBY_XO","FS","RX","TX","--"};
    drow(1,C_WHITE,C_BLACK,"Mode: %s",modes[(s>>4)&7]);
    drow(2,C_WHITE,C_BLACK,"BUSY: %s",gpio_get_level(PIN_LORA_BUSY)?"HIGH":"low");
    drow(3,C_WHITE,C_BLACK,"DIO1: %s",gpio_get_level(PIN_LORA_DIO1)?"HIGH":"low");
    drow(4,C_WHITE,C_BLACK,"RF_EN: %s",gpio_get_level(PIN_LORA_RFEN)?"ON":"off");
    drow(5,C_DGRAY,C_BLACK,"");
    drow(6,C_WHITE,C_BLACK,"Result: %s",g_tests[IDX_LORA].detail);
    drow(7,C_DGRAY,C_BLACK,"");
    detail_btn(8,2,"RE-RUN TEST",C_TEAL);
    drow(10,C_DGRAY,C_BLACK,"");
    detail_btn(11,2,"SEND TEST PACKET",C_VIOLET);
    drow(13,C_GRAY,C_BLACK,"%s",g_live.lora_tx_detail);
}

/* --- SD CARD --- */
static void detail_draw_sd(void)
{
    detail_header("SD Card");
    uint8_t gpb=0; mcp_read(MCP_GPIOB,&gpb);
    bool ins=!(gpb&GPB_SD_DET);
    drow(0,ins?C_GREEN:C_YELLOW,C_BLACK,"Card: %s",ins?"INSERTED":"not present");
    if(g_live.sd_mb){
        drow(1,C_WHITE,C_BLACK,"Type: %s",g_live.sd_type);
        drow(2,C_WHITE,C_BLACK,"Size: %"PRIu32" MB",g_live.sd_mb);
        drow(3,C_WHITE,C_BLACK,"Write: %s",g_live.sd_write_ok?"OK":"FAIL");
    } else {
        drow(1,C_GRAY,C_BLACK,"(no data yet)");
        drow(2,C_DGRAY,C_BLACK,"");
        drow(3,C_DGRAY,C_BLACK,"");
    }
    drow(4,C_DGRAY,C_BLACK,"");
    drow(5,C_WHITE,C_BLACK,"Result: %s",g_tests[IDX_SD].detail);
    drow(6,C_DGRAY,C_BLACK,"");
    detail_btn(7,2,"RE-RUN TEST",C_TEAL);
}

/* --- GPS --- */
static void detail_draw_gps(void)
{
    detail_header("GPS");
    gps_data_t *g=&g_live.gps;
    if(!g_live.gps_valid){
        drow(0,C_YELLOW,C_BLACK,"No data yet"); drow(1,C_GRAY,C_BLACK,"Press SCAN GPS");
    } else {
        uint16_t fc=g->fix?C_GREEN:C_YELLOW;
        drow(0,fc,C_BLACK,"Fix: %s",g->fix?"3D FIX":"searching");
        if(g->fix){
            drow(1,C_WHITE,C_BLACK,"Lat: %.4f",g->lat);
            drow(2,C_WHITE,C_BLACK,"Lon: %.4f",g->lon);
            drow(3,C_WHITE,C_BLACK,"Alt: %.1fm",g->alt_m);
            drow(4,C_WHITE,C_BLACK,"Spd: %.1f km/h",g->speed_kmh);
            drow(5,C_WHITE,C_BLACK,"Sats: %d  HDOP:%.1f",g->sats_used,g->hdop);
            drow(6,C_WHITE,C_BLACK,"Time: %s UTC",g->time_str);
        } else {
            drow(1,C_WHITE,C_BLACK,"Sats: %d",g->sats_used);
            drow(2,C_DGRAY,C_BLACK,"");
            drow(3,C_DGRAY,C_BLACK,"");
            drow(4,C_DGRAY,C_BLACK,"");
            drow(5,C_DGRAY,C_BLACK,"");
            drow(6,C_DGRAY,C_BLACK,"");
        }
        drow(7,C_DGRAY,C_BLACK,"");
    }
    detail_btn(8,2,"SCAN GPS (6s)",C_TEAL);
}

/* --- MICROPHONE --- */
static void detail_draw_mic(void)
{
    detail_header("Microphone");
    drow(0,C_WHITE,C_BLACK,"RMS:  ----");
    drow(1,C_DGRAY,C_BLACK,"");        /* live level bar */
    drow(2,C_WHITE,C_BLACK,"Peak: ----");
    drow(3,C_WHITE,C_BLACK,"dBFS: ----");
    drow(4,C_DGRAY,C_BLACK,"");
    detail_btn(5,2,"RUN MIC TEST",C_TEAL);
    drow(7,C_GRAY,C_BLACK,"Last: %s",g_tests[IDX_MIC].detail);
}
/* Continuously-updating live level meter (fast 100ms chunks) so the user
 * gets immediate visual feedback while talking/tapping near the mic. */
static void detail_refresh_mic(void)
{
    i2s_chan_handle_t rx;
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    cc.dma_desc_num=4; cc.dma_frame_num=256;
    if(i2s_new_channel(&cc,NULL,&rx)!=ESP_OK) return;
    i2s_std_config_t sc={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=PIN_I2S_BCLK,
                   .ws=PIN_I2S_LRCLK,.dout=I2S_GPIO_UNUSED,.din=PIN_I2S_DIN}};
    i2s_channel_init_std_mode(rx,&sc); i2s_channel_enable(rx);
    const int N=1600; /* 100ms @ 16kHz */
    int32_t *buf=malloc(N*4);
    double rms=0,peak=0;
    if(buf){
        rms=mic_read_chunk(rx,buf,N,&peak);
        free(buf);
    }
    i2s_channel_disable(rx); i2s_del_channel(rx);
    g_live.mic_rms=(float)rms; g_live.mic_peak=(float)peak;
    float dbfs=rms>1e-7f ? 20.0f*(float)log10(rms) : -99.0f;
    float bar_frac=rms>0?(float)(1.0+log10(rms+1e-9)/5.0):0; /* log scale 0-1 */
    if(bar_frac<0) bar_frac=0;
    uint16_t lev_col=bar_frac>0.7f?C_RED:bar_frac>0.4f?C_YELLOW:C_GREEN;
    drow(0,lev_col,C_BLACK,"RMS:  %.6f",(float)rms);
    draw_bar(DPAD,DCONTENT_Y+1*DROW_H+2,SCR_W-2*DPAD,10,bar_frac,lev_col,C_GRAY);
    drow(2,C_WHITE,C_BLACK,"Peak: %.6f",(float)peak);
    drow(3,C_WHITE,C_BLACK,"dBFS: %.1f",dbfs);
}

/* --- SPEAKER --- */
static void detail_draw_speaker(void)
{
    detail_header("Speaker");
    drow(0,C_DGRAY,C_BLACK,"");
    detail_btn(1,3,"PLAY 440 Hz",C_TEAL);
    drow(4,C_DGRAY,C_BLACK,"");
    detail_btn(5,3,"PLAY 1000 Hz",C_VIOLET);
    drow(8,C_DGRAY,C_BLACK,"");
    detail_btn(9,3,"PLAY SWEEP",C_BLUE);
    drow(12,C_DGRAY,C_BLACK,"");
    drow(13,C_GRAY,C_BLACK,"%s",g_tests[IDX_SPEAKER].detail);
}

/* --- TOUCH --- */
#define TOUCH_MAP_Y   (DCONTENT_Y + 7*DROW_H)
#define TOUCH_MAP_H   (SCR_H - TOUCH_MAP_Y - 2)
#define TOUCH_MAP_W   (SCR_W - 2*DPAD)
#define TOUCH_MAP_X   DPAD

static void detail_draw_touch(void)
{
    detail_header("Touch XPT2046");
    drow(0,C_WHITE,C_BLACK,"Raw X: ----");
    drow(1,C_WHITE,C_BLACK,"Raw Y: ----");
    drow(2,C_WHITE,C_BLACK,"Raw Z: ----");
    drow(3,C_DGRAY,C_BLACK,"");
    drow(4,C_WHITE,C_BLACK,"Scr X: ---");
    drow(5,C_WHITE,C_BLACK,"Scr Y: ---");
    drow(6,C_DGRAY,C_BLACK,"");
    detail_btn(7,3,g_cal.valid?"RECALIBRATE":"CALIBRATE NOW",C_ORANGE);
    drow(10,C_DGRAY,C_BLACK,"");
    /* Map area */
    disp_fill(TOUCH_MAP_X, TOUCH_MAP_Y, TOUCH_MAP_W, TOUCH_MAP_H, C_DGRAY);
    disp_text(TOUCH_MAP_X+4, TOUCH_MAP_Y+(TOUCH_MAP_H-16)/2,
              "Touch to see position", C_GRAY, C_DGRAY, 1);
    g_dot_x=-1; g_dot_y=-1;
}
static void detail_refresh_touch(void)
{
    touch_pt_t p=touch_read();
    drow(0,C_WHITE,C_BLACK,"Raw X: %u",p.raw_x);
    drow(1,C_WHITE,C_BLACK,"Raw Y: %u",p.raw_y);
    drow(2,C_WHITE,C_BLACK,"Raw Z: %d",p.z);
    drow(4,p.pressed?C_GREEN:C_WHITE,C_BLACK,"Scr X: %d",p.x);
    drow(5,p.pressed?C_GREEN:C_WHITE,C_BLACK,"Scr Y: %d",p.y);

    /* dot in map area */
    if(g_dot_x>=0) {
        int ex=g_dot_x-5, ey=g_dot_y-5;
        if(ex<TOUCH_MAP_X) ex=TOUCH_MAP_X;
        if(ey<TOUCH_MAP_Y) ey=TOUCH_MAP_Y;
        disp_fill(ex,ey,11,11,C_DGRAY);
    }
    if(p.pressed && p.y >= HDR_H) {
        int16_t dx=TOUCH_MAP_X+(int16_t)((int)p.x*TOUCH_MAP_W/SCR_W);
        int16_t dy=TOUCH_MAP_Y+(int16_t)((int)p.y*TOUCH_MAP_H/SCR_H);
        if(dx<TOUCH_MAP_X) dx=TOUCH_MAP_X;
        if(dy<TOUCH_MAP_Y) dy=TOUCH_MAP_Y;
        if(dx>TOUCH_MAP_X+TOUCH_MAP_W-6) dx=TOUCH_MAP_X+TOUCH_MAP_W-6;
        if(dy>TOUCH_MAP_Y+TOUCH_MAP_H-6) dy=TOUCH_MAP_Y+TOUCH_MAP_H-6;
        disp_fill(dx-5,dy-5,11,11,C_RED);
        g_dot_x=dx; g_dot_y=dy;
    } else {
        g_dot_x=-1; g_dot_y=-1;
    }
}

/* --- WIFI & BLE --- */
static void detail_draw_wifi(void)
{
    detail_header("WiFi & BLE");
    drow(0,C_WHITE,C_BLACK,"WiFi APs found: %u",(unsigned)g_live.wifi_ap_count);
    for(int i=0;i<3;i++){
        if(g_live.wifi_ssid[i][0])
            drow(1+i,C_TEAL,C_BLACK,"%-16s %ddBm",g_live.wifi_ssid[i],g_live.wifi_rssi[i]);
        else
            drow(1+i,C_DGRAY,C_BLACK,"");
    }
    drow(4,C_DGRAY,C_BLACK,"");
    drow(5,g_live.bt_ok?C_GREEN:C_RED,C_BLACK,"BLE controller: %s",
         g_live.bt_ok?"OK":"FAIL");
    drow(6,C_DGRAY,C_BLACK,"");
    drow(7,C_WHITE,C_BLACK,"Result: %s",g_tests[IDX_WIFI].detail);
    drow(8,C_DGRAY,C_BLACK,"");
    detail_btn(9,2,"RE-RUN TEST",C_TEAL);
}

/* Function tables indexed by test index */
static void (*const g_detail_draw[N_TESTS])(void) = {
    detail_draw_system, detail_draw_battery, detail_draw_mcp,
    detail_draw_lora,   detail_draw_sd,      detail_draw_gps,
    detail_draw_mic,    detail_draw_speaker, detail_draw_touch,
    detail_draw_wifi,
};
static void (*const g_detail_refresh[N_TESTS])(void) = {
    detail_refresh_system, detail_refresh_battery, detail_refresh_mcp,
    NULL, NULL, NULL,
    detail_refresh_mic, NULL, detail_refresh_touch,
    NULL,
};
static const uint32_t g_refresh_ms[N_TESTS] = {
    2000, 1000, 200, 0, 0, 0, 150, 0, 80, 0,   /* mic live 150ms, wifi manual (0), touch 80ms */
};

/* =========================================================================
 * Detail view touch handlers (buttons inside the content area)
 * ========================================================================= */
static void open_detail(int idx);  /* forward */

static void handle_detail_touch(int idx, const touch_pt_t *p)
{
    if (!p->pressed) return;
    uint16_t y = (uint16_t)p->y;

    switch (idx) {
    case IDX_LORA:
        if (y >= DCONTENT_Y+8*DROW_H && y < DCONTENT_Y+10*DROW_H) {
            test_lora(&g_tests[IDX_LORA]);
            detail_draw_lora();
        } else if (y >= DCONTENT_Y+11*DROW_H && y < DCONTENT_Y+13*DROW_H) {
            detail_btn(11,2,"Sending...",C_GRAY);
            lora_send_test_packet(g_live.lora_tx_detail,sizeof(g_live.lora_tx_detail));
            detail_draw_lora();
        }
        break;
    case IDX_SD:
        if (y >= DCONTENT_Y+7*DROW_H && y < DCONTENT_Y+9*DROW_H) {
            test_sd_card(&g_tests[IDX_SD]);
            detail_draw_sd();
        }
        break;
    case IDX_GPS:
        if (y >= DCONTENT_Y+8*DROW_H && y < DCONTENT_Y+10*DROW_H) {
            drow(8,C_YELLOW,C_BLACK,"Scanning GPS...");
            test_gps(&g_tests[IDX_GPS]);
            detail_draw_gps();
        }
        break;
    case IDX_MIC:
        if (y >= DCONTENT_Y+5*DROW_H && y < DCONTENT_Y+7*DROW_H) {
            mic_run_test(&g_tests[IDX_MIC]);
            detail_draw_mic();
        }
        break;
    case IDX_SPEAKER:
        if (y >= DCONTENT_Y+1*DROW_H && y < DCONTENT_Y+4*DROW_H) {
            detail_btn(1,3,"Playing 440Hz...",C_GRAY);
            play_tone(440.0f, 700);
            detail_btn(1,3,"PLAY 440 Hz",C_TEAL);
        } else if (y >= DCONTENT_Y+5*DROW_H && y < DCONTENT_Y+8*DROW_H) {
            detail_btn(5,3,"Playing 1000Hz..",C_GRAY);
            play_tone(1000.0f, 700);
            detail_btn(5,3,"PLAY 1000 Hz",C_VIOLET);
        } else if (y >= DCONTENT_Y+9*DROW_H && y < DCONTENT_Y+12*DROW_H) {
            detail_btn(9,3,"Playing sweep...",C_GRAY);
            play_tone(-1.0f, 1200);
            detail_btn(9,3,"PLAY SWEEP",C_BLUE);
        }
        break;
    case IDX_TOUCH:
        if (y >= DCONTENT_Y+7*DROW_H && y < DCONTENT_Y+10*DROW_H) {
            g_screen = SCREEN_MAIN;  /* exit detail first */
            run_calibration();
            open_detail(IDX_TOUCH);
        }
        break;
    case IDX_WIFI:
        if (y >= DCONTENT_Y+9*DROW_H && y < DCONTENT_Y+11*DROW_H) {
            detail_btn(9,2,"Scanning...",C_GRAY);
            test_wifi_bt(&g_tests[IDX_WIFI]);
            detail_draw_wifi();
        }
        break;
    default: break;
    }
}

/* =========================================================================
 * Main menu drawing
 * ========================================================================= */
static uint16_t result_color(test_result_t r) {
    switch(r){
    case RESULT_PASS:    return C_GREEN;
    case RESULT_WARN:    return C_ORANGE;
    case RESULT_FAIL:    return C_RED;
    case RESULT_RUNNING: return C_YELLOW;
    default:             return C_GRAY;
    }
}
static const char *result_label(test_result_t r) {
    switch(r){
    case RESULT_PASS: return "PASS";
    case RESULT_WARN: return "WARN";
    case RESULT_FAIL: return "FAIL";
    case RESULT_RUNNING: return " ...";
    default: return "    ";
    }
}
static void main_draw_row(int i) {
    const test_entry_t *e=&g_tests[i];
    uint16_t y=HDR_H+i*MAIN_ROW_H;
    uint16_t bg=(i&1)?C_DGRAY:C_BLACK;
    disp_fill(0,y,SCR_W,MAIN_ROW_H,bg);
    disp_text(DPAD, y+(MAIN_ROW_H-16)/2, e->name, C_WHITE, bg, 2);
    uint16_t bx=SCR_W-BADGE_W, bc=result_color(e->result);
    disp_fill(bx, y+2, BADGE_W-2, MAIN_ROW_H-4, bc);
    disp_text(bx+4, y+(MAIN_ROW_H-16)/2, result_label(e->result), C_BLACK, bc, 2);
}
static void main_draw_all(void) {
    disp_fill(0,0,SCR_W,HDR_H,C_NAVY);
    disp_text(DPAD,(HDR_H-16)/2,"LoRa RevB Test",C_WHITE,C_NAVY,2);
    for(int i=0;i<N_TESTS;i++) main_draw_row(i);
    disp_fill(0,RUNALL_Y,SCR_W,RUNALL_H,C_TEAL);
    int tx=(SCR_W-7*16)/2;
    disp_text(tx,RUNALL_Y+(RUNALL_H-16)/2,"RUN ALL",C_WHITE,C_TEAL,2);
}
static void run_test(int i) {
    test_entry_t *e=&g_tests[i];
    e->result=RESULT_RUNNING; e->detail[0]='\0';
    main_draw_row(i);
    ESP_LOGI(TAG,"[%d/%d] %s",i+1,N_TESTS,e->name);
    e->fn(e);
    main_draw_row(i);
    ESP_LOGI(TAG,"  %s  %s",result_label(e->result),e->detail);
}
static void run_all(void) {
    disp_fill(0,RUNALL_Y,SCR_W,RUNALL_H,C_YELLOW);
    disp_text((SCR_W-7*16)/2,RUNALL_Y+(RUNALL_H-16)/2,"RUN ALL",C_BLACK,C_YELLOW,2);
    for(int i=0;i<N_TESTS;i++) run_test(i);
    disp_fill(0,RUNALL_Y,SCR_W,RUNALL_H,C_TEAL);
    disp_text((SCR_W-7*16)/2,RUNALL_Y+(RUNALL_H-16)/2,"RUN ALL",C_WHITE,C_TEAL,2);
}

/* =========================================================================
 * Navigation helpers
 * ========================================================================= */
static void open_detail(int idx)
{
    g_detail_idx = idx;
    g_screen     = SCREEN_DETAIL;
    g_last_refresh_ms = 0;
    g_dot_x = -1; g_dot_y = -1;
    disp_fill(0, 0, SCR_W, SCR_H, C_BLACK);  /* wipe leftover main-menu rows */
    g_detail_draw[idx]();
    /* Trigger first refresh immediately for live tests */
    if (g_detail_refresh[idx]) g_detail_refresh[idx]();
}

static void back_to_main(void)
{
    g_screen = SCREEN_MAIN;
    main_draw_all();
}

/* =========================================================================
 * Entry point
 * ========================================================================= */
void app_main(void)
{
    ESP_LOGI(TAG,"LoRa Device RevB Board Test v2");

    /* NVS (needed for touch calibration) */
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase(); nvs_flash_init();}

    init_gpio();
    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_mcp());
    ESP_ERROR_CHECK(lora_bus_init());
    ESP_ERROR_CHECK(touch_dev_init());
    ESP_ERROR_CHECK(display_bus_init());
    ili9341_init();

    /* Load calibration from NVS (or keep default) */
    cal_load();
    if(!g_cal.valid) ESP_LOGW(TAG,"No touch calibration – tap Touch→CALIBRATE");

    /* Splash */
    disp_fill(0,0,SCR_W,SCR_H,C_NAVY);
    disp_text(10,40,"LoRa Device RevB",C_WHITE,C_NAVY,2);
    disp_text(10,72,"Board Test v2",C_TEAL,C_NAVY,2);
    if(!g_cal.valid)
        disp_text(10,104,"No touch cal!",C_ORANGE,C_NAVY,2);
    vTaskDelay(pdMS_TO_TICKS(800));

    main_draw_all();
    run_all();

    ESP_LOGI(TAG,"Auto-run done. Touch a row for details.");

    for(;;) {
        touch_pt_t tp = touch_read();

        if (g_screen == SCREEN_MAIN) {
            if (tp.pressed) {
                touch_wait_release();
                if (tp.y < HDR_H) {
                    /* nothing */
                } else if (tp.y >= RUNALL_Y) {
                    run_all();
                } else {
                    int idx = (tp.y - HDR_H) / MAIN_ROW_H;
                    if (idx >= 0 && idx < N_TESTS) open_detail(idx);
                }
            }
        } else { /* SCREEN_DETAIL */
            /* Auto-refresh */
            uint32_t now_ms = (uint32_t)(esp_timer_get_time()/1000);
            uint32_t iv = g_refresh_ms[g_detail_idx];
            if (iv && g_detail_refresh[g_detail_idx] &&
                now_ms - g_last_refresh_ms >= iv) {
                g_detail_refresh[g_detail_idx]();
                g_last_refresh_ms = now_ms;
            }
            /* Touch */
            if (tp.pressed) {
                if (tp.y < HDR_H) {
                    /* back button */
                    touch_wait_release();
                    back_to_main();
                } else {
                    touch_wait_release();
                    handle_detail_touch(g_detail_idx, &tp);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
