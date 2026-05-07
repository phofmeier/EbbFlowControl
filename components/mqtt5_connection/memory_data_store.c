#include "memory_data_store.h"
#include "configuration.h"
#include "generic_data_store.h"

#include "esp_log.h"
#include <sys/time.h>

#define MAX_MEMORY_DATA_ITEMS                                                  \
  (CONFIG_MQTT_DATA_LOGGING_MEMORY_STORE_SIZE_MULTIPLE *                       \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct memory_data_item_t)))

static struct generic_data_store_t memory_data_store;

cJSON *memory_data_item_to_json(const struct memory_data_item_t *item) {
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
  cJSON_AddNumberToObject(data, "free_heap_size", item->free_heap_size);
  cJSON_AddNumberToObject(data, "min_free_heap_size", item->min_free_heap_size);
  cJSON_AddNumberToObject(data, "store_total_bytes", item->store_total_bytes);
  cJSON_AddNumberToObject(data, "store_used_bytes", item->store_used_bytes);

  return data;
}

void memory_data_store_init() {
  generic_data_store_init(&memory_data_store, sizeof(struct memory_data_item_t),
                          MAX_MEMORY_DATA_ITEMS, "/store/log_data/mem",
                          (to_json_func_t)memory_data_item_to_json);
}

void memory_data_store_restore_stack() {
  generic_data_store_restore_stack(&memory_data_store);
}

void memory_data_store_push(const uint32_t free_heap_size,
                            const uint32_t min_free_heap_size,
                            const size_t store_total_bytes,
                            const size_t store_used_bytes) {
  time_t current_time;
  time(&current_time);
  struct memory_data_item_t item = {.timestamp = current_time,
                                    .free_heap_size = free_heap_size,
                                    .min_free_heap_size = min_free_heap_size,
                                    .store_total_bytes = store_total_bytes,
                                    .store_used_bytes = store_used_bytes};
  generic_data_store_push(&memory_data_store, &item);
}

bool memory_data_store_pop_and_stash(struct memory_data_item_t *item) {
  return generic_data_store_pop_and_stash(&memory_data_store, item);
}
