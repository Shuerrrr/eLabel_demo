#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

void wifi_connect(void);

void fake_connect(void);

void wifi_disconnect(void);

void fake_disconnect(void);

void start_blufi(void);//随便调用内部有锁

uint8_t get_wifi_status(void);

void set_wifi_status(uint8_t status);

#endif