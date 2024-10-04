/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
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

#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    #if defined CONFIG_LV_USE_DEMO_WIDGETS
        #include "lv_examples/src/lv_demo_widgets/lv_demo_widgets.h"
    #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
        #include "lv_examples/src/lv_demo_keypad_encoder/lv_demo_keypad_encoder.h"
    #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
        #include "lv_examples/src/lv_demo_benchmark/lv_demo_benchmark.h"
    #elif defined CONFIG_LV_USE_DEMO_STRESS
        #include "lv_examples/src/lv_demo_stress/lv_demo_stress.h"
    #else
        #error "No demo application selected."
    #endif
#endif

/*********************
 *      DEFINES
 *********************/
#define SD_WP 14;
#define SD_CD 9;
#define SD_MISO 13
#define SD_MOSI 11
#define SD_CLK 12
#define SD_CS 10

#define MOUNT_POINT "/sdcard"

#define TAG "demo"
#define LV_TICK_PERIOD_MS 1
#define EC56_GPIO_A 21
#define EC56_GPIO_B 47

#define BUTTON_GPIO1 2

#define BUZZER_GPIO_PWM 8
static uint8_t flag = 0;
static uint8_t icnt = 0;
static int encoderTestNum = 0;
static uint8_t pressed = 0;
static xQueueHandle gpioEventQueue = NULL;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);
static void encoder_gpio_init(void);
static void buzzer_pwm_init();
static void SDcard_init();


static void SDcard_test()
{
    FILE* f = fopen("/sdcard/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello %s!\n", "world");
    fclose(f);
    ESP_LOGI(TAG, "File written");
}

static void SDcard_init()
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    // slot_config.gpio_wp = SD_WP;
    // slot_config.gpio_cd = SD_CD;
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    SDcard_test();

    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    // //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}


// static void SDcard_deinit(sdmmc_card_t *card)
// {
    
// }

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    if(gpio_num == EC56_GPIO_A)
    {
        if(gpio_get_level(EC56_GPIO_A) && icnt == 0)
        {
            flag = 0;
            if(gpio_get_level(EC56_GPIO_B)) flag = 1;
            icnt = 1;
        } 
        if(icnt && !gpio_get_level(EC56_GPIO_A))
        {
            if(gpio_get_level(EC56_GPIO_B) == 0 && flag == 1)
            {
                encoderTestNum++;
            }
            if(gpio_get_level(EC56_GPIO_B) == 1 && flag == 0)
            {
                encoderTestNum--;
            }
            icnt = 0;
        }
    }
    else if(gpio_num == BUTTON_GPIO1)
    {
        lv_event_send(lv_scr_act(), LV_EVENT_CLICKED, NULL);  // Trigger the click event
    }
    // xQueueSendFromISR(gpioEventQueue, &gpio_num, NULL);
}

static void encoder_gpio_init(void)
{
    gpio_config_t io_conf = {};
	// A 相 IO 配置为中断上升沿/下降沿都触发的模式
    io_conf.intr_type = GPIO_INTR_ANYEDGE; /* 上升沿/下降沿都触发 */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL<<EC56_GPIO_A; // A 相接 D4
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

	// B 相 IO 配置为普通的输入模式
    io_conf.intr_type = GPIO_INTR_DISABLE; /* 禁用中断 */
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL<<EC56_GPIO_B; // B 相接 D15
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    //按键中断
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL<<BUTTON_GPIO1);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(EC56_GPIO_A, gpio_isr_handler, (void*)EC56_GPIO_A);
    gpio_isr_handler_add(BUTTON_GPIO1, gpio_isr_handler, (void*)BUTTON_GPIO1);
    // gpio_isr_handler_add(EC56_GPIO_B, gpio_isr_handler, (void*)EC56_GPIO_B);
}

static void buzzer_pwm_init()
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2700,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = BUZZER_GPIO_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

static void buzzer_pwm_start(uint32_t new_frequency)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 700);
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, new_frequency);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */

    printf("Starting WIFI Test!\n");
    Z_WiFi_Init();

    printf("Starting LVGL example\n");
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);

    gpioEventQueue = xQueueCreate(10, sizeof(uint32_t));
    encoder_gpio_init();
    buzzer_pwm_init();

    // sdmmc_card_t *card;
    // SDcard_init();
    // SDcard_deinit(card);

    while(1)
    {
        pressed = gpio_get_level(BUTTON_GPIO1);
        vTaskDelay(100 / portTICK_RATE_MS);
        printf("encoderTestNum: %d,pressed: %d \n", encoderTestNum,pressed);
        // buzzer_pwm_start(1000);
        // vTaskDelay(1000 / portTICK_RATE_MS);
        // buzzer_pwm_start(500);
        // vTaskDelay(1000 / portTICK_RATE_MS);
        // buzzer_pwm_start(100);
        // vTaskDelay(1000 / portTICK_RATE_MS);
    }
    // xTaskCreate(encoder_button_task, "encoder_button_task", 2048, NULL, 2, NULL);
}

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static void guiTask(void *pvParameter) {

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = (lv_color_t*)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),MALLOC_CAP_DMA); //MALLOC_CAP_DMA);MALLOC_CAP_SPIRAM
    // if (buf1 == NULL) {
    // printf("SPIRAM allocation failed, falling back to DMA-capable memory\n");
    // buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    // }
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
// #ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
//     lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
//     assert(buf2 != NULL);
// #else
    static lv_color_t *buf2 = NULL;
// #endif

    static lv_disp_draw_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;

// #if defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT || defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT_INVERTED
    // disp_drv.rotated = 2;
// #endif

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
    disp_drv.set_px_cb = disp_driver_set_px;
// #ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    
// #endif

    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();
    lv_refr_now(NULL);

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            // printf("before lv_task_handler\n");
            lv_task_handler();
            // printf("lv_task_handler\n");
            xSemaphoreGive(xGuiSemaphore);
       }
    }

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}

static void create_demo_application(void)
{
    /* When using a monochrome display we only show "Hello World" centered on the
     * screen */
    printf("Creating the demo application\n");
    

// #if defined CONFIG_LV_TFT_DISPLAY_MONOCHROME || 
//     defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S

    /* use a pretty small demo for monochrome displays */
    /* Get the current screen  */
    // lv_obj_t * scr = lv_disp_get_scr_act(NULL);

    // /*Create a Label on the currently active screen*/
    // lv_obj_t * label1 =  lv_label_create(scr);

    // /*Modify the Label's text*/
    // lv_label_set_text(label1, "Hello\nworld");

    // /* Align the Label to the center
    //  * NULL means align on parent (which is the screen now)
    //  * 0, 0 at the end means an x, y offset after alignment*/
    // lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);

    ui_init();

// #else
//         /* Otherwise we show the selected demo */
//     #if defined CONFIG_LV_USE_DEMO_WIDGETS
//         lv_demo_widgets();
//         printf("lv_demo_widgets\n");
//     #elif defined CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER
//         lv_demo_keypad_encoder();
//     #elif defined CONFIG_LV_USE_DEMO_BENCHMARK
//         lv_demo_benchmark();
//     #elif defined CONFIG_LV_USE_DEMO_STRESS
//         lv_demo_stress();
//     #else
//         #error "No demo application selected."
//     #endif
// #endif
}

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}
