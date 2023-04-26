# set(COMPONENT_SRCS "main.c")
# set(COMPONENT_EMBED_FILES "../cert/google.crt")
idf_component_register( SRCS "main.c" 
                        INCLUDE_DIRS "."
                        # EMBED_FILES ${project_dir}/cert/google2.crt
                        )

spiffs_create_partition_image(storage ../spiffs_dir/web_app FLASH_IN_PROJECT)  
