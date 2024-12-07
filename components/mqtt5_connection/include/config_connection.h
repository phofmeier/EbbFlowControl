#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION

#include "configuration.h"
#include "mqtt_shared.h"

/**
 * @brief Subscribe to the configuration channel to receive new config values.
 *
 * @param client mqtt client
 */
void subscribe_to_config_channel(esp_mqtt_client_handle_t client);

/**
 * @brief Callback function if a new config arrived.
 *
 * @param event event including the arrived data
 */
void new_configuration_received_cb(esp_mqtt_event_handle_t event);

/**
 * @brief Publish the current configuration to the server
 *
 * @param client mqtt client
 */
void send_current_configuration(esp_mqtt_client_handle_t client);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION */
