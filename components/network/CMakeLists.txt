idf_component_register(SRCS "network.c" "../../main/global_message/global_nvs.c" "../../main/global_message/global_message.c"
                    INCLUDE_DIRS "include" "../../main/global_message"
                    REQUIRES esp_wifi
                    REQUIRES bt)
