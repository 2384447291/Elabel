idf_component_register(SRCS "ota.c" "../../main/global_message/global_tool.c"
                        INCLUDE_DIRS "include" "../../main/global_message"
                        REQUIRES esp_http_client
                        REQUIRES esp_https_ota
                        REQUIRES app_update)
