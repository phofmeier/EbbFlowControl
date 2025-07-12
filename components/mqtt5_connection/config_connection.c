#include "config_connection.h"

// #include "mqtt5_connection.h"
// #include "mqtt_shared.h"

#include "cJSON.h"
#include "configuration.h"
#include "esp_log.h"

static const char *TAG = "mqtt5_config";

/**
 * @brief property for subscribing to the config channel
 *
 */
static esp_mqtt5_subscribe_property_config_t config_subscribe_property = {
    .subscribe_id = 1,
    .no_local_flag = false,
    .retain_as_published_flag = false,
    .retain_handle = 0,
    .is_share_subscribe = false,
    .share_name = NULL,
};

/**
 * @brief property to publish to the config channel
 *
 */
static esp_mqtt5_publish_property_config_t config_publish_property = {
    .payload_format_indicator = 1,
    .message_expiry_interval = 1000,
    .topic_alias = 0,
    .response_topic = NULL,
    .correlation_data = NULL,
    .correlation_data_len = 0,
};

void subscribe_to_config_channel(esp_mqtt_client_handle_t client) {
  esp_mqtt5_client_set_user_property(&config_subscribe_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_subscribe_property(client, &config_subscribe_property);
  int msg_id =
      esp_mqtt_client_subscribe(client, CONFIG_MQTT_CONFIG_RECEIVE_TOPIC, 0);
  esp_mqtt5_client_delete_user_property(
      config_subscribe_property.user_property);
  config_subscribe_property.user_property = NULL;
  ESP_LOGD(TAG, "Subscribed to config channel, msg_id=%d", msg_id);
}

void new_configuration_received_cb(esp_mqtt_event_handle_t event) {
  ESP_LOGI(TAG, "New Configuration received");
  set_config_from_json(event->data, event->data_len);
  send_current_configuration(event->client);
}

void send_current_configuration(esp_mqtt_client_handle_t client) {
  cJSON *json_config = get_config_as_json();
  char *json_string = cJSON_PrintUnformatted(json_config);

  esp_mqtt5_client_set_user_property(&config_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client, &config_publish_property);
  int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_CONFIG_SEND_TOPIC,
                                       json_string, 0, 1, 1);
  esp_mqtt5_client_delete_user_property(config_publish_property.user_property);
  config_publish_property.user_property = NULL;
  ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
  cJSON_Delete(json_config);
  cJSON_free(json_string);
}
