idf_component_register(
    SRCS "ota_update.c"
    INCLUDE_DIRS "include"
    # EMBED_FILES ${project_dir}/cert/google2.crt
    REQUIRES    nvs_flash 
                esp_http_client 
                app_update 
                esp_https_ota)
