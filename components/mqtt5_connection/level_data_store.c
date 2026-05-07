#include "level_data_store.h"
#include "configuration.h"
#include "generic_data_store.h"

#include "esp_log.h"
#include <sys/time.h>

#define MAX_LEVEL_DATA_ITEMS                                                   \
  (CONFIG_MQTT_DATA_LOGGING_LIGHT_STORE_SIZE_MULTIPLE *                        \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct level_data_item_t)))

static struct generic_data_store_t level_data_store;

cJSON *level_data_item_to_json(const struct level_data_item_t *item) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "id", configuration.id);

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
  cJSON_AddNumberToObject(data, "distance_mm", item->distance_mm);
  return data;
}

void level_data_store_init() {
  generic_data_store_init(&level_data_store, sizeof(struct level_data_item_t),
                          MAX_LEVEL_DATA_ITEMS, "/store/log_data/level",
                          (to_json_func_t)level_data_item_to_json);
}

void level_data_store_restore_stack() {
  generic_data_store_restore_stack(&level_data_store);
}

void level_data_store_push(uint32_t distance_mm) {
  time_t current_time;
  time(&current_time);
  struct level_data_item_t item = {.timestamp = current_time,
                                   .distance_mm = distance_mm};
  generic_data_store_push(&level_data_store, &item);
}

bool level_data_store_pop_and_stash(struct level_data_item_t *item) {
  return generic_data_store_pop_and_stash(&level_data_store, item);
}
