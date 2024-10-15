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
#include "mqtt_client_test.h"
#include "cJSON/cJSON.h"
#include "main.h"


static void Z_Mqtt_Init(void);
static esp_err_t example_espnow_init(void);

static const char *TAG = "espnow_example";
static char rev_topic[40] = "service/to/firmware/";
static char send_topic[40] = "firmware/to/service/";
bool Z_mqtt_connect_flag = false;             //记录是否连接上MQTT服务器的一个标志,如果连接上了才可以发布信息
bool Z_http_connect_flag = false;             //记录是否连接上HTTP服务器的一个标志,如果连接上了才可以发布信息


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

bool mqttMessage_parse(char *data, MqttMessage *msg)
{
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse error");
        return false;
    }
    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (method) {
        strcpy(msg->method, method->valuestring);
    } else {
        ESP_LOGE(TAG, "Method not found");
        return false;
    }
    cJSON *did = cJSON_GetObjectItem(root, "did");
    if (did) {
        msg->did = did->valueint;
    } else {
        ESP_LOGE(TAG, "Did not found");
        return false;
    }
    cJSON *isReply = cJSON_GetObjectItem(root, "isReply");
    if (isReply) {
        msg->isReply = isReply->valueint;
    } else {
        ESP_LOGE(TAG, "isReply not found");
        return false;
    }
    cJSON *msgId = cJSON_GetObjectItem(root, "msgId");
    if (msgId) {
        msg->msgId = msgId->valueint;
    } else {
        ESP_LOGE(TAG, "msgId not found");
        return false;
    }
    
    if(strcmp(msg->method, "outFocus") == 0)
    {
        cJSON *msg_focus = cJSON_GetObjectItem(root, "msg");
        if (msg_focus) {
            cJSON *id = cJSON_GetObjectItem(msg_focus, "id");
            if (id) {
                msg->msg_focus.id = id->valueint;
            } else {
                ESP_LOGE(TAG, "id not found");
                return false;
            }
            cJSON *Focus = cJSON_GetObjectItem(msg_focus, "Focus");
            if (Focus) {
                msg->msg_focus.Focus = Focus->valueint;
            } else {
                ESP_LOGE(TAG, "Focus not found");
                return false;
            }
        } else {
            ESP_LOGE(TAG, "msg not found");
            return false;
        }
    }
    else
    {
        ESP_LOGE(TAG, "method not found");
        return false;
    }

    cJSON_Delete(root);
    return true;
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
        espnow_recv_buf.payload = buf->payload;
        //打印所有buf成员变量
        printf("elabel_state = %d\n",espnow_recv_buf.elabel_state);
        printf("task_method = %d\n",espnow_recv_buf.task_method);
        printf("changeTaskId = %d\n",espnow_recv_buf.changeTaskId);
        printf("msg_type = %d\n",espnow_recv_buf.msg_type);
        printf("chosenTaskId = %d\n",espnow_recv_buf.chosenTaskId);
        printf("TimeCountdown = %d\n",espnow_recv_buf.TimeCountdown);
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
    example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;
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

                    if (is_broadcast && (send_param->broadcast == false)) {
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

                    /* Delay a while before sending the next data. */
                    if (send_param->delay > 0) {
                        vTaskDelay(send_param->delay/portTICK_PERIOD_MS);
                    }

                    ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                    memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                    example_espnow_data_prepare(send_param);

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
                            send_param->broadcast = false;
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
                        ESP_LOGI(TAG, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);
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
    if(IS_BROADCAST_ADDR(mac_addr))
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

static int recived_len = 0;

static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        printf( "connected to web-server\n");
        recived_len = 0;
        Z_http_connect_flag = true;
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->user_data)
        {
            memcpy(evt->user_data + recived_len, evt->data, evt->data_len); // 将分片的每一片数据都复制到user_data
            recived_len += evt->data_len;//累计偏移更新
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "finished a request and response!");
        recived_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "disconnected to web-server");
        recived_len = 0;
        Z_http_connect_flag = false;
        break;
    case HTTP_EVENT_ERROR:
        printf( "http error\n");
        recived_len = 0;
        Z_http_connect_flag = false;
        break;
    default:
        break;
    }

    return ESP_OK;
}

static char response_data[1024];

static void http_client_init(void)
{
    const esp_http_client_config_t config = {
        .url = "http://120.77.1.151",
        .event_handler = http_client_event_handler,
        .user_data = response_data,
    };
    client = esp_http_client_init(&config);
    if(!client)  printf("httpclient init error!\r\n");
    else printf("httpclient init success!\r\n");
    
}

