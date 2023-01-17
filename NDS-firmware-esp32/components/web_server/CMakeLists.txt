idf_component_register(SRCS "server.c" 
                    INCLUDE_DIRS "."
                    REQUIRES wifi esp_http_server nvs_flash spiffs json led mdns dns_hijack_srv)
