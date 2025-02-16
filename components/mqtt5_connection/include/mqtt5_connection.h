#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION

#include "mqtt_shared.h"

/**
 * @brief Initialize the mqtt connection.
 *
 */
void mqtt5_conn_init();

/**
 * @brief Task to check the connection and retry if it fails.
 *
 * @param pvParameters (unused)
 */
void mqtt5_check_connection_task(void *pvParameters);

/**
 * @brief Create the task to check the connection and retry if it fails.
 *
 * @return TaskHandle_t handle to the created task
 */
TaskHandle_t mqtt5_create_connection_checker_task();

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION */
