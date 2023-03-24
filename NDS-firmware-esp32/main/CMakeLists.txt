
# set(COMPONENT_SRCS "main.c" )
# set(COMPONENT_ADD_INCLUDEDIRS ".")

# set(COMPONENT_SRCDIRS "." "../components")
# set(COMPONENT_ADD_INCLUDEDIRS "." "../components")

idf_component_register(SRCS "main.c"
                  REQUIRES relay_pattern_generate restart_reset_random_intr reboot_counter web_server wifi)
                  
#   register_component()
spiffs_create_partition_image(storage ../spiffs_dir/web_app FLASH_IN_PROJECT)  