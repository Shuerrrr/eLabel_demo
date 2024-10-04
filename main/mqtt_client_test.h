#ifndef _MQTT_CLIENT_H
#define _MQTT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif
/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#include "esp_now.h"
#include <esp_http_client.h>
#include "mqtt_client.h"
 
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

#define ESPNOW_QUEUE_SIZE           6
#define ESP_NOW_ETH_ALEN 6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} example_espnow_event_id_t;


typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} example_espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} example_espnow_event_recv_cb_t;

typedef union {
    example_espnow_event_send_cb_t send_cb;
    example_espnow_event_recv_cb_t recv_cb;
} example_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    example_espnow_event_id_t id;
    example_espnow_event_info_t info;
} example_espnow_event_t;

enum {
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

/* User defined field of ESPNOW data in this example. */
typedef struct {
    uint8_t type;                         //Broadcast or unicast ESPNOW data.
    uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
    uint16_t seq_num;                     //Sequence number of ESPNOW data.
    uint16_t crc;                         //CRC16 value of ESPNOW data.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t payload[0];                   //Real payload of ESPNOW data.
} __attribute__((packed)) example_espnow_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
    bool unicast;                         //Send unicast ESPNOW data.
    bool broadcast;                       //Send broadcast ESPNOW data.
    uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint16_t count;                       //Total count of unicast ESPNOW data to be sent.
    uint16_t delay;                       //Delay between sending two ESPNOW data, unit: ms.
    int len;                              //Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
} example_espnow_send_param_t;

typedef struct {
    uint8_t id;
    bool Focus;
} MqttMsg_focus_rec;

typedef struct {
    char sn[18];
} MqttMsg_focus_send;


typedef struct {
    int did;
    int isReply;
    int msgId;
    char method[20];

    MqttMsg_focus_rec msg_focus;
} MqttMessage;

typedef struct {
    int did;
    int isReply;
    int msgId;
    char method[20];

    MqttMsg_focus_send msg_focus;
} MqttMessage_reply_focus;


MqttMessage Mqtt_msg;

typedef enum {
    ENTER_FOCUS,
    ADDTODO,
} http_task_t;

char mac_str[18];
void Z_WiFi_Init(void);

esp_http_client_event_handle_t client;            //http客户端句柄
esp_mqtt_client_handle_t emcht;             //MQTT客户端句柄

void http_client_sendMsg(esp_http_client_event_handle_t client, http_task_t task);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif