#include "http.h"
#include "http_recieve.h"
#include "global_message.h"
#include "http_send.h"
//--------------------------------------http中控使用的参数--------------------------------------//
esp_http_client_handle_t client;              //http客户端句柄
http_state m_http_state;
TaskQueue m_taskqueue;
http_task_struct* m_dealing_task;
esp_http_client_handle_t* get_client(void)
{
    return &client;
}

SemaphoreHandle_t Task_list_Mutex;
uint64_t id = 0;
//--------------------------------------http中控使用的参数--------------------------------------//

//--------------------------------------TaskQueue--------------------------------------//
char* taskToString(http_task_t task) {
    switch(task) {
        case NO_TASK:
            return "No Task";
        case ENTER_FOCUS:
            return "Enter Focus";
        case OUT_FOCUS:
            return "Out of Focus";
        case DELETTODO:
            return "Delete Todo";
        case ADDTODO:
            return "Add Todo";
        case FINDLATESTVERSION:
            return "Find Latest Version";
        case FINDTODOLIST:
            return "Find Todo List";
        default:
            return "Unknown Task";
    }
}

// 创建和初始化 http_task_struct 的函数
http_task_struct *create_http_task_struct(http_task_t task_type, char *params[], int param_count, bool need_stuck) {
    // 动态分配内存
    http_task_struct *new_task = (http_task_struct *)malloc(sizeof(http_task_struct));
    if (new_task == NULL) {
        printf("new_task Memory allocation failed!\n");
        return NULL;
    }

    // 初始化任务的成员变量
    new_task->task = task_type;
    new_task->need_stuck = need_stuck;
    new_task->unique_id = id++;

    // 为参数数组分配内存并复制字符串
    for (int i = 0; i < MAX_PARA; ++i) 
    {
        if (i < param_count && params[i] != NULL) 
        {
            new_task->parament[i] = strdup(params[i]);  // 分配内存并复制字符串
            if (new_task->parament[i] == NULL) 
            {
                printf("Memory allocation for parament failed!\n");
                // 释放已分配的内存以避免内存泄漏
                for (int j = 0; j < i; ++j) {
                    free(new_task->parament[j]);
                }
                free(new_task);
                return NULL;
            }
        } 
        else 
        {
            new_task->parament[i] = NULL;  // 确保未使用的参数为 NULL
        }
    }
    return new_task;
}

// 判断队列是否为空
bool isEmpty(TaskQueue *q) {
    return q->front == q->rear;
}
// 判断队列是否已满
bool isFull(TaskQueue *q) {
    return (q->rear + 1) % MAX_TASK_SIZE == q->front;
}

// 入队操作
bool enqueue(TaskQueue *q, http_task_struct *task) 
{
    if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
    {
        if (isFull(q)) {
            printf("Queue is full!\n");
            xSemaphoreGive(Task_list_Mutex);
            return false;
        }
        // 释放原来位置中的内存
        if(q->data[q->rear]!=NULL)
        {
            for (int i = 0; i < MAX_PARA; ++i) {
                if (q->data[q->rear]->parament[i] != NULL) {
                    free(q->data[q->rear]->parament[i]);
                    q->data[q->rear]->parament[i] = NULL;
                }
            }
        }
        q->data[q->rear] = task; // 将任务加入队列
        q->rear = (q->rear + 1) % MAX_TASK_SIZE; // 循环更新队尾索引
        xSemaphoreGive(Task_list_Mutex);
    }
    ESP_LOGI("http_task_list","enqueue:");
    printTaskList(q);
    return true;
}

bool enqueue_front(TaskQueue *q, http_task_struct *task) 
{
    if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
    {
        if (isFull(q)) {
            printf("Queue is full!\n");
            xSemaphoreGive(Task_list_Mutex);
            return false;
        }

        // 更新队头索引到前一个位置
        q->front = (q->front - 1 + MAX_TASK_SIZE) % MAX_TASK_SIZE;

        // 释放原来位置中的内存,巨抽象，由于出列入列，会把我的正在用的内存释放掉
        if(q->data[q->front]!=NULL)
        {
            for (int i = 0; i < MAX_PARA; ++i) 
            {
                if (q->data[q->front]->parament[i] != NULL) {
                    free(q->data[q->front]->parament[i]);
                    q->data[q->front]->parament[i] = NULL;
                }
            }
        }

        q->data[q->front] = task; // 将任务插入到新的队头位置
        xSemaphoreGive(Task_list_Mutex);
    }
    ESP_LOGI("http_task_list","enqueue_front:");
    printTaskList(q);
    return true;
}

// 出队操作
bool dequeue(TaskQueue* q, http_task_struct* dealing_task) 
{
    if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
    {
        if (isEmpty(q)) 
        {
            printf("Queue is empty!\n");
            xSemaphoreGive(Task_list_Mutex);
            return false;
        }
        http_task_struct* _task = q->data[q->front]; // 返回队头任务的指针
        // 将出列任务的信息复制到 dealing_task
        dealing_task->unique_id = _task->unique_id;
        dealing_task->need_stuck = _task->need_stuck;
        dealing_task->task = _task->task;

        //清零释放参数数组
        for (int i = 0; i < MAX_PARA; ++i) 
        {
            if(dealing_task->parament[i]!= NULL) 
            {
                free(dealing_task->parament[i]);
                dealing_task->parament[i] = NULL;
            }
        }

        // 复制参数数组
        for (int i = 0; i < MAX_PARA; ++i) 
        {
            if (_task->parament[i] != NULL) 
            {
                dealing_task->parament[i] = strdup(_task->parament[i]); // 复制字符串
                if (dealing_task->parament[i] == NULL) 
                {
                    printf("Memory allocation failed while copying task parameters!\n");
                    // 如果内存分配失败，需要释放已分配的内存，避免内存泄漏
                    for (int j = 0; j < i; ++j) 
                    {
                        free(dealing_task->parament[j]);
                        dealing_task->parament[j] = NULL;
                    }
                    xSemaphoreGive(Task_list_Mutex);
                    return false;
                }
            } 
        }
        q->front = (q->front + 1) % MAX_TASK_SIZE; // 循环更新队头索引
        xSemaphoreGive(Task_list_Mutex);
        ESP_LOGI("http_task_list","dequeue:");
        printTaskList(q);
    }
    return true;
}

//是否拥有find_task_list这一个任务
bool have_task_find_task_list(TaskQueue* q)
{
    bool is_have = false;
    if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
    {
        if (isEmpty(q)) 
        {
            xSemaphoreGive(Task_list_Mutex);
            return false;
        }
        for (int i = q->front; i != q->rear; i = (i + 1) % MAX_TASK_SIZE) {
            if(q->data[i]->task == FINDTODOLIST)
            {
                is_have = true;
            }
        }
        xSemaphoreGive(Task_list_Mutex);
    }
    return is_have;
}

// 初始化队列
void initQueue(TaskQueue *q) {
    q->front = 0;
    q->rear = 0;
}

void printTaskList(TaskQueue *q) 
{
    if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
    {
        if (isEmpty(q)) {
            ESP_LOGE("http_task_list","Task queue is empty.\n");
            xSemaphoreGive(Task_list_Mutex);
            return;
        }
        ESP_LOGI("http_task_list","front is %d, rear is %d.\n", q->front, q->rear);
        for (int i = q->front; i != q->rear; i = (i + 1) % MAX_TASK_SIZE) {
            ESP_LOGI("http_task_list","Task %d: para is %p, address is %p , content is %s\n", i, (void*)&(q->data[i]->parament), (void*)(q->data[i]),taskToString(q->data[i]->task));
        }
        xSemaphoreGive(Task_list_Mutex);
    }
}
//--------------------------------------TaskQueue--------------------------------------//



// 定义一个静态缓冲区来存储接收的数据
static char *response_buffer = NULL;
static int response_buffer_len = 0;

esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            m_http_state = send_fail;
            ESP_LOGE(HTTP_TAG, "Get_butongbuyang_HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            m_http_state = send_recieving;
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            response_buffer = realloc(response_buffer, response_buffer_len + evt->data_len + 1);
            if (response_buffer == NULL) {
                ESP_LOGE(HTTP_TAG, "Failed to allocate memory for response buffer");
                return ESP_ERR_NO_MEM;
            }
            memcpy(response_buffer + response_buffer_len, evt->data, evt->data_len);
            response_buffer_len += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_FINISH");
            if (response_buffer != NULL) 
            {
                ESP_LOGI(HTTP_TAG, "Full response: %s", response_buffer);
                parse_json_response(response_buffer,m_dealing_task,&m_http_state);
                // 这里可以对完整的响应数据进行处理
                free(response_buffer); // 释放内存
                response_buffer = NULL;
                response_buffer_len = 0;
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            m_http_state = send_fail;
            ESP_LOGI(HTTP_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void http_client_sendMsg(http_task_struct* task);


TaskHandle_t phttp_Task_state;
void http_client_update(void *Parameters )
{
    int64_t send_processing_start_time = 0;
    while(1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); 
        // ESP_LOGI(HTTP_TAG, "m_http_state is %d",m_http_state);
        if(m_http_state == send_waiting)
        { 
            bool need_send = false;
            if (xSemaphoreTake(Task_list_Mutex, portMAX_DELAY) == pdTRUE)
            {
                if(!isEmpty(&m_taskqueue))   
                {
                    xSemaphoreGive(Task_list_Mutex);
                    dequeue(&m_taskqueue,m_dealing_task);
                    need_send = true;
                }
                else
                {
                    xSemaphoreGive(Task_list_Mutex);
                }
            }
            if(need_send)
            {
                ESP_LOGI(HTTP_TAG, "Start send task %d.",m_dealing_task->task);
                m_http_state = send_processing;
                http_client_sendMsg(m_dealing_task);
                send_processing_start_time = esp_timer_get_time();
            }
        } 
        else if (m_http_state == send_processing)
        {
            int64_t current_time = esp_timer_get_time();
            // 检查是否已经持续超过 4 秒
            if ((current_time - send_processing_start_time) > 4000000) // 1000ms = 1000000微秒
            {
                ESP_LOGE(HTTP_TAG, "No response, resend %d.",m_dealing_task->task);
                http_client_sendMsg(m_dealing_task);
                send_processing_start_time = esp_timer_get_time();      // 超时则将状态置为 send_fail
            }
        }        
        else if(m_http_state == send_fail)
        {
            ESP_LOGE(HTTP_TAG, "Send Fail, resend %d.",m_dealing_task->task);
            //删除并重新初始化
            esp_http_client_cleanup(*get_client());
            const esp_http_client_config_t config = {
                .url = "http://120.77.1.151",
                .event_handler = http_client_event_handler,
            };
            client = esp_http_client_init(&config);

            //有点夸张了在下一个函数跑的同时这个表就被置成finish了，
            //所以置标的操作要放到前面，放到后面的话，会把置好的finish标刷回去
            m_http_state = send_processing; 
            http_client_sendMsg(m_dealing_task);
            send_processing_start_time = esp_timer_get_time();    
        }

        //处理task_list的更新
        if(get_task_list_state() == updating_from_server)
        {
            if(m_dealing_task->task == FINDTODOLIST && m_http_state == send_waiting && !have_task_find_task_list(&m_taskqueue))
            {
                set_task_list_state(firmware_need_update);                
            }
            if(m_dealing_task->task != FINDTODOLIST && !have_task_find_task_list(&m_taskqueue))
            {
                set_task_list_state(firmware_need_update);
            }
        }

    }
}

void http_client_init(void)
{
    const esp_http_client_config_t config = {
        .url = "http://120.77.1.151",
        .event_handler = http_client_event_handler,
    };
    client = esp_http_client_init(&config);
    if(!client)  ESP_LOGE(HTTP_TAG,"httpclient init error!\r\n");
    else ESP_LOGI(HTTP_TAG,"httpclient init success!\r\n"); 

    initQueue(&m_taskqueue);
    m_http_state = send_waiting;

    Task_list_Mutex = xSemaphoreCreateMutex();
    xTaskCreate(http_client_update, "http_client_update", 8192, NULL, 10, phttp_Task_state);

    m_dealing_task = create_http_task_struct(NO_TASK, NULL, 0 ,false);

    //暂时放到这里
    get_global_data()->usertoken  = strdup("d7e2be05eece4ce09c74baf798a39b99");
}

void http_client_sendMsg(http_task_struct* task)
{
    if(get_global_data()->usertoken == NULL)
    {
        ESP_LOGE(HTTP_TAG,"No usertoken detect\n");
    }
    else
    {
        http_send(task);
    }
}

//优先级很高所以都用enqueue_front
void http_get_todo_list(bool need_stuck)
{
    set_task_list_state(updating_from_server);
    http_task_struct *m_task = create_http_task_struct(FINDTODOLIST, NULL, 0 ,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck){
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue_front(&m_taskqueue,m_task);
    }
}

void http_add_to_do(char *title, char*todoType, bool need_stuck)
{
    char *params[] = {title, todoType};  // 示例参数
    int param_count = 2;
    http_task_struct *m_task = create_http_task_struct(ADDTODO,params,param_count,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck){
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue(&m_taskqueue,m_task);
    }
}

void http_in_focus(char *id, int fallingTime, bool need_stuck)
{
    char fallingTime_str[20];        
    sprintf(fallingTime_str, "%d", fallingTime);

    char *params[] = {id, "1", fallingTime_str};  // 示例参数
    int param_count = 3;
    http_task_struct *m_task = create_http_task_struct(ENTER_FOCUS,params,param_count,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck){
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue(&m_taskqueue,m_task);
    }
}

void http_out_focus(char *id,bool need_stuck)
{
    char *params[] = {id, "2"};  // 示例参数
    int param_count = 2;
    http_task_struct *m_task = create_http_task_struct(OUT_FOCUS,params,param_count,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck){
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue(&m_taskqueue,m_task);
    }
}

void http_delet_todo(char *id,bool need_stuck)
{  
    char *params[] = {id};  // 示例参数
    int param_count = 1;
    http_task_struct *m_task = create_http_task_struct(DELETTODO,params,param_count,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck){
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue(&m_taskqueue,m_task);
    }
}

void http_get_latest_version(bool need_stuck)
{
    http_task_struct *m_task = create_http_task_struct(FINDLATESTVERSION, NULL, 0 ,need_stuck);
    if (m_task == NULL) 
    {
        ESP_LOGE(HTTP_TAG, "Failed to create http_task_struct!");
        return;
    }
    if(need_stuck)
    {
        enqueue_front(&m_taskqueue,m_task);
        //保证正在进行的task是我输入进去的task,下面的while会一直等待直到执行到我输入的task
        while(m_dealing_task->unique_id != m_task->unique_id) vTaskDelay(100 / portTICK_PERIOD_MS);
        //进入到下一层死循环，指导dealing——task的stuck被置为false跳出循环
        while(m_dealing_task->need_stuck) vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else{
        enqueue(&m_taskqueue,m_task);
    }
}
