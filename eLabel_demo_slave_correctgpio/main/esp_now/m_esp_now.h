#ifndef M_ESP_NOW_H
#define M_ESP_NOW_H

#ifdef __cplusplus
extern "C" {
#endif
/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#include "esp_now.h"
 
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

#define ESPNOW_QUEUE_SIZE 18 //6
#define ESP_NOW_ETH_ALEN 6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

esp_err_t example_espnow_init(void);

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

#define MAX_SLAVES 17
#define ESPNOW_MAXDELAY 512

extern uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN];
extern uint8_t slave_num;
extern uint8_t slave_mac[MAX_SLAVES][ESP_NOW_ETH_ALEN];
QueueHandle_t s_example_espnow_queue;

/* User defined field of ESPNOW data in this example. */
typedef struct {
    uint8_t type;                         //Broadcast or unicast ESPNOW data.
    uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
    uint8_t elabel_state;                 //State of elabel
    uint8_t task_method;                  //1:修改任务内容，2：add任务，3:删任务
    uint8_t changeTaskId;                      //若修改任务，任务ID
    uint8_t msg_type;                    //0:啥都没变，1：elabel_State变了，2：task_method要执行了,3:state和method都要执行了
    uint8_t chosenTaskId;                 //state = 3调用进入focus所选的任务
    uint16_t seq_num;                     //Sequence number of ESPNOW data.
    uint16_t crc;                         //CRC16 value of ESPNOW data.
    uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
    uint32_t TimeCountdown;               //state = 3调用倒计时时间
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

    uint8_t elabel_state;                 //State of elabel
    uint8_t task_method;                  //1:修改任务内容，2：add任务，3:删任务
    uint8_t changeTaskId;                      //若修改任务，任务ID
    uint8_t msg_type;                    //0:啥都没变，1：elabel_State变了，2：task_method要执行了,3:state和method都要执行了
    uint8_t chosenTaskId;                 //state = 3调用进入focus所选的任务
    uint32_t TimeCountdown;               //state = 3调用倒计时时间

} example_espnow_send_param_t;

example_espnow_send_param_t *send_param;

uint8_t BoardcasttoPair;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif