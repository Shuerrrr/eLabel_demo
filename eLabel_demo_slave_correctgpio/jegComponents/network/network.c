#include "network.h"
#include "blufi.h"
#include "m_esp_now.h"

#define WIFI_CONNECT "WIFI_CONNECT"

bool is_wifi_init = false;
bool is_wifi_preprocess = false;

static void wifi_init_sta(void);
static void get_nvs_info(void);
static void wifi_preprocess(void);

uint8_t get_wifi_status(void)
{
    return wifi_state;
}

void set_wifi_status(uint8_t status)
{
    wifi_state = status;
}

void wifi_connect(void)
{
    if(!is_wifi_preprocess)
    {
        is_wifi_preprocess = true;
        wifi_preprocess();
    }
    if(!is_wifi_init)
    {
        wifi_init_sta();  
        is_wifi_init = true;
    }
}

void fake_disconnect(void)
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    wifi_state = 0x00;     
}

void fake_connect(void)
{
    ESP_ERROR_CHECK(esp_wifi_connect());    
    wifi_state = 0x01;   
}

void wifi_disconnect(void)
{
    if(is_wifi_init == false)
    {
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    is_wifi_init = false;
    wifi_state = 0x00;
    ESP_LOGI(WIFI_CONNECT,"Successful_disconnect_wifi. \n");
}

void start_blufi(void)
{
    ESP_LOGI(GATTS_TABLE_TAG,"bluficonfig start ....... \n");

    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_SSID], sizeof(wifi_ssid), (const uint8_t*)wifi_ssid);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_PASS], sizeof(wifi_passwd), (const uint8_t*)wifi_passwd);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_CUSTOM], sizeof(customer), (const uint8_t*)customer);
    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_STATE], sizeof(wifi_state), (const uint8_t*)&wifi_state);

    if(!is_blufi_init)
    {
        xTaskCreate(blufi_notify, "blufi_notify", 8192, NULL, 10, pTask_blufi);

        blufi_notify_flag = true;

        is_blufi_init = true;

        esp_err_t ret;

        if(!is_release_classic_ble)
        {
            is_release_classic_ble = true;
            // 释放经典蓝牙资源，保证设备不工作在经典蓝牙下面
            ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));   
        }
   
        // 初始化蓝牙控制器
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        } 

        // 使能蓝牙控制器，工作在 BLE mode
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        }  

        // 初始化蓝牙主机，使能蓝牙主机
        ret = esp_bluedroid_init();
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        ret = esp_bluedroid_enable();
        if (ret) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }

        // 注册 GATT 回调函数
        ret = esp_ble_gatts_register_callback(gatts_event_handler);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gatts register error, error code = %x", ret);
            return;
        }

        // 注册 GAP 回调函数, 蓝牙是通过GAP建立通信的(广播)
        ret = esp_ble_gap_register_callback(gap_event_handler);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gap register error, error code = %x", ret);
            return;
        }

        //注册 service 
        ret = esp_ble_gatts_app_register(ESP_APP_ID);
        if (ret){
            ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error, error code = %x", ret);
            return;
        }
        //设置 mtu
        esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
        if (local_mtu_ret){
            ESP_LOGE(GATTS_TABLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
        }
    }
}

/* 系统事件循环处理函数 */
static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    static int retry_num = 0;           /* 记录wifi重连次数 */
    /* 系统事件为WiFi事件 */
    if (event_base == WIFI_EVENT) 
    {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) /* 当wifi失去STA连接 */ 
        {
            wifi_state = 0x01;
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(WIFI_CONNECT,"retry to connect to the AP %d times. \n",retry_num);
            ESP_LOGI(WIFI_CONNECT,"Retry connecting wifi with Password: %s and ssid %s\n", wifi_passwd, wifi_ssid);
            if (retry_num > 0)  /* WiFi重连次数大于10 */
            {
                /* 将WiFi连接事件标志组的WiFi连接失败事件位置1 */
                ESP_LOGE(WIFI_CONNECT,"Fail connecting wifi with Password: %s and ssid %s\n", wifi_passwd, wifi_ssid);
                // 清零 wifi_state
                wifi_state = 0x00;
                start_blufi();   //开启蓝牙接口
                retry_num = 0;
                ESP_ERROR_CHECK(esp_wifi_disconnect());
            }
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED) /* 当wifi成功连接 */ 
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data; /* 获取IP地址信息*/
            ESP_LOGI(WIFI_CONNECT,"got ip:%d.%d.%d.%d \n" , IP2STR(&event->ip_info.ip));  /* 打印ip地址*/
            retry_num = 0;                                              /* WiFi重连次数清零 */
            nvs_handle wificfg_nvs_handler;
            ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );
            ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_ssid",wifi_ssid) );
            ESP_ERROR_CHECK( nvs_set_str(wificfg_nvs_handler,"wifi_passwd",wifi_passwd) );
            ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
            nvs_close(wificfg_nvs_handler);                     /* 关闭 */ 
            ESP_LOGI(WIFI_CONNECT,"Success save wifi with Password: %s and ssid %s\n", wifi_passwd, wifi_ssid);
            ESP_LOGI(WIFI_CONNECT,"Success connecting wifi with Password: %s and ssid %s\n", wifi_passwd, wifi_ssid);
            wifi_state = 0x02;
        }
    }
}

