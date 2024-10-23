#ifndef MQTT_RECIEVE_H
#define MQTT_RECIEVE_H
#include "m_mqtt.h"
#include "global_message.h"
#include "http.h"

#define MQTT_RECIEVE_TAG "MQTT_RECIEVE_TAG"
MqttMessage_recieve Mqtt_msg;

bool mqttMessage_parse(char *data, MqttMessage_recieve *mqtt_msg)
{
    // 解析 JSON 字符串
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE(MQTT_RECIEVE_TAG, "JSON parse error");
        return false;
    }
    // 提取 "method" 字段
    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (cJSON_IsString(method)) {
        strcpy(mqtt_msg->method ,method->valuestring);
    } else {
        ESP_LOGE(MQTT_RECIEVE_TAG, "Method not found");
        return false;
    }
    // 提取 "did" 字段
    cJSON *did = cJSON_GetObjectItem(root, "did");
    if (cJSON_IsNumber(did)) {
        mqtt_msg->did = did->valueint;
    } else {
        ESP_LOGE(MQTT_RECIEVE_TAG, "Did not found");
        return false;
    }
    // 提取 "isReply" 字段
    cJSON *isReply = cJSON_GetObjectItem(root, "isReply");
    if (cJSON_IsNumber(isReply)) {
        mqtt_msg->isReply = isReply->valueint;
    } else {
        ESP_LOGE(MQTT_RECIEVE_TAG, "isReply not found");
        return false;
    }
    // 提取 "msgId" 字段
    cJSON *msgId = cJSON_GetObjectItem(root, "msgId");
    if (cJSON_IsNumber(msgId)) {
        mqtt_msg->msgId = msgId->valueint;
    } else {
        ESP_LOGE(MQTT_RECIEVE_TAG, "msgId not found");
        return false;
    }

    // 提取 "msg" 字段
    cJSON *msg = cJSON_GetObjectItem(root, "msg");
    if (!cJSON_IsObject(msg)) 
    {
        ESP_LOGE(MQTT_RECIEVE_TAG, "msg not found");
        return false;
    }
    
    if(strcmp(mqtt_msg->method, "outFocus") == 0)
    {
        mqtt_msg->order = Out_focus;
        cJSON *Focus = cJSON_GetObjectItem(msg, "outFocus");
        if (cJSON_IsBool(Focus)) 
        {
            mqtt_msg->is_focus = Focus->valueint;
        } 
        else 
        {
            ESP_LOGE(MQTT_RECIEVE_TAG, "Focus not found");
            return false;
        }
    }
    else if(strcmp(mqtt_msg->method, "sync") == 0)
    {
        cJSON *_dataType = cJSON_GetObjectItem(msg, "dataType");
        if (cJSON_IsString(_dataType)) 
        {
            strcpy(mqtt_msg->dataType ,method->valuestring);
        } 
        else 
        {
            ESP_LOGE(MQTT_RECIEVE_TAG, "dataType not found");
            return false;
        }

        if(strcmp(mqtt_msg->dataType, "focus") == 0)
        {
            mqtt_msg->order = Enter_focus;
        }
        else if(strcmp(mqtt_msg->dataType, "todo") == 0)
        {
            mqtt_msg->order = Tasklist_change;
        }
        else
        {
            ESP_LOGE(MQTT_RECIEVE_TAG, "dataType can not figure ----,%s",mqtt_msg->dataType);
            return false;           
        }
    }
    else
    {
        ESP_LOGE(MQTT_RECIEVE_TAG, "method can not figure ----,%s",mqtt_msg->method);
        return false;
    }

    cJSON_Delete(root);
    return true;
}
#endif

static void solve_message(char *response)
{
    if(mqttMessage_parse(response, &Mqtt_msg))
    {
        if(Mqtt_msg.isReply)//先回复后处理
        {
            MqttMessage_reply Mqtt_msg_reply;
            Mqtt_msg_reply.did = Mqtt_msg.did;
            Mqtt_msg_reply.isReply = 1;
            Mqtt_msg_reply.msgId = Mqtt_msg.msgId;
            strcpy(Mqtt_msg_reply.method, Mqtt_msg.method);
            strcpy(Mqtt_msg_reply.msg,get_global_data()->mac_str);
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "did", Mqtt_msg_reply.did);
            cJSON_AddNumberToObject(json, "isReply", Mqtt_msg_reply.isReply);
            cJSON_AddNumberToObject(json, "msgId", Mqtt_msg_reply.msgId);
            cJSON_AddStringToObject(json, "method", Mqtt_msg_reply.method);
            cJSON *msg = cJSON_CreateObject();
            cJSON_AddStringToObject(msg, "sn", Mqtt_msg_reply.msg);
            cJSON_AddItemToObject(json, "msg", msg);
            char *data = cJSON_Print(json);
            esp_mqtt_client_publish(*get_mqtt_client(),get_send_topic(),data,0,1,0);
            cJSON_Delete(json);
            free(data);
        }

        if(Mqtt_msg.order == Out_focus)
        {
            get_global_data()->m_focus_state->is_focus = false;
            get_global_data()->m_focus_state->is_created_by_myself = false;
            http_get_todo_list(false);
        }
        else if(Mqtt_msg.order == Enter_focus)
        {
            get_global_data()->m_focus_state->is_focus = true;
            get_global_data()->m_focus_state->is_created_by_myself = false;
            http_get_todo_list(false);
        }
        else if(Mqtt_msg.order == Tasklist_change)
        {
            http_get_todo_list(false);
        }
    }
}