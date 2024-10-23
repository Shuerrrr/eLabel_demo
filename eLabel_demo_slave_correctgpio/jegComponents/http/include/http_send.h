#ifndef HTTP_SEND_H
#define HTTP_SEND_H
#include <esp_http_client.h>
#include "cJSON.h"
#include "esp_log.h"
#include "global_message.h"
#include "http.h"

//BOUNDARY 是一个分隔符，用于区分 multipart/form-data 请求中不同部分的边界。
//在发送多个字段时，每个字段都用这个边界来分开。它通常是一个唯一的字符串，以确保各部分不会混淆。

//BOUNDARY 可以设置为任何字符串，只要确保它在整个请求体中是唯一的，
//并且不会与任何字段的内容冲突。常见做法是使用较长且复杂的字符串，例如包含随机字符的字符串，以增加其唯一性
char m_boundary[20]; // Ensure enough size for boundary
void generate_boundary(char *boundary, size_t size) {
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL));
    snprintf(boundary, size, "------------------------");
    for (size_t i = 0; i < 8; ++i) {
        boundary[size - 9 + i] = chars[rand() % strlen(chars)];
    }
    boundary[size - 1] = '\0';
}

void http_send(http_task_struct* m_task_struct)
{
    esp_http_client_handle_t client = *get_client();
    if(m_task_struct->task==ADDTODO)
    {
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/addTodo");
        esp_http_client_set_header(client,"Content-Type","application/json");
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造 JSON 数据
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "title",    m_task_struct->parament[0]);
        cJSON_AddStringToObject(root, "todoType", m_task_struct->parament[1]);

        // 将 JSON 数据转换为字符串
        const char *post_data = cJSON_Print(root);

        // 将 JSON 数据设置为 HTTP 请求体
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
        // 释放 JSON 对象
        cJSON_Delete(root);
    }
    else if(m_task_struct->task==ENTER_FOCUS)
    {
        generate_boundary(m_boundary, sizeof(m_boundary));
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/enterFocus");
        // 设置 Content-Type
        char content_type[100]; // 确保大小足够
        snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
        esp_http_client_set_header(client, "Content-Type", content_type);
        // 设置 userToken
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造请求体
        char body[1024];
        snprintf(body, sizeof(body),
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
            "%s\r\n"
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"focus\"\r\n\r\n"
            "%s\r\n"
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"startTime\"\r\n\r\n"
            "%lld\r\n"
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"fallTiming\"\r\n\r\n"
            "%s\r\n"
            "--%s--\r\n",
            m_boundary,m_task_struct->parament[0],m_boundary,m_task_struct->parament[1],m_boundary,get_unix_time(),m_boundary,m_task_struct->parament[2],m_boundary);
        // 发送请求体
        esp_http_client_set_post_field(client, body, strlen(body));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
    else if(m_task_struct->task==OUT_FOCUS)
    {
        generate_boundary(m_boundary, sizeof(m_boundary));
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/outFocus");
        // 设置 Content-Type
        char content_type[100]; // 确保大小足够
        snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
        esp_http_client_set_header(client, "Content-Type", content_type);
        // 设置 userToken
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造请求体
        char body[256];
        snprintf(body, sizeof(body),
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
            "%s\r\n"
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"focus\"\r\n\r\n"
            "%s\r\n"
            "--%s--\r\n",
            m_boundary, m_task_struct->parament[0], m_boundary, m_task_struct->parament[1], m_boundary);
        // 发送请求体
        esp_http_client_set_post_field(client, body, strlen(body));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
    else if(m_task_struct->task==DELETTODO)
    {
        generate_boundary(m_boundary, sizeof(m_boundary));
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/deleteTodo");
        // 设置 Content-Type
        char content_type[100]; // 确保大小足够
        snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
        esp_http_client_set_header(client, "Content-Type", content_type);
        // 设置 userToken
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造请求体
        char body[256];
        snprintf(body, sizeof(body),
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"id\"\r\n\r\n"
            "%s\r\n"
            "--%s--\r\n",
            m_boundary, m_task_struct->parament[0], m_boundary);
        // 发送请求体
        esp_http_client_set_post_field(client, body, strlen(body));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
    else if(m_task_struct->task==FINDTODOLIST)
    { 
        generate_boundary(m_boundary, sizeof(m_boundary));
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/todo/todoList");
        // 设置 Content-Type
        char content_type[100]; // 确保大小足够
        snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
        esp_http_client_set_header(client, "Content-Type", content_type);
        // 设置 userToken
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造请求体
        char body[256];
        snprintf(body, sizeof(body),
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"page\"\r\n\r\n"
            "1\r\n"
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"pageSize\"\r\n\r\n"
            "500\r\n"
            "--%s--\r\n",
            m_boundary, m_boundary, m_boundary);

        // 发送请求体
        esp_http_client_set_post_field(client, body, strlen(body));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
    else if(m_task_struct->task==FINDLATESTVERSION)
    {
        generate_boundary(m_boundary, sizeof(m_boundary));
        esp_http_client_set_method(client,HTTP_METHOD_POST);
        esp_http_client_set_url(client,"http://120.77.1.151:8080/userApi/deviceFirmware/getDeviceUpToDateFirmwareVersion");
        // 设置 Content-Type
        char content_type[100]; // 确保大小足够
        snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", m_boundary);
        esp_http_client_set_header(client, "Content-Type", content_type);
        // 设置 userToken
        esp_http_client_set_header(client, "userToken", get_global_data()->usertoken);
        // 构造请求体
        char body[256];
        snprintf(body, sizeof(body),
            "--%s\r\n"
            "Content-Disposition: form-data; name=\"version\"\r\n\r\n"
            "%s\r\n"
            "--%s--\r\n",
            m_boundary, FIRMWARE_VERSION, m_boundary);
        // 发送请求体
        esp_http_client_set_post_field(client, body, strlen(body));
        // 发送请求
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }
    }
}
#endif