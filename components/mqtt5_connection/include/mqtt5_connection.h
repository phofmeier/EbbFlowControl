#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION

#include "mqtt_client.h"

#define CONFIG_BROKER_URL "brocker_url"

static esp_mqtt_client_handle_t mqtt_client;

static esp_mqtt5_user_property_item_t user_property_arr[] = {
    {"board", "esp32"}, {"u", "user"}, {"p", "password"}};

#define USE_PROPERTY_ARR_SIZE                                                  \
  sizeof(user_property_arr) / sizeof(esp_mqtt5_user_property_item_t)

void mqtt5_conn_init(void);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_MQTT5_CONNECTION */