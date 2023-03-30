idf_component_register(SRCS "restart_reset_random_intr.c" 
                        INCLUDE_DIRS "include" 
                        REQUIRES relay_pattern_generate web_server nvs_flash)
