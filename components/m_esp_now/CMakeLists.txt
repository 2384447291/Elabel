idf_component_register(SRCS "esp_now_remind.cpp" "Esp_now_host.cpp" "Esp_now_slave.cpp" "Esp_now_client.cpp" "../network/network.c" "../m_http/http.c" "../../main/global_message/global_nvs.c" "../Codec/codec.cpp" "esp_now_remind.cpp"
                        INCLUDE_DIRS "include"  "../network/include" "../m_http/include" "../../main/global_message" "../Codec/include"
                        REQUIRES network
                        REQUIRES m_http
                        REQUIRES Codec)

# 只对C++源文件设置编译选项
set_source_files_properties("Esp_now_host.cpp" "Esp_now_slave.cpp" "Esp_now_client.cpp" "esp_now_remind.cpp"
                          PROPERTIES COMPILE_OPTIONS "-std=gnu++17")
