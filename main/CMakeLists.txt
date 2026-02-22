idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES bt
                    PRIV_REQUIRES nvs_flash)