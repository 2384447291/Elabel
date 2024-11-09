idf_component_register(SRCS "control_driver.c" "../../main/global_message/global_message.c"
                    INCLUDE_DIRS "include" "../../main/global_message"
                    REQUIRES driver
                    REQUIRES http)
