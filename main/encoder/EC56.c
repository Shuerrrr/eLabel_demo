#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "EC56.h"
 
 
static xQueueHandle gpioEventQueue = NULL;
static uint8_t count = 0;

static void time_out_task(void *arg)
{
    vTaskDelay(200 / portTICK_RATE_MS);
    count = 0;
    vTaskDelete(NULL);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpioEventQueue, &gpio_num, NULL);
}

static void ec56_task(void)
{
     uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpioEventQueue, &io_num, portMAX_DELAY)) {
            // printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            if(io_num == EC56_GPIO_A)
            {                
                if(count == 1)
                {
                    count = 3;
                    printf("+ turn\n");
                }
                else if(count == 0)
                {
                    count = 2;
                    xTaskCreate(time_out_task, "time_out_task", 2048, NULL, 10, NULL);
                }                                  
            } 
            else if(io_num == EC56_GPIO_B)
            {
                if(count == 2)
                {
                    count = 3;
                    printf("- turn\n");
                }
                else if(count==0){
                    count = 1;
                    xTaskCreate(time_out_task, "time_out_task", 2048, NULL, 10, NULL);
                }
            }           
        }
    }
}

void ec56_init(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL<<EC56_GPIO_A) | (1ULL<<EC56_GPIO_B);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpioEventQueue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(ec56_task, "ec56_task", 2048, NULL, 1, NULL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(EC56_GPIO_A, gpio_isr_handler, (void*)EC56_GPIO_A);
    gpio_isr_handler_add(EC56_GPIO_B, gpio_isr_handler, (void*)EC56_GPIO_B);
    
}

