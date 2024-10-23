#ifndef HTTP_RECIEVE_H
#define HTTP_RECIEVE_H
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "http.h"
#include "global_message.h"
void parse_json_response(char *response, http_task_struct *m_task_struct, http_state *m_http_state) 
{
    // 解析 JSON
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        ESP_LOGE("HTTP","JSON parse error!\n");
        return;
    }
    else{
        ESP_LOGI("HTTP", "Start parse response\n");
    };

    // 获取 msg 字段
    cJSON *msg = cJSON_GetObjectItem(json, "msg");
    if (cJSON_IsString(msg) && (msg->valuestring != NULL)) {
        ESP_LOGI("HTTP", "Message: %s\n", msg->valuestring);
    }

    // 获取 code 字段
    cJSON *code = cJSON_GetObjectItem(json, "code");
    if (cJSON_IsNumber(code)) {
        ESP_LOGI("HTTP", "Code: %d\n", code->valueint);
        if(code->valueint != 200)
        {
            *m_http_state = send_fail;
            return;
        }
    }
    if(m_task_struct->task == FINDTODOLIST)
    {
        // 获取 rows 数组
        cJSON *data = cJSON_GetObjectItem(json, "rows");
        int array_size = cJSON_GetArraySize(data);
        clean_todo_list(get_global_data()->m_todo_list);
        for (int i = 0; i < array_size; i++) 
        {
            cJSON *item = cJSON_GetArrayItem(data, i);
            TodoItem todo;

            // 解析各字段
            todo.createBy = cJSON_GetStringValue(cJSON_GetObjectItem(item, "createBy"));

            char *create_time_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "createTime"));
            if (create_time_str != NULL) {
                todo.createTime = strtoll(create_time_str, NULL, 10);
            }

            todo.updateBy = cJSON_GetStringValue(cJSON_GetObjectItem(item, "updateBy"));

            char *updateTime_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "updateTime"));
            if (updateTime_str != NULL) {
                todo.updateTime = strtoll(updateTime_str, NULL, 10);
            }

            todo.remark = cJSON_GetStringValue(cJSON_GetObjectItem(item, "remark"));

            todo.id = cJSON_GetObjectItem(item, "id")->valueint;

            todo.title = cJSON_GetStringValue(cJSON_GetObjectItem(item, "title"));

            todo.isPressing = cJSON_GetObjectItem(item, "isPressing")->valueint;

            todo.todoType = cJSON_GetStringValue(cJSON_GetObjectItem(item, "todoType"));

            todo.isComplete = cJSON_GetObjectItem(item, "isComplete")->valueint;

            char *startTime_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "startTime"));
            if (startTime_str != NULL) {
                todo.startTime = strtoll(startTime_str, NULL, 10);
            }

            todo.fallTiming = cJSON_GetObjectItem(item, "fallTiming")->valueint;

            todo.isFocus = cJSON_GetObjectItem(item, "isFocus")->valueint;

            todo.isImportant = cJSON_GetObjectItem(item, "isImportant")->valueint;

            Global_data* _global_data = get_global_data();
            add_or_update_todo_item(_global_data->m_todo_list, todo);
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDTODOLIST\n");
    }
    else if(m_task_struct->task == FINDLATESTVERSION)
    {   
       // 获取嵌套的 data 对象
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL) {
            cJSON *nested_code = cJSON_GetObjectItem(data, "code");
            if(nested_code->valueint == 1)
            {
                ESP_LOGI("HTTP", "This version is newest\n");
            }
            else
            {
                cJSON *nested_data = cJSON_GetObjectItem(data, "data");
                if (nested_data != NULL) {
                    const char *version = cJSON_GetStringValue(cJSON_GetObjectItem(nested_data, "version"));
                    const char *deviceModel = cJSON_GetStringValue(cJSON_GetObjectItem(nested_data, "deviceModel"));
                    const char *newest_firmware_url = cJSON_GetStringValue(cJSON_GetObjectItem(nested_data, "firmwareUrl"));
                    const char *createTime = cJSON_GetStringValue(cJSON_GetObjectItem(nested_data, "createTime"));
                    //该函数只会调用一次，所以不用释放空指针
                    get_global_data()->version  = strdup(version);
                    get_global_data()->deviceModel  = strdup(deviceModel);
                    get_global_data()->createTime  = strdup(createTime);
                    get_global_data()->newest_firmware_url  = strdup(newest_firmware_url);
                }
            }
        }
        ESP_LOGI("HTTP", "Successful get response post task is FINDLATESTVERSION\n");        
    }
    else if (m_task_struct->task == ADDTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ADDTODO.\n title is %s, todo type is %s.\n",m_task_struct->parament[0],m_task_struct->parament[1]);
    }   
    else if (m_task_struct->task == ENTER_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is ENTER_FOCUS.\n Task%s enter focus\n",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == OUT_FOCUS)
    {
        ESP_LOGI("HTTP", "Successful get response post task is OUT_FOCUS.\n Task%s out focus\n",m_task_struct->parament[0]);
    }  
    else if (m_task_struct->task == DELETTODO)
    {
        ESP_LOGI("HTTP", "Successful get response post task is DELETTODO.\n Task%s is deleted\n",m_task_struct->parament[0]);
    }  


    // 释放 JSON 对象
    cJSON_Delete(json);
    if(m_task_struct->need_stuck)
    {
        m_task_struct->need_stuck = false;
    }
    *m_http_state = send_waiting;
}
#endif