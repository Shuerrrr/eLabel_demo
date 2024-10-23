idf_component_register(SRCS "network.c" "../../main/esp_now/m_esp_now.c" "../../main/mainTask.c"
                    INCLUDE_DIRS "include" "../../main/esp_now"
                    REQUIRES spi_flash
                    REQUIRES nvs_flash
                    REQUIRES lvgl
                    REQUIRES lvgl_esp32_drivers
                    REQUIRES fatfs
                    REQUIRES bt)
