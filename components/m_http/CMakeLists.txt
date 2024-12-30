idf_component_register(SRCS "../../main/cJSON/cJSON.c" "http.c" "../../main/global_message/global_message.c" "../../main/global_message/global_time.c" 
                    INCLUDE_DIRS "include" "../../main/global_message" "../../main/cJSON"
                    REQUIRES esp_http_client)
