#include "mqtt5_connection.h"

#include "config_connection.h"
#include "data_logging.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wifi_utils.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VERSION_STRING "v0.0.1"

static const char *TAG = "mqtt5";

/** Number of current connection attempts.*/
static int disconnect_counter_ = 0;

/** FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_mqtt5_event_group_;

/** Bit to signal that the connection failed.*/
#define MQTT5_CONNECTION_FAILED_BIT BIT0

/** client handle of the connection  */
esp_mqtt_client_handle_t client_;

bool mqtt5_connected = false;

/**
 * @brief Publish message that the status is connected.
 *
 * @param client mqtt5 client
 */
void send_status_connected(esp_mqtt_client_handle_t client) {
  // build message
  int rssi_level = -100;
  ESP_ERROR_CHECK(wifi_utils_get_connection_strength(&rssi_level));

  char connected_message[78];
  int connected_message_length =
      sprintf(connected_message,
              "{\"id\": %3u, \"connection\": \"connected\", \"rssi_level\": "
              "%i, \"version\": \"%s\"}",
              configuration.id, rssi_level, VERSION_STRING);

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
  ESP_LOGI(TAG, "sent Status connected successful, msg=%s", connected_message);
  ESP_LOGI(TAG, "sent Status connected successful, topic=%s",
           CONFIG_MQTT_STATUS_TOPIC);
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

  ESP_LOGI(TAG, "free heap size is %" PRIu32 ", minimum %" PRIu32,
           esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    mqtt5_connected = true;
    disconnect_counter_ = 0;
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    print_user_property(event->property->user_property);
    subscribe_to_config_channel(client);
    send_status_connected(client);
    send_current_configuration(client);
    set_connected();
    break;
  case MQTT_EVENT_DISCONNECTED:
    mqtt5_connected = false;
    if (CONFIG_MQTT_MAX_RECONNECT_ATTEMPTS > 0 &&
        disconnect_counter_ > CONFIG_MQTT_MAX_RECONNECT_ATTEMPTS) {
      xEventGroupSetBits(s_mqtt5_event_group_, MQTT5_CONNECTION_FAILED_BIT);
    }

    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED counter: %i", disconnect_counter_);
    print_user_property(event->property->user_property);
    disconnect_counter_++; // Increased after each disconnect.
    set_disconnected();
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
    set_data_published(event->msg_id);
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
  case MQTT_EVENT_DELETED:
    set_disconnected();
    ESP_LOGI(TAG, "MQTT_EVENT_DELETED, msg_id=%d", event->msg_id);
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

int mqtt_sent_message(const char *topic, const char *data) {
  if (!mqtt5_connected) {
    return -1;
  }

  static esp_mqtt5_publish_property_config_t config_publish_property = {
      .payload_format_indicator = 1,
      .message_expiry_interval = 1000,
      .topic_alias = 0,
      .response_topic = NULL,
      .correlation_data = NULL,
      .correlation_data_len = 0,
  };

  esp_mqtt5_client_set_user_property(&config_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client_, &config_publish_property);
  int msg_id = esp_mqtt_client_enqueue(client_, topic, data, 0, 1, 1, true);

  esp_mqtt5_client_delete_user_property(config_publish_property.user_property);
  config_publish_property.user_property = NULL;
  ESP_LOGI(TAG, "sent data, msg_id=%d", msg_id);
  return msg_id;
}

void mqtt5_conn_init() {
  s_mqtt5_event_group_ = xEventGroupCreate();

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
  ESP_LOGI(TAG, "Build last will %3u", configuration.id);
  char last_will_message[74];
  int last_will_message_count = sprintf(
      last_will_message,
      "{\"id\": %3u, \"connection\": \"disconnected\", \"version\": \"%s\"}",
      configuration.id, VERSION_STRING);

  esp_mqtt_client_config_t mqtt5_cfg = {
      .broker.address.uri = CONFIG_MQTT_BROKER_URI,
      .session.protocol_ver = MQTT_PROTOCOL_V_5,
      .network.disable_auto_reconnect = false,
      .network.reconnect_timeout_ms = CONFIG_MQTT_TIMEOUT_RECONNECT_MS,
      .credentials.username = CONFIG_MQTT_USERNAME,
      .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
      .session.last_will.topic = CONFIG_MQTT_STATUS_TOPIC,
      .session.last_will.msg = last_will_message,
      .session.last_will.msg_len = last_will_message_count,
      .session.last_will.qos = 1,
      .session.last_will.retain = true,
  };

  client_ = esp_mqtt_client_init(&mqtt5_cfg);

  /* Set connection properties and user properties */
  esp_mqtt5_client_set_user_property(&connect_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_user_property(&connect_property.will_user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_connect_property(client_, &connect_property);

  // Need to delete user property
  esp_mqtt5_client_delete_user_property(connect_property.user_property);
  esp_mqtt5_client_delete_user_property(connect_property.will_user_property);

  // Register event handler
  esp_mqtt_client_register_event(client_, ESP_EVENT_ANY_ID, mqtt5_event_handler,
                                 NULL);
  ESP_ERROR_CHECK(esp_mqtt_client_start(client_));
}

void mqtt5_check_connection_task(void *pvParameters) {
  for (;;) {
    EventBits_t bits =
        xEventGroupWaitBits(s_mqtt5_event_group_, MQTT5_CONNECTION_FAILED_BIT,
                            pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & MQTT5_CONNECTION_FAILED_BIT) {
      ESP_LOGI(TAG, "MQTT connection failed. Wait before retrying.");
      ESP_ERROR_CHECK(esp_mqtt_client_stop(client_));
      vTaskDelay(pdMS_TO_TICKS(CONFIG_MQTT_WAIT_TIME_BETWEEN_RETRY_MS));
      disconnect_counter_ = 0;
      xEventGroupClearBits(s_mqtt5_event_group_, MQTT5_CONNECTION_FAILED_BIT);
      ESP_ERROR_CHECK(esp_mqtt_client_start(client_));
    }
  }
}

/* Stack Size for the connection check task*/
#define STACK_SIZE 2048

/* Structure that will hold the TCB of the task being created. */
static StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
static StackType_t xStack[STACK_SIZE];

TaskHandle_t mqtt5_create_connection_checker_task() {
  // Static task without dynamic memory allocation
  TaskHandle_t task_handle = xTaskCreateStatic(
      mqtt5_check_connection_task, "MQTT5ConnectionChecker", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure.
                             */
  return task_handle;
}
