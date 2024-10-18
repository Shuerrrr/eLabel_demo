idf_component_register(SRCS "m_mqtt.c" "../http/cJSON.c" "../../main/global_message/global_message.c"
                        INCLUDE_DIRS "include" "../../main/global_message" "../http/include"
                        REQUIRES mqtt
                        REQUIRES esp_http_client)
                        #这里需要加这一句因为导入了http.h
