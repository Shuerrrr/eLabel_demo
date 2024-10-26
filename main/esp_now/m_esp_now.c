#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crc.h"
#include "m_esp_now.h"
#include "../main.h"

static const char *TAG = "espnow_example";

static uint16_t s_example_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = { 0, 0 };

static void example_espnow_deinit(example_espnow_send_param_t *send_param);

static void add_slave(uint8_t *mac_addr) {
    if (slave_num < MAX_SLAVES) {
        memcpy(slave_mac[slave_num], mac_addr, 6);  // Copy MAC address to slave list
        slave_num++;  // Increment the count of connected slaves
        printf("Add a new device to slave list, total:%d\n", slave_num);
    } else {
        printf("Slave list full, cannot add more devices.\n");
    }
}

/* Parse received ESPNOW data. */
int example_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        espnow_recv_buf.elabel_state = buf->elabel_state;
        espnow_recv_buf.task_method = buf->task_method;
        espnow_recv_buf.changeTaskId = buf->changeTaskId;
        espnow_recv_buf.msg_type = buf->msg_type;
        espnow_recv_buf.chosenTaskId = buf->chosenTaskId;
        espnow_recv_buf.TimeCountdown = buf->TimeCountdown;
        uint8_t i = 0;
        while(buf->payload[i] != 0)
        {
            espnow_recv_buf.payload[i] = buf->payload[i];
            i++;
        }
        espnow_recv_buf.payload[i] = 0;
        //打印所有buf成员变量
        if(espnow_recv_buf.msg_type == 5)
        {
            setUsing_wifi_channel(espnow_recv_buf.TimeCountdown);
        }
        printf("elabel_state = %d\n",espnow_recv_buf.elabel_state);
        printf("task_method = %d\n",espnow_recv_buf.task_method);
        printf("changeTaskId = %d\n",espnow_recv_buf.changeTaskId);
        printf("msg_type = %d\n",espnow_recv_buf.msg_type);
        printf("chosenTaskId = %d\n",espnow_recv_buf.chosenTaskId);
        printf("TimeCountdown = %d\n",espnow_recv_buf.TimeCountdown);
        printf("payload = %s\n",(char*)espnow_recv_buf.payload);
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void example_espnow_data_prepare(example_espnow_send_param_t *send_param)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(example_espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? EXAMPLE_ESPNOW_DATA_BROADCAST : EXAMPLE_ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_example_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    buf->elabel_state = send_param->elabel_state;
    buf->task_method = send_param->task_method;
    buf->changeTaskId = send_param->changeTaskId;
    buf->msg_type = send_param->msg_type;
    buf->chosenTaskId = send_param->chosenTaskId;
    buf->TimeCountdown = send_param->TimeCountdown;

    /* Fill all remaining bytes after the data with random values */
    // esp_fill_random(buf->payload, send_param->len - sizeof(example_espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    uint32_t recv_magic = 0;
    bool is_broadcast = false;
    int ret;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    // example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;

    if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        example_espnow_deinit(send_param);
        vTaskDelete(NULL);
    }

    while (1)
    {
        if(xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.id) {
                case EXAMPLE_ESPNOW_SEND_CB:
                {
                    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                    is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);

                    ESP_LOGD(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

                    if (is_broadcast && (send_param->state == 1)) {
                        break;
                    }

                    // if (!is_broadcast) {
                    //     if (send_param->count == 0) {
                    //         // ESP_LOGI(TAG, "pair");
                    //         printf("slave_num = %d\n", slave_num);
                    //         // example_espnow_deinit(send_param);
                    //         // vTaskDelete(NULL);
                    //         break;
                    //     }
                    //     send_param->count--;
                    // }
                    example_espnow_data_prepare(send_param);
                    if(is_broadcast)
                    {
                        vTaskDelay(send_param->delay/portTICK_PERIOD_MS);
                        /* Delay a while before sending the next data. */
                        ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                        memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);

                        /* Send the next data after the previous data is sent. */
                        printf("send_param->len = %d\n",send_param->len);
                        if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                            ESP_LOGE(TAG, "Send error");
                            // example_espnow_deinit(send_param);
                            // vTaskDelete(NULL);
                            break;
                        }
                        break;
                    }
                    else
                    {
                        for(int i = 0; i< slave_num;i++)
                        {
                            if(espnow_send_buf.espnow_callback_flag == 1)
                            {
                                uint8_t *mac_addrrr = slave_mac[i];
                                uint8_t *recv_mac_addr = send_cb->mac_addr;
                                for(int i = 0; i < 6; i++)
                                {
                                    ESP_LOGE("fuck","%d",recv_mac_addr[i]);
                                    ESP_LOGE("fuck","%d",mac_addrrr[i]);
                                }
                                if(memcmp(mac_addrrr,recv_mac_addr,6) == 0)
                                {
                                    ESP_LOGE("fuck","fuck");
                                    espnow_send_buf.espnow_callback_flag = 0;
                                    continue;
                                }
                            }
                            vTaskDelay(20 / portTICK_PERIOD_MS);
                            uint8_t *mac_addrrr = slave_mac[i];
                            ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(mac_addrrr));
                            memcpy(send_param->dest_mac, mac_addrrr, ESP_NOW_ETH_ALEN);

                            /* Send the next data after the previous data is sent. */
                            printf("send_param->len = %d\n",send_param->len);

                            if (send_param->dest_mac == NULL) {
                                ESP_LOGE(TAG, "Send callback: MAC address is NULL");
                                return;
                            }
                            else if(send_param->buffer == NULL) {
                                ESP_LOGE(TAG, "Send callback: data is NULL");
                                return;
                            }
                            else
                            {
                                ESP_LOGE(TAG,"Send callback: MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
                                send_param->dest_mac[0], send_param->dest_mac[1],send_param->dest_mac[2], send_param->dest_mac[3], send_param->dest_mac[4], send_param->dest_mac[5]);
                                for (int i = 0; i < send_param->len; i++) {
                                    printf("%02x ", send_param->buffer[i]);
                                }
                            }
                            ESP_LOGE(TAG,"wocaonima");

                            if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                                ESP_LOGE(TAG, "Send error");
                                // example_espnow_deinit(send_param);
                                // vTaskDelete(NULL);
                                continue;
                            }
                        }
                        break;
                    }
                    
                }
                case EXAMPLE_ESPNOW_RECV_CB:
                {
                    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                    ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                    printf("ret = %d",ret);
                    free(recv_cb->data);
                    if (ret == EXAMPLE_ESPNOW_DATA_UNICAST && send_param->state == 0) {
                        // ESP_LOGI(TAG, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                        /* If MAC address does not exist in peer list, add it to peer list. */
                        if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                            esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                            if (peer == NULL) {
                                ESP_LOGE(TAG, "Malloc peer information fail");
                                // example_espnow_deinit(send_param);
                                // vTaskDelete(NULL);
                                break;
                            }
                            memset(peer, 0, sizeof(esp_now_peer_info_t));
                            peer->channel = 1;
                            peer->ifidx = ESPNOW_WIFI_IF;
                            peer->encrypt = false;
                            memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                            memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                            add_slave(recv_cb->mac_addr);
                            ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                            free(peer);
                        }

                        /* Indicates that the device has received broadcast ESPNOW data. */
                        if (send_param->state == 0) {
                            send_param->state = 1;
                            refresh_slave_tasklist_flag = false;
                            // send_param->broadcast = false;
                            printf("Get Pair msg, state = 1\n");
                        }

                        /* If receive broadcast ESPNOW data which indicates that the other device has received
                        * broadcast ESPNOW data and the local magic number is bigger than that in the received
                        * broadcast ESPNOW data, stop sending broadcast ESPNOW data and start sending unicast
                        * ESPNOW data.
                        */
                        // if (recv_state == 1) {
                        //     /* The device which has the bigger magic number sends ESPNOW data, the other one
                        //      * receives ESPNOW data.
                        //      */
                        //     if (send_param->unicast == false && send_param->magic >= recv_magic) {
                        // 	    ESP_LOGI(TAG, "Start sending unicast data");
                        // 	    ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(recv_cb->mac_addr));

                        // 	    /* Start sending unicast ESPNOW data. */
                        //         memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        //         example_espnow_data_prepare(send_param);
                        //         if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                        //             ESP_LOGE(TAG, "Send error");
                        //             example_espnow_deinit(send_param);
                        //             vTaskDelete(NULL);
                        //         }
                        //         else {
                        //             send_param->broadcast = false;
                        //             send_param->unicast = true;
                        //         }
                        //     }
                        // }
                    }
                    else if (ret == EXAMPLE_ESPNOW_DATA_UNICAST) {
                        espnow_send_buf.espnow_callback_flag = 1;
                        ESP_LOGI(TAG, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
                        memcpy(last_recv_mac,recv_cb->mac_addr,6);
                        espnow_send_buf.espnow_callback_flag = 1;
                        //配对后收到的unicast数据
                        /* If receive unicast ESPNOW data, also stop sending broadcast ESPNOW data. */
                        send_param->broadcast = false;
                    }
                    else {
                        ESP_LOGI(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                    }
                    break;
                }
                default:
                    ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                    break;
            }
        }
    }
}

