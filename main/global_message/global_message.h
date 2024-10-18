#ifndef GLOBAL_MESSAGE_H
#define GLOBAL_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define FIRMWARE_VERSION "2.0.0"

void SNTP_syset_time(void);

void HTTP_syset_time(void);

long long get_unix_time(void);

//--------------------------------------Focus 对应的结构体--------------------------------------//
typedef struct {
    bool is_focus; 
    bool is_created_by_myself;
    int focus_task_id;
} Focus_state;

//--------------------------------------Task_list对应的结构体--------------------------------------//
typedef enum {
    newest,
    updating_from_server,
    firmware_need_update,
} task_list_state;

task_list_state get_task_list_state(void);

void set_task_list_state(task_list_state _task_list_state);

//2表示否 1表示是 0表示没东西
typedef struct {
    char *createBy;
    long long createTime;
    char *updateBy;
    long long updateTime;
    char *remark;
    int id;
    char *title;
    int isPressing;
    char *todoType;
    int isComplete;
    long long startTime;
    int fallTiming;
    int isFocus;
    int isImportant;
} TodoItem;

typedef struct {
    TodoItem *items;  // 动态数组存储 TodoItem
    int size;         // 当前数组中的元素数量
} TodoList;

// 根据 ID 查找 TodoItem
TodoItem* find_todo_by_id(TodoList *list, int id);

// 根据 Title 查找 TodoItem
TodoItem* find_todo_by_title(TodoList *list, const char *title);

// 向 TodoList 添加一个 TodoItem
void add_or_update_todo_item(TodoList *list, TodoItem item);

//清除 TodoList
void clean_todo_list(TodoList *list);

//-------------------------------------- Global_data--------------------------------------//
typedef struct {
    Focus_state* m_focus_state;
    TodoList* m_todo_list;

    char* usertoken;

    char mac_str[18];
    
    char* newest_firmware_url;
    char *version;
    char *deviceModel;
    char *createTime;
} Global_data;
// 获取单例实例的函数
Global_data* get_global_data();

#endif