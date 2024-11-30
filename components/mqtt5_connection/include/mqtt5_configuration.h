#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONFIGURATION
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONFIGURATION

#include "mqtt_client.h"

static esp_mqtt5_user_property_item_t user_property_arr[] = {
    {"board", "esp32"}, {"u", "user"}, {"p", "password"}};

#define USE_PROPERTY_ARR_SIZE                                                  \
  sizeof(user_property_arr) / sizeof(esp_mqtt5_user_property_item_t)

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONFIGURATION */
