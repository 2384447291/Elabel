idf_component_register(SRCS screens/ui_HalfmindScreen.c
    screens/ui_OTAScreen.c
    screens/ui_TaskScreen.c
    screens/ui_FocusScreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    INCLUDE_DIRS "."
    REQUIRES lvgl)

