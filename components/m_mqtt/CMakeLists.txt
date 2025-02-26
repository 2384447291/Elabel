idf_component_register(SRCS "m_mqtt.c" "../../main/cJSON/cJSON.c" "../../main/global_message/global_message.c" "../../main/global_message/global_time.c"
                        INCLUDE_DIRS "include" "../../main/global_message" "../../main/cJSON" "../m_http/include"
                        REQUIRES mqtt
                        REQUIRES m_http)
