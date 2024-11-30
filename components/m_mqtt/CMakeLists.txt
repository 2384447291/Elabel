idf_component_register(SRCS "m_mqtt.c" "../http/cJSON.c" "../../main/global_message/global_message.c"
                        INCLUDE_DIRS "include" "../../main/global_message" "../http/include"
                        REQUIRES mqtt
                        PRIV_REQUIRES http
                        REQUIRES lvgl
                        REQUIRES ui)
                        #这里需要加这一句因为导入了http.h
