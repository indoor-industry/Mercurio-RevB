#include "gps.h"
#include "board_config.h"
#include "mcp23017.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "gps";

/* =========================================================================
 * NMEA parser (minimal: GGA + RMC)
 * ========================================================================= */
static float nmea_coord(const char *s, const char *dir)
{
    /* ddmm.mmmm or dddmm.mmmm → decimal degrees */
    if (!s || !*s) return 0.0f;
    float raw=(float)atof(s);
    int deg=(int)(raw/100);
    float min=raw-(float)deg*100;
    float dd=(float)deg+min/60.0f;
    if (dir && (dir[0]=='S'||dir[0]=='W')) dd=-dd;
    return dd;
}
static void nmea_parse_gga(const char *line, gps_data_t *g)
{
    char buf[128]; strncpy(buf,line,sizeof(buf)-1);
    char *f[15]={0}; int fi=0;
    char *p=buf;
    f[fi++]=p;
    while(*p&&fi<15){ if(*p==','){ *p='\0'; f[fi++]=p+1; } p++; }
    if(fi<10) return;
    int quality=atoi(f[6]); g->fix=(quality>0);
    g->lat=nmea_coord(f[2],f[3]);
    g->lon=nmea_coord(f[4],f[5]);
    g->sats_used=atoi(f[7]);
    g->hdop=(float)atof(f[8]);
    g->alt_m=(float)atof(f[9]);
    /* time from f[1]: HHMMSS.ss */
    if(strlen(f[1])>=6){
        snprintf(g->time_str,sizeof(g->time_str),"%c%c:%c%c:%c%c",
                 f[1][0],f[1][1],f[1][2],f[1][3],f[1][4],f[1][5]);
    }
}
static void nmea_parse_rmc(const char *line, gps_data_t *g)
{
    char buf[128]; strncpy(buf,line,sizeof(buf)-1);
    char *f[15]={0}; int fi=0; char *p=buf;
    f[fi++]=p;
    while(*p&&fi<15){ if(*p==','){ *p='\0'; f[fi++]=p+1; } p++; }
    if(fi<8) return;
    g->speed_kmh=(float)atof(f[7])*1.852f; /* knots→km/h */
}

void test_gps(test_entry_t *t)
{
    uint8_t olat=0; mcp_read(MCP_OLATA,&olat);
    mcp_write(MCP_OLATA,olat|GPA_GPS_RST|GPA_GPS_WKUP);
    vTaskDelay(pdMS_TO_TICKS(100));
    const uart_config_t uc={.baud_rate=9600,.data_bits=UART_DATA_8_BITS,
        .parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    uart_driver_install(GPS_UART,512,0,0,NULL,0);
    uart_param_config(GPS_UART,&uc);
    uart_set_pin(GPS_UART,PIN_GPS_TX,PIN_GPS_RX,-1,-1);
    char line[128]; int pos=0,found=0;
    int64_t dl=esp_timer_get_time()+6000000LL;
    gps_data_t gd={0};
    while(esp_timer_get_time()<dl&&found<8){
        uint8_t c;
        if(uart_read_bytes(GPS_UART,&c,1,pdMS_TO_TICKS(50))==1){
            if(c=='\n'||pos>=(int)sizeof(line)-1){
                line[pos]='\0';
                if(pos>5&&line[0]=='$'){
                    ESP_LOGI(TAG,"GPS: %s",line);
                    if(strncmp(line,"$GNGGA",6)==0||strncmp(line,"$GPGGA",6)==0)
                        nmea_parse_gga(line+1,&gd);
                    if(strncmp(line,"$GNRMC",6)==0||strncmp(line,"$GPRMC",6)==0)
                        nmea_parse_rmc(line+1,&gd);
                    found++;
                }
                pos=0;
            } else if(c!='\r') line[pos++]=(char)c;
        }
    }
    uart_driver_delete(GPS_UART);
    g_live.gps=gd; g_live.gps_valid=(found>0);
    if(found){
        if(gd.fix) snprintf(t->detail,sizeof(t->detail),"Fix %.4f,%.4f %ds",gd.lat,gd.lon,gd.sats_used);
        else        snprintf(t->detail,sizeof(t->detail),"%d sentences no fix",found);
        t->result=RESULT_PASS;
    } else {
        snprintf(t->detail,sizeof(t->detail),"no NMEA (antenna?)");
        t->result=RESULT_WARN;
    }
}
