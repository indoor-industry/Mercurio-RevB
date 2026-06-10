#include "mcp23017.h"
#include "board_config.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"

static const char *TAG = "mcp23017";

static i2c_master_bus_handle_t g_i2c_bus;
static i2c_master_dev_handle_t g_mcp_dev;

esp_err_t init_i2c(void)
{
    const i2c_master_bus_config_t bc={
        .i2c_port=I2C_NUM_0,.sda_io_num=PIN_I2C_SDA,.scl_io_num=PIN_I2C_SCL,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,
        .flags.enable_internal_pullup=true};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc,&g_i2c_bus),TAG,"i2c_bus");
    const i2c_device_config_t dc={
        .dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=MCP_ADDR,.scl_speed_hz=400000};
    return i2c_master_bus_add_device(g_i2c_bus,&dc,&g_mcp_dev);
}

esp_err_t init_mcp(void)
{
    gpio_set_level(PIN_EXP_RST,1); vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(mcp_write(MCP_IODIRA,0xD2),TAG,"iodira");
    ESP_RETURN_ON_ERROR(mcp_write(MCP_IODIRB,0xFF),TAG,"iodirb");
    ESP_RETURN_ON_ERROR(mcp_write(MCP_GPPUA, GPA_SW1),TAG,"gppua");
    ESP_RETURN_ON_ERROR(mcp_write(MCP_GPPUB, GPB_SW2|GPB_TOUCH_IRQ),TAG,"gppub");
    return mcp_write(MCP_OLATA, GPA_GPS_RST|GPA_DISP_RST);
}

esp_err_t mcp_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2]={reg,val};
    return i2c_master_transmit(g_mcp_dev, buf, 2, 100);
}
esp_err_t mcp_read(uint8_t reg, uint8_t *out)
{
    return i2c_master_transmit_receive(g_mcp_dev, &reg, 1, out, 1, 100);
}
esp_err_t mcp_read2(uint8_t *gpa, uint8_t *gpb)
{
    if (mcp_read(MCP_GPIOA, gpa) != ESP_OK) return ESP_FAIL;
    return mcp_read(MCP_GPIOB, gpb);
}

void test_mcp23017(test_entry_t *t)
{
    uint8_t gpa=0,gpb=0;
    if(mcp_read2(&gpa,&gpb)!=ESP_OK){
        snprintf(t->detail,sizeof(t->detail),"I2C no response"); t->result=RESULT_FAIL; return;}
    snprintf(t->detail,sizeof(t->detail),"GPA=%02X GPB=%02X SW1=%d SW2=%d",
             gpa,gpb,(gpa&GPA_SW1)?0:1,(gpb&GPB_SW2)?0:1);
    t->result=RESULT_PASS;
}
