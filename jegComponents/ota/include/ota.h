#ifndef OTA_H
#define OTA_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

typedef enum {
    no_need_ota,
    ota_ing,
    ota_fail,
} ota_state;

ota_state get_ota_status(void);

void set_ota_status(ota_state _ota_state);

void start_ota(void);

#endif