#include "main.h"
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


uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t slave_num = 0;
uint8_t slave_mac[MAX_SLAVES][ESP_NOW_ETH_ALEN] = { 0 };

// 创建新任务
void espnow_send_buf_reset(espnow_send_param_buf* buff)
{
    buff->msg_type = 0;
    buff->task_method = 0;
}

TaskNode* create_task(const char* task_content) {
    TaskNode *new_task = (TaskNode *)malloc(sizeof(TaskNode));
    if (new_task == NULL) {
        printf("内存分配失败\n");
        return NULL;
    }
    new_task->task = strdup(task_content);  // 复制任务内容
    new_task->next = NULL;
    return new_task;
}

// 添加任务到链表末尾
void add_task(TaskNode **head, const char *task_content) {
    TaskNode *new_task = create_task(task_content);
    if (*head == NULL) {
        *head = new_task;  // 如果链表为空，设置为头结点
    } else {
        TaskNode *temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_task;
    }
    printf("任务 \"%s\" 添加成功！\n", task_content);
    tasklen++;
}

// 删除指定位置的任务
void delete_task(TaskNode **head, int position) {
    if (*head == NULL) {
        printf("任务列表为空，无法删除任务！\n");
        return;
    }

    TaskNode *temp = *head;

    // 删除头结点
    if (position == 0) {
        *head = temp->next;
        printf("任务 \"%s\" 已删除\n", temp->task);
        free(temp->task);  // 释放任务字符串的内存
        free(temp);        // 释放节点的内存
        return;
    }

    // 找到要删除任务的前一个节点
    for (int i = 0; temp != NULL && i < position - 1; i++) {
        temp = temp->next;
    }

    // 如果找不到任务
    if (temp == NULL || temp->next == NULL) {
        printf("指定位置的任务不存在！\n");
        return;
    }

    TaskNode *next = temp->next->next;
    printf("任务 \"%s\" 已删除\n", temp->next->task);
    free(temp->next->task);  // 释放任务字符串的内存
    free(temp->next);        // 释放节点的内存
    temp->next = next;
    tasklen--;
}

// 打印任务列表
void print_tasks(TaskNode *head) {
    TaskNode *temp = head;
    int index = 0;
    printf("任务列表:\n");
    while (temp != NULL) {
        printf("任务[%d]: %s\n", index, temp->task);
        temp = temp->next;
        index++;
    }
}

// 释放所有任务的内存
void free_tasks(TaskNode *head) {
    TaskNode *temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp->task);  // 释放任务字符串的内存
        free(temp);        // 释放节点的内存
    }
}

char* find_task_by_position(TaskNode *head, int position) {
    TaskNode *temp = head;
    int index = 0;

    // 遍历链表直到找到指定位置的任务
    while (temp != NULL) {
        if (index == position) {
            return temp->task;  // 找到任务，返回节点指针
        }
        temp = temp->next;
        index++;
    }

    // 如果超出了链表长度，返回 NULL
    return NULL;
}

void tasks2str(TaskNode *head)
{
    TaskNode *temp = head;
    int index = 0;  // 用于记录当前字符串的累积长度

    // 清空 taskstr 以防止之前的数据干扰
    memset(taskstr, 0, sizeof(taskstr));

    while (temp != NULL) {
        // 获取当前任务字符串的长度
        int len = strlen(temp->task);

        // 检查缓冲区是否还有足够空间来存储任务和换行符
        if (index + len + 1 >= sizeof(taskstr)) {
            printf("缓冲区溢出，任务字符串过长！\n");
            break;
        }

        // 将任务字符串和换行符拼接到 taskstr 中
        sprintf(taskstr + index, "%s\n", temp->task);

        // 更新索引，len 表示当前任务的长度，+1 是换行符的长度
        index += len + 1;

        // 移动到下一个任务
        temp = temp->next;
    }
}

void stateInit()
{
    label_state = Preparing;
    EncoderValue = lastEncoderValue = 0;
    task_list = NULL;
    tasklen = 0;
    chosenTaskNum = lastChosenTaskNum = 0;
    TimeCountdown = TimeCountdownOffset = 120;
    LastTimeCountdown = 0;
    _button_tick_buf = 0;
    TimeTick = 0;
}

void ChangeState(ELABEL_STATE state)
{
    // Exit(label_state);
    Enter(state);
    label_state = state;
}

//espnow消息背面不间断地发，上层通信看接口状况，改变了就发

