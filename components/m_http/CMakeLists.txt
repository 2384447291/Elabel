idf_component_register(SRCS "../../main/cJSON/cJSON.c" "http.c" "httpmusic.c" "http_send.c" "../../main/global_message/global_message.c" "../../main/global_message/global_time.c" "../../main/global_message/global_tool.c" "../../main/global_message/global_nvs.c"
                    INCLUDE_DIRS "include" "../../main/global_message" "../../main/cJSON"
                    REQUIRES esp_http_client
                    REQUIRES nvs_flash
                    REQUIRES esp_timer
                    REQUIRES fatfs)
