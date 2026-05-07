#include "pump_data_store.h"
#include "configuration.h"
#include "generic_data_store.h"

#include "esp_log.h"
#include <sys/time.h>

#define MAX_PUMP_DATA_ITEMS                                                    \
  (CONFIG_MQTT_DATA_LOGGING_PUMP_STORE_SIZE_MULTIPLE *                         \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct pump_data_item_t)))

static struct generic_data_store_t pump_data_store;

cJSON *pump_data_item_to_json(const struct pump_data_item_t *item) {
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
  if (item->pump_on) {
    cJSON_AddStringToObject(data, "status", "start");
  } else {
    cJSON_AddStringToObject(data, "status", "stop");
  }

  return data;
}

void pump_data_store_init() {
  generic_data_store_init(&pump_data_store, sizeof(struct pump_data_item_t),
                          MAX_PUMP_DATA_ITEMS, "/store/log_data/pump",
                          (to_json_func_t)pump_data_item_to_json);
}

void pump_data_store_restore_stack() {
  generic_data_store_restore_stack(&pump_data_store);
}

void pump_data_store_push(bool pump_on) {
  time_t current_time;
  time(&current_time);
  struct pump_data_item_t item = {.timestamp = current_time,
                                  .pump_on = pump_on};
  generic_data_store_push(&pump_data_store, &item);
}

bool pump_data_store_pop_and_stash(struct pump_data_item_t *item) {
  return generic_data_store_pop_and_stash(&pump_data_store, item);
}
