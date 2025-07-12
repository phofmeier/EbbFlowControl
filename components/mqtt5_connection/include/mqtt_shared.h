#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT_SHARED
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT_SHARED

#include "mqtt_client.h"

/**
 * @brief user property included in all messages
 *
 */
static esp_mqtt5_user_property_item_t user_property_arr[] = {
    {"version", "v0.0.0"},
};

#define USE_PROPERTY_ARR_SIZE                                                  \
  sizeof(user_property_arr) / sizeof(esp_mqtt5_user_property_item_t)

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT_SHARED */
