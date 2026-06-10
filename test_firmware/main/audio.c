#include "audio.h"
#include "board_config.h"
#include "display.h"
#include "mcp23017.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double mic_read_chunk(i2s_chan_handle_t rx, int32_t *buf, int n, double *peak_out)
{
    size_t got=0;
    uint32_t timeout_ms=(uint32_t)(n*1000/16000)+200;
    i2s_channel_read(rx,buf,n*4,&got,pdMS_TO_TICKS(timeout_ms));
    int cnt=(int)(got/4);
    double s2=0,pk=0;
    for(int i=0;i<cnt;i++){
        double s=(double)(buf[i]>>8)/(1<<23);
        s2+=s*s; if(fabs(s)>pk) pk=fabs(s);
    }
    if(peak_out) *peak_out=pk;
    return cnt?sqrt(s2/cnt):0;
}

static esp_err_t mic_open(i2s_chan_handle_t *rx)
{
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    cc.dma_desc_num=4; cc.dma_frame_num=256;
    esp_err_t err=i2s_new_channel(&cc,NULL,rx);
    if(err!=ESP_OK) return err;
    i2s_std_config_t sc={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=PIN_I2S_BCLK,
                   .ws=PIN_I2S_LRCLK,.dout=I2S_GPIO_UNUSED,.din=PIN_I2S_DIN}};
    i2s_channel_init_std_mode(*rx,&sc);
    i2s_channel_enable(*rx);
    return ESP_OK;
}

void test_microphone(test_entry_t *t)
{
    i2s_chan_handle_t rx;
    if(mic_open(&rx)!=ESP_OK){
        snprintf(t->detail,sizeof(t->detail),"i2s fail"); t->result=RESULT_FAIL; return;}
    const int N=8000;
    int32_t *buf=malloc(N*4);
    double rms=0,peak=0;
    if(buf){
        rms=mic_read_chunk(rx,buf,N,&peak);
        free(buf);
    }
    i2s_channel_disable(rx); i2s_del_channel(rx);
    g_live.mic_rms=(float)rms; g_live.mic_peak=(float)peak;
    snprintf(t->detail,sizeof(t->detail),"RMS=%.5f peak=%.5f",(float)rms,(float)peak);
    t->result=rms>0.0001?RESULT_PASS:RESULT_WARN;
}

/* Active test: measure the ambient noise floor, then prompt the user to
 * make noise and compare the peak level against that floor. PASS only if
 * a clearly louder signal was detected, so a dead mic can't pass on
 * electrical noise alone. */
void mic_run_test(test_entry_t *t)
{
    i2s_chan_handle_t rx;
    if(mic_open(&rx)!=ESP_OK){
        snprintf(t->detail,sizeof(t->detail),"i2s fail"); t->result=RESULT_FAIL; return;}

    const int N=1600; /* 100ms chunks @ 16kHz */
    int32_t *buf=malloc(N*4);
    if(!buf){
        i2s_channel_disable(rx); i2s_del_channel(rx);
        snprintf(t->detail,sizeof(t->detail),"out of mem"); t->result=RESULT_FAIL; return;}

    drow(5,C_YELLOW,C_BLACK,"Stay quiet...");
    drow(6,C_YELLOW,C_BLACK,"measuring noise floor");
    double floor_rms=0;
    for(int c=0;c<4;c++) floor_rms+=mic_read_chunk(rx,buf,N,NULL);
    floor_rms/=4.0;

    detail_btn(5,2,"SPEAK / CLAP NOW!",C_ORANGE);
    double peak_rms=0,abs_peak=0;
    for(int c=0;c<8;c++){
        double pk=0;
        double r=mic_read_chunk(rx,buf,N,&pk);
        if(r>peak_rms)  peak_rms=r;
        if(pk>abs_peak) abs_peak=pk;
    }
    free(buf);
    i2s_channel_disable(rx); i2s_del_channel(rx);

    g_live.mic_rms=(float)peak_rms; g_live.mic_peak=(float)abs_peak;
    float ratio=(float)(peak_rms/(floor_rms>1e-6?floor_rms:1e-6));
    snprintf(t->detail,sizeof(t->detail),"floor=%.5f peak=%.5f x%.1f",
             floor_rms,peak_rms,ratio);
    t->result=(peak_rms>0.001 && ratio>3.0f)?RESULT_PASS:RESULT_WARN;
}

