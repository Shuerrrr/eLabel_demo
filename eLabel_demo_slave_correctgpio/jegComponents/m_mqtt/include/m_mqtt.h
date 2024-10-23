#ifndef M_MQTT_H
#define M_MQTT_H

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crc.h"
#include "mqtt_client.h"
#include "cJSON.h"
//fuck，不能取名字为mqtt和原生文件夹重名了，但是http就可以，nt
#define MQTT_TAG "mqtt"

typedef enum {
    No_order,
    Enter_focus,
    Out_focus,
    Tasklist_change,
} Mqtt_order;

typedef struct {
    Mqtt_order order;
    int did; //设备id
    int isReply; //是否需要回复 
    int msgId;   //本消息的ID
    char method[20]; //消息名称

    bool is_focus;

    char dataType[20];
    char changeType[20];
    

} MqttMessage_recieve;

typedef struct {
    int did;
    int isReply;
    int msgId;
    char method[20];
    char msg[20];   
} MqttMessage_reply;

void mqtt_client_init(void);
esp_mqtt_client_handle_t* get_mqtt_client(void);
char* get_send_topic(void);


#endif