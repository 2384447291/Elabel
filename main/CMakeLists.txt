idf_component_register(SRCS "main.cpp" "global_message/global_message.c"  
                        "elabel_state_machine/ElabelController.cpp"
                        "elabel_state_machine/ChoosingTaskState.cpp"
                        "elabel_state_machine/OperatingTaskState.cpp"
                    INCLUDE_DIRS "global_message" "elabel_state_machine")