void play_tone(float freq, int ms)
{
    uint8_t olat=0; mcp_read(MCP_OLATA,&olat);
    mcp_write(MCP_OLATA,olat|GPA_AUDIO_EN); vTaskDelay(pdMS_TO_TICKS(10));
    i2s_chan_handle_t tx;
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    cc.dma_desc_num=4; cc.dma_frame_num=256;
    if(i2s_new_channel(&cc,&tx,NULL)!=ESP_OK){
        mcp_write(MCP_OLATA,olat&~GPA_AUDIO_EN); return;}
    i2s_std_config_t sc={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=PIN_I2S_BCLK,
                   .ws=PIN_I2S_LRCLK,.dout=PIN_I2S_DOUT,.din=I2S_GPIO_UNUSED}};
    i2s_channel_init_std_mode(tx,&sc); i2s_channel_enable(tx);
    int n=16000*ms/1000;
    int16_t *t=malloc(n*2); size_t wr=0;
    if(t){
        if(freq<0){
            /* sweep 440→2000 Hz */
            float phase=0;
            for(int i=0;i<n;i++){
                float f=440.0f+(2000.0f-440.0f)*(float)i/n;
                phase+=2.0f*(float)M_PI*f/16000.0f;
                t[i]=(int16_t)(16000.0f*sinf(phase));
            }
        } else {
            for(int i=0;i<n;i++) t[i]=(int16_t)(16000.0f*sinf(2.0f*(float)M_PI*freq*i/16000.0f));
        }
        i2s_channel_write(tx,t,n*2,&wr,pdMS_TO_TICKS((uint32_t)ms+500)); free(t);
    }
    i2s_channel_disable(tx); i2s_del_channel(tx);
    mcp_read(MCP_OLATA,&olat); mcp_write(MCP_OLATA,olat&~GPA_AUDIO_EN);
}

void test_speaker(test_entry_t *t)
{
    uint8_t olat=0; mcp_read(MCP_OLATA,&olat);
    mcp_write(MCP_OLATA,olat|GPA_AUDIO_EN); vTaskDelay(pdMS_TO_TICKS(10));
    i2s_chan_handle_t tx;
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    cc.dma_desc_num=4; cc.dma_frame_num=256;
    if(i2s_new_channel(&cc,&tx,NULL)!=ESP_OK){
        snprintf(t->detail,sizeof(t->detail),"i2s fail");
        mcp_write(MCP_OLATA,olat&~GPA_AUDIO_EN); t->result=RESULT_FAIL; return;}
    i2s_std_config_t sc={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=PIN_I2S_BCLK,
                   .ws=PIN_I2S_LRCLK,.dout=PIN_I2S_DOUT,.din=I2S_GPIO_UNUSED}};
    i2s_channel_init_std_mode(tx,&sc); i2s_channel_enable(tx);
    const int N=9600;
    int16_t *tone=malloc(N*2); size_t wr=0;
    if(tone){
        for(int i=0;i<N;i++) tone[i]=(int16_t)(16000.0*sin(2.0*M_PI*440.0*i/16000.0));
        i2s_channel_write(tx,tone,N*2,&wr,pdMS_TO_TICKS(3000)); free(tone);
    }
    i2s_channel_disable(tx); i2s_del_channel(tx);
    mcp_read(MCP_OLATA,&olat); mcp_write(MCP_OLATA,olat&~GPA_AUDIO_EN);
    snprintf(t->detail,sizeof(t->detail),"440Hz %zu B",wr);
    t->result=wr>0?RESULT_PASS:RESULT_FAIL;
}
