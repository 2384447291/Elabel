idf_component_register(
    SRCS "codec.cpp" "../../main/global_message/global_message.c"
    INCLUDE_DIRS "include" "../../main/spiffs_data/include" "../../main/global_message"
    REQUIRES driver esp_codec_dev fatfs
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
