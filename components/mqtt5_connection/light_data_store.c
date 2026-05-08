#include "light_data_store.h"
#include "configuration.h"
#include "generic_data_store.h"

#include "esp_log.h"
#include <sys/time.h>

#define MAX_LIGHT_DATA_ITEMS                                                   \
  (CONFIG_MQTT_DATA_LOGGING_STORE_SIZE_MULTIPLE *                              \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct light_data_item_t)))

static struct generic_data_store_t light_data_store;

cJSON *light_data_item_to_json(const struct light_data_item_t *item) {
  cJSON *data = cJSON_CreateObject();
  // add id
  cJSON_AddNumberToObject(data, "id", configuration.id);
  // add current timestamp
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&item->timestamp, &timeinfo);
  char time_string[32];
  int time_len = strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_len += snprintf(time_string + time_len, sizeof(time_string) - time_len,
                       ".%06ld", tv_now.tv_usec);
  strftime(time_string + time_len, sizeof(time_string) - time_len, "%z",
           &timeinfo);
  cJSON_AddStringToObject(data, "ts", time_string);
  cJSON_AddNumberToObject(data, "intensity", item->intensity);
  return data;
}

void light_data_store_init() {
  generic_data_store_init(&light_data_store, sizeof(struct light_data_item_t),
                          MAX_LIGHT_DATA_ITEMS, "/store/log_data/light",
                          (to_json_func_t)light_data_item_to_json);
}

void light_data_store_restore_stack() {
  generic_data_store_restore_stack(&light_data_store);
}

void light_data_store_push(uint16_t intensity) {
  time_t current_time;
  time(&current_time);
  struct light_data_item_t item = {.timestamp = current_time,
                                   .intensity = intensity};
  generic_data_store_push(&light_data_store, &item);
}

bool light_data_store_pop_and_stash(struct light_data_item_t *item) {
  return generic_data_store_pop_and_stash(&light_data_store, item);
}
