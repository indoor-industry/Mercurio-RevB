/*
 * WiFi Test
 */

#ifndef WIFI_TEST_H
#define WIFI_TEST_H

#include <stdbool.h>

void test_wifi(void);
bool wifi_test_ok(void);
int wifi_get_networks_found(void);

#endif // WIFI_TEST_H
