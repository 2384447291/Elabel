idf_component_register(SRCS "Esp_now_host.cpp" "Esp_now_slave.cpp" "Esp_now_client.cpp" "../network/network.c" "../m_http/http.c" "../../main/global_message/global_nvs.c" "../../main/global_message/global_message.h"
                        INCLUDE_DIRS "include"  "../network/include" "../m_http/include" "../../main/global_message"
                        REQUIRES network
                        REQUIRES m_http)

# 只对C++源文件设置编译选项
set_source_files_properties("Esp_now_host.cpp" "Esp_now_slave.cpp" "Esp_now_client.cpp"
                          PROPERTIES COMPILE_OPTIONS "-std=gnu++17")
