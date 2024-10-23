#include "ota.h"
#include "global_message.h"
#define TAG "OTA"

ota_state m_ota_state = no_need_ota;

ota_state get_ota_status(void)
{
    return m_ota_state;
}

void set_ota_status(ota_state _ota_state)
{
    m_ota_state = _ota_state;
}

esp_err_t simple_http_event_handler(esp_http_client_event_t *evt)
{
    return ESP_OK;
}

void simple_ota_example_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
        .url = get_global_data()->newest_firmware_url,
        .event_handler = simple_http_event_handler,
    };

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        m_ota_state = ota_fail;
        ESP_LOGE(TAG, "Firmware upgrade failed");
        vTaskDelete(NULL); // 删除当前任务
    }

    // 如果删除任务失败，则这个代码不会被执行
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGE(TAG, "OTA TASK IS NOT DELETE");
    }
}

void start_ota(void)
{
    m_ota_state = ota_ing;
    xTaskCreate(&simple_ota_example_task, "ota_task", 4096, NULL, 10, NULL);
    while(m_ota_state != ota_fail)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
