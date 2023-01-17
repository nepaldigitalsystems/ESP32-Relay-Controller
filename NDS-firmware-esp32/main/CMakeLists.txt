
set(COMPONENT_SRCS "main.c" )
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_SRCDIRS "." "../components")
set(COMPONENT_ADD_INCLUDEDIRS "." "../components")
register_component()

spiffs_create_partition_image(storage ../spiffs_dir/web_app FLASH_IN_PROJECT)  