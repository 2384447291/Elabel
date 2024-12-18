idf_component_register(SRCS "control_driver.c" "../../main/global_message/global_message.c" "ws2812_control.c" "led_strip_rmt_ws2812.c"
                    INCLUDE_DIRS "include" "../../main/global_message" 
                    REQUIRES driver
                    REQUIRES http
                    REQUIRES lvgl
                    REQUIRES ui)