static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if(send_param->state != 1)
    {
        if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
            ESP_LOGW(TAG, "Send send queue fail");
        }
    }
}

static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }


    printf("Receive data from "MACSTR", len: %d\n", MAC2STR(mac_addr), len);
    // if (IS_BROADCAST_ADDR(mac_addr)) {
    // //     /* If added a peer with encryption before, the receive packets may be
    // //      * encrypted as peer-to-peer message or unencrypted over the broadcast channel.
    // //      * Users can check the destination address to distinguish it.
    // //      */
    //     printf("Receive broadcast ESPNOW data");
    // } else {
    //     printf("Receive unicast ESPNOW data");
    // }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
        // 检查内存是否足够
    if (heap_caps_get_free_size(MALLOC_CAP_8BIT) < len) {
        ESP_LOGE(TAG, "Not enough memory to allocate receive data");
        return;
    }

    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}
 
esp_err_t example_espnow_init(void)
{
    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_example_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(example_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(example_espnow_recv_cb) );
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif
    /* Set primary master key. */
    // ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = 1;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(example_espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(example_espnow_send_param_t));
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 1;
    send_param->magic = esp_random();
    send_param->count = 0;//CONFIG_ESPNOW_SEND_COUNT;
    send_param->delay = 1000;//CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = sizeof(example_espnow_data_t);//CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(sizeof(example_espnow_data_t)+50);
    example_espnow_data_t *tempbuf = send_param->buffer; 
    tempbuf->elabel_state = 0;
    tempbuf->task_method = 0;
    tempbuf->changeTaskId = 0;
    tempbuf->msg_type = 0;
    tempbuf->chosenTaskId = 0;
    tempbuf->TimeCountdown = 0;
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    example_espnow_data_prepare(send_param);

// d8:3b:da:97:ee:88
// d8:3b:da:97:ee:24
    uint8_t slave_mac_serve1[6] = {0xD8,0x3B,0xDA,0x97,0xEE,0x24};
    uint8_t slave_mac_serve2[6] = {0xD8,0x3B,0xDA,0x97,0xEE,0x88};
    if (esp_now_is_peer_exist(slave_mac_serve1) == false) {
        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
        if (peer == NULL) {
            ESP_LOGE(TAG, "Malloc peer information fail");
        }
        memset(peer, 0, sizeof(esp_now_peer_info_t));
        peer->channel = 1;
        peer->ifidx = ESPNOW_WIFI_IF;
        peer->encrypt = false;
        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
        memcpy(peer->peer_addr, slave_mac_serve1, ESP_NOW_ETH_ALEN);
        add_slave(slave_mac_serve1);
        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        free(peer);
    }
    if (esp_now_is_peer_exist(slave_mac_serve2) == false) {
        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
        if (peer == NULL) {
            ESP_LOGE(TAG, "Malloc peer information fail");
        }
        memset(peer, 0, sizeof(esp_now_peer_info_t));
        peer->channel = 1;
        peer->ifidx = ESPNOW_WIFI_IF;
        peer->encrypt = false;
        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
        memcpy(peer->peer_addr, slave_mac_serve2, ESP_NOW_ETH_ALEN);
        add_slave(slave_mac_serve2);
        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        free(peer);
    }
    
    xTaskCreate(example_espnow_task, "example_espnow_task", 20000, send_param, 0, &pxespnowTask);

    return ESP_OK;
}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}
 