static void wifi_preprocess(void)
{
    //...................................初始化非易失性存储库 (NVS)..........................................//
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // //删除连接的历史记录
    // nvs_handle wificfg_nvs_handler;
    // ESP_ERROR_CHECK( nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );

    // nvs_erase_key(wificfg_nvs_handler, "wifi_ssid");
    // nvs_erase_key(wificfg_nvs_handler, "wifi_passwd");
    // ESP_LOGI(WIFI_CONNECT,"wifi_cfg erase. \n");

    // ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    // nvs_close(wificfg_nvs_handler);                     /* 关闭 */
    //...................................初始化非易失性存储库 (NVS)..........................................//


    //...................................初始化wifi连接handler..........................................//
    /* 初始化底层TCP/IP堆栈。在应用程序启动时，应该调用此函数一次。*/
    ESP_ERROR_CHECK(esp_netif_init());
    /* 创建一个事件循环 */  
    // 调用 esp_event_handler_register 前，必须先调用 esp_event_loop_create_default()
    // 因为事件处理程序(esp_event_handler_register)需要一个已初始化的事件循环来接收和处理事件。
    // 如果没有创建事件循环，事件处理程序无法正常工作。
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	/* 将事件处理程序注册到系统默认事件循环，分别是WiFi事件 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // 设置通用sta处理函数
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
    /* 创建STA */
    esp_netif_create_default_wifi_sta();
    /* 防止在省电模式，wifi质量不好 */ 
    esp_wifi_set_ps(WIFI_PS_NONE);
    //...................................初始化wifi连接handler..........................................//
}

static void get_nvs_info(void)
{
    for(int i = 0; i < 3; i++)
    {
        is_set[i] = false;
    }

    wifi_state = 0x00;
    is_blufi_init = false;

    //...................................获取在NVS中存储的信息..........................................//
    nvs_handle wificfg_nvs_handler; /* 定义一个NVS操作句柄 */
    ESP_ERROR_CHECK(nvs_open("WiFi_cfg", NVS_READWRITE, &wificfg_nvs_handler) );//打开一个名叫"WiFi_cfg"的可读可写nvs空间

    //使用 blufi中定义的wifi_ssid和wifi_passwd 来存储wifi信息
    size_t len;                     /* 从NVS中获取ssid */
    len = sizeof(wifi_ssid);  
    esp_err_t ssid_err = nvs_get_str(wificfg_nvs_handler,"wifi_ssid",wifi_ssid,&len) ;
    if(ssid_err != ESP_OK)
    {
        ESP_LOGE(WIFI_CONNECT,"No history wifi_ssid found. \n");
    }
    else
    {
        ESP_LOGI(WIFI_CONNECT,"history wifi_ssid found : %s. \n", wifi_ssid);
        is_set[0] = true;
    }

    len = sizeof(wifi_passwd);      /* 从NVS中获取passwd */
    esp_err_t passwd_err = nvs_get_str(wificfg_nvs_handler,"wifi_passwd",wifi_passwd,&len) ;
    if(passwd_err != ESP_OK)
    {
        ESP_LOGE(WIFI_CONNECT,"No history wifi_passwd found. \n");
    }
    else
    {
        ESP_LOGI(WIFI_CONNECT,"history wifi_passwd found : %s. \n", wifi_passwd);
        is_set[1] = true;
    }

    len = sizeof(customer);      /* 从NVS中获取customer */
    esp_err_t customer_err = nvs_get_str(wificfg_nvs_handler,"customer",customer,&len) ;
    if(customer_err != ESP_OK)
    {
        ESP_LOGE("Customer","No customer found. \n");
    }
    else
    {
        ESP_LOGI("Customer","history customer found : %s. \n", customer);
        is_set[2] = true;
    }

    ESP_ERROR_CHECK( nvs_commit(wificfg_nvs_handler) ); /* 提交 */
    nvs_close(wificfg_nvs_handler);                     /* 关闭 */
    //...................................获取在NVS中存储的wifi信息..........................................//
}


static void wifi_init_sta(void)
{
    get_nvs_info();
    //......................................初始化wifi..........................................//
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* 根据cfg参数初始化wifi连接所需要的资源 */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* 设置WiFi的工作模式为 STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    /* 启动WiFi连接 */
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(WIFI_CONNECT,"wifi_init_sta finished. \n");

    example_espnow_init();
    //......................................初始化wifi..........................................//


    // //...................................判断是否能找到历史记录..........................................//
    // if(is_set[0] == true && is_set[1] == true)
    // {
    //     wifi_config_t wifi_config = {
    //                     .sta = {
    //                         .ssid = "",
    //                         .password = "",
    //                     },
    //                 };
    //     bzero(&wifi_config, sizeof(wifi_config_t)); /* 将结构体数据清零 */
    //     memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    //     memcpy(wifi_config.sta.password, wifi_passwd, sizeof(wifi_config.sta.password));
    //     ESP_ERROR_CHECK(esp_wifi_disconnect());
    //     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    //     ESP_ERROR_CHECK(esp_wifi_connect());
    //     wifi_state = 0x01;
    //     ESP_LOGI(WIFI_CONNECT,"Start connecting wifi with Password: %s and ssid %s \n", wifi_passwd, wifi_ssid);
    // }
    // else
    // {
    //     ESP_LOGE(WIFI_CONNECT,"No WIFI History");
    //     wifi_state = 0x00;
    //     start_blufi();//开启蓝牙配网
    // }
    //...................................判断是否能找到历史记录..........................................//
}

