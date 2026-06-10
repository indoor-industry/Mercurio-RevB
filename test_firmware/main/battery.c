#include "battery.h"
#include "board_config.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static adc_oneshot_unit_handle_t g_bat_adc = NULL;

adc_oneshot_unit_handle_t bat_adc_get(void)
{
    if (!g_bat_adc) {
        const adc_oneshot_unit_init_cfg_t ic={.unit_id=BAT_ADC_UNIT};
        if (adc_oneshot_new_unit(&ic,&g_bat_adc)==ESP_OK) {
            const adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
            adc_oneshot_config_channel(g_bat_adc,BAT_ADC_CHAN,&cc);
        }
    }
    return g_bat_adc;
}

void test_battery(test_entry_t *t)
{
    adc_oneshot_unit_handle_t h=bat_adc_get();
    if(!h){
        snprintf(t->detail,sizeof(t->detail),"ADC init failed"); t->result=RESULT_FAIL; return;}
    int sum=0;
    for(int i=0;i<16;i++){int r=0;adc_oneshot_read(h,BAT_ADC_CHAN,&r);sum+=r;vTaskDelay(pdMS_TO_TICKS(5));}
    float vpin=(float)(sum/16)*3.1f/4095.0f;
    float vbat=vpin*2.0f;
    g_live.bat_v=vbat; g_live.bat_raw=sum/16;
    snprintf(t->detail,sizeof(t->detail),"%.2fV",vbat);
    t->result=(vbat>2.8f&&vbat<4.35f)?RESULT_PASS:(vbat<0.1f)?RESULT_WARN:RESULT_FAIL;
}
