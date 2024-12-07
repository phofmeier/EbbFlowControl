#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION

#include "configuration.h"
#include "mqtt_client.h"

#define CONFIG_MQTT_CONFIG_TOPIC "config/topic"

void subscribe_to_config_channel(esp_mqtt_client_handle_t client);

void new_configuration_received_cb(esp_mqtt_event_handle_t event);

void send_current_configuration();

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_CONFIG_CONNECTION */