void http_client_sendMsg(esp_http_client_event_handle_t client,http_task_t task)
{
    // if(!Z_http_connect_flag) return;
    printf("http_client_sendMsg\r\n");
    switch (task)
    {
    case ADDTODO:
    {
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/addTodo");
        esp_http_client_set_header(client,"Content-Type","application/json");
        esp_http_client_set_header(client, "userToken", "d7e2be05eece4ce09c74baf798a39b99");
        // 构造 JSON 数据
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "title", "New TODO");
        cJSON_AddStringToObject(root, "todoType", "TODO TASK1");

        // 将 JSON 数据转换为字符串
        const char *post_data = cJSON_Print(root);

        // 将 JSON 数据设置为 HTTP 请求体
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            printf("HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            printf("HTTP POST request failed: %s", esp_err_to_name(err));
        }
        // 释放 JSON 对象
        cJSON_Delete(root);
        // 清理客户端
        // esp_http_client_cleanup(client);
        break;
    }
    case FINDLATESTVERSION:
    {
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/deviceFirmware/getDeviceUpToDateFirmwareVersion");
        esp_http_client_set_header(client,"Content-Type","multipart/form-data");
        esp_http_client_set_header(client, "userToken", "d7e2be05eece4ce09c74baf798a39b99");
        // 构造 JSON 数据
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "version", "1");
        // 将 JSON 数据转换为字符串
        const char *post_data = cJSON_Print(root);

        // 将 JSON 数据设置为 HTTP 请求体
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            printf("HTTP POST Status = %d, content_length = %d\n",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
                    
            // 获取响应内容
        char response_buffer[1024];  // 假设响应不会超过 1024 字节
        int content_length = esp_http_client_get_content_length(client);
        printf("content_length = %d,response_buffer = %d\n",content_length,sizeof(response_buffer));
        if (content_length <= (int)sizeof(response_buffer)) {
            int bytes_read = esp_http_client_read(client, response_buffer, sizeof(response_buffer) - 1);
            if (bytes_read >= 0) {
                response_buffer[bytes_read] = '\0';  // 确保以 NULL 结尾
                printf("Response: %s\n", response_buffer);

                // 解析响应 JSON
                cJSON *response_json = cJSON_Parse(response_buffer);
                if (response_json != NULL) {
                    cJSON *msg = cJSON_GetObjectItem(response_json, "msg");
                    cJSON *code = cJSON_GetObjectItem(response_json, "code");
                    cJSON *data = cJSON_GetObjectItem(response_json, "data");
                    if (msg && code && data) {
                        printf("Message: %s, Code: %d\n", msg->valuestring, code->valueint);

                        cJSON *data_content = cJSON_GetObjectItem(data, "data");
                        if (data_content) {
                            cJSON *version = cJSON_GetObjectItem(data_content, "version");
                            cJSON *firmwareUrl = cJSON_GetObjectItem(data_content, "firmwareUrl");
                            if (version && firmwareUrl) {
                                printf("Latest Version: %s, Firmware URL: %s\n", version->valuestring, firmwareUrl->valuestring);
                            }
                        }
                    }
                    cJSON_Delete(response_json);  // 释放 JSON 对象
                } 
                else {
                    printf("Failed to parse JSON response\n");
                }

            } else {
            printf("Failed to read response\n");
            }
        } else {
            printf("Response content too large to handle\n");
        }
        } else {
            printf("HTTP POST request failed: %s", esp_err_to_name(err));
        }
        // 释放 JSON 对象
        cJSON_Delete(root);
        // 清理客户端
        // esp_http_client_cleanup(client);
        break;
    }
    default:
        break;
    }
    
}


//WiFI事件处理函数
static void  wifi_event_fun(void* handler_arg,esp_event_base_t event_base,int32_t event_id,void* event_data){
    printf("%s,%d\r\n",event_base,event_id);
 
    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){   //获取到IP地址之后
        ip_event_got_ip_t* got_ip = (ip_event_got_ip_t*) event_data;
        printf("Got ip:%s\r\n",ip4addr_ntoa(&got_ip->ip_info.ip));
    }
    else if(event_id==WIFI_EVENT_STA_START){                 //如果是STA开启了,那么尝试连接
        // esp_wifi_connect();      
        example_espnow_init();                               
        printf("start connect wifi,espnow\r\n");                   
    }else if(event_id==WIFI_EVENT_STA_CONNECTED){       //连接上WiFI之后
        wifi_event_sta_connected_t* event_info = (wifi_event_sta_connected_t*) event_data;  // 获取连接信息
        Z_Mqtt_Init();                                  //开始连接MQTT服务器
        printf("success connect wifi\r\n");
        printf("Wi-Fi connected, channel: %d\n", event_info->channel);
        
    }else if(event_id == WIFI_EVENT_STA_DISCONNECTED){   // WiFi 断开
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        printf("Disconnected reason: %d\n", disconnected->reason); // 打印断开原因

        if (disconnected->reason == WIFI_REASON_AUTH_FAIL) {
            printf("Authentication failure! Check SSID or password.\n");
        } else {
            printf("Reconnecting...\n");
            // esp_wifi_connect();                             // 尝试重连WiFi
        }
        printf("lose connect wifi\r\n");
    }
}

