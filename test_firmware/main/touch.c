#include "touch.h"
#include "board_config.h"
#include "display.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TOUCH_Z_THRES 80
#define NVS_NS  "board_test"
#define NVS_KEY "touch_cal"

static spi_device_handle_t g_touch_spi;

touch_cal_t g_cal = {
    /* default linear – calibration will replace this */
    .ax= (float)SCR_W /3600.0f, .bx=0.0f, .cx=-(float)SCR_W *200.0f/3600.0f,
    .ay=0.0f, .by= (float)SCR_H /3600.0f, .cy=-(float)SCR_H *200.0f/3600.0f,
    .valid=false,
};

esp_err_t touch_dev_init(void)
{
    const spi_device_interface_config_t td={.clock_speed_hz=2*1000*1000,.mode=0,
        .spics_io_num=PIN_TOUCH_CS,.queue_size=4};
    return spi_bus_add_device(SPI_A_HOST,&td,&g_touch_spi);
}

static uint16_t touch_raw(uint8_t ctrl)
{
    uint8_t tx[3]={ctrl,0,0}, rx[3]={0};
    spi_transaction_t t={.length=24,.tx_buffer=tx,.rx_buffer=rx};
    spi_device_polling_transmit(g_touch_spi, &t);
    return (((uint16_t)rx[1]<<8)|rx[2])>>3;
}

static void touch_sample(uint16_t *rx_out, uint16_t *ry_out, int n)
{
    uint32_t rx=0, ry=0;
    for(int i=0;i<n;i++){ rx+=touch_raw(0xD0); ry+=touch_raw(0x90); }
    *rx_out=(uint16_t)(rx/n);
    *ry_out=(uint16_t)(ry/n);
}

touch_pt_t touch_read(void)
{
    touch_pt_t pt={0};
    uint16_t z1=touch_raw(0xB0), z2=touch_raw(0xC0);
    pt.pressed=(z1>TOUCH_Z_THRES)&&(z2<(4096-TOUCH_Z_THRES));
    if(!pt.pressed) return pt;
    touch_sample(&pt.raw_x,&pt.raw_y,8);
    float sx=g_cal.ax*(float)pt.raw_x + g_cal.bx*(float)pt.raw_y + g_cal.cx;
    float sy=g_cal.ay*(float)pt.raw_x + g_cal.by*(float)pt.raw_y + g_cal.cy;
    pt.x=(int16_t)(sx<0?0:sx>=SCR_W?SCR_W-1:(int16_t)sx);
    pt.y=(int16_t)(sy<0?0:sy>=SCR_H?SCR_H-1:(int16_t)sy);
    pt.z=(int16_t)z1;
    return pt;
}

void touch_wait_release(void)
{
    while(touch_read().pressed) vTaskDelay(pdMS_TO_TICKS(20));
}

void cal_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    touch_cal_t tmp; size_t sz=sizeof(tmp);
    if (nvs_get_blob(h,"touch_cal",&tmp,&sz)==ESP_OK && sz==sizeof(tmp) && tmp.valid)
        g_cal=tmp;
    nvs_close(h);
}
void cal_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY, &g_cal, sizeof(g_cal));
    nvs_commit(h); nvs_close(h);
}

void test_touch_hw(test_entry_t *t)
{
    uint16_t y0=DCONTENT_Y+8*MAIN_ROW_H;
    disp_fill(0,y0,SCR_W-BADGE_W,MAIN_ROW_H,C_BLACK);
    disp_text(DPAD,y0+(MAIN_ROW_H-16)/2,"Tap screen…",C_YELLOW,C_BLACK,2);
    touch_pt_t best={0};
    int64_t dl=esp_timer_get_time()+4000000LL;
    while(esp_timer_get_time()<dl){
        touch_pt_t p=touch_read();
        if(p.pressed&&p.z>best.z) best=p;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if(best.pressed){
        snprintf(t->detail,sizeof(t->detail),"X=%d Y=%d Z=%d",best.x,best.y,best.z);
        t->result=RESULT_PASS;
    } else {
        snprintf(t->detail,sizeof(t->detail),"no touch");
        t->result=RESULT_WARN;
    }
}
