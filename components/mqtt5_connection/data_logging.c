#include "data_logging.h"

#include "configuration.h"
#include "esp_log.h"
#include "mqtt_shared.h"
#include <sys/time.h>
#include <time.h>

#define MAX_SAVED_MESSAGES 40

static const char *TAG = "mqtt5_data_logging";

/**
 * @brief Enum with the status of amessage.
 *
 */
enum sent_map_status {
  SENT_MAP_EMPTY = 0,
  SENT_MAP_SCHEDULED = 1,
  SENT_MAP_ENQUEUED = 2,
};

/**
 * @brief Struct to save the sent messages.
 *
 */
struct sent_message_data {
  enum sent_map_status status;
  int message_id;
  char *data;
  char *topic;
};

/**
 * @brief index of the next free message in the sent map.
 *
 */
static int free_sent_map_index = 0;

/**
 * @brief Array to save the sent messages.
 *
 */
static struct sent_message_data sent_map[MAX_SAVED_MESSAGES];

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

/**
 * @brief Build the data string to send to the mqtt broker.
 *
 * Add id and timestamp to the json data and return as a string.
 *
 * @param data cJSON data to send.
 * @return char* string with the json data needs to be freed.
 */
char *build_data_string(cJSON *data) {
  // add id
  cJSON_AddNumberToObject(data, "id", configuration.id);
  // add current timestamp
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char time_string[70];
  int time_len = strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_len += snprintf(time_string + time_len, sizeof(time_string) - time_len,
                       ".%06ld", tv_now.tv_usec);
  strftime(time_string + time_len, sizeof(time_string) - time_len, "%z",
           &timeinfo);
  cJSON_AddStringToObject(data, "ts", time_string);

  return cJSON_PrintUnformatted(data);
}

/**
 * @brief Send the message from the sent map to the mqtt broker.
 *
 * @param index index of the message in the sent map.
 */
void sent_message_from_map(int index) {

  esp_mqtt5_client_set_user_property(&config_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client_, &config_publish_property);
  int msg_id = esp_mqtt_client_enqueue(client_, sent_map[index].topic,
                                       sent_map[index].data, 0, 1, 1, true);
  sent_map[index].message_id = msg_id;
  if (msg_id >= 0) {
    sent_map[index].status = SENT_MAP_ENQUEUED;
  }

  esp_mqtt5_client_delete_user_property(config_publish_property.user_property);
  config_publish_property.user_property = NULL;
  ESP_LOGI(TAG, "sent successful, msg_id=%d", msg_id);
}

void send_timed_data(const char *topic, cJSON *data) {
  char *json_string = build_data_string(data);
  cJSON_Delete(data);

  // add to sent map
  const int curr_index = free_sent_map_index;
  free_sent_map_index++;
  if (free_sent_map_index >= MAX_SAVED_MESSAGES) {
    free_sent_map_index = 0;
  }

  sent_map[curr_index].status = SENT_MAP_SCHEDULED;
  sent_map[curr_index].message_id = -1;
  sent_map[curr_index].data = json_string;
  sent_map[curr_index].topic = strdup(topic);

  if (!mqtt5_connected) {
    ESP_LOGI(TAG, "Client not connected, message saved");
    return;
  }
  sent_message_from_map(curr_index);
}

void remove_from_sent_map(int msg_id) {
  for (int i = 0; i < MAX_SAVED_MESSAGES; i++) {
    if (sent_map[i].message_id == msg_id) {
      free(sent_map[i].data);
      free(sent_map[i].topic);
      sent_map[i].status = SENT_MAP_EMPTY;
      sent_map[i].message_id = -1;
      sent_map[i].data = NULL;
      sent_map[i].topic = NULL;
      break;
    }
  }
}

void resend_saved_messages() {
  for (int i = 0; i < MAX_SAVED_MESSAGES; i++) {
    if (sent_map[i].status == SENT_MAP_SCHEDULED) {
      sent_message_from_map(i);
    }
  }
}

void reschedule_message(int message_id) {
  for (int i = 0; i < MAX_SAVED_MESSAGES; i++) {
    if (sent_map[i].message_id == message_id) {
      sent_map[i].status = SENT_MAP_SCHEDULED;
      break;
    }
  }
}
