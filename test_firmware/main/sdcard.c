#include "sdcard.h"
#include "board_config.h"
#include "mcp23017.h"

#include <stdio.h>
#include <inttypes.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

void test_sd_card(test_entry_t *t)
{
    uint8_t gpb=0; mcp_read(MCP_GPIOB,&gpb);
    if(gpb&GPB_SD_DET){
        snprintf(t->detail,sizeof(t->detail),"no card (CD)"); t->result=RESULT_WARN; return;}
    esp_vfs_fat_sdmmc_mount_config_t mc={.format_if_mount_failed=false,.max_files=4,.allocation_unit_size=16*1024};
    sdmmc_card_t *card=NULL;
    sdmmc_host_t host=SDSPI_HOST_DEFAULT(); host.slot=SPI_B_HOST;
    sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs=PIN_SD_CS; slot.host_id=SPI_B_HOST;
    if(esp_vfs_fat_sdspi_mount("/sdcard",&host,&slot,&mc,&card)!=ESP_OK){
        snprintf(t->detail,sizeof(t->detail),"mount failed"); t->result=RESULT_FAIL; return;}
    FILE *f=fopen("/sdcard/revb_test.txt","w");
    bool wr=(f!=NULL);
    if(f){fprintf(f,"RevB board test\n");fclose(f);}
    uint32_t mb=(uint32_t)(card->csd.capacity>>11);
    snprintf(g_live.sd_type,sizeof(g_live.sd_type),"%s",
             card->ocr&(1<<30)?"SDHC":"SDSC");
    g_live.sd_mb=mb; g_live.sd_write_ok=wr;
    esp_vfs_fat_sdcard_unmount("/sdcard",card);
    snprintf(t->detail,sizeof(t->detail),"%"PRIu32"MB %s wr=%s",mb,g_live.sd_type,wr?"OK":"FAIL");
    t->result=wr?RESULT_PASS:RESULT_FAIL;
}
