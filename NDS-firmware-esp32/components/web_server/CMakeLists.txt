idf_component_register(SRCS "server.c" 
                    INCLUDE_DIRS "include"
                    REQUIRES    esp_http_server 
                                json spiffs 
                                relay_pattern_generate 
                                web_server 
                                wifi_internal 
                                dns_hijack_srv)
