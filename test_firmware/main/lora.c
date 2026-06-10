#include "lora.h"
#include "board_config.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"

static const char *TAG = "lora";

static spi_device_handle_t g_lora_spi;

#define LORA_IRQ_TXDONE  0x0001
#define LORA_IRQ_TIMEOUT 0x0200

esp_err_t lora_bus_init(void)
{
    const spi_bus_config_t bus={
        .mosi_io_num=PIN_LORA_MOSI,.miso_io_num=PIN_LORA_MISO,
        .sclk_io_num=PIN_LORA_SCLK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=256};
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI_A_HOST,&bus,SPI_DMA_CH_AUTO),TAG,"spa");
    const spi_device_interface_config_t ld={.clock_speed_hz=8*1000*1000,.mode=0,
        .spics_io_num=PIN_LORA_CS,.queue_size=4};
    return spi_bus_add_device(SPI_A_HOST,&ld,&g_lora_spi);
}

static void lora_wait_busy(void)
{
    int64_t dl=esp_timer_get_time()+200000LL;
    while(gpio_get_level(PIN_LORA_BUSY)&&esp_timer_get_time()<dl)
        esp_rom_delay_us(100);
}
static void lora_cmd(const uint8_t *tx, uint8_t *rx, size_t len)
{
    lora_wait_busy();
    spi_transaction_t t={.length=len*8,.tx_buffer=tx,.rx_buffer=rx};
    spi_device_polling_transmit(g_lora_spi,&t);
}
static uint8_t lora_status(void)
{
    uint8_t tx[2]={0xC0,0}, rx[2]={0};
    lora_cmd(tx,rx,2); return rx[1];
}

/* --- TX command set (used by the "send test packet" feature) --- */
static void lora_set_standby(uint8_t mode)
{
    uint8_t tx[2]={0x80,mode}; lora_cmd(tx,NULL,2);
}
static void lora_set_packet_type(uint8_t type)
{
    uint8_t tx[2]={0x8A,type}; lora_cmd(tx,NULL,2);
}
static void lora_set_rf_frequency(double freq_hz)
{
    uint32_t f=(uint32_t)(freq_hz*(double)(1ULL<<25)/32000000.0);
    uint8_t tx[5]={0x86,(uint8_t)(f>>24),(uint8_t)(f>>16),(uint8_t)(f>>8),(uint8_t)f};
    lora_cmd(tx,NULL,5);
}
static void lora_set_pa_config(uint8_t duty, uint8_t hp_max, uint8_t dev_sel, uint8_t lut)
{
    uint8_t tx[5]={0x95,duty,hp_max,dev_sel,lut}; lora_cmd(tx,NULL,5);
}
static void lora_set_tx_params(int8_t power, uint8_t ramp)
{
    uint8_t tx[3]={0x8E,(uint8_t)power,ramp}; lora_cmd(tx,NULL,3);
}
static void lora_set_buffer_base(uint8_t tx_base, uint8_t rx_base)
{
    uint8_t tx[3]={0x8F,tx_base,rx_base}; lora_cmd(tx,NULL,3);
}
static void lora_write_buffer(uint8_t offset, const uint8_t *data, uint8_t len)
{
    uint8_t tx[2+16]; tx[0]=0x0E; tx[1]=offset;
    if(len>16) len=16;
    memcpy(&tx[2],data,len);
    lora_cmd(tx,NULL,2u+len);
}
static void lora_set_modulation_params(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro)
{
    uint8_t tx[5]={0x8B,sf,bw,cr,ldro}; lora_cmd(tx,NULL,5);
}
static void lora_set_packet_params(uint16_t preamble, uint8_t header,
                                    uint8_t len, uint8_t crc, uint8_t iq)
{
    uint8_t tx[7]={0x8C,(uint8_t)(preamble>>8),(uint8_t)preamble,header,len,crc,iq};
    lora_cmd(tx,NULL,7);
}
static void lora_set_dio_irq_params(uint16_t irq, uint16_t dio1, uint16_t dio2, uint16_t dio3)
{
    uint8_t tx[9]={0x08,(uint8_t)(irq>>8),(uint8_t)irq,(uint8_t)(dio1>>8),(uint8_t)dio1,
                   (uint8_t)(dio2>>8),(uint8_t)dio2,(uint8_t)(dio3>>8),(uint8_t)dio3};
    lora_cmd(tx,NULL,9);
}
static void lora_set_tx(uint32_t timeout)
{
    uint8_t tx[4]={0x83,(uint8_t)(timeout>>16),(uint8_t)(timeout>>8),(uint8_t)timeout};
    lora_cmd(tx,NULL,4);
}
static uint16_t lora_get_irq_status(void)
{
    uint8_t tx[4]={0x12,0,0,0}, rx[4]={0};
    lora_cmd(tx,rx,4);
    return ((uint16_t)rx[2]<<8)|rx[3];
}
static void lora_clear_irq_status(uint16_t mask)
{
    uint8_t tx[3]={0x02,(uint8_t)(mask>>8),(uint8_t)mask}; lora_cmd(tx,NULL,3);
}

