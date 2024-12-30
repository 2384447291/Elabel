idf_component_register(SRCS "control_driver.cpp" "ws2812_control.c" "led_strip_rmt_ws2812.c" "buzzer.cpp" "LED.cpp"
                    INCLUDE_DIRS "include" 
                    REQUIRES driver)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
