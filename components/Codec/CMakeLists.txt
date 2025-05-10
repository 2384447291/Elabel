idf_component_register(
    SRCS "codec.cpp"
    INCLUDE_DIRS "include" "../../main/spiffs_data/include"
    REQUIRES driver esp_codec_dev fatfs
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
