idf_component_register(SRCS "network.c" 
                    INCLUDE_DIRS "include" "../../main/global_message"
                    REQUIRES spi_flash
                    REQUIRES nvs_flash
                    REQUIRES bt)
