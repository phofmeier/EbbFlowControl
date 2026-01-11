idf_component_register(SRCS "config_page.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server configuration
                    EMBED_FILES html/config_page.html)
