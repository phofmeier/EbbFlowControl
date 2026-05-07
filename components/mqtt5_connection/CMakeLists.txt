idf_component_register(SRCS "generic_data_store.c" "light_data_store.c" "memory_data_store.c" "pump_data_store.c" "level_data_store.c" "config_connection.c" "data_logging.c" "mqtt5_connection.c"
                        INCLUDE_DIRS
                       "include" REQUIRES mqtt vfs spiffs esp_app_format)
