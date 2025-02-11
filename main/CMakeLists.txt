idf_component_register(SRCS "main.cpp" "global_message/global_message.c"  "global_message/global_time.c"  "global_message/global_draw.c" "global_message/global_nvs.c" 
                        "elabel_state_machine/ElabelController.cpp"
                        "elabel_state_machine/ChoosingTaskState.cpp"
                        "elabel_state_machine/OperatingTaskState.cpp"
                        "elabel_state_machine/FocusTaskState.cpp"
                        "elabel_state_machine/ActiveState.cpp"
                        "elabel_state_machine/HostActiveState.cpp"
                        "elabel_state_machine/InitState.cpp"
                        "elabel_state_machine/OTAState.cpp"
                        "elabel_state_machine/SlaveActiveState.cpp"
                        "battery_manager/battery_manager.cpp"
                    INCLUDE_DIRS "global_message" "elabel_state_machine" "battery_manager" "." 
                    REQUIRES esp_adc_cal
                    REQUIRES lvgl
                    REQUIRES control_driver
                    REQUIRES lvgl_esp32_drivers
                    REQUIRES m_http
                    REQUIRES m_mqtt
                    REQUIRES m_esp_now
                    REQUIRES network
                    REQUIRES ota
                    REQUIRES ui
                    )
