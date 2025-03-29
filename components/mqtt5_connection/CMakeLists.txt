idf_component_register(SRCS "data_logging.c" "config_connection.c" "data_logging.c" "mqtt5_connection.c"
                        INCLUDE_DIRS
                       "include" REQUIRES mqtt json)
