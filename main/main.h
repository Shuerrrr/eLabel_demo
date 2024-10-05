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