#include "mqtt5_connection.h"

#include "config_connection.h"
#include "esp_log.h"
#include "esp_system.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt5";

/**
 * @brief Publish message that the status is connected.
 *
 * @param client mqtt5 client
 */
void send_status_connected(esp_mqtt_client_handle_t client) {
  // build message
  char *connected_message = "{id: \"   \", connection: \"connected\"}";
  sprintf(&connected_message[6], "%3uhh", configuration.id);
  static const int connected_message_length = 37;

  static esp_mqtt5_publish_property_config_t status_publish_property = {
      .payload_format_indicator = 1,
      .message_expiry_interval = 1000,
      .topic_alias = 0,
      .response_topic = NULL,
      .correlation_data = NULL,
      .correlation_data_len = 0,
      .content_type = NULL,
  };

  esp_mqtt5_client_set_user_property(&status_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client, &status_publish_property);
  int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_STATUS_TOPIC,
                                       connected_message,
                                       connected_message_length, 1, 1);
  esp_mqtt5_client_delete_user_property(status_publish_property.user_property);
  status_publish_property.user_property = NULL;
  ESP_LOGI(TAG, "sent Status connected successful, msg_id=%d", msg_id);
}

/**
 * @brief Helper function to log an error if it occurs
 *
 * @param message pointer to the message
 * @param error_code code of the error
 */
static void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
  }
}

/**
 * @brief Property value for disconnecting
 *
 */
static esp_mqtt5_disconnect_property_config_t disconnect_property = {
    .session_expiry_interval = 60,
    .disconnect_reason = 0,
};

/**
 * @brief Helper function to print the user property
 *
 * @param user_property current user property
 */
static void print_user_property(mqtt5_user_property_handle_t user_property) {
  if (user_property) {
    uint8_t count = esp_mqtt5_client_get_user_property_count(user_property);
    if (count) {
      esp_mqtt5_user_property_item_t *item =
          malloc(count * sizeof(esp_mqtt5_user_property_item_t));
      if (esp_mqtt5_client_get_user_property(user_property, item, &count) ==
          ESP_OK) {
        for (int i = 0; i < count; i++) {
          esp_mqtt5_user_property_item_t *t = &item[i];
          ESP_LOGI(TAG, "key is %s, value is %s", t->key, t->value);
          free((char *)t->key);
          free((char *)t->value);
        }
      }
      free(item);
    }
  }
}

/**
 * @brief Event handler for all mqtt events
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt5_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32,
           base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;

  ESP_LOGD(TAG, "free heap size is %" PRIu32 ", minimum %" PRIu32,
           esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    print_user_property(event->property->user_property);
    subscribe_to_config_channel(client);
    send_status_connected(client);
    send_current_configuration(client);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    print_user_property(event->property->user_property);
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    print_user_property(event->property->user_property);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    print_user_property(event->property->user_property);
    esp_mqtt5_client_set_user_property(&disconnect_property.user_property,
                                       user_property_arr,
                                       USE_PROPERTY_ARR_SIZE);
    esp_mqtt5_client_set_disconnect_property(client, &disconnect_property);
    esp_mqtt5_client_delete_user_property(disconnect_property.user_property);
    disconnect_property.user_property = NULL;
    esp_mqtt_client_disconnect(client);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    print_user_property(event->property->user_property);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    print_user_property(event->property->user_property);
    if (strcmp(event->topic, CONFIG_MQTT_CONFIG_RECEIVE_TOPIC)) {
      new_configuration_received_cb(event);
      break;
    }
    ESP_LOGI(TAG, "Unknown data Received");
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
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    print_user_property(event->property->user_property);
    ESP_LOGI(TAG, "MQTT5 return code is %d",
             event->error_handle->connect_return_code);
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls",
                           event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack",
                           event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",
                           event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(TAG, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

void mqtt5_conn_init() {
  esp_mqtt5_connection_property_config_t connect_property = {
      .session_expiry_interval = 10,
      .maximum_packet_size = 1024,
      .receive_maximum = 65535,
      .topic_alias_maximum = 2,
      .request_resp_info = false,
      .request_problem_info = false,
      .will_delay_interval = 10,
      .payload_format_indicator = true,
      .message_expiry_interval = 10,
      .response_topic = NULL,
      .correlation_data = NULL,
      .correlation_data_len = 0,
  };

  // Build last will message as json
  char *last_will_message = "{id: \"   \", connection: \"disconnected\"}";
  sprintf(&last_will_message[6], "%3uhh", configuration.id);

  esp_mqtt_client_config_t mqtt5_cfg = {
      .broker.address.uri = CONFIG_MQTT_BROKER_URI,
      .session.protocol_ver = MQTT_PROTOCOL_V_5,
      .network.disable_auto_reconnect = false,
      .credentials.username = CONFIG_MQTT_USERNAME,
      .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
      .session.last_will.topic = CONFIG_MQTT_STATUS_TOPIC,
      .session.last_will.msg = last_will_message,
      .session.last_will.msg_len = sizeof(last_will_message),
      .session.last_will.qos = 1,
      .session.last_will.retain = true,
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt5_cfg);

  /* Set connection properties and user properties */
  esp_mqtt5_client_set_user_property(&connect_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_user_property(&connect_property.will_user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_connect_property(client, &connect_property);

  // Need to delete user property
  esp_mqtt5_client_delete_user_property(connect_property.user_property);
  esp_mqtt5_client_delete_user_property(connect_property.will_user_property);

  // Register event handler
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt5_event_handler,
                                 NULL);
  esp_mqtt_client_start(client);
}
