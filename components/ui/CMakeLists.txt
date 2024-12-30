idf_component_register(SRCS screens/ui_HalfmindScreen.c
    screens/ui_BindScreen.c
    screens/ui_OTAScreen.c
    screens/ui_TaskScreen.c
    screens/ui_OperatingScreen.c
    screens/ui_FocusScreen.c
    screens/ui_UpdateScreen.c
    screens/ui_ShutdownScreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    fonts/ui_font_Chinese.c
    INCLUDE_DIRS "."
    REQUIRES lvgl)
