#include "data_logging.h"

#include "configuration.h"
#include "esp_log.h"
#include "mqtt_shared.h"
#include <sys/time.h>
#include <time.h>

static const char *TAG = "mqtt5_data_logging";

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

void send_timed_data(const char *topic, cJSON *data) {
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

  // send data
  char *json_string = cJSON_PrintUnformatted(data);

  esp_mqtt5_client_set_user_property(&config_publish_property.user_property,
                                     user_property_arr, USE_PROPERTY_ARR_SIZE);
  esp_mqtt5_client_set_publish_property(client_, &config_publish_property);
  int msg_id = esp_mqtt_client_publish(client_, topic, json_string, 0, 1, 1);
  esp_mqtt5_client_delete_user_property(config_publish_property.user_property);
  config_publish_property.user_property = NULL;
  ESP_LOGI(TAG, "sent successful, msg_id=%d", msg_id);
  cJSON_Delete(data);
  free(json_string);
}
