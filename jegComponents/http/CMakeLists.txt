idf_component_register(SRCS "cJSON.c" "http.c" "../../main/global_message/global_message.c"
                    INCLUDE_DIRS "include" "../../main/global_message"
                    REQUIRES esp_http_client
                    REQUIRES mqtt)
