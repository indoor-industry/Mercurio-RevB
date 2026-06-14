/*
 * GPS Driver (L70-R via UART)
 */

#ifndef GPS_H
#define GPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GPS_BUF_SIZE 256

typedef struct {
    bool valid;
    double latitude;
    double longitude;
    int satellites;
    char fix_quality;
    char time_str[12];
} gps_position_t;

void gps_init(void);
int gps_read(char *buf, size_t max_len, uint32_t timeout_ms);
void gps_process_buffer(const char *buf);
const gps_position_t* gps_get_position(void);

#endif // GPS_H