static void solve_message(esp_mqtt_event_handle_t event)
{
    if(event->topic_len == 0 || event->data_len == 0) return;
    // char *sname = (char*)(malloc(strlen(rev_topic_first)+strlen(mac_str)));
    // sprintf(sname,"%s%s",rev_topic_first,mac_str);
    // printf("sname:%s\r\n",sname);
    // printf("event->topic:%.*s\r\n",event->topic_len,event->topic);
    // char *
    // if(strcmp(event->topic,sname) == 0)
    // {
        if(mqttMessage_parse(event->data, &Mqtt_msg))
        {
            if(strcmp(Mqtt_msg.method, "outFocus") == 0)
            {
                if(Mqtt_msg.isReply)//需要回复
                {
                    MqttMessage_reply_focus Mqtt_msg_reply;
                    Mqtt_msg_reply.did = Mqtt_msg.did;
                    Mqtt_msg_reply.isReply = 1;
                    Mqtt_msg_reply.msgId = Mqtt_msg.msgId;
                    strcpy(Mqtt_msg_reply.method, Mqtt_msg.method);
                    strcpy(Mqtt_msg_reply.msg_focus.sn,mac_str);
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddNumberToObject(json, "did", Mqtt_msg_reply.did);
                    cJSON_AddNumberToObject(json, "isReply", Mqtt_msg_reply.isReply);
                    cJSON_AddNumberToObject(json, "msgId", Mqtt_msg_reply.msgId);
                    cJSON_AddStringToObject(json, "method", Mqtt_msg_reply.method);
                    cJSON *msg_focus = cJSON_CreateObject();
                    cJSON_AddStringToObject(msg_focus, "sn", Mqtt_msg_reply.msg_focus.sn);
                    cJSON_AddItemToObject(json, "msg", msg_focus);
                    char *data = cJSON_Print(json);
                    cJSON_Delete(json);
                    // char *sname = (char*)(malloc(strlen(send_topic_first)+strlen(mac_str)));
                    // sprintf(sname,"%s%s",send_topic_first,mac_str);
                    esp_mqtt_client_publish(emcht,send_topic,data,0,1,0);
                }
            }
        }
    // }
    // else
    // {
    //     printf("topic error\r\n");
    //     return;
    // }
}
 
//MQTT事件处理函数
static void mqtt_event_fun(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    printf("%s,%d\r\n",event_base,event_id);
 
    if(event_id==MQTT_EVENT_CONNECTED){                 //连接上MQTT服务器
        Z_mqtt_connect_flag=true;
        // char *sname = (char*)(malloc(strlen(rev_topic_first)+strlen(mac_str)));
        // sprintf(sname,"%s%s",rev_topic_first,mac_str);
        esp_mqtt_client_subscribe(emcht,rev_topic,1);    //订阅一个测试主题
        printf("success connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DISCONNECTED){        //断开MQTT服务器连接
        Z_mqtt_connect_flag=false;
        printf("lose connect mqtt\r\n");
    }else if(event_id==MQTT_EVENT_DATA){                //收到订阅信息
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t )event_data;   //强转获取存放订阅信息的参数
        printf("receive data : %.*s from %.*s\r\n",event->data_len,event->data,event->topic_len,event->topic);
        solve_message(event);
        printf("solve message after\r\n");
    }
}

static esp_err_t example_espnow_init(void)
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
    send_param = malloc(sizeof(example_espnow_send_param_t))+50;
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(example_espnow_send_param_t)+50);
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 0;
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

    xTaskCreate(example_espnow_task, "example_espnow_task", 4096, send_param, 0, NULL);

    return ESP_OK;
}
 
void Z_WiFi_Init(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    esp_event_loop_create_default();            //创建默认事件循环
    esp_netif_init();                           //初始化TCP/IP堆栈
    esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_fun,NULL);       //绑定事件处理函数
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,wifi_event_fun,NULL);
    esp_netif_create_default_wifi_sta();        //创建STA
    wifi_init_config_t wict = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wict);                       //初始化WiFI
    esp_wifi_set_mode(WIFI_MODE_STA);           //设为STA模式
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("MAC: %s\n", mac_str);
    strcat(rev_topic,mac_str);
    strcat(send_topic,mac_str);
    wifi_config_t wct = {
        .sta = {
            .ssid="“KUMIKO”的 iPhone",
            .password="23468888",
            // .threshold.authmode = WIFI_AUTH_WPA2_PSK,  // 确保路由器使用 WPA2
        }
    };
    esp_wifi_set_config(WIFI_IF_STA,&wct);      //设置WiFi
    esp_wifi_start();                           //启动WiFi
    http_client_init();
}
 
static void Z_Mqtt_Init(void){
    esp_mqtt_client_config_t emcct = {
        .uri="mqtt://120.77.1.151",  //MQTT服务器的uri
        .port=1883,                   //MQTT服务器的端口
        // .username="shu123",
        // .password="shu123"
    };
    emcht = esp_mqtt_client_init(&emcct);           //初始化MQTT客户端获取句柄
    if(!emcht)  printf("mqtt init error!\r\n");
    
    //注册MQTT事件处理函数
    if(esp_mqtt_client_register_event(emcht,ESP_EVENT_ANY_ID,mqtt_event_fun,NULL)!=ESP_OK)  printf("mqtt register error!\r\n");
 
    //开启MQTT客户端
    if(esp_mqtt_client_start(emcht) != ESP_OK)  printf("mqtt start errpr!\r\n");

}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}
 