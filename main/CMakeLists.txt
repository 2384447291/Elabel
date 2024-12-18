idf_component_register(SRCS "main.cpp" "global_message/global_message.c"  
                        "elabel_state_machine/ElabelController.cpp"
                        "elabel_state_machine/ChoosingTaskState.cpp"
                        "elabel_state_machine/OperatingTaskState.cpp"
                        "elabel_state_machine/FocusTaskState.cpp"
                        "elabel_state_machine/NoWifiState.cpp"
                        "elabel_state_machine/InitState.cpp"
                    INCLUDE_DIRS "global_message" "elabel_state_machine" "." 
                    REQUIRES lvgl
                    REQUIRES ui
                    REQUIRES http
                    REQUIRES control_driver
                    REQUIRES m_esp_now
                    REQUIRES m_mqtt
                    REQUIRES network
                    REQUIRES ota
                    REQUIRES lvgl_esp32_drivers
                    )
