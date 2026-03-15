idf_component_register(SRCS "light_data_store.c" "memory_data_store.c" "pump_data_store.c" "config_connection.c" "data_logging.c" "mqtt5_connection.c"
                        INCLUDE_DIRS
                       "include" REQUIRES mqtt json vfs spiffs esp_app_format)