void Enter(ELABEL_STATE state)
{
    switch (state)
    {
    case Preparing:
        EncoderValue = lastEncoderValue = 0;
        if(!tasklen) task_list = NULL;
        chosenTaskNum = 0;
        needFlashEpaper = true;
        break;
    case Operating:
        //好像没什么要做的
        statetype = 0;
        EncoderValue = lastEncoderValue = 0;
        needFlashEpaper = true;
        TimeCountdownOffset = TimeCountdown = 120;
        _button_tick_buf = 0;
        break;
    case Focus:
        needFlashEpaper = true;
        _button_tick_buf = TimeTick;
        break;
    case Stopping:
        needFlashEpaper = true;
        break;
    default:
        break;
    }
}

void Execute()
{
    while(1)
    {
        sync_recv_update();
        switch (label_state)
        {
        case Preparing:
            
            //显示任务列表...needFlashEpaper写在通信中
            
            //按下按键确认changestate
            if(!gpio_get_level(BUTTON_GPIO1) && task_list != NULL)
            {
                OperatingLabelFlag = true;
                lv_event_send(lv_scr_act(), LV_EVENT_CLICKED, NULL);
                char* taskname = find_task_by_position(task_list,chosenTaskNum);

                char *sname = (char*)(malloc(strlen(taskname)+strlen("\n\n\n\nTimeSet:")));
                sprintf(sname,"%s%s",taskname,"\n\n\n\nTimeSet:");
                lv_label_set_text(ui_Label3, sname);
                free(sname);
                
                ChangeState(Operating);
                espnow_send_buf.msg_type = 1;
                espnow_send_buf.elabel_state = 2;
                espnow_send_buf.chosenTaskId = chosenTaskNum;
                
                sync_to_slaves(NULL);

                printf("change into Operating\n");
                continue;
            }

            if(tasklen)//旋钮选择任务
            {
                if(EncoderValue < 0)
                {
                    EncoderValue = tasklen - 1;
                }
                chosenTaskNum = (EncoderValue/ENCODER_K) % tasklen;
                if(chosenTaskNum != lastChosenTaskNum)
                {
                    needFlashEpaper = true;
                }
                lastChosenTaskNum = chosenTaskNum;
                // printf("chosenTaskNum: %d\n",chosenTaskNum);
            }

            if(needFlashEpaper)
            {
                //调用lvgl刷新屏幕
                lv_roller_set_selected(ui_Roller1, chosenTaskNum, LV_ANIM_OFF);
                needFlashEpaper = false;
            }
            break;
        case Operating:
            //显示操作中/更改时间
            if(OperatingLabelFlag)
            {
                //旋钮changestate
                if(gpio_get_level(BUTTON_GPIO1) && TimeTick - _button_tick_buf > 50) //50*10ms
                {
                    if(statetype != 1)
                    {
                        _ui_flag_modify(ui_Label10, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                        _ui_flag_modify(ui_Label9, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
                        statetype = 1;
                    }

                    if(EncoderValue/ENCODER_K2 < 0)
                    {
                        lv_event_send(lv_scr_act(), LV_EVENT_CLICKED, NULL);
                        ChangeState(Preparing);

                        espnow_send_buf.msg_type = 1;
                        espnow_send_buf.elabel_state = 1;
                        sync_to_slaves(NULL);
                        continue;
                    }
                    else if(EncoderValue/ENCODER_K2 > 0)
                    {
                        lv_event_send(lv_scr_act(), LV_EVENT_PRESSED, NULL);
                        char* taskname = find_task_by_position(task_list,chosenTaskNum);
                        char *sname = (char*)(malloc(strlen(taskname)+strlen("\n\n\n\nTimeCountdown:")));
                        sprintf(sname,"%s%s",taskname,"\n\n\n\nTimeCountdown:");
                        lv_label_set_text(ui_Label7, sname);
                        free(sname);
                        ChangeState(Focus);

                        espnow_send_buf.msg_type = 1;
                        espnow_send_buf.elabel_state = 3;
                        sync_to_slaves(NULL);
                        continue;
                    }
                }
                else if(gpio_get_level(BUTTON_GPIO1)) //刚松开时记录时间，并屏蔽旋钮changestate
                {
                    EncoderValue = 0;
                    TimeCountdownOffset = TimeCountdown;
                }
                else //记得在按下中断中把encodervalue清零,按键加旋钮改倒计时时间
                {
                    if (statetype != 2)
                    {
                        _ui_flag_modify(ui_Label10, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                        _ui_flag_modify(ui_Label9, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
                        statetype = 2;
                    }
                
                    TimeCountdown = TimeCountdownOffset + EncoderValue * 2;
                    _button_tick_buf = TimeTick;
                }
                
                if(TimeCountdown != LastTimeCountdown) needFlashEpaper = true;
                LastTimeCountdown = TimeCountdown;
            }
            else
            {
                printf("operating");
            }
            // else显示操作中 
            
            if(needFlashEpaper)
            {
                //调用lvgl刷新屏幕,可能需要局刷数字
                if(OperatingLabelFlag)
                {
                    char timestr[10] = "00:00";
                    uint32_t total_seconds = TimeCountdown;  // 倒计时总秒数
                    uint8_t minutes = total_seconds / 60;  // 计算分钟数
                    uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
                    // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
                    sprintf(timestr, "%02d:%02d", minutes, seconds);
                    lv_label_set_text(ui_Label2, timestr);
                }
                else
                {
                    //调用lvgl刷新屏幕操作中即可
                }
                needFlashEpaper = false;
            }
            
            break;
        case Focus:
            //按钮长按/倒计时结束后短按changestate
            if(gpio_get_level(BUTTON_GPIO1))//松开时
            {
                if(TimeTick - _button_tick_buf > 200)//200*10ms,长按
                {
                    lv_event_send(lv_scr_act(), LV_EVENT_CLICKED, NULL);
                    ChangeState(Preparing);

                    espnow_send_buf.msg_type = 1;
                    espnow_send_buf.elabel_state = 1;
                    sync_to_slaves(NULL);
                    continue;
                }
                else if(TimeTick - _button_tick_buf <= 200 && TimeTick - _button_tick_buf > 10) //10*10ms,短按
                {
                    if(!TimeCountdown)
                    {
                        delete_task(&task_list,chosenTaskNum);//删除当前任务
                        tasks2str(task_list);
                        lv_roller_set_options(ui_Roller1, taskstr, LV_ROLLER_MODE_NORMAL);
                        lv_event_send(lv_scr_act(), LV_EVENT_CLICKED, NULL);
                        ChangeState(Preparing);

                        espnow_send_buf.msg_type = 3;
                        espnow_send_buf.elabel_state = 1;
                        espnow_send_buf.task_method = 3;
                        espnow_send_buf.chosenTaskId = chosenTaskNum;
                        sync_to_slaves(NULL);

                        continue;
                    }
                }
                else
                {
                    _button_tick_buf = TimeTick;
                }
            }

            //显示倒计时时间--
            if(TimeCountdown > 0 && TimeTick % 100 == 0) TimeCountdown--;

            if(TimeCountdown != LastTimeCountdown)
            {
                needFlashEpaper = true;
            }
            LastTimeCountdown = TimeCountdown;
            if(needFlashEpaper)
            {
                char timestr[10] = "00:00";
                uint32_t total_seconds = TimeCountdown;  // 倒计时总秒数
                uint8_t minutes = total_seconds / 60;  // 计算分钟数
                uint8_t seconds = total_seconds % 60;  // 计算剩余秒数
                // 使用 sprintf 将分钟和秒格式化为 "MM:SS" 格式的字符串
                sprintf(timestr, "%02d:%02d", minutes, seconds);
                lv_label_set_text(ui_Label5, timestr);
                //调用lvgl刷新屏幕,局刷数字
                needFlashEpaper = false;
            }

            break;
        case Stopping:
            break;
        default:
            break;
        }
        TimeTick++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void sync_to_slaves(const char *task_content)
{
    if(send_param->state == 0) return;
    send_param->msg_type = espnow_send_buf.msg_type;
    send_param->task_method = espnow_send_buf.task_method;
    send_param->elabel_state = espnow_send_buf.elabel_state;
    send_param->chosenTaskId = espnow_send_buf.chosenTaskId;
    send_param->TimeCountdown = espnow_send_buf.TimeCountdown;
    send_param->changeTaskId = espnow_send_buf.changeTaskId;

    switch(espnow_send_buf.msg_type)
    {
        case 1:
        {
            send_param->len = sizeof(example_espnow_data_t);
            break;
        }
        case 2:
            send_param->len = strlen(task_content) + sizeof(example_espnow_data_t);
            example_espnow_data_t *buf11 = (example_espnow_data_t*)send_param->buffer;
            strcpy((char*)(buf11->payload), task_content);
            break;
        case 3:
            send_param->len = strlen(task_content) + sizeof(example_espnow_data_t);
            example_espnow_data_t *buf22 = (example_espnow_data_t*)send_param->buffer;
            strcpy((char*)(buf22->payload), task_content);
            break;
        default:
            send_param->len = sizeof(example_espnow_data_t);
            break;
    }
    for(int i = 0; i < slave_num; i++)
    {
        uint8_t *mac_addr = slave_mac[i];
        example_espnow_event_t evt;
        example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

        if (mac_addr == NULL) {
            // ESP_LOGE(TAG, "Send cb arg error");
            return;
        }

        evt.id = EXAMPLE_ESPNOW_SEND_CB;
        memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
        // send_cb->status = status;
        if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
            printf("Send send queue fail\n");
        }
    }
    espnow_send_buf_reset(&espnow_send_buf);
}

void sync_recv_update()
{
    switch (espnow_recv_buf.msg_type)
    {
    case 1:
        if(espnow_recv_buf.elabel_state == 1)
        {
            _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_Screen3_screen_init);
            ChangeState(Preparing);
        }
        else if(espnow_recv_buf.elabel_state == 2)
        {
            OperatingLabelFlag = false;
            chosenTaskNum = espnow_recv_buf.chosenTaskId;
            printf("change into Operating\n");
            ChangeState(Operating);
        }
        else if(espnow_recv_buf.elabel_state == 3)
        {
            TimeCountdownOffset = TimeCountdown = espnow_recv_buf.TimeCountdown;
            chosenTaskNum = espnow_recv_buf.chosenTaskId;

            _ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_Screen3_screen_init);
            char* taskname = find_task_by_position(task_list,chosenTaskNum);
            char *sname = (char*)(malloc(strlen(taskname)+strlen("\n\n\n\nTimeCountdown:")));
            sprintf(sname,"%s%s",taskname,"\n\n\n\nTimeCountdown:");
            lv_label_set_text(ui_Label7, sname);
            free(sname);

            ChangeState(Focus);
        }
        break;
    case 2:
        if(espnow_recv_buf.task_method == 2)
        {
            add_task(&task_list, (char*)espnow_recv_buf.payload);
        }
        else if(espnow_recv_buf.task_method == 3)
        {
            delete_task(&task_list, espnow_recv_buf.changeTaskId);
        }
        else if(espnow_recv_buf.task_method == 1)
        {
            char *task = find_task_by_position(task_list, espnow_recv_buf.changeTaskId);
            if(task != NULL)
            {
                free(task);
                task = strdup((char*)espnow_recv_buf.payload);
            }
        }
        break;
    case 3:
        if(espnow_recv_buf.task_method == 2)
        {
            add_task(&task_list, (char*)espnow_recv_buf.payload);
        }
        else if(espnow_recv_buf.task_method == 3)
        {
            delete_task(&task_list, espnow_recv_buf.changeTaskId);
        }
        else if(espnow_recv_buf.task_method == 1)
        {
            char *task = find_task_by_position(task_list, espnow_recv_buf.changeTaskId);
            if(task != NULL)
            {
                free(task);
                task = strdup((char*)espnow_recv_buf.payload);
            }
        }


        if(espnow_recv_buf.elabel_state == 1)
        {
            _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_Screen3_screen_init);
            ChangeState(Preparing);
        }
        else if(espnow_recv_buf.elabel_state == 2)
        {
            OperatingLabelFlag = false;
            chosenTaskNum = espnow_recv_buf.chosenTaskId;
            printf("change into Operating\n");
            ChangeState(Operating);
        }
        else if(espnow_recv_buf.elabel_state == 3)
        {
            TimeCountdownOffset = TimeCountdown = espnow_recv_buf.TimeCountdown;
            chosenTaskNum = espnow_recv_buf.chosenTaskId;

            _ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_NONE, 500, 500, &ui_Screen3_screen_init);
            char* taskname = find_task_by_position(task_list,chosenTaskNum);
            char *sname = (char*)(malloc(strlen(taskname)+strlen("\n\n\n\nTimeCountdown:")));
            sprintf(sname,"%s%s",taskname,"\n\n\n\nTimeCountdown:");
            lv_label_set_text(ui_Label7, sname);
            free(sname);

            ChangeState(Focus);
        }
        break;
    
    default:
        break;
    }
    espnow_send_buf_reset(&espnow_recv_buf);
}