void test_lora(test_entry_t *t)
{
    gpio_set_level(PIN_LORA_RST,0); vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(PIN_LORA_RST,1); gpio_set_level(PIN_LORA_RFEN,1);
    vTaskDelay(pdMS_TO_TICKS(20));
    int64_t tl=esp_timer_get_time();
    while(gpio_get_level(PIN_LORA_BUSY)){
        if(esp_timer_get_time()-tl>3000000LL){
            snprintf(t->detail,sizeof(t->detail),"BUSY stuck");
            t->result=RESULT_FAIL; return;}}
    uint8_t s=lora_status();
    if(!s||s==0xFF){
        snprintf(t->detail,sizeof(t->detail),"SPI fail 0x%02X",s);
        t->result=RESULT_FAIL; return;}
    g_live.lora_status=s;
    uint8_t mode=(s>>4)&7;
    snprintf(t->detail,sizeof(t->detail),"stat=0x%02X mode=%u",s,mode);
    /* After reset the chip boots into STBY_RC (mode 2); that's our PASS check */
    t->result=(mode==2)?RESULT_PASS:RESULT_WARN;
    uint8_t sleep_cmd[2]={0x84,0x00};
    lora_cmd(sleep_cmd,NULL,2);
}

/* Configure radio for LoRa and transmit a short test packet.
 * Returns true if the chip reports TxDone within the timeout. */
bool lora_send_test_packet(char *out, size_t out_sz)
{
    static const char msg[] = "BOARDTEST";
    const uint8_t len=(uint8_t)(sizeof(msg)-1);

    gpio_set_level(PIN_LORA_RFEN,1);
    lora_set_standby(0x00);                       /* STDBY_RC */
    lora_set_packet_type(0x01);                   /* LoRa */
    lora_set_rf_frequency(LORA_TEST_FREQ_HZ);
    lora_set_pa_config(0x02,0x02,0x00,0x01);       /* modest power, SX1262 */
    lora_set_tx_params(10,0x04);                  /* 10 dBm, 200us ramp */
    lora_set_buffer_base(0,0);
    lora_write_buffer(0,(const uint8_t*)msg,len);
    lora_set_modulation_params(7,0x07,0x01,0);     /* SF7, BW125, CR4/5 */
    lora_set_packet_params(8,0x00,len,0x01,0x00);  /* preamble8, explicit, CRC on */
    lora_clear_irq_status(0xFFFF);
    lora_set_dio_irq_params(LORA_IRQ_TXDONE|LORA_IRQ_TIMEOUT,
                             LORA_IRQ_TXDONE|LORA_IRQ_TIMEOUT,0,0);
    lora_set_tx(0x00FA00);                         /* ~1s HW timeout */

    bool done=false, timeout=false;
    int64_t dl=esp_timer_get_time()+2000000LL;
    while(esp_timer_get_time()<dl){
        uint16_t irq=lora_get_irq_status();
        if(irq&LORA_IRQ_TXDONE){ done=true; break; }
        if(irq&LORA_IRQ_TIMEOUT){ timeout=true; break; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lora_clear_irq_status(0xFFFF);
    lora_set_standby(0x00);

    if(done)
        snprintf(out,out_sz,"TX OK \"%s\" @%.1fMHz",msg,LORA_TEST_FREQ_HZ/1e6);
    else
        snprintf(out,out_sz,"TX %s",timeout?"timeout":"no IRQ");
    return done;
}
