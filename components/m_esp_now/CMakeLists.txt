idf_component_register(SRCS "m_esp_now.c"  "../network/network.c" "../../main/global_message/global_message.c"
                        INCLUDE_DIRS "include"  "../network/include" "../../main/global_message"
                        PRIV_REQUIRES network
                        PRIV_REQUIRES http)
                        # 写法问题，globalmessage和http连接的太深了
