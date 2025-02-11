idf_component_register(SRCS "Esp_now_host.cpp" "Esp_now_slave.cpp" "Esp_now_client.cpp" "../network/network.c" 
                        INCLUDE_DIRS "include"  "../network/include" 
                        PRIV_REQUIRES network)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
