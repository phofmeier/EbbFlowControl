#include "configuration.h"
#include "esp_log.h"
#include "mqtt5_connection.h"

#include "config_connection.h"

static const char *TAG = "mqtt5_config";

static esp_mqtt5_subscribe_property_config_t config_subscribe_property = {
    .subscribe_id = 25555,
    .no_local_flag = false,
    .retain_as_published_flag = false,
    .retain_handle = 0,
    .is_share_subscribe = true,
    .share_name = "group1",
};

static esp_mqtt5_publish_property_config_t config_publish_property = {
    .payload_format_indicator = 1,
    .message_expiry_interval = 1000,
    .topic_alias = 0,
    .response_topic = "/topic/test/response",
    .correlation_data = "123456",
    .correlation_data_len = 6,
};

void subscribe_to_config_channel(esp_mqtt_client_handle_t client) {

  esp_mqtt5_client_set_user_property(&config_subscribe_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_subscribe_property(client, &config_subscribe_property);
  int msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
  esp_mqtt5_client_delete_user_property(
      config_subscribe_property.user_property);
  config_subscribe_property.user_property = NULL;
  ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

  send_current_configuration(client);
}

void new_configuration_received_cb(esp_mqtt_event_handle_t event) {
  ESP_LOGI(TAG, "New Configuration received");
  ESP_LOGI(TAG, "payload_format_indicator is %d",
           event->property->payload_format_indicator);
  ESP_LOGI(TAG, "response_topic is %.*s", event->property->response_topic_len,
           event->property->response_topic);
  ESP_LOGI(TAG, "correlation_data is %.*s",
           event->property->correlation_data_len,
           event->property->correlation_data);
  ESP_LOGI(TAG, "content_type is %.*s", event->property->content_type_len,
           event->property->content_type);
  ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
  ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

  set_config_from_json(event->data, event->data_len);
  send_current_configuration(event->client);
}

void send_current_configuration(esp_mqtt_client_handle_t client) {
  cJSON *json_config = get_config_as_json();
  char *json_string = cJSON_PrintUnformatted(json_config);

  esp_mqtt5_client_set_user_property(&config_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client, &config_publish_property);
  int msg_id =
      esp_mqtt_client_publish(client, "/topic/qos1", json_string, 0, 1, 1);
  esp_mqtt5_client_delete_user_property(config_publish_property.user_property);
  config_publish_property.user_property = NULL;
  ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
  cJSON_Delete(json_config);
  free(json_string);
}