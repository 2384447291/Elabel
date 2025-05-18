idf_component_register(SRCS "main.cpp" "global_message/global_message.c"  "global_message/global_time.c"  "global_message/global_draw.c" "global_message/global_nvs.c" 
                        "elabel_state_machine/ElabelController.cpp"
                        "elabel_state_machine/ChoosingTaskState.cpp"
                        "elabel_state_machine/OperatingTaskState.cpp"
                        "elabel_state_machine/OperatingRecorderState.cpp"
                        "elabel_state_machine/OperatingTimerState.cpp"
                        "elabel_state_machine/FocusTaskState.cpp"
                        "elabel_state_machine/ActiveState.cpp"
                        "elabel_state_machine/HostActiveState.cpp"
                        "elabel_state_machine/InitState.cpp"
                        "elabel_state_machine/OTAState.cpp"
                        "elabel_state_machine/SlaveActiveState.cpp"
                        "elabel_state_machine/InfoState.cpp"
                        "elabel_state_machine/NoWifiState.cpp"
                        "elabel_state_machine/NoHostState.cpp"
                        "battery_manager/battery_manager.cpp"
                    INCLUDE_DIRS "global_message" "elabel_state_machine" "battery_manager" "spiffs_data/include" "." 
                    REQUIRES esp_adc
                    REQUIRES esp_wifi
                    REQUIRES nvs_flash
                    REQUIRES esp_driver_gpio

                    REQUIRES lvgl
                    REQUIRES Codec

                    REQUIRES lvgl_esp32_drivers
                    REQUIRES control_driver
                    REQUIRES m_http
                    REQUIRES m_mqtt
                    REQUIRES m_esp_now
                    REQUIRES network
                    REQUIRES ota
                    REQUIRES ui
                    )
