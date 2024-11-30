idf_component_register(SRCS "network.c"
                    INCLUDE_DIRS "include"
                    REQUIRES spi_flash
                    REQUIRES nvs_flash
                    REQUIRES bt
                    REQUIRES lvgl
                    REQUIRES ui)
