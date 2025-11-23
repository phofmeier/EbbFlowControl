if(CONFIG_OTA_USE_CERT_BUNDLE)
idf_component_register(SRCS "ota_updater.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES mbedtls esp_http_client app_update esp_https_ota)
else()
idf_component_register(SRCS "ota_updater.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES mbedtls esp_http_client app_update esp_https_ota
                     # Embed the server root certificate into the final binary
                    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)
endif()
