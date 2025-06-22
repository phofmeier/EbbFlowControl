idf_component_register(SRCS "config_connection.c" "data_logging.c" "mqtt5_connection.c"
                        INCLUDE_DIRS
                       "include" REQUIRES mqtt json vfs fatfs)
