idf_component_register(SRCS "callback.cpp" "button.cpp" "encoder.cpp" "control_driver.cpp" "buzzer.cpp"
                    INCLUDE_DIRS "include" 
                    REQUIRES driver)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
