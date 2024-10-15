#ifndef _MAIN_H_
#define _MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "ui/ui.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "mqtt_client_test.h"

#include "cJSON/cJSON.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"


// 定义任务节点结构
#define BUTTON_GPIO1 2
#define ENCODER_K2 3
#define ENCODER_K 1

bool needFlashEpaper;
char taskstr[256];

typedef struct TaskNode {
    char *task;               // 任务字符串
    struct TaskNode *next;    // 下一个任务节点
} TaskNode;

// 创建新任务
TaskNode* create_task(const char* task_content);

// 添加任务到链表末尾
void add_task(TaskNode **head, const char *task_content);

// 删除指定位置的任务
void delete_task(TaskNode **head, int position);

// 打印任务列表
void print_tasks(TaskNode *head);

// 释放所有任务的内存
void free_tasks(TaskNode *head);

char* find_task_by_position(TaskNode *head, int position);
void tasks2str(TaskNode *head);

typedef struct{
    uint8_t elabel_state;                 //State of elabel
    uint8_t task_method;                  //1:修改任务内容，2：add任务，3:删任务
    uint8_t changeTaskId;                      //若修改任务，任务ID
    uint8_t msg_type;                    //0:啥都没变，1：elabel_State变了，2：task_method要执行了,3:state和method都要执行了
    uint8_t chosenTaskId;                 //state = 3调用进入focus所选的任务
    uint32_t TimeCountdown;               //state = 3调用倒计时时间
    uint8_t *payload;
} espnow_send_param_buf;

espnow_send_param_buf espnow_send_buf;
espnow_send_param_buf espnow_recv_buf;

void espnow_send_buf_reset(espnow_send_param_buf* buff);

void sync_to_slaves(const char *task_content);

void sync_recv_update();


typedef enum {
    Preparing,
    Operating,
    Focus,
    Stopping,
} ELABEL_STATE;

ELABEL_STATE label_state;
void ChangeState(ELABEL_STATE state);
void Execute();
void Enter(ELABEL_STATE state);
void stateInit();
// void Exit(ELABEL_STATE state);

int EncoderValue,lastEncoderValue;
TaskNode *task_list;
uint8_t tasklen;
uint8_t chosenTaskNum,lastChosenTaskNum;
uint32_t TimeCountdown;
uint32_t TimeCountdownOffset;
uint32_t LastTimeCountdown;
bool OperatingLabelFlag;
uint32_t TimeTick;
uint32_t _button_tick_buf;
uint8_t statetype; //防止label多次刷新



#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif