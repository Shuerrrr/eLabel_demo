#ifndef HTTP_H
#define HTTP_H

#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"

#define MAX_TASK_SIZE 64
#define HTTP_TAG "HTTP"
#define MAX_PARA 10

typedef enum {
    NO_TASK,
    ENTER_FOCUS,
    OUT_FOCUS,
    DELETTODO,
    ADDTODO,
    FINDLATESTVERSION,
    FINDTODOLIST,
} http_task_t;

char* taskToString(http_task_t task);

typedef enum {
    send_waiting,
    send_processing,
    send_recieving,
    send_fail,
} http_state;

typedef struct {
    uint64_t unique_id;
    http_task_t task;
    char *parament[MAX_PARA];//不搞二维数组了纯恶心自己，算了选择定长，享受美好人生。
    bool need_stuck;
}http_task_struct;
//--------------------------------------TaskQueue--------------------------------------//
typedef struct {
    http_task_struct* data[MAX_TASK_SIZE];        // 存储队列元素
    int front;                        // 队头索引
    int rear;                         // 队尾索引
} TaskQueue;

bool isEmpty(TaskQueue *q);
bool isFull(TaskQueue *q);
bool enqueue(TaskQueue *q, http_task_struct *task);
bool enqueue_front(TaskQueue *q, http_task_struct *task);
bool dequeue(TaskQueue* q, http_task_struct* dealing_task);
esp_http_client_handle_t* get_client(void);
http_task_struct *create_http_task_struct(http_task_t task_type, char *params[], int param_count, bool need_stuck);
void initQueue(TaskQueue *q);
void printTaskList(TaskQueue *q);
//--------------------------------------TaskQueue--------------------------------------//


void http_client_init(void);

void http_get_latest_version(bool need_stuck);

void http_add_to_do(char *title, char*todoType, bool need_stuck);

void http_get_todo_list(bool need_stuck);

void http_in_focus(char *id, int fallingTime, bool need_stuck);

void http_out_focus(char *id,bool need_stuck);

void http_delet_todo(char *id,bool need_stuck);

#endif